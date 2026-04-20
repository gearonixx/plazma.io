#pragma once

#include <qobject.h>
#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

#include <QDebug>
#include <QObject>
#include <QThread>

#include <atomic>
#include <mutex>

#include "basic_types.h"
#include "detail.h"

namespace td_api = td::td_api;

using RequestPtr = td::td_api::object_ptr<td::td_api::Function>;
using ResponsePtr = td::td_api::object_ptr<td::td_api::Object>;

using ResponseHandler = Fn<void(ResponsePtr)>;

using RequestId = td::ClientManager::RequestId;

using RequestPtr = td::td_api::object_ptr<td::td_api::Function>;
using UserPtr = std::shared_ptr<td_api::user>;

class TelegramRequestBuilder {
public:
    static RequestPtr setTdLibParameters();
    static RequestPtr getVersion();
    static RequestPtr setPhoneNumber(const lcString& phone_number);
    static RequestPtr checkAuthCode(const lcString& code);
    static RequestPtr getMe();
};

class TelegramClient final : public QObject {
    Q_OBJECT

signals:
    void phoneNumberRequired();
    void authCodeRequired();
    void userLoaded(UserPtr me);

    // Emitted from the polling thread whenever TDLib replies to a request with
    // a td_api::error. Consumers get the raw TDLib code + message; it is up to
    // higher layers (Session) to decide how to present it.
    void tdlibError(int code, const QString& message);

public slots:
    void phoneNumberReceived(const QString& phone_number);
    void authCodeReceived(const QString& auth_code);

public:
    TelegramClient();
    ~TelegramClient();
    bool IsAuthorized() const;

    void startPolling();

private:
    static constexpr int LOG_LEVEL = 1;
    static constexpr int RECEIVE_TIMEOUT = 5;
    UserPtr me_;

    void createInstance();
    bool isUnsolicitedUpdate(const td::ClientManager::Response& response) const;

    template <typename T>
    void send_request(td::td_api::object_ptr<T> request, ResponseHandler response_handler = [](ResponsePtr) {}) {
        const auto request_id = update_request_id();

        if (response_handler) {
            std::lock_guard<std::mutex> lock(response_handlers_mu_);
            response_handlers_.emplace(request_id, std::move(response_handler));
        }

        client_manager_->send(client_id_, request_id, std::move(request));
    }

    template <typename T>
    void log_request(T& req) {
        std::string full = td_api::to_string(req);
        std::string name = full.substr(0, full.find(' '));

        qDebug() << "[REQUEST] Sending:" << QString::fromStdString(name);
    }

    void send(RequestPtr req, const ResponseHandler& handler = [](ResponsePtr) {}) {
        log_request(req);
        send_request(std::move(req), handler);
    }

    void process_update(td_api::object_ptr<td_api::Object> update);
    td::ClientManager::RequestId update_request_id();

    std::unique_ptr<td::ClientManager> client_manager_;
    td::ClientManager::ClientId client_id_{0};
    std::atomic<td::ClientManager::RequestId> current_request_id_{0};
    std::unique_ptr<QThread> polling_thread_;

    std::mutex response_handlers_mu_;
    std::map<RequestId, ResponseHandler> response_handlers_;

    bool is_authorized_{false};

    void pollForUpdates();
};
