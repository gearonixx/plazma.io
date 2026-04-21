#include "utils.h"

#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonParseError>
#include <QRandomGenerator>

Utils::Utils(QQmlApplicationEngine* engine) : m_engine(engine) { s_instance = this; }

Utils* Utils::instance() { return s_instance; }

static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
static constexpr int kAlphabetLen = sizeof(kAlphabet) - 1;

// Qt's QTemporaryFile template placeholder — replaced with random chars on open().
static constexpr auto kTempFileRandomPlaceholder = "XXXXXX";

QString Utils::getRandomString(int len) {
    if (len <= 0) return {};

    auto* rng = QRandomGenerator::system();

    QString out(len, Qt::Uninitialized);
    for (int i = 0; i < len; ++i) {
        out[i] = QLatin1Char(kAlphabet[rng->bounded(kAlphabetLen)]);
    }
    return out;
}

QString Utils::safeBase64Decode(QString string) {
    QByteArray ba = string.replace(QChar('-'), QChar('+')).replace(QChar('_'), QChar('/')).toUtf8();
    return QByteArray::fromBase64(ba, QByteArray::Base64Option::OmitTrailingEquals);
}

QString Utils::verifyJsonString(const QString& source) {
    QJsonParseError error;
    QJsonDocument::fromJson(source.toUtf8(), &error);

    if (error.error == QJsonParseError::NoError) {
        return {};
    }
    qWarning() << "Json parse returns:" << error.errorString();
    return error.errorString();
}

QJsonObject Utils::jsonFromString(const QString& string) {
    QJsonDocument doc = QJsonDocument::fromJson(string.trimmed().toUtf8());
    return doc.object();
}

QString Utils::jsonToString(const QJsonObject& json, QJsonDocument::JsonFormat format) {
    QJsonDocument doc;
    doc.setObject(json);
    return doc.toJson(format);
}

QString Utils::jsonToString(const QJsonArray& array, QJsonDocument::JsonFormat format) {
    QJsonDocument doc;
    doc.setArray(array);
    return doc.toJson(format);
}

bool Utils::initializePath(const QString& path) {
    QDir dir;
    if (!dir.mkpath(path)) {
        qWarning().noquote() << QString("Cannot initialize path: '%1'").arg(path);
        return false;
    }
    return true;
}

bool Utils::createEmptyFile(const QString& path) {
    QFile f(path);
    return f.open(QIODevice::WriteOnly | QIODevice::Truncate);
}

std::unique_ptr<QTemporaryFile> Utils::makeTempFile(const QString& prefix, const QString& suffix) {
    auto file = std::make_unique<QTemporaryFile>(prefix + QLatin1String(kTempFileRandomPlaceholder) + suffix);
    file->setAutoRemove(true);

    return file;
}

void Utils::logException(const std::exception& e) {
    qCritical() << e.what();
    try {
        std::rethrow_if_nested(e);
    } catch (const std::exception& nested) {
        logException(nested);
    } catch (...) {
    }
}

void Utils::logException(const std::exception_ptr& eptr) {
    try {
        if (eptr) std::rethrow_exception(eptr);
    } catch (const std::exception& e) {
        logException(e);
    } catch (...) {
    }
}
