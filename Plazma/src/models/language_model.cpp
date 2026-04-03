#include "language_model.h"

#include <QMetaEnum>
#include <memory>

using Lang = LanguageSettings::AvailablePageEnum;

// TODO(grnx): impl i18n from telegram?
LanguageModel::LanguageModel(std::shared_ptr<Settings> settings, QObject* parent)
    : settings_(settings), QAbstractListModel(parent) {
    QMetaEnum metaEnum = QMetaEnum::fromType<LanguageSettings::AvailablePageEnum>();

    for (int key = 0; key < metaEnum.keyCount(); key++) {
        auto language = static_cast<Lang>(key);
        availableLanguages_.push_back(ModelLanguageData{getLocalLanguageName(language), language});
    }
}

QString LanguageModel::getLocalLanguageName(const Lang language) {
    QString languageName;

    switch (language) {
        case Lang::English:
            languageName = "English";
            break;
        case Lang::Russian:
            languageName = "Русский";
            break;
        case Lang::China_cn:
            languageName = "\347\256\200\344\275\223\344\270\255\346\226\207";
            break;
        default:
            break;
    };

    return languageName;
}

void LanguageModel::changeLanguage(const Lang language) {
    qDebug() << "[i18n] changeLanguage called:" << static_cast<int>(language);

    switch (language) {
        case Lang::English:
            emit updateTranslations(QLocale::English);
            break;
        case Lang::Russian:
            emit updateTranslations(QLocale::Russian);
            break;
        case Lang::China_cn:
            emit updateTranslations(QLocale::Chinese);
            break;
        default:
            emit updateTranslations(QLocale::English);
            break;
    }
}

// slots
QString LanguageModel::getCurrentLanguageName() const { return availableLanguages_[getCurrentLanguageIndex()].name; };

// TODO(grnx)
// ideally AvailableLanguageEnum values would just match QLocale::Language values directly,
// so you wouldn't need any mapping at all

// TODO
// telegram geolocation
int LanguageModel::getCurrentLanguageIndex() const {
    QLocale currentLocale = settings_->getAppLanguage();

    switch (currentLocale.language()) {
        case QLocale::English:
            return static_cast<int>(Lang::English);
        case QLocale::Russian:
            return static_cast<int>(Lang::Russian);
        case QLocale::Chinese:
            return static_cast<int>(Lang::China_cn);
        default:
            return static_cast<int>(Lang::English);
    }
};

//

int LanguageModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return static_cast<int>(availableLanguages_.size());
}

QVariant LanguageModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(availableLanguages_.size())) {
        return QVariant();
    }

    return static_cast<int>(availableLanguages_[index.row()].page);
}
