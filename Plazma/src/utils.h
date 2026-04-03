#pragma once

#include <QObject>
#include <QQmlApplicationEngine>

class Utils : public QObject {
    Q_OBJECT

private:
public:
    static Utils* instance();

    explicit Utils(QQmlApplicationEngine* engine);

private:
    static inline Utils* s_instance = nullptr;
    QQmlApplicationEngine* m_engine;
};