#include "session.h"
#include "api.h"

#include <QDebug>

Session::Session(QObject* parent) : QObject(parent), api_(std::make_unique<Api>(this)) {}

void Session::start(const UserLogin& user) {
    userId_ = user.userId;
    username_ = user.username;
    firstName_ = user.firstName;
    lastName_ = user.lastName;
    phoneNumber_ = user.phoneNumber;
    premium_ = user.isPremium;
    valid_ = true;

    if (!errorMessage_.isEmpty()) {
        errorMessage_.clear();
        emit errorChanged();
    }

    qDebug() << "[SESSION] Started for user" << userId_ << firstName_ << lastName_;

    emit sessionChanged();
}

void Session::reportError(int statusCode, const QString& message) {
    errorMessage_ = statusCode > 0
        ? QStringLiteral("HTTP %1: %2").arg(statusCode).arg(message)
        : message;

    qWarning() << "[SESSION] login failed:" << errorMessage_;

    emit errorChanged();
}

void Session::reportAuthError(int tdlibCode, const QString& rawMessage) {
    // Translate known TDLib error shapes into something a user can act on.
    // Unknowns fall through to the raw message so nothing ever goes silent.
    QString friendly;
    if (rawMessage.contains(QStringLiteral("already in use"), Qt::CaseInsensitive) ||
        rawMessage.contains(QStringLiteral("Can't lock file"), Qt::CaseInsensitive)) {
        friendly = tr("Another Plazma instance is already running. "
                      "Close it and try again.");
    } else if (rawMessage.contains(QStringLiteral("Initialization parameters are needed"))) {
        // This is the *consequence* of a prior setTdlibParameters failure
        // (almost always the binlog lock above). If that case already set a
        // message, don't overwrite it with this less useful one.
        if (!errorMessage_.isEmpty()) return;
        friendly = tr("Still getting ready. Please wait a moment and try again.");
    } else if (rawMessage.contains(QStringLiteral("PHONE_NUMBER_INVALID"))) {
        friendly = tr("That phone number doesn't look right. Check the country code.");
    } else if (rawMessage.contains(QStringLiteral("PHONE_CODE_INVALID"))) {
        friendly = tr("The code doesn't match. Try again.");
    } else if (rawMessage.contains(QStringLiteral("PHONE_CODE_EXPIRED"))) {
        friendly = tr("The code has expired. Request a new one.");
    } else {
        friendly = QStringLiteral("TDLib %1: %2").arg(tdlibCode).arg(rawMessage);
    }

    errorMessage_ = friendly;
    qWarning() << "[SESSION] auth error:" << tdlibCode << rawMessage << "→" << friendly;

    emit errorChanged();
}
