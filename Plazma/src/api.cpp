#include "api.h"

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

    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, done = std::move(done_), fail = std::move(fail_)]() {
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
    });
}

RequestBuilder Api::request(const QString& endpoint, const QJsonObject& body, const HttpMethod& method) {
    Q_ASSERT(nam_ != nullptr);

    QNetworkRequest req(QUrl(QString(kBaseUrl) + endpoint));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Connection", "close");

    qDebug() << "[API]" << toMethodString(method) << endpoint;

    return RequestBuilder(nam_, req, method, QJsonDocument(body).toJson(QJsonDocument::Compact));
}

RequestBuilder Api::request(const QString& endpoint, QHttpMultiPart* multiPart) {
    Q_ASSERT(nam_ != nullptr);

    QNetworkRequest req(QUrl(QString(kBaseUrl) + endpoint));

    qDebug() << "[API] POST (multipart)" << endpoint;

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

    request("/v1/auth/login", body, HttpMethod::kPost)
        .done([this](const QJsonObject& json) {
            QString validationError;

            qDebug() << json;

            const auto user = validators::ensureLoginResponse(json, validationError);

            if (!user) {
                qWarning() << "[API] loginUser validation failed:" << validationError;
                emit loginError(0, validationError);
            } else {
                qDebug() << "[API] loginUser =>" << user->userId << user->username;
                emit loginSuccess(*user);
            }
        })
        .fail([this](int statusCode, const QString& error) {
            qWarning() << "[API] loginUser failed — POST" << (QString(kBaseUrl) + "/v1/auth/login")
                       << "status:" << statusCode << "error:" << error;
            emit loginError(statusCode, error);
        })
        .send();
}

void Api::uploadFile(
    const QString& endpoint,
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

    request(endpoint, multiPart)
        .done([this, endpoint, filename](const QJsonObject&) {
            qDebug() << "[API] upload ok:" << endpoint;
            emit uploadFinished(endpoint, filename);
        })
        .fail([this, endpoint](int code, const QString& error) {
            qWarning() << "[API] upload failed:" << code << error;
            emit uploadFailed(endpoint, code, error);
        })
        .send();
}

void Api::fetchVideos(Fn<void(QJsonArray)> onOk, Fn<void(int, QString)> onFail) {
    request("/v1/videos", {}, HttpMethod::kGet)
        .done([ok = std::move(onOk)](const QJsonObject& json) {
            const auto arr = json.value("videos").toArray();
            qDebug() << "[API] fetchVideos =>" << arr.size() << "items";
            if (ok) ok(arr);
        })
        .fail([fail = std::move(onFail)](int code, const QString& error) {
            qWarning() << "[API] fetchVideos failed:" << code << error;
            if (fail) fail(code, error);
        })
        .send();
}
