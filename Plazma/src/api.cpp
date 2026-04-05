#include "api.h"

namespace validators {
std::optional<UserLogin> ensureLoginResponse(const QJsonObject& json, QString& error);
}

void Api::call(const QString& endpoint, const QJsonObject& body, const HttpMethod& method) {
    Q_ASSERT(nam_ != nullptr);

    QNetworkRequest req(QUrl(QString(kBaseUrl) + endpoint));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Connection", "close");

    QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);
    auto* reply = nam_->sendCustomRequest(req, toMethodString(method), data);

    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);

    qDebug() << "[API]" << toMethodString(method) << endpoint;
}

void Api::loginUser(const UserLogin& user) {
    Q_ASSERT(nam_ != nullptr);

    QNetworkRequest req(QUrl(QString(kBaseUrl) + "/v1/auth/login"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Connection", "close");

    QJsonObject body{
        {"user_id", user.userId},
        {"username", user.username},
        {"first_name", user.firstName},
        {"last_name", user.lastName},
        {"phone_number", user.phoneNumber},
        {"is_premium", user.isPremium},
    };

    QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);
    auto* reply = nam_->sendCustomRequest(req, "POST", data);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            qWarning() << "[API] loginUser failed:" << reply->errorString();
            emit loginError(statusCode, reply->errorString());
        } else {
            auto doc = QJsonDocument::fromJson(reply->readAll());
            auto json = doc.object();

            QString validationError;
            const auto user = validators::ensureLoginResponse(json, validationError);

            if (!user) {
                qWarning() << "[API] loginUser validation failed:" << validationError;
                emit loginError(0, validationError);
            } else {
                qDebug() << "[API] loginUser =>" << user->userId << user->username;
                emit loginSuccess(*user);
            }
        }

        reply->deleteLater();
    });

    qDebug() << "[API] POST /v1/auth/login";
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
