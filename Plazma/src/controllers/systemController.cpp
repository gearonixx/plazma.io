#include "systemController.h"

void SystemsController::setQmlRoot(QObject* qmlRoot) { m_qmlRoot_ = qmlRoot; };

bool SystemsController::isAuthenticated() { return false; }
