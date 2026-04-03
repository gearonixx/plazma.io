#include "auth_code_model.h"
#include <QDebug>

void AuthorizationCodeModel::submitAuthCode(const QString& code) {
    Q_ASSERT_X(
        waitingForAuthCode_,
        "AuthorizationCodeModel::submitAuthCode",
        "submitAuthCode called while not waiting for authorization code"
    );

    emit authCodeSent(code);

    waitingForAuthCode_ = false;
    emit waitingForCodeChanged();
};
