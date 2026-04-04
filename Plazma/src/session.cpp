#include "session.h"

#include <QDebug>

Session::Session(QObject* parent) : QObject(parent) {}

void Session::start(const UserPtr& user) {
    Q_ASSERT(user != nullptr);

    userId_ = user->id_;
    username_ = extractUsername(user);
    firstName_ = QString::fromStdString(user->first_name_);
    lastName_ = QString::fromStdString(user->last_name_);
    phoneNumber_ = QString::fromStdString(user->phone_number_);
    premium_ = user->is_premium_;
    valid_ = true;

    qDebug() << "[SESSION] Started for user" << userId_ << firstName_ << lastName_;

    emit sessionChanged();
}

QString Session::extractUsername(const UserPtr& user) {
    if (user->usernames_ && !user->usernames_->active_usernames_.empty()) {
        return QString::fromStdString(user->usernames_->active_usernames_[0]);
    }
    return {};
}
