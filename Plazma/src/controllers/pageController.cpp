#include "pageController.h"

PageController::PageController(QObject* parent) : QObject(parent) {};

void PageController::showOnStartup() {
    //
};

// PageLogin
QString PageController::getPagePath(PageLoader::PageEnum page) {
    QMetaEnum metaEnum = QMetaEnum::fromType<PageLoader::PageEnum>();
    QString pageName = metaEnum.valueToKey(static_cast<int>(page));

    return "qrc:/ui/Pages/" + pageName + ".qml";
};