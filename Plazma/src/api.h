#pragma once

#include <QHttpMultiPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QUrlQuery>

#include "session.h"
#include "storage/task_queue.h"

enum class HttpMethod { kGet, kPost, kHead, kPut, kDelete, kPatch, kOptions };

enum class Endpoint { kAuthLogin, kVideos, kVideosUpload };

[[nodiscard]] static QString toEndpointString(Endpoint endpoint) {
    static const std::unordered_map<Endpoint, QString> kPaths = {
        {Endpoint::kAuthLogin, QStringLiteral("/v1/auth/login")},
        {Endpoint::kVideos, QStringLiteral("/v1/videos")},
        {Endpoint::kVideosUpload, QStringLiteral("/v1/videos/upload")},
    };

    return kPaths.at(endpoint);
}

[[nodiscard]] static QByteArray toMethodString(HttpMethod method) {
    static const std::unordered_map<HttpMethod, QByteArray> kMethods = {
        {HttpMethod::kGet, "GET"},
        {HttpMethod::kPost, "POST"},
        {HttpMethod::kHead, "HEAD"},
        {HttpMethod::kPut, "PUT"},
        {HttpMethod::kDelete, "DELETE"},
        {HttpMethod::kPatch, "PATCH"},
        {HttpMethod::kOptions, "OPTIONS"},
    };
    return kMethods.at(method);
}

class RequestBuilder {
public:
    RequestBuilder(QNetworkAccessManager* nam, QNetworkRequest req, HttpMethod method, QByteArray body)
        : nam_(nam), req_(std::move(req)), method_(method), body_(std::move(body)) {}

    RequestBuilder(QNetworkAccessManager* nam, QNetworkRequest req, QHttpMultiPart* multiPart)
        : nam_(nam), req_(std::move(req)), method_(HttpMethod::kPost), multiPart_(multiPart) {}

    RequestBuilder& done(Fn<void(QJsonObject)> cb) {
        done_ = std::move(cb);
        return *this;
    }
    RequestBuilder& fail(Fn<void(int, QString)> cb) {
        fail_ = std::move(cb);
        return *this;
    }
    void send();

private:
    QNetworkAccessManager* nam_;
    QNetworkRequest req_;
    HttpMethod method_;
    QByteArray body_;
    QHttpMultiPart* multiPart_ = nullptr;
    Fn<void(QJsonObject)> done_;
    Fn<void(int, QString)> fail_;
};

class Api : public QObject {
    Q_OBJECT
public:
    explicit Api(QObject* parent = nullptr)
        : QObject(parent),
          nam_(new QNetworkAccessManager(this)),
          file_loader_(std::make_unique<plazma::task_queue::TaskQueue>()) {}

    // request() returns a RequestBuilder — you must chain .send() on it or
    // the HTTP call never happens, so dropping the result is always a bug.
    [[nodiscard]] RequestBuilder
    request(Endpoint endpoint, const QJsonObject& body = {}, const HttpMethod& method = HttpMethod::kGet);
    [[nodiscard]] RequestBuilder request(
        Endpoint endpoint,
        const QUrlQuery& params,
        const QJsonObject& body = {},
        const HttpMethod& method = HttpMethod::kGet
    );
    [[nodiscard]] RequestBuilder request(Endpoint endpoint, QHttpMultiPart* multiPart);
    void loginUser(const UserLogin& user);
    void uploadFile(
        Endpoint endpoint,
        const QString& fieldName,
        const QString& filename,
        const QString& mime,
        const QByteArray& filedata,
        const QByteArray& thumbnail = {},
        const QString& thumbnailMime = QStringLiteral("image/jpeg")
    );
    // Fetches the video feed. If `query` is non-empty, passes it through as the
    // `?q=` search param; the server is expected to match against title/author
    // (see the contract comment in api.cpp). Empty `query` returns the full
    // chronological feed.
    void fetchVideos(const QString& query, Fn<void(QJsonArray)> onSuccess, Fn<void(int, QString)> onError = {});

    // Mutations on a single video, keyed by `id`. The REST contract is:
    //   PATCH  /v1/videos/{id}   body: {"title": "..."}   → 200
    //   DELETE /v1/videos/{id}                            → 204
    // Paths are built against kBaseUrl directly since the Endpoint enum only
    // covers fixed paths. Both callbacks are optional.
    void renameVideo(
        const QString& id,
        const QString& newTitle,
        Fn<void()> onSuccess = {},
        Fn<void(int, QString)> onError = {}
    );
    void deleteVideo(const QString& id, Fn<void()> onSuccess = {}, Fn<void(int, QString)> onError = {});

    // ─── Playlists ────────────────────────────────────────────────────────
    //
    // All playlist endpoints require auth and are scoped to the caller. The
    // wire format is documented authoritatively in
    // `PlazmaServer/docs/playlists.md`; this header keeps callsite ergonomics
    // small and matches the existing `done/fail` callback shape.
    //
    // The client supplies its own UUID v7 for `createPlaylist` so that
    // PlaylistsModel can preserve its synchronous `createPlaylist(name) →
    // id` contract — the id is generated locally up front, then this call
    // upserts it on the server. See playlists.md §10 for idempotency.
    void listPlaylists(
        Fn<void(QJsonArray /*playlists*/)> onSuccess,
        Fn<void(int, QString)> onError = {}
    );
    void createPlaylistRemote(
        const QString& id,
        const QString& name,
        Fn<void(QJsonObject /*playlist*/)> onSuccess = {},
        Fn<void(int, QString)> onError = {}
    );
    void renamePlaylistRemote(
        const QString& id,
        const QString& newName,
        Fn<void(QJsonObject /*playlist*/)> onSuccess = {},
        Fn<void(int, QString)> onError = {}
    );
    void deletePlaylistRemote(
        const QString& id,
        Fn<void()> onSuccess = {},
        Fn<void(int, QString)> onError = {}
    );
    void addPlaylistItem(
        const QString& playlistId,
        const QJsonObject& itemSnapshot,  // {video_id, title, url, …} — see playlists.md §3.7
        Fn<void(QJsonObject /*item*/, QJsonObject /*playlist*/)> onSuccess = {},
        Fn<void(int, QString)> onError = {}
    );
    void removePlaylistItem(
        const QString& playlistId,
        const QString& videoId,
        Fn<void()> onSuccess = {},
        Fn<void(int, QString)> onError = {}
    );

    // Streaming GET for offline downloads. Returns the underlying QNetworkReply
    // so the caller can wire readyRead()/downloadProgress()/finished() to write
    // bytes incrementally to disk instead of buffering the whole file in RAM.
    //
    // `resumeOffset` > 0 sends a `Range: bytes=N-` request so an interrupted
    // transfer can pick up where it left off. The caller is responsible for
    // checking the response status — 206 confirms the resume, 200 means the
    // server ignored the Range header and is sending the whole file from byte 0
    // (so the on-disk .part must be truncated before bytes are appended).
    //
    // Ownership: the reply's parent is the QNAM (Qt's default); the caller
    // must call reply->deleteLater() when they're done, and if they want to
    // cancel mid-transfer they can call reply->abort(). We don't go through
    // RequestBuilder because RB collapses the stream into a single QJsonObject
    // callback — fine for JSON APIs, useless for a 400 MB video file.
    [[nodiscard]] QNetworkReply* startDownload(const QUrl& url, qint64 resumeOffset = 0);

    [[nodiscard]] plazma::task_queue::TaskQueue* fileLoader() const { return file_loader_.get(); }

    void setAuthToken(QByteArray token) { authToken_ = std::move(token); }
    [[nodiscard]] bool hasAuthToken() const { return !authToken_.isEmpty(); }

signals:
    void loginSuccess(UserLogin user);
    void loginError(int statusCode, QString error);
    void uploadFinished(QString endpoint, QString filename);
    void uploadFailed(QString endpoint, int statusCode, QString error);

private:
    static constexpr auto kBaseUrl = "http://localhost:8080";

    QNetworkAccessManager* nam_;
    QByteArray authToken_;
    int requestId_ = 0;

    void applyAuth(QNetworkRequest& req) const {
        if (!authToken_.isEmpty()) req.setRawHeader("Authorization", "Bearer " + authToken_);
    }

    std::unique_ptr<plazma::task_queue::TaskQueue> file_loader_;
};
