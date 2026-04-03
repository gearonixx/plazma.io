#pragma once

#include "core/controllers/coreController.h"
#include "settings.h"

#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
#include <QGuiApplication>
#else
#include <QApplication>
#endif

#include <QQmlApplicationEngine>

#include <memory>
#include "client.h"

#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
#define PLAZMA_BASE_CLASS QGuiApplication
#else
#define PLAZMA_BASE_CLASS QApplication
#endif

class PlazmaApplication : public PLAZMA_BASE_CLASS {
    Q_OBJECT

    void onObjectCreated(QObject* qmlObject, const QUrl& objectUrl);

public:
    PlazmaApplication(int& argc, char* argv[]);

    void init();

    void registerTypes();

    QQmlApplicationEngine* qmlEngine() const;
public slots:
    void forceQuit();

private:
    QQmlApplicationEngine* qmlEngine_{};

    QUrl rootQmlFileUrl_{};

    std::shared_ptr<Settings> settings_;

    QScopedPointer<CoreController> coreController_;
    QSharedPointer<TelegramClient> telegramClient_;

    static bool forceQuit_;

protected:
    bool eventFilter(QObject* obj, QEvent* event) override { return PLAZMA_BASE_CLASS::eventFilter(obj, event); };
};
