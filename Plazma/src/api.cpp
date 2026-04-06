#include "api.h"

namespace validators {
std::optional<UserLogin> ensureLoginResponse(const QJsonObject& json, QString& error);
}

void RequestBuilder::send() {
    auto* reply = nam_->sendCustomRequest(req_, toMethodString(method_), body_);

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
            qWarning() << "[API] loginUser failed:" << error;
            emit loginError(statusCode, error);
        })
        .send();
}

void Api::uploadFile(
    const QString& endpoint,
    const QString& fieldName,
    const QString& filename,
    const QString& mime,
    const QByteArray& filedata
) {
    Q_ASSERT(nam_ != nullptr);

    auto* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart filePart;
    filePart.setHeader(
        QNetworkRequest::ContentTypeHeader, mime
    );
    filePart.setHeader(
        QNetworkRequest::ContentDispositionHeader,
        QStringLiteral("form-data; name=\"%1\"; filename=\"%2\"").arg(fieldName, filename)
    );
    filePart.setBody(filedata);
    multiPart->append(filePart);

    QNetworkRequest req(QUrl(QString(kBaseUrl) + endpoint));

    auto* reply = nam_->post(req, multiPart);
    multiPart->setParent(reply);

    connect(reply, &QNetworkReply::finished, this, [reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[API] upload failed:" << reply->errorString();
        } else {
            qDebug() << "[API] upload ok:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        }
        reply->deleteLater();
    });

    qDebug() << "[API] POST (multipart)" << endpoint << filename << filedata.size() << "bytes";
}
