#pragma once

#undef signals
#include <gtk/gtk.h>
#define signals Q_SIGNALS

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QWidget>
#include <functional>
#include <memory>

#include "../core/file_utilities.h"
#include "src/api.h"
#include "src/storage/prepare.h"
#include "src/storage/task_queue.h"

namespace platform {

struct DialogResult final {
    QStringList paths;
    QByteArray remoteContent;
};

using ResultCb = std::function<void(const DialogResult& result)>;

enum class Type {
    ReadFile,
    ReadFiles,
    ReadFolder,
    WriteFile,
};

bool GetFileDialog(
    QPointer<QWidget> parent,
    QStringList& files,
    QByteArray& remoteContent,
    const QString& caption,
    const QString& filter,
    Type type
);

class FileDialog : public QObject {
    Q_OBJECT

public:
    explicit FileDialog(Api* api, QObject* parent = nullptr);
    ~FileDialog();

    void GetOpenPaths(
        QPointer<QWidget> parent,
        const QString& caption,
        const QString& filter,
        ResultCb callback,
        std::function<void()> failed
    );

    void prepareFileTasks(
        storages::prepare::PreparedList&& bundle,
        plazma::task_queue::SendMediaType type = plazma::task_queue::SendMediaType::File
    );

    void attachFiles(plazma::task_queue::SendMediaType type = plazma::task_queue::SendMediaType::File);

private:
    Api* api_ = nullptr;
};  // class FileDialog

}  // namespace platform

namespace Platform::FileDialog {

inline bool Get(
    QPointer<QWidget> parent,
    QStringList& files,
    QByteArray& remoteContent,
    const QString& caption,
    const QString& filter,
    platform::Type type,
    QString startFile
) {
    if (!gtk_init_check(nullptr, nullptr)) {
        return false;
    }

    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        caption.toUtf8().constData(),
        nullptr,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel",
        GTK_RESPONSE_CANCEL,
        "_Open",
        GTK_RESPONSE_ACCEPT,
        nullptr
    );

    if (type == platform::Type::ReadFiles) {
        gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
    }

    GtkFileFilter* videoFilter = gtk_file_filter_new();
    gtk_file_filter_set_name(videoFilter, "Video files");
    gtk_file_filter_add_mime_type(videoFilter, "video/*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), videoFilter);

    GtkFileFilter* allFilter = gtk_file_filter_new();
    gtk_file_filter_set_name(allFilter, "All files");
    gtk_file_filter_add_pattern(allFilter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), allFilter);

    // Set starting directory
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), startFile.toUtf8().constData());

    gint result = gtk_dialog_run(GTK_DIALOG(dialog));

    if (result == GTK_RESPONSE_ACCEPT) {
        GSList* filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
        for (GSList* f = filenames; f; f = f->next) {
            files.push_back(QString::fromUtf8((char*)f->data));
            g_free(f->data);
        }
        g_slist_free(filenames);
        gtk_widget_destroy(dialog);
        return true;
    }

    gtk_widget_destroy(dialog);
    return false;
}

}  // namespace Platform::FileDialog