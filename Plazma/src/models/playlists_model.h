#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <vector>

class Api;
class Session;

// PlaylistsModel
// ──────────────
// Single-source-of-truth for YouTube-style playlists, server-backed via the
// endpoints documented in `PlazmaServer/docs/playlists.md`. The model keeps a
// **local mirror** of the user's playlists for synchronous QML reads (the
// existing call sites depend on `createPlaylist(name) → id`,
// `playlistsContaining(...)`, etc. being synchronous), and applies every
// mutation in an "optimistic + reconcile" pattern:
//
//   1. Apply the change locally and emit Qt model signals so the UI updates
//      immediately.
//   2. Fire the corresponding API call.
//   3. On 2xx, reconcile any server-canonical fields (e.g. `added_at`,
//      `cover_thumbnails`) into the local row.
//   4. On 4xx/5xx, roll back the local change and emit `notify(QString)` so
//      the UI can surface the error.
//
// Persistence is the server. The model intentionally does **not** cache to
// QSettings — a stale local cache after another device mutated state would
// be worse than a brief loading state. `refresh()` is called automatically
// when the session goes valid; QML can also trigger it manually for a
// pull-to-refresh gesture.
//
// Ordering: case-insensitive lexicographic on `name`, ties broken by `id` so
// repeated names (which the validator prevents, but defensively) stay
// stable.
class PlaylistsModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString currentPlaylistId READ currentPlaylistId NOTIFY currentChanged)
    Q_PROPERTY(QString currentPlaylistName READ currentPlaylistName NOTIFY currentChanged)
    Q_PROPERTY(QVariantList currentVideos READ currentVideos NOTIFY currentChanged)
    Q_PROPERTY(QString lastCreatedId READ lastCreatedId NOTIFY lastCreatedChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        VideoCountRole,
        ThumbnailsRole,   // QStringList, up to 4 thumbnails — the cover mosaic
        FirstThumbRole,   // convenience: first non-empty thumbnail or ""
        UpdatedAtRole,
    };

    struct Video {
        QString id;
        QString title;
        QString url;
        qint64 size = 0;
        QString mime;
        QString author;
        QString createdAt;
        QString thumbnail;
        QString storyboard;
        QString description;  // preserved so the watch page keeps the description when replayed from a playlist entry
        QString addedAt;      // ISO-8601 UTC
    };

    struct Playlist {
        QString id;
        QString name;
        QString createdAt;
        QString updatedAt;
        std::vector<Video> videos;
    };

    // Api may be null for tests / pre-login bootstrap; the model degrades
    // gracefully (mutations no-op on the server, local state still works).
    // Once Session::valid flips true, refresh() is invoked automatically.
    explicit PlaylistsModel(Api* api = nullptr, Session* session = nullptr, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const { return static_cast<int>(playlists_.size()); }
    [[nodiscard]] QString currentPlaylistId() const { return currentId_; }
    [[nodiscard]] QString currentPlaylistName() const;
    [[nodiscard]] QVariantList currentVideos() const;
    [[nodiscard]] QString lastCreatedId() const { return lastCreatedId_; }
    [[nodiscard]] bool loading() const { return loading_; }

public slots:
    // Returns the new playlist's id on success, or an empty string if the name
    // is invalid or already taken (case-insensitive). The id is generated
    // locally (UUID v7-shaped) and registered with the server in the
    // background — the contract is preserved synchronously.
    Q_INVOKABLE QString createPlaylist(const QString& name);
    Q_INVOKABLE bool renamePlaylist(const QString& id, const QString& newName);
    Q_INVOKABLE bool deletePlaylist(const QString& id);

    // Adds video to playlist. Idempotent — returns false if the video was
    // already present (so the UI can surface "Already in this playlist").
    Q_INVOKABLE bool addVideoToPlaylist(const QString& playlistId, const QVariantMap& video);
    Q_INVOKABLE bool removeVideoFromPlaylist(const QString& playlistId, const QString& videoId);

    Q_INVOKABLE bool playlistContains(const QString& playlistId, const QString& videoId) const;
    Q_INVOKABLE QString playlistName(const QString& id) const;
    Q_INVOKABLE QStringList playlistsContaining(const QString& videoId) const;

    // Returns a lightweight summary list {id, name, contains} for every
    // playlist — used by the "Save to playlist" submenu, which needs to know
    // which playlists already have the video.
    Q_INVOKABLE QVariantList summariesForVideo(const QString& videoId) const;

    // Validation — empty/whitespace is rejected.
    Q_INVOKABLE bool isValidName(const QString& name) const;
    Q_INVOKABLE bool isNameTaken(const QString& name, const QString& exceptId = QString()) const;

    // Select a playlist as "open". The detail page binds to currentVideos.
    // Triggers a lazy fetch of items if they haven't been loaded yet.
    Q_INVOKABLE void openPlaylist(const QString& id);
    Q_INVOKABLE void closeCurrent();

    // Pulls the latest snapshot from the server and reconciles the local
    // mirror. Safe to call repeatedly — in-flight calls are coalesced.
    Q_INVOKABLE void refresh();

signals:
    void countChanged();
    void currentChanged();
    void lastCreatedChanged();
    void loadingChanged();
    void notify(QString message);

private:
    // ── Local mutation helpers (synchronous, drive Qt model signals) ──
    void sortByName();
    int indexOf(const QString& id) const;
    Playlist* find(const QString& id);
    const Playlist* find(const QString& id) const;
    void touch(Playlist& p);
    void emitRowChanged(int row);
    QStringList previewThumbnails(const Playlist& p) const;
    QString firstThumbnail(const Playlist& p) const;

    // ── Reconcile from server-side JSON (after refresh / mutation reply) ──
    void reconcileSummariesFromServer(const QJsonArray& summaries);
    void reconcileSingleSummary(const QJsonObject& summary);
    void reconcileItemsFor(const QString& playlistId, const QJsonArray& items);
    static Playlist summaryFromJson(const QJsonObject& obj);
    static Video itemFromJson(const QJsonObject& obj);
    static QJsonObject videoSnapshotForServer(const Video& v);

    static Video videoFromVariant(const QVariantMap& m);
    static QVariantMap videoToVariant(const Video& v);
    static QString makeId();
    static QString isoNowUtc();

    void setLoading(bool v);

    QPointer<Api> api_;
    std::vector<Playlist> playlists_;
    QString currentId_;
    QString lastCreatedId_;
    bool loading_ = false;
    bool refreshInFlight_ = false;
    bool refreshQueued_ = false;
};
