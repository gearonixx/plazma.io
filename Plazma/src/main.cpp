#include <clocale>

#include <QApplication>
#ifdef Q_OS_WIN
#  include <windows.h>
#endif

#include <qcoreapplication.h>
#include <qguiapplication.h>
#include <QPushButton>

#include "client.h"

#include "core/fatal.h"
#include "core/osSignalHandler.h"

#include "application.h"

#include "version.h"

Q_DECL_EXPORT int main(int argc, char* argv[]) {
    std::setlocale(LC_NUMERIC, "C");
#ifdef Q_OS_WIN
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif

    PlazmaApplication app(argc, argv);

    std::setlocale(LC_NUMERIC, "C");
    OsSignalHandler::setup();

    app.setApplicationName(APPLICATION_NAME);
    app.setOrganizationName(ORGANIZATION_NAME);
    app.setApplicationDisplayName(APPLICATION_NAME);
    app.setApplicationVersion(APP_VERSION);

    // Install fatal-error plumbing after app identity is set so the default
    // callback's crash.log lands in the right <AppDataLocation>.
    plazma::fatal::install();

    qDebug() << app.organizationName();

    qRegisterMetaType<UserPtr>("UserPtr");

    app.registerTypes();

    app.init();

    qInfo().noquote() << QString("Started %1 version %2 %3").arg(APPLICATION_NAME, APP_VERSION, APPLICATION_ID);
    qInfo().noquote() << QString("%1 (%2)").arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture());

    app.exec();

    return 0;
}
