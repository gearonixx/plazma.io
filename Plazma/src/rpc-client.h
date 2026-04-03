// rpc_client.h
#pragma once

#include <QHttpMultiPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>

#include "storage/task_queue.h"

/// @brief HTTP request method
enum class HttpMethod { kGet, kPost, kHead, kPut, kDelete, kPatch, kOptions };

static QByteArray toMethodString(HttpMethod method) {
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

class RpcClient : public QObject {
    Q_OBJECT
public:
    explicit RpcClient(
        const QString& host,
        int port,
        const QString& user,
        const QString& password,
        QObject* parent = nullptr
    )
        : QObject(parent),
          nam_(new QNetworkAccessManager(this)),
          file_loader_(std::make_unique<plazma::task_queue::TaskQueue>()) {}

    explicit RpcClient(QObject* parent = nullptr)
        : QObject(parent),
          nam_(new QNetworkAccessManager(this)),
          file_loader_(std::make_unique<plazma::task_queue::TaskQueue>()) {}

    void call(const QString& endpoint, const QJsonObject& body = {}, const HttpMethod& method = HttpMethod::kGet) {
        Q_ASSERT(nam_ != nullptr);

        QNetworkRequest req(QUrl(QString(kBaseUrl) + endpoint));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        req.setRawHeader("Connection", "close");

        QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);
        auto* reply = nam_->sendCustomRequest(req, toMethodString(method), data);

        connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);

        qDebug() << "[RPC]" << toMethodString(method) << endpoint;
    }

    void uploadFile(
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

    plazma::task_queue::TaskQueue* fileLoader() const { return file_loader_.get(); }

    // void call(const QString& method, const QJsonArray& params = {}) {
    //     QJsonObject request;
    //     request["jsonrpc"] = "2.0";
    //     request["id"] = ++requestId_;
    //     request["method"] = method;
    //     request["params"] = params;
    //
    //     QNetworkRequest req(endpoint_);
    //     req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    //     req.setRawHeader("Authorization", authHeader_.toUtf8());
    //     req.setRawHeader("Connection", "close");
    //
    //     QByteArray body = QJsonDocument(request).toJson(QJsonDocument::Compact);
    //     auto* reply = nam_->post(req, body);
    //
    //     connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    //         if (reply->error() == QNetworkReply::NoError) {
    //             auto doc = QJsonDocument::fromJson(reply->readAll());
    //             auto obj = doc.object();
    //             if (obj.contains("error") && !obj["error"].isNull()) {
    //                 emit rpcError(obj["error"].toObject()["message"].toString());
    //             } else {
    //                 emit rpcResponse(obj["result"]);
    //             }
    //         } else {
    //             emit rpcError(reply->errorString());
    //         }
    //         reply->deleteLater();
    //     });
    // }

signals:
    void rpcResponse(QJsonValue result);
    void rpcError(QString error);

private:
    static constexpr auto kBaseUrl = "http://localhost:8080";

    QNetworkAccessManager* nam_;
    int requestId_ = 0;

    std::unique_ptr<plazma::task_queue::TaskQueue> file_loader_;
};