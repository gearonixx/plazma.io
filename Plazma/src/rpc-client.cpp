#include "rpc-client.h"
#include "session.h"

void RpcClient::call(const QString& endpoint, const QJsonObject& body, const HttpMethod& method) {
    Q_ASSERT(nam_ != nullptr);

    QNetworkRequest req(QUrl(QString(kBaseUrl) + endpoint));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Connection", "close");

    QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);
    auto* reply = nam_->sendCustomRequest(req, toMethodString(method), data);

    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);

    qDebug() << "[RPC]" << toMethodString(method) << endpoint;
}

void RpcClient::loginUser(const Session& session) {
    Q_ASSERT(nam_ != nullptr);

    QNetworkRequest req(QUrl(QString(kBaseUrl) + "/v1/auth/login"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Connection", "close");

    QJsonObject body{
        {"user_id", session.userId()},
        {"username", session.username()},
        {"first_name", session.firstName()},
        {"last_name", session.lastName()},
        {"phone_number", session.phoneNumber()},
        {"is_premium", session.premium()},
    };

    QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);
    auto* reply = nam_->sendCustomRequest(req, "POST", data);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            qWarning() << "[RPC] loginUser failed:" << reply->errorString();
            emit loginError(statusCode, reply->errorString());
        } else {
            auto doc = QJsonDocument::fromJson(reply->readAll());
            auto obj = doc.object();
            qDebug() << "[RPC] loginUser =>" << obj;
            emit loginSuccess(obj);
        }

        reply->deleteLater();
    });

    qDebug() << "[RPC] POST /v1/auth/login";
}

void RpcClient::uploadFile(
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
            qWarning() << "[RPC] upload failed:" << reply->errorString();
        } else {
            qDebug() << "[RPC] upload ok:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        }
        reply->deleteLater();
    });

    qDebug() << "[RPC] POST (multipart)" << endpoint << filename << filedata.size() << "bytes";
}
