#include <QApplication>

#include <qcoreapplication.h>
#include <qguiapplication.h>
#include <QPushButton>

#include "client.h"

#include "core/osSignalHandler.h"

#include "application.h"

#include "version.h"

Q_DECL_EXPORT int main(int argc, char* argv[]) {
    PlazmaApplication app(argc, argv);
    OsSignalHandler::setup();

    app.setApplicationName(APPLICATION_NAME);
    app.setOrganizationName(ORGANIZATION_NAME);
    app.setApplicationDisplayName(APPLICATION_NAME);
    app.setApplicationVersion(APP_VERSION);

    qDebug() << app.organizationName();

    app.registerTypes();

    app.init();

    qInfo().noquote() << QString("Started %1 version %2 %3").arg(APPLICATION_NAME, APP_VERSION, APPLICATION_ID);
    qInfo().noquote() << QString("%1 (%2)").arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture());

    app.exec();

    return 0;
}
