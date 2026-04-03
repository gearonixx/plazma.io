#pragma once

#include "../client.h"

class UserModel : public QObject {
    Q_OBJECT
public:
    explicit UserModel(TelegramClient* client, QObject* parent = nullptr) : QObject(parent), client_(client) {
        Q_ASSERT(client_ != nullptr);

        connect(client_, &TelegramClient::userLoaded, this, &UserModel::onUserLoaded);
    }

    bool isLoaded() const { return me_ != nullptr; }

    UserPtr me() const { return me_; }

    qint64 id() const {
        Q_ASSERT(isLoaded());
        return me_->id_;
    }
    QString firstName() const {
        Q_ASSERT(isLoaded());
        return QString::fromStdString(me_->first_name_);
    }
    QString lastName() const {
        Q_ASSERT(isLoaded());
        return QString::fromStdString(me_->last_name_);
    }
    QString phoneNumber() const {
        Q_ASSERT(isLoaded());
        return QString::fromStdString(me_->phone_number_);
    }
    QString username() const {
        Q_ASSERT(isLoaded());
        if (me_->usernames_ && !me_->usernames_->active_usernames_.empty()) {
            return QString::fromStdString(me_->usernames_->active_usernames_[0]);
        }
        return {};
    }
    bool isPremium() const {
        Q_ASSERT(isLoaded());
        return me_->is_premium_;
    }

signals:
    void userChanged();

private slots:
    void onUserLoaded(UserPtr me) {
        me_ = me;
        emit userChanged();
    }

private:
    TelegramClient* client_;
    UserPtr me_;
};