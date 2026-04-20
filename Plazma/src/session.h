#pragma once

#include <QObject>
#include <QString>

#include <memory>

#include "basic_types.h"

struct UserLogin final {
    qint64 userId = 0;
    QString username;
    QString firstName;
    QString lastName;
    QString phoneNumber;
    bool isPremium = false;
};

class Api;

class Session final : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool valid READ valid NOTIFY sessionChanged)
    Q_PROPERTY(int64 userId READ userId NOTIFY sessionChanged)
    Q_PROPERTY(QString username READ username NOTIFY sessionChanged)
    Q_PROPERTY(QString firstName READ firstName NOTIFY sessionChanged)
    Q_PROPERTY(QString lastName READ lastName NOTIFY sessionChanged)
    Q_PROPERTY(QString phoneNumber READ phoneNumber NOTIFY sessionChanged)
    Q_PROPERTY(bool premium READ premium NOTIFY sessionChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)

public:
    explicit Session(QObject* parent = nullptr);

    void start(const UserLogin& user);
    void reportError(int statusCode, const QString& message);
    QString errorMessage() const { return errorMessage_; }

public slots:
    // Handles errors originating from TDLib (as opposed to the HTTP API).
    // Maps well-known cases (binlog lock, un-initialized parameters) to a
    // user-friendly message before publishing on `errorMessage`.
    void reportAuthError(int tdlibCode, const QString& rawMessage);

    [[nodiscard]] Api& api() { return *api_; }

    [[nodiscard]] bool valid() const { return valid_; }
    [[nodiscard]] int64 userId() const { return userId_; }
    [[nodiscard]] QString username() const { return username_; }
    [[nodiscard]] QString firstName() const { return firstName_; }
    [[nodiscard]] QString lastName() const { return lastName_; }
    [[nodiscard]] QString phoneNumber() const { return phoneNumber_; }
    [[nodiscard]] bool premium() const { return premium_; }

signals:
    void sessionChanged();
    void errorChanged();

private:
    std::unique_ptr<Api> api_;

    bool valid_ = false;
    int64 userId_ = 0;
    QString username_;
    QString firstName_;
    QString lastName_;
    QString phoneNumber_;
    bool premium_ = false;
    QString errorMessage_;
};
