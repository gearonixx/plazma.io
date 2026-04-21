#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QString>
#include <QTemporaryFile>

#include <exception>
#include <memory>

class Utils : public QObject {
    Q_OBJECT

public:
    [[nodiscard]] static Utils* instance();

    explicit Utils(QQmlApplicationEngine* engine);

    [[nodiscard]] Q_INVOKABLE static QString getRandomString(int len);

    [[nodiscard]] Q_INVOKABLE static QString safeBase64Decode(QString string);

    [[nodiscard]] Q_INVOKABLE static QString verifyJsonString(const QString& source);
    [[nodiscard]] Q_INVOKABLE static QJsonObject jsonFromString(const QString& string);

    [[nodiscard]] static QString
    jsonToString(const QJsonObject& json, QJsonDocument::JsonFormat format = QJsonDocument::Indented);
    [[nodiscard]] static QString
    jsonToString(const QJsonArray& array, QJsonDocument::JsonFormat format = QJsonDocument::Indented);

    // initializePath / createEmptyFile return success — caller must check
    // because failures are silent (no exceptions, no logging on success path).
    [[nodiscard]] Q_INVOKABLE static bool initializePath(const QString& path);
    [[nodiscard]] Q_INVOKABLE static bool createEmptyFile(const QString& path);

    // Returned file is unopened; caller must open() before use. Keep the
    // unique_ptr alive for as long as the temp path is needed — autoRemove
    // deletes the file when the object is destroyed.
    [[nodiscard]] static std::unique_ptr<QTemporaryFile>
    makeTempFile(const QString& prefix, const QString& suffix);

    static void logException(const std::exception& e);
    static void logException(const std::exception_ptr& eptr = std::current_exception());

private:
    static inline Utils* s_instance = nullptr;
    QQmlApplicationEngine* m_engine;
};
