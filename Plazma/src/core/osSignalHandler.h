#pragma once

#include <QObject>

class OsSignalHandler : public QObject {
    Q_OBJECT
public:
    static void setup();

private:
    explicit OsSignalHandler(QObject* parent = nullptr);
    static void handleSignal(int signal);
};
