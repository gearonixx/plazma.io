#include "coreController.h"
#include "../../utils.h"

#include <QCoreApplication>
#include <QDebug>
#include <QQmlContext>

#include <QDirIterator>

#include <QTranslator>

#include "version.h"

#include "../../models/user_model.h"
#include "../../session.h"

CoreController::CoreController(
    QQmlApplicationEngine* engine,
    std::shared_ptr<Settings> settings,
    TelegramClient* client,
    QObject* parent
)
    : QObject(parent), settings_(std::move(settings)), qmlEngine_(engine) {
    initModels(client);
    initControllers();

    initSignalHandlers();

    translator_.reset(new QTranslator());
    updateTranslator(settings_->getAppLanguage());

    new Utils(engine);
};

void CoreController::initModels(TelegramClient* client) {
    phoneNumberModel_.reset(new PhoneNumberModel(client));
    qmlRegisterSingletonInstance<PhoneNumberModel>(APPLICATION_ID, 1, 0, "PhoneNumberModel", phoneNumberModel_.data());

    authCodeModel_.reset(new AuthorizationCodeModel(client));
    qmlRegisterSingletonInstance<AuthorizationCodeModel>(
        APPLICATION_ID, 1, 0, "AuthorizationCodeModel", authCodeModel_.data()
    );

    userModel_.reset(new UserModel(client));
    qmlRegisterSingletonInstance<UserModel>(APPLICATION_ID, 1, 0, "UserModel", userModel_.data());

    session_.reset(new Session());
    qmlRegisterSingletonInstance<Session>(APPLICATION_ID, 1, 0, "Session", session_.data());

    language_model_.reset(new LanguageModel(settings_));
    qmlRegisterSingletonInstance<LanguageModel>(APPLICATION_ID, 1, 0, "LanguageModel", language_model_.data());

};

void CoreController::initControllers() {
    // TODO
    auto tmp_ptr = std::shared_ptr<QVariant>();
    systemsController_.reset(new SystemsController(tmp_ptr, this));
    qmlEngine_->rootContext()->setContextProperty("SystemsController", systemsController_.data());

    pageController_.reset(new PageController());
    qmlEngine_->rootContext()->setContextProperty("PageController", pageController_.data());

    rpc_.reset(new RpcClient());

    fileDialog_.reset(new platform::FileDialog(rpc_.data()));
    fileDialogModel_.reset(new FileDialogModel(fileDialog_.data()));
    qmlRegisterSingletonInstance<FileDialogModel>(APPLICATION_ID, 1, 0, "FileDialogModel", fileDialogModel_.data());
}

void CoreController::initSignalHandlers() {
    initTranslationsBindings();
    initAuthBindings();
}

void CoreController::initAuthBindings() {
    connect(userModel_.data(), &UserModel::userChanged, this, [this]() {
        if (!userModel_->isLoaded()) return;

        session_->start(userModel_->me());
        rpc_->loginUser(*session_);
    });
}

void CoreController::setQmlRoot() const {
    if (qmlEngine_->rootObjects().isEmpty()) {
        qDebug() << "No rootObjects loaded";
        QCoreApplication::exit(0);
        return;
    }

    systemsController_->setQmlRoot(qmlEngine_->rootObjects().at(0));
}

QSharedPointer<PageController> CoreController::pageController() const { return pageController_; }

void CoreController::initTranslationsBindings() {
    connect(language_model_.get(), &LanguageModel::updateTranslations, this, &CoreController::updateTranslator);
    connect(this, &CoreController::translationsUpdated, language_model_.get(), &LanguageModel::translationsUpdated);
};

void CoreController::updateTranslator(const QLocale& locale) const {
    qDebug() << "[i18n] updateTranslator called, locale:" << locale.name()
             << "language:" << locale.language();

    if (!translator_->isEmpty()) {
        QCoreApplication::removeTranslator(translator_.data());
        qDebug() << "[i18n] removed old translator";
    }

    if (locale.language() == QLocale::English) {
        settings_->setAppLanguage(locale);
        qmlEngine_->retranslate();
        qDebug() << "[i18n] switched to English (no .qm)";
        emit translationsUpdated();
        return;
    }

    const QString lang = locale.name().split("_").first();
    const QString strFileName = QString(":/locales/%1.qm").arg(lang);

    if (translator_->load(strFileName)) {
        if (QCoreApplication::installTranslator(translator_.data())) {
            settings_->setAppLanguage(locale);
        } else {
            qWarning() << "Failed to install translation file:" << strFileName;
            settings_->setAppLanguage(QLocale::English);
        }
    } else {
        qWarning() << "Failed to load translation file:" << strFileName;
    }

    qmlEngine_->retranslate();

    emit translationsUpdated();
};