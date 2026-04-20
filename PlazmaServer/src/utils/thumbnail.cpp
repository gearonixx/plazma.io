#include "thumbnail.hpp"

#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

#include <userver/logging/log.hpp>

namespace real_medium::utils::thumbnail {
namespace {

// Map MIME to a file extension. Giving ffmpeg a matching suffix helps its
// container-probing heuristics lock onto the right demuxer immediately.
std::string ExtFromMime(std::string_view mime) {
    if (mime == "video/mp4")        return "mp4";
    if (mime == "video/webm")       return "webm";
    if (mime == "video/x-matroska") return "mkv";
    if (mime == "video/quicktime")  return "mov";
    if (mime == "video/x-msvideo")  return "avi";
    if (mime == "video/ogg")        return "ogv";
    return "bin";
}

// Compose a unique temp path under /tmp. We can't use mkstemp because ffmpeg
// wants a specific extension; pid+base_name+counter is collision-resistant
// enough for our concurrency (single-digit parallel uploads per process).
std::string MakeTempPath(std::string_view prefix,
                         std::string_view base_name,
                         std::string_view ext) {
    static std::atomic<unsigned long> counter{0};
    std::ostringstream oss;
    oss << "/tmp/" << prefix
        << '_' << getpid()
        << '_' << base_name
        << '_' << counter.fetch_add(1, std::memory_order_relaxed)
        << '.' << ext;
    return oss.str();
}

void WriteFile(const std::string& path, std::string_view data) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("thumbnail: open for write failed: " + path);
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!out) throw std::runtime_error("thumbnail: write failed: " + path);
}

std::string ReadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("thumbnail: open for read failed: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Quote a single argv element for a POSIX sh command line. Any single-quote in
// the value is terminated, escaped, and reopened — classic `'\''` dance.
std::string ShellQuote(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('\'');
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else           out.push_back(c);
    }
    out.push_back('\'');
    return out;
}

// Run ffmpeg with a fixed binary + explicitly-quoted argv. All values come from
// server-controlled strings or integers, but we quote anyway so a filename that
// somehow contained spaces/quotes wouldn't break the command or inject a shell
// metacharacter. Captures stderr+stdout; logs on nonzero exit.
int RunFfmpeg(const std::vector<std::string>& args) {
    std::ostringstream cmd;
    cmd << "ffmpeg -hide_banner -loglevel error -nostdin";
    for (const auto& a : args) {
        cmd << ' ' << ShellQuote(a);
    }
    cmd << " 2>&1";
    const std::string cmd_str = cmd.str();

    FILE* fp = popen(cmd_str.c_str(), "r");
    if (!fp) throw std::runtime_error("thumbnail: popen failed");

    std::string output;
    std::array<char, 512> buf{};
    while (std::fgets(buf.data(), static_cast<int>(buf.size()), fp)) {
        output.append(buf.data());
    }
    const int status = pclose(fp);
    if (status != 0) {
        LOG_WARNING() << "ffmpeg failed (status=" << status << "): "
                      << (output.empty() ? "<no output>" : output);
    }
    return status;
}

// RAII temp-file cleanup. Unlink is best-effort and silent on missing files.
struct TempFile {
    std::string path;
    ~TempFile() { if (!path.empty()) ::unlink(path.c_str()); }
};

}  // namespace

std::string Extract(std::string_view video_bytes,
                    std::string_view mime,
                    std::string_view base_name,
                    const Options& opts) {
    if (video_bytes.empty()) throw std::runtime_error("thumbnail: empty video bytes");

    const std::string ext = ExtFromMime(mime);
    TempFile in{MakeTempPath("plazma_vin", base_name, ext)};
    TempFile out{MakeTempPath("plazma_thumb", base_name, "jpg")};

    WriteFile(in.path, video_bytes);

    const std::string scale = "scale=" + std::to_string(opts.width) + ":-2";

    // Primary attempt: seek to seek_seconds before decoding (fast, pre-decode seek).
    int rc = RunFfmpeg({
        "-y",
        "-ss", std::to_string(opts.seek_seconds),
        "-i",  in.path,
        "-frames:v", "1",
        "-vf", scale,
        "-q:v", std::to_string(opts.quality),
        "-f", "image2",
        out.path,
    });

    // Short videos (< seek_seconds): retry from t=0 without the seek.
    if (rc != 0) {
        rc = RunFfmpeg({
            "-y",
            "-i",  in.path,
            "-frames:v", "1",
            "-vf", scale,
            "-q:v", std::to_string(opts.quality),
            "-f", "image2",
            out.path,
        });
    }

    if (rc != 0) throw std::runtime_error("thumbnail: ffmpeg extraction failed");
    auto jpg = ReadFile(out.path);
    if (jpg.empty()) throw std::runtime_error("thumbnail: ffmpeg produced empty output");
    return jpg;
}

std::string ExtractStoryboard(std::string_view video_bytes,
                              std::string_view mime,
                              std::string_view base_name,
                              const StoryboardOptions& opts) {
    if (video_bytes.empty()) throw std::runtime_error("storyboard: empty video bytes");

    const std::string ext = ExtFromMime(mime);
    TempFile in{MakeTempPath("plazma_vin", base_name, ext)};
    TempFile out{MakeTempPath("plazma_sb", base_name, "jpg")};

    WriteFile(in.path, video_bytes);

    // fps=N/D samples N frames every D seconds; scale normalizes tile size;
    // tile=CxR assembles them into a single image. ffmpeg stops producing
    // output once the grid is full, so long videos are implicitly clamped.
    std::ostringstream vf;
    vf << "fps=" << opts.sample_fps_num << '/' << opts.sample_fps_den
       << ",scale=" << opts.tile_width << ':' << opts.tile_height
       << ",tile=" << opts.tiles_per_row << 'x' << opts.rows;

    const int rc = RunFfmpeg({
        "-y",
        "-i", in.path,
        "-frames:v", "1",
        "-vf", vf.str(),
        "-q:v", std::to_string(opts.quality),
        "-f", "image2",
        out.path,
    });
    if (rc != 0) throw std::runtime_error("storyboard: ffmpeg extraction failed");

    auto jpg = ReadFile(out.path);
    if (jpg.empty()) throw std::runtime_error("storyboard: ffmpeg produced empty output");
    return jpg;
}

}  // namespace real_medium::utils::thumbnail
