#include "file_dialog_model.h"

FileDialogModel::FileDialogModel(platform::FileDialog* fileDialog, QObject* parent)
    : QObject(parent), fileDialog_(fileDialog) {
    Q_ASSERT(fileDialog != nullptr);
}

QStringList FileDialogModel::selectedPaths() const { return selectedPaths_; }

void FileDialogModel::openFilePicker() {
    fileDialog_->attachFiles(plazma::task_queue::SendMediaType::Video);
}
