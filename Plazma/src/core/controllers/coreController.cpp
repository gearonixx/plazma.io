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
#include "../../ui/mpv_object.h"

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
    initTdlibErrorBindings(client);

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

    settingsModel_.reset(new SettingsModel(settings_));
    qmlRegisterSingletonInstance<SettingsModel>(APPLICATION_ID, 1, 0, "SettingsModel", settingsModel_.data());

    qmlRegisterType<MpvObject>(APPLICATION_ID, 1, 0, "MpvObject");
};

void CoreController::initControllers() {
    // TODO
    auto tmp_ptr = std::shared_ptr<QVariant>();
    systemsController_.reset(new SystemsController(tmp_ptr, this));
    qmlEngine_->rootContext()->setContextProperty("SystemsController", systemsController_.data());

    pageController_.reset(new PageController());
    qmlEngine_->rootContext()->setContextProperty("PageController", pageController_.data());

    fileDialog_.reset(new platform::FileDialog(&session_->api()));
    fileDialogModel_.reset(new FileDialogModel(fileDialog_.data()));
    qmlRegisterSingletonInstance<FileDialogModel>(APPLICATION_ID, 1, 0, "FileDialogModel", fileDialogModel_.data());

    videoFeedModel_.reset(new VideoFeedModel(&session_->api()));
    qmlRegisterSingletonInstance<VideoFeedModel>(APPLICATION_ID, 1, 0, "VideoFeedModel", videoFeedModel_.data());

    profileModel_.reset(new ProfileModel(session_.data(), &session_->api()));
    qmlRegisterSingletonInstance<ProfileModel>(APPLICATION_ID, 1, 0, "ProfileModel", profileModel_.data());

    playlistsModel_.reset(new PlaylistsModel(&session_->api(), session_.data()));
    qmlRegisterSingletonInstance<PlaylistsModel>(APPLICATION_ID, 1, 0, "PlaylistsModel", playlistsModel_.data());

    downloadsModel_.reset(new DownloadsModel(&session_->api(), nullptr, settings_.get()));
    qmlRegisterSingletonInstance<DownloadsModel>(APPLICATION_ID, 1, 0, "DownloadsModel", downloadsModel_.data());

    connect(
        &session_->api(),
        &Api::uploadFinished,
        videoFeedModel_.data(),
        [this](const QString&, const QString& filename) {
            emit videoFeedModel_->uploadFinished(filename);
            videoFeedModel_->refresh();
            // Keep the profile grid in sync so a fresh upload shows up on
            // the "my videos" page without the user having to hit refresh.
            profileModel_->refresh();
        }
    );
    connect(
        &session_->api(),
        &Api::uploadFailed,
        videoFeedModel_.data(),
        [this](const QString&, int code, const QString& error) { emit videoFeedModel_->uploadFailed(code, error); }
    );

    // Cross-model sync: a delete or rename on the profile page should be
    // reflected in the main feed too. We just trigger a refresh on the feed
    // — cheap and keeps them authoritatively consistent with the server.
    connect(profileModel_.data(), &ProfileModel::videoDeleted, videoFeedModel_.data(), [this](const QString&) {
        videoFeedModel_->refresh();
    });
    connect(profileModel_.data(), &ProfileModel::videoRenamed, videoFeedModel_.data(), [this](const QString&, const QString&) {
        videoFeedModel_->refresh();
    });
}

void CoreController::initSignalHandlers() {
    initTranslationsBindings();
    initAuthBindings();
}

void CoreController::initTdlibErrorBindings(TelegramClient* client) {
    // tdlibError is emitted on the TDLib polling thread; AutoConnection will
    // resolve to QueuedConnection because `this` and `session_` live on the
    // GUI thread — the slot runs on the GUI thread and it is safe to mutate
    // QObject state and drive QML bindings.
    connect(client, &TelegramClient::tdlibError, session_.data(), &Session::reportAuthError);
}

void CoreController::initAuthBindings() {
    connect(userModel_.data(), &UserModel::userChanged, this, [this]() {
        if (!userModel_->isLoaded()) return;

        UserLogin user{
            .userId = userModel_->id(),
            .username = userModel_->username(),
            .firstName = userModel_->firstName(),
            .lastName = userModel_->lastName(),
            .phoneNumber = userModel_->phoneNumber(),
            .isPremium = userModel_->isPremium(),
        };

        session_->api().loginUser(user);
    });

    connect(&session_->api(), &Api::loginSuccess, this, [this](const UserLogin& user) { session_->start(user); });

    connect(&session_->api(), &Api::loginError, this, [this](int statusCode, const QString& error) {
        session_->reportError(statusCode, error);
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
    qDebug() << "[i18n] updateTranslator called, locale:" << locale.name() << "language:" << locale.language();

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