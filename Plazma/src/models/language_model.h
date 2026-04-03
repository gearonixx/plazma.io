#pragma once

#include <qqml.h>

#include <QObject>
#include <QString>
#include <memory>

#include <QAbstractListModel>

#include "src/settings.h"

namespace LanguageSettings {
Q_NAMESPACE
enum class AvailablePageEnum { English = 0, Russian, China_cn };

Q_ENUM_NS(AvailablePageEnum);

static void declareQmlAvailableLanguageEnum() {
    qmlRegisterUncreatableMetaObject(staticMetaObject, "AvailablePageEnum", 1, 0, "AvailablePageEnum", QString());
}
}  // namespace LanguageSettings

struct ModelLanguageData {
    QString name;
    LanguageSettings::AvailablePageEnum page;
};

class LanguageModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit LanguageModel(std::shared_ptr<Settings> settings, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;

    Q_PROPERTY(QString currentLanguageName READ getCurrentLanguageName NOTIFY translationsUpdated)
    Q_PROPERTY(int currentLanguageIndex READ getCurrentLanguageIndex NOTIFY translationsUpdated)

    QString getLocalLanguageName(const LanguageSettings::AvailablePageEnum language);
public slots:
    void changeLanguage(const LanguageSettings::AvailablePageEnum language);
    QString getCurrentLanguageName() const;
    int getCurrentLanguageIndex() const;

signals:
    void updateTranslations(const QLocale);
    void translationsUpdated() const;

    void temporarySignalToSendVideo();

private:
    QVector<ModelLanguageData> availableLanguages_;
    std::shared_ptr<Settings> settings_;
};
