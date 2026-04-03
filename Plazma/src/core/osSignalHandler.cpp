#include "osSignalHandler.h"
#include <QObject>

namespace {
static bool initialized = false;

}

OsSignalHandler::OsSignalHandler(QObject* parent) : QObject(parent) {};

void OsSignalHandler::setup() {
    if (initialized) {
        return;
    }

    initialized = true;
};