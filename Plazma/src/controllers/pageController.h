#pragma once

#include <qqml.h>

#include <QObject>

namespace PageLoader {
Q_NAMESPACE

enum class PageEnum { PageStart = 0, PageLogin };

Q_ENUM_NS(PageEnum);

static void declareQmlEnum() {
    qmlRegisterUncreatableMetaObject(staticMetaObject, "PageEnum", 1, 0, "PageEnum", "Error: only enums");
}
}  // namespace PageLoader

class PageController : public QObject {
    Q_OBJECT

public:
    explicit PageController(QObject* parent = nullptr);

    void showOnStartup();

public slots:
    QString getPagePath(PageLoader::PageEnum page);

signals:
    void goToPage(PageLoader::PageEnum page);
};