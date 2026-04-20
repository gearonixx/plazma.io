#pragma once

#include <QHttpMultiPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>

#include "session.h"
#include "storage/task_queue.h"

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

class RequestBuilder {
public:
    RequestBuilder(QNetworkAccessManager* nam, QNetworkRequest req, HttpMethod method, QByteArray body)
        : nam_(nam), req_(std::move(req)), method_(method), body_(std::move(body)) {}

    RequestBuilder(QNetworkAccessManager* nam, QNetworkRequest req, QHttpMultiPart* multiPart)
        : nam_(nam), req_(std::move(req)), method_(HttpMethod::kPost), multiPart_(multiPart) {}

    RequestBuilder& done(Fn<void(QJsonObject)> cb) { done_ = std::move(cb); return *this; }
    RequestBuilder& fail(Fn<void(int, QString)> cb) { fail_ = std::move(cb); return *this; }
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

    RequestBuilder request(const QString& endpoint, const QJsonObject& body = {}, const HttpMethod& method = HttpMethod::kGet);
    RequestBuilder request(const QString& endpoint, QHttpMultiPart* multiPart);
    void loginUser(const UserLogin& user);
    void uploadFile(
        const QString& endpoint,
        const QString& fieldName,
        const QString& filename,
        const QString& mime,
        const QByteArray& filedata,
        const QByteArray& thumbnail = {},
        const QString& thumbnailMime = QStringLiteral("image/jpeg")
    );
    void fetchVideos(Fn<void(QJsonArray)> onOk, Fn<void(int, QString)> onFail = {});

    plazma::task_queue::TaskQueue* fileLoader() const { return file_loader_.get(); }

signals:
    void loginSuccess(UserLogin user);
    void loginError(int statusCode, QString error);
    void uploadFinished(QString endpoint, QString filename);
    void uploadFailed(QString endpoint, int statusCode, QString error);

private:
    static constexpr auto kBaseUrl = "http://localhost:8080";

    QNetworkAccessManager* nam_;
    int requestId_ = 0;

    std::unique_ptr<plazma::task_queue::TaskQueue> file_loader_;
};
