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

    void call(const QString& endpoint, const QJsonObject& body = {}, const HttpMethod& method = HttpMethod::kGet);
    void loginUser(const class Session& session);
    void uploadFile(
        const QString& endpoint,
        const QString& fieldName,
        const QString& filename,
        const QString& mime,
        const QByteArray& filedata
    );

    plazma::task_queue::TaskQueue* fileLoader() const { return file_loader_.get(); }

signals:
    void loginSuccess(QJsonObject user);
    void loginError(int statusCode, QString error);

private:
    static constexpr auto kBaseUrl = "http://localhost:8080";

    QNetworkAccessManager* nam_;
    int requestId_ = 0;

    std::unique_ptr<plazma::task_queue::TaskQueue> file_loader_;
};
