#pragma once

#include <qlocale.h>

#include <QObject>
#include <QSettings>

#include <QDebug>

class Settings : public QObject {
public:
    explicit Settings(QObject* parent = nullptr);

    QLocale getAppLanguage() const {
        const QString localeStr = settings_.value("config/language", QLocale::system().name()).toString();

        qDebug() << "default locale name " << localeStr;

        return QLocale(localeStr);
    }

    void setAppLanguage(QLocale locale) { setValue("config/language", locale.name()); }

private:
    // TODO
    void setValue(const QString name, const QVariant& value) { settings_.setValue(name, value); };

    mutable QSettings settings_;
};