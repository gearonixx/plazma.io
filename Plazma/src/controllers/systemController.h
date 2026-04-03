#pragma once

#include <QObject>
#include <QVariant>

class SystemsController : public QObject {
    Q_OBJECT

public:
    template <typename T>
    explicit SystemsController(const std::shared_ptr<T>& setting, const QObject* parent = nullptr) {};

public slots:
    void setQmlRoot(QObject* qmlRoot);
    static bool isAuthenticated();

private:
    QObject* m_qmlRoot_;
};