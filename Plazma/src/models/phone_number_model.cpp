#include "phone_number_model.h"
#include <QDebug>

void PhoneNumberModel::submitPhoneNumber(const QString& phone_number) {
    // We used to assert on waitingForPhone_ here, but that crashed debug
    // builds when the user hit "Next" a second time after TDLib rejected
    // the first attempt (e.g. binlog locked, invalid number). TDLib itself
    // safely rejects stale submissions, so allow the retry to flow through.
    if (!waitingForPhone_) {
        qWarning() << "[PHONE] submit while not waiting for phone — "
                      "likely a retry after a TDLib error";
    }

    emit phoneNumberSent(phone_number);

    if (waitingForPhone_) {
        waitingForPhone_ = false;
        emit waitingForPhoneChanged();
    }
};
