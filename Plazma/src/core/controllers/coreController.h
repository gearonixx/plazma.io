#pragma once

#include <QCoreApplication>
#include <QQmlApplicationEngine>

#include "src/controllers/pageController.h"
#include "src/controllers/systemController.h"
#include "src/api.h"
#include "src/settings.h"

#include "src/models/auth_code_model.h"
#include "src/models/language_model.h"
#include "src/models/phone_number_model.h"
#include "src/models/file_dialog_model.h"
#include "src/models/user_model.h"
#include "src/platform/file_dialog.h"
#include "src/session.h"

class CoreController : public QObject {
    Q_OBJECT;

public:
    explicit CoreController(
        QQmlApplicationEngine* engine_,
        std::shared_ptr<Settings> settings,
        TelegramClient* client,
        QObject* parent = nullptr
    );

    QSharedPointer<PageController> pageController() const;
    void setQmlRoot() const;

signals:
    void translationsUpdated() const;

private:
    void initModels(TelegramClient* client);
    void initControllers();

    void initSignalHandlers();

    void initTranslationsBindings();
    void initAuthBindings();
    void updateTranslator(const QLocale& locale) const;

    QQmlApplicationEngine* qmlEngine_{};

    std::shared_ptr<Settings> settings_{};

    QSharedPointer<QTranslator> translator_;

    QSharedPointer<PageController> pageController_;

    QScopedPointer<SystemsController> systemsController_;

    QSharedPointer<LanguageModel> language_model_;

    QSharedPointer<PhoneNumberModel> phoneNumberModel_;
    QSharedPointer<AuthorizationCodeModel> authCodeModel_;

    QSharedPointer<UserModel> userModel_;
    QSharedPointer<Session> session_;


    QScopedPointer<platform::FileDialog> fileDialog_;
    QSharedPointer<FileDialogModel> fileDialogModel_;
};