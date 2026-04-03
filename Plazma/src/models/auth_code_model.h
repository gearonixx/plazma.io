#pragma once

#include "../client.h"

class AuthorizationCodeModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool waitingForAuthCode READ waitingForAuthCode NOTIFY waitingForCodeChanged)

public:
    explicit AuthorizationCodeModel(TelegramClient* client_, QObject* parent = nullptr)
        : QObject(parent), client_(client_) {
        Q_ASSERT(client_ != nullptr);

        connect(client_, &TelegramClient::authCodeRequired, this, &AuthorizationCodeModel::onAuthCodeRequired);
        connect(this, &AuthorizationCodeModel::authCodeSent, client_, &TelegramClient::authCodeReceived);
    };

    Q_INVOKABLE void submitAuthCode(const QString& code);

    // getters
    bool waitingForAuthCode() const { return waitingForAuthCode_; }

signals:
    void waitingForCodeChanged();
    void authCodeSent(const QString& code);

private slots:
    void onAuthCodeRequired() {
        waitingForAuthCode_ = true;
        emit waitingForCodeChanged();
    };

private:
    TelegramClient* client_;
    bool waitingForAuthCode_ = false;
};