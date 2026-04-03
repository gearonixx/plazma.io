 #pragma once

#include <QObject>
#include <QStringList>

#include "src/platform/file_dialog.h"

class FileDialogModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(QStringList selectedPaths READ selectedPaths NOTIFY pathsChanged)

public:
    explicit FileDialogModel(platform::FileDialog* fileDialog, QObject* parent = nullptr);

    QStringList selectedPaths() const;

public slots:
    void openFilePicker();

signals:
    void pathsChanged();
    void fileSelected(const QString& path);

private:
    platform::FileDialog* fileDialog_ = nullptr;
    QStringList selectedPaths_;
};
