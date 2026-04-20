#pragma once

#include <string>
#include <string_view>

namespace real_medium::utils::thumbnail {

struct Options {
    int width        = 480;  // ffmpeg scale=W:-2 preserves aspect, forces even height
    int seek_seconds = 3;    // extraction timestamp; falls back to t=0 for short clips
    int quality      = 3;    // ffmpeg -q:v  (2..5 is the sweet spot for JPEG)
};

struct StoryboardOptions {
    int tile_width     = 160;
    int tile_height    = 90;
    int tiles_per_row  = 10;
    int rows           = 10;  // 10x10 = 100 frames
    int sample_fps_num = 1;   // sample rate = num/den frames per second
    int sample_fps_den = 2;   // default fps=1/2  → one frame every 2s → covers ~200s
    int quality        = 5;
};

// Extract a single JPEG thumbnail from a video blob via the ffmpeg CLI.
// base_name disambiguates concurrent temp files (pass the video UUID).
// Throws std::runtime_error on failure. Blocking — run on fs-task-processor.
std::string Extract(std::string_view video_bytes,
                    std::string_view mime,
                    std::string_view base_name,
                    const Options& opts = {});

// Extract a storyboard sprite: a `rows x tiles_per_row` grid of small frames
// sampled at `sample_fps_num/sample_fps_den` fps. Encoded as a single JPEG.
std::string ExtractStoryboard(std::string_view video_bytes,
                              std::string_view mime,
                              std::string_view base_name,
                              const StoryboardOptions& opts = {});

}  // namespace real_medium::utils::thumbnail
