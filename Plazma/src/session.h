#pragma once

#include <QObject>
#include <QString>

#include "basic_types.h"
#include "client.h"

class Session final : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool valid READ valid NOTIFY sessionChanged)
    Q_PROPERTY(int64 userId READ userId NOTIFY sessionChanged)
    Q_PROPERTY(QString username READ username NOTIFY sessionChanged)
    Q_PROPERTY(QString firstName READ firstName NOTIFY sessionChanged)
    Q_PROPERTY(QString lastName READ lastName NOTIFY sessionChanged)
    Q_PROPERTY(QString phoneNumber READ phoneNumber NOTIFY sessionChanged)
    Q_PROPERTY(bool premium READ premium NOTIFY sessionChanged)

public:
    explicit Session(QObject* parent = nullptr);

    void start(const UserPtr& user);

    [[nodiscard]] bool valid() const { return valid_; }
    [[nodiscard]] int64 userId() const { return userId_; }
    [[nodiscard]] QString username() const { return username_; }
    [[nodiscard]] QString firstName() const { return firstName_; }
    [[nodiscard]] QString lastName() const { return lastName_; }
    [[nodiscard]] QString phoneNumber() const { return phoneNumber_; }
    [[nodiscard]] bool premium() const { return premium_; }

signals:
    void sessionChanged();

private:
    static QString extractUsername(const UserPtr& user);

    bool valid_ = false;
    int64 userId_ = 0;
    QString username_;
    QString firstName_;
    QString lastName_;
    QString phoneNumber_;
    bool premium_ = false;
};
