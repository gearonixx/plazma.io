#include "phone_number_model.h"
#include <QDebug>

void PhoneNumberModel::submitPhoneNumber(const QString& phone_number) {
    Q_ASSERT_X(
        waitingForPhone_,
        "PhoneNumberModel::submitPhoneNumber",
        "submitPhoneNumber called while not waiting for phone number"
    );

    emit phoneNumberSent(phone_number);

    waitingForPhone_ = false;
    emit waitingForPhoneChanged();
};
