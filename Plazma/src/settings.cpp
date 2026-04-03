#include "settings.h"
#include "version.h"

Settings::Settings(QObject* parent) : settings_(ORGANIZATION_NAME, APPLICATION_NAME, parent) {}