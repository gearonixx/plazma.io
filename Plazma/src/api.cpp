#include "api.h"

#include <QUrlQuery>

#include "validators/json_guards.h"

namespace validators {
std::optional<UserLogin> ensureLoginResponse(const QJsonObject& json, QString& error);
}

void RequestBuilder::send() {
    QNetworkReply* reply = nullptr;

    if (multiPart_) {
        reply = nam_->post(req_, multiPart_);
        multiPart_->setParent(reply);
    } else {
        reply = nam_->sendCustomRequest(req_, toMethodString(method_), body_);
    }

    QObject::connect(
        reply, &QNetworkReply::finished, reply, [reply, done = std::move(done_), fail = std::move(fail_)]() {
            if (reply->error() != QNetworkReply::NoError) {
                if (fail) {
                    auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    fail(code, reply->errorString());
                }
            } else if (done) {
                auto doc = QJsonDocument::fromJson(reply->readAll());
                done(doc.object());
            }
            reply->deleteLater();
        }
    );
}

RequestBuilder Api::request(Endpoint endpoint, const QJsonObject& body, const HttpMethod& method) {
    return request(endpoint, QUrlQuery{}, body, method);
}

RequestBuilder Api::request(
    Endpoint endpoint,
    const QUrlQuery& params,
    const QJsonObject& body,
    const HttpMethod& method
) {
    Q_ASSERT(nam_ != nullptr);

    const auto path = toEndpointString(endpoint);
    QUrl url(QString(kBaseUrl) + path);
    if (!params.isEmpty()) url.setQuery(params);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Connection", "close");
    applyAuth(req);

    qDebug() << "[API]" << toMethodString(method) << url.toString(QUrl::RemoveUserInfo);

    return RequestBuilder(nam_, req, method, QJsonDocument(body).toJson(QJsonDocument::Compact));
}

RequestBuilder Api::request(Endpoint endpoint, QHttpMultiPart* multiPart) {
    Q_ASSERT(nam_ != nullptr);

    const auto path = toEndpointString(endpoint);
    QNetworkRequest req(QUrl(QString(kBaseUrl) + path));
    applyAuth(req);

    qDebug() << "[API] POST (multipart)" << path;

    return RequestBuilder(nam_, req, multiPart);
}

void Api::loginUser(const UserLogin& user) {
    QJsonObject body{
        {"user_id", user.userId},
        {"username", user.username},
        {"first_name", user.firstName},
        {"last_name", user.lastName},
        {"phone_number", user.phoneNumber},
        {"is_premium", user.isPremium},
    };

    request(Endpoint::kAuthLogin, body, HttpMethod::kPost)
        .done([this](const QJsonObject& json) {
            QString validationError;

            qDebug() << json;

            const auto user = validators::ensureLoginResponse(json, validationError);

            if (!user) {
                qWarning() << "[API] loginUser validation failed:" << validationError;
                emit loginError(0, validationError);
                return;
            }

            const auto token = validators::extractNonEmptyString(json, QStringLiteral("token"), validationError);
            if (!token) {
                qWarning() << "[API] loginUser missing token:" << validationError;
                emit loginError(0, validationError);
                return;
            }
            setAuthToken(token->toUtf8());

            qDebug() << "[API] loginUser =>" << user->userId << user->username;
            emit loginSuccess(*user);
        })
        .fail([this](int statusCode, const QString& error) {
            qWarning() << "[API] loginUser failed — POST"
                       << (QString(kBaseUrl) + toEndpointString(Endpoint::kAuthLogin))
                       << "status:" << statusCode << "error:" << error;
            emit loginError(statusCode, error);
        })
        .send();
}

void Api::uploadFile(
    Endpoint endpoint,
    const QString& fieldName,
    const QString& filename,
    const QString& mime,
    const QByteArray& filedata,
    const QByteArray& thumbnail,
    const QString& thumbnailMime
) {
    Q_ASSERT(nam_ != nullptr);

    auto* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, mime);
    filePart.setHeader(
        QNetworkRequest::ContentDispositionHeader,
        QStringLiteral("form-data; name=\"%1\"; filename=\"%2\"").arg(fieldName, filename)
    );
    filePart.setBody(filedata);
    multiPart->append(filePart);

    // Optional optimistic thumbnail (Phase 2). The server keys off name="thumbnail",
    // stores it immediately, and its async ffmpeg job skips primary extraction.
    if (!thumbnail.isEmpty()) {
        QHttpPart thumbPart;
        thumbPart.setHeader(QNetworkRequest::ContentTypeHeader, thumbnailMime);
        thumbPart.setHeader(
            QNetworkRequest::ContentDispositionHeader,
            QStringLiteral("form-data; name=\"thumbnail\"; filename=\"thumb.jpg\"")
        );
        thumbPart.setBody(thumbnail);
        multiPart->append(thumbPart);
    }

    const auto path = toEndpointString(endpoint);
    request(endpoint, multiPart)
        .done([this, path, filename](const QJsonObject&) {
            qDebug() << "[API] upload ok:" << path;
            emit uploadFinished(path, filename);
        })
        .fail([this, path](int code, const QString& error) {
            qWarning() << "[API] upload failed:" << code << error;
            emit uploadFailed(path, code, error);
        })
        .send();
}

// ─── Feed / search contract (frontend ↔ backend) ────────────────────────────
//
//   GET /v1/videos              → full chronological feed (latest first).
//   GET /v1/videos?q=<query>    → search results for <query>.
//
// Request
//   - `q` is a URL-encoded UTF-8 string, trimmed client-side. Max length the
//     frontend will ever send is ~256 chars (the search TextField).
//   - Empty/absent `q` means "no search — return the default feed". The
//     frontend sends `q` only when non-empty, so the handler should treat
//     missing and empty-string `q` identically.
//
// Response (200)
//   {
//     "videos": [
//       {
//         "id":         "<string, stable video id>",
//         "title":      "<string>",
//         "url":        "<string, absolute media URL>",
//         "size":       <number, bytes>,
//         "mime":       "<string, e.g. video/mp4>",
//         "author":     "<string>",
//         "created_at": "<ISO-8601 string>",
//         "thumbnail":  "<string, absolute URL or empty>",
//         "storyboard": "<string, absolute URL of the 10×10 sprite or empty>"
//       },
//       ...
//     ]
//   }
//
// Ordering
//   - No `q`: `created_at` descending.
//   - With `q`: relevance descending (ElasticSearch BM25 is fine — suggested
//     field boosting: title^3, author^2, description^1). Tie-break by
//     `created_at` desc so results are stable for identical scores.
//
// Errors
//   - Non-2xx status + plain-text or JSON body. The frontend surfaces
//     `"HTTP <code>: <reply->errorString()>"` in the error banner, so human
//     readable messages help but aren't parsed.
//
// Performance notes for the handler
//   - The frontend debounces typing (~470ms combined) and dedups by query
//     string, but does NOT cancel in-flight requests on the wire — it just
//     ignores stale responses client-side. So the handler must be cheap
//     enough that back-to-back queries don't pile up; target p95 < 200ms.
//   - No pagination yet. Cap the response at ~200 rows until we add a cursor.
// ────────────────────────────────────────────────────────────────────────────
void Api::renameVideo(
    const QString& id,
    const QString& newTitle,
    Fn<void()> onSuccess,
    Fn<void(int, QString)> onError
) {
    Q_ASSERT(nam_ != nullptr);

    QNetworkRequest req(QUrl(QString(kBaseUrl) + QStringLiteral("/v1/videos/") + id));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Connection", "close");
    applyAuth(req);

    const auto body = QJsonDocument(QJsonObject{{"title", newTitle}}).toJson(QJsonDocument::Compact);

    qDebug() << "[API] PATCH" << req.url().toString(QUrl::RemoveUserInfo);

    RequestBuilder(nam_, req, HttpMethod::kPatch, body)
        .done([resolve = std::move(onSuccess), id](const QJsonObject&) {
            qDebug() << "[API] renameVideo ok:" << id;
            if (resolve) resolve();
        })
        .fail([reject = std::move(onError), id](int code, const QString& error) {
            qWarning() << "[API] renameVideo failed:" << id << code << error;
            if (reject) reject(code, error);
        })
        .send();
}

QNetworkReply* Api::startDownload(const QUrl& url, qint64 resumeOffset) {
    Q_ASSERT(nam_ != nullptr);

    QNetworkRequest req(url);
    // Identify ourselves; some CDNs geo-throttle empty UA traffic.
    req.setRawHeader("User-Agent", "Plazma/1.0 (+https://plazma.local)");
    // Force identity encoding. Videos are already compressed, so a transparent
    // gzip/deflate layer just burns CPU and — worse — makes Content-Length
    // refer to encoded bytes while readyRead() delivers decoded ones, which
    // breaks both progress accounting and any byte-range resume that follows.
    req.setRawHeader("Accept-Encoding", "identity");
    // Opt into HTTP/2 multiplexing where the server offers it.
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    // Qt's default cap is 50, which is way more than any sane CDN needs and
    // gives a malicious / misconfigured server a long redirect chain to walk
    // before we time out. tdesktop uses 5 (storage/file_download_web.cpp).
    req.setMaximumRedirectsAllowed(5);
    // Idle-transfer timeout. Qt 6 resets this on every received byte, so it
    // aborts on a true stall (wifi drop, server stuck) without killing
    // slow-but-alive transfers. Setting it on the request is portable across
    // Qt minor versions; the equivalent QNetworkReply::setTransferTimeout
    // was deprecated/removed in some 6.x patch releases.
    constexpr int kIdleTimeoutMs = 30000;
    req.setTransferTimeout(kIdleTimeoutMs);

    if (resumeOffset > 0) {
        const auto rangeHeader = QByteArrayLiteral("bytes=") + QByteArray::number(resumeOffset) + '-';
        req.setRawHeader("Range", rangeHeader);
        qDebug() << "[API] GET (download, resume from" << resumeOffset << ")" << url.toString(QUrl::RemoveUserInfo);
    } else {
        qDebug() << "[API] GET (download)" << url.toString(QUrl::RemoveUserInfo);
    }
    return nam_->get(req);
}

void Api::deleteVideo(const QString& id, Fn<void()> onSuccess, Fn<void(int, QString)> onError) {
    Q_ASSERT(nam_ != nullptr);

    QNetworkRequest req(QUrl(QString(kBaseUrl) + QStringLiteral("/v1/videos/") + id));
    req.setRawHeader("Connection", "close");
    applyAuth(req);

    qDebug() << "[API] DELETE" << req.url().toString(QUrl::RemoveUserInfo);

    RequestBuilder(nam_, req, HttpMethod::kDelete, QByteArray{})
        .done([resolve = std::move(onSuccess), id](const QJsonObject&) {
            qDebug() << "[API] deleteVideo ok:" << id;
            if (resolve) resolve();
        })
        .fail([reject = std::move(onError), id](int code, const QString& error) {
            qWarning() << "[API] deleteVideo failed:" << id << code << error;
            if (reject) reject(code, error);
        })
        .send();
}

// ─── Playlists ──────────────────────────────────────────────────────────────
//
// Playlist endpoints don't fit the fixed-path Endpoint enum (every route
// except `GET /v1/playlists` has either `{id}` or `{id}/items[/{video_id}]`
// in the path), so we build URLs directly here, the same way renameVideo
// and deleteVideo do above. Each method packs the body, fires through
// RequestBuilder, and unwraps the response with the existing validators.
namespace {

QNetworkRequest buildPlaylistRequest(QString path, const QByteArray& authToken) {
    QNetworkRequest req(QUrl(QStringLiteral("http://localhost:8080") + std::move(path)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Connection", "close");
    if (!authToken.isEmpty()) req.setRawHeader("Authorization", "Bearer " + authToken);
    return req;
}

}  // namespace

void Api::listPlaylists(Fn<void(QJsonArray)> onSuccess, Fn<void(int, QString)> onError) {
    Q_ASSERT(nam_ != nullptr);

    QNetworkRequest req(QUrl(QString(kBaseUrl) + QStringLiteral("/v1/playlists")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Connection", "close");
    applyAuth(req);

    qDebug() << "[API] GET" << req.url().toString(QUrl::RemoveUserInfo);

    RequestBuilder(nam_, req, HttpMethod::kGet, QByteArray{})
        .done(validators::resolveArrayField(
            "playlists", "listPlaylists",
            [resolve = std::move(onSuccess)](const QJsonArray& playlists) {
                qDebug() << "[API] listPlaylists =>" << playlists.size() << "items";
                if (resolve) resolve(playlists);
            }
        ))
        .fail([reject = std::move(onError)](int code, const QString& error) {
            qWarning() << "[API] listPlaylists failed:" << code << error;
            if (reject) reject(code, error);
        })
        .send();
}

void Api::createPlaylistRemote(
    const QString& id,
    const QString& name,
    Fn<void(QJsonObject)> onSuccess,
    Fn<void(int, QString)> onError
) {
    Q_ASSERT(nam_ != nullptr);

    auto req = buildPlaylistRequest(QStringLiteral("/v1/playlists"), authToken_);
    const auto body =
        QJsonDocument(QJsonObject{{"id", id}, {"name", name}}).toJson(QJsonDocument::Compact);

    qDebug() << "[API] POST" << req.url().toString(QUrl::RemoveUserInfo) << "id=" << id;

    RequestBuilder(nam_, req, HttpMethod::kPost, body)
        .done(validators::resolveObjectField(
            "playlist", "createPlaylistRemote",
            [resolve = std::move(onSuccess)](const QJsonObject& playlist) {
                if (resolve) resolve(playlist);
            }
        ))
        .fail([reject = std::move(onError), id](int code, const QString& error) {
            qWarning() << "[API] createPlaylistRemote failed:" << id << code << error;
            if (reject) reject(code, error);
        })
        .send();
}

void Api::renamePlaylistRemote(
    const QString& id,
    const QString& newName,
    Fn<void(QJsonObject)> onSuccess,
    Fn<void(int, QString)> onError
) {
    Q_ASSERT(nam_ != nullptr);

    auto req = buildPlaylistRequest(QStringLiteral("/v1/playlists/") + id, authToken_);
    const auto body = QJsonDocument(QJsonObject{{"name", newName}}).toJson(QJsonDocument::Compact);

    qDebug() << "[API] PATCH" << req.url().toString(QUrl::RemoveUserInfo);

    RequestBuilder(nam_, req, HttpMethod::kPatch, body)
        .done(validators::resolveObjectField(
            "playlist", "renamePlaylistRemote",
            [resolve = std::move(onSuccess)](const QJsonObject& playlist) {
                if (resolve) resolve(playlist);
            }
        ))
        .fail([reject = std::move(onError), id](int code, const QString& error) {
            qWarning() << "[API] renamePlaylistRemote failed:" << id << code << error;
            if (reject) reject(code, error);
        })
        .send();
}

void Api::deletePlaylistRemote(const QString& id, Fn<void()> onSuccess, Fn<void(int, QString)> onError) {
    Q_ASSERT(nam_ != nullptr);

    auto req = buildPlaylistRequest(QStringLiteral("/v1/playlists/") + id, authToken_);

    qDebug() << "[API] DELETE" << req.url().toString(QUrl::RemoveUserInfo);

    RequestBuilder(nam_, req, HttpMethod::kDelete, QByteArray{})
        .done([resolve = std::move(onSuccess), id](const QJsonObject&) {
            qDebug() << "[API] deletePlaylistRemote ok:" << id;
            if (resolve) resolve();
        })
        .fail([reject = std::move(onError), id](int code, const QString& error) {
            qWarning() << "[API] deletePlaylistRemote failed:" << id << code << error;
            if (reject) reject(code, error);
        })
        .send();
}

void Api::addPlaylistItem(
    const QString& playlistId,
    const QJsonObject& itemSnapshot,
    Fn<void(QJsonObject, QJsonObject)> onSuccess,
    Fn<void(int, QString)> onError
) {
    Q_ASSERT(nam_ != nullptr);

    auto req = buildPlaylistRequest(
        QStringLiteral("/v1/playlists/") + playlistId + QStringLiteral("/items"), authToken_
    );
    const auto body = QJsonDocument(itemSnapshot).toJson(QJsonDocument::Compact);

    qDebug() << "[API] POST" << req.url().toString(QUrl::RemoveUserInfo)
             << "video_id=" << itemSnapshot.value("video_id").toString();

    RequestBuilder(nam_, req, HttpMethod::kPost, body)
        .done([resolve = std::move(onSuccess), playlistId](const QJsonObject& json) {
            const auto item = json.value("item").toObject();
            const auto playlist = json.value("playlist").toObject();
            if (item.isEmpty()) {
                qWarning() << "[API] addPlaylistItem: response missing 'item'";
                return;
            }
            if (resolve) resolve(item, playlist);
        })
        .fail([reject = std::move(onError), playlistId](int code, const QString& error) {
            qWarning() << "[API] addPlaylistItem failed:" << playlistId << code << error;
            if (reject) reject(code, error);
        })
        .send();
}

void Api::removePlaylistItem(
    const QString& playlistId,
    const QString& videoId,
    Fn<void()> onSuccess,
    Fn<void(int, QString)> onError
) {
    Q_ASSERT(nam_ != nullptr);

    auto req = buildPlaylistRequest(
        QStringLiteral("/v1/playlists/") + playlistId + QStringLiteral("/items/") + videoId, authToken_
    );

    qDebug() << "[API] DELETE" << req.url().toString(QUrl::RemoveUserInfo);

    RequestBuilder(nam_, req, HttpMethod::kDelete, QByteArray{})
        .done([resolve = std::move(onSuccess)](const QJsonObject&) {
            if (resolve) resolve();
        })
        .fail([reject = std::move(onError), playlistId, videoId](int code, const QString& error) {
            qWarning() << "[API] removePlaylistItem failed:" << playlistId << videoId << code << error;
            if (reject) reject(code, error);
        })
        .send();
}

void Api::fetchVideos(const QString& query, Fn<void(QJsonArray)> onSuccess, Fn<void(int, QString)> onError) {
    QUrlQuery queryParams;
    if (!query.isEmpty()) queryParams.addQueryItem("q", query);

    auto onVideos = [resolve = std::move(onSuccess)](const QJsonArray& videos) {
        qDebug() << "[API] fetchVideos =>" << videos.size() << "items";
        if (resolve) resolve(videos);
    };

    request(Endpoint::kVideos, queryParams, {}, HttpMethod::kGet)
        .done(validators::resolveArrayField("videos", "fetchVideos", std::move(onVideos)))
        .fail([reject = std::move(onError)](int code, const QString& error) {
            qWarning() << "[API] fetchVideos failed:" << code << error;
            if (reject) reject(code, error);
        })
        .send();
}
