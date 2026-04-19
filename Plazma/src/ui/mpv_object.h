#pragma once

#include <QVariantList>
#include <QtQuick/QQuickFramebufferObject>

#include <mpv/client.h>
#include <mpv/render_gl.h>

class MpvObject : public QQuickFramebufferObject {
    Q_OBJECT
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double bufferedPosition READ bufferedPosition NOTIFY bufferedPositionChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(bool loop READ loop WRITE setLoop NOTIFY loopChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool hasMedia READ hasMedia NOTIFY hasMediaChanged)
    Q_PROPERTY(int videoWidth READ videoWidth NOTIFY videoSizeChanged)
    Q_PROPERTY(int videoHeight READ videoHeight NOTIFY videoSizeChanged)
    Q_PROPERTY(QVariantList audioTracks READ audioTracks NOTIFY tracksChanged)
    Q_PROPERTY(QVariantList subtitleTracks READ subtitleTracks NOTIFY tracksChanged)

public:
    explicit MpvObject(QQuickItem* parent = nullptr);
    ~MpvObject() override;

    Renderer* createRenderer() const override;

    QString source() const { return source_; }
    void setSource(const QString& source);

    bool paused() const { return paused_; }
    double duration() const { return duration_; }
    double position() const { return position_; }
    double bufferedPosition() const { return bufferedPosition_; }

    double volume() const { return volume_; }
    void setVolume(double v);

    bool muted() const { return muted_; }
    void setMuted(bool m);

    double speed() const { return speed_; }
    void setSpeed(double s);

    bool loop() const { return loop_; }
    void setLoop(bool l);

    bool loading() const { return loading_; }
    bool hasMedia() const { return hasMedia_; }

    int videoWidth() const { return videoWidth_; }
    int videoHeight() const { return videoHeight_; }

    QVariantList audioTracks() const { return audioTracks_; }
    QVariantList subtitleTracks() const { return subtitleTracks_; }

    mpv_handle* handle() const { return mpv_; }

public slots:
    void load(const QString& path);
    void stop();
    void playPause();
    void play();
    void pause();
    void seek(double seconds);
    void seekRelative(double deltaSeconds);
    void seekPercent(double percent);
    void toggleMute();
    void toggleLoop();
    void setAudioTrack(int id);
    void setSubtitleTrack(int id);
    void frameStep();
    void frameBackStep();
    QString takeScreenshot();

signals:
    void sourceChanged();
    void pausedChanged();
    void durationChanged();
    void positionChanged();
    void bufferedPositionChanged();
    void volumeChanged();
    void mutedChanged();
    void speedChanged();
    void loopChanged();
    void loadingChanged();
    void hasMediaChanged();
    void videoSizeChanged();
    void tracksChanged();
    void fileLoaded();
    void playbackError(const QString& message);
    void endReached();
    void screenshotSaved(const QString& path);
    void onMpvEvents();

private slots:
    void handleMpvEvents();

private:
    static void onMpvWakeup(void* ctx);
    void setPausedState(bool paused);
    void refreshTrackList();

    mpv_handle* mpv_ = nullptr;
    QString source_;
    bool paused_ = true;
    double duration_ = 0.0;
    double position_ = 0.0;
    double bufferedPosition_ = 0.0;
    double volume_ = 100.0;
    bool muted_ = false;
    double speed_ = 1.0;
    bool loop_ = false;
    bool loading_ = false;
    bool hasMedia_ = false;
    int videoWidth_ = 0;
    int videoHeight_ = 0;
    QVariantList audioTracks_;
    QVariantList subtitleTracks_;
};
