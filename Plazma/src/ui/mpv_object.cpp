#include "mpv_object.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QMetaObject>
#include <QtCore/QStandardPaths>
#include <QtCore/QVariant>
#include <QtCore/QVariantMap>
#include <QtGui/QOpenGLContext>
#include <QtOpenGL/QOpenGLFramebufferObject>
#include <QtOpenGL/QOpenGLFramebufferObjectFormat>
#include <QtQuick/QQuickWindow>

#include <stdexcept>

namespace {

void* getProcAddress(void* /*ctx*/, const char* name) {
    QOpenGLContext* glctx = QOpenGLContext::currentContext();
    if (!glctx) return nullptr;
    return reinterpret_cast<void*>(glctx->getProcAddress(QByteArray(name)));
}

QVariant mpvNodeToVariant(const mpv_node& node) {
    switch (node.format) {
        case MPV_FORMAT_STRING: return QString::fromUtf8(node.u.string);
        case MPV_FORMAT_FLAG:   return node.u.flag != 0;
        case MPV_FORMAT_INT64:  return static_cast<qint64>(node.u.int64);
        case MPV_FORMAT_DOUBLE: return node.u.double_;
        case MPV_FORMAT_NODE_ARRAY: {
            QVariantList list;
            for (int i = 0; i < node.u.list->num; ++i)
                list.append(mpvNodeToVariant(node.u.list->values[i]));
            return list;
        }
        case MPV_FORMAT_NODE_MAP: {
            QVariantMap map;
            for (int i = 0; i < node.u.list->num; ++i)
                map.insert(QString::fromUtf8(node.u.list->keys[i]),
                           mpvNodeToVariant(node.u.list->values[i]));
            return map;
        }
        default: return {};
    }
}

class MpvRenderer final : public QQuickFramebufferObject::Renderer {
public:
    explicit MpvRenderer(MpvObject* owner) : owner_(owner) {
        mpv_opengl_init_params gl_init_params{getProcAddress, nullptr};
        int advanced_control = 1;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
            {MPV_RENDER_PARAM_ADVANCED_CONTROL, &advanced_control},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        if (mpv_render_context_create(&render_ctx_, owner_->handle(), params) < 0) {
            throw std::runtime_error("failed to initialize mpv render context");
        }
        mpv_render_context_set_update_callback(render_ctx_, MpvRenderer::onUpdate, owner_);
    }

    ~MpvRenderer() override {
        if (render_ctx_) mpv_render_context_free(render_ctx_);
    }

    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override {
        QOpenGLFramebufferObjectFormat fmt;
        fmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        return new QOpenGLFramebufferObject(size, fmt);
    }

    void render() override {
        QOpenGLFramebufferObject* fbo = framebufferObject();
        mpv_opengl_fbo mpfbo{
            static_cast<int>(fbo->handle()),
            fbo->width(),
            fbo->height(),
            0,
        };
        int flip_y = 0;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo},
            {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        mpv_render_context_render(render_ctx_, params);
    }

private:
    static void onUpdate(void* ctx) {
        QMetaObject::invokeMethod(
            static_cast<MpvObject*>(ctx), "update", Qt::QueuedConnection
        );
    }

    MpvObject* owner_;
    mpv_render_context* render_ctx_ = nullptr;
};

}  // namespace

MpvObject::MpvObject(QQuickItem* parent) : QQuickFramebufferObject(parent) {
    mpv_ = mpv_create();
    if (!mpv_) throw std::runtime_error("mpv_create failed");

    mpv_set_option_string(mpv_, "terminal", "no");
    mpv_set_option_string(mpv_, "msg-level", "all=warn");
    mpv_set_option_string(mpv_, "vo", "libmpv");
    mpv_set_option_string(mpv_, "hwdec", "auto-safe");
    mpv_set_option_string(mpv_, "keep-open", "yes");
    mpv_set_option_string(mpv_, "idle", "yes");
    mpv_set_option_string(mpv_, "force-seekable", "yes");

    if (mpv_initialize(mpv_) < 0) {
        throw std::runtime_error("mpv_initialize failed");
    }

    mpv_observe_property(mpv_, 0, "pause",              MPV_FORMAT_FLAG);
    mpv_observe_property(mpv_, 0, "duration",           MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_, 0, "time-pos",           MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_, 0, "demuxer-cache-time", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_, 0, "volume",             MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_, 0, "mute",               MPV_FORMAT_FLAG);
    mpv_observe_property(mpv_, 0, "speed",              MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_, 0, "core-idle",          MPV_FORMAT_FLAG);
    mpv_observe_property(mpv_, 0, "seeking",            MPV_FORMAT_FLAG);
    mpv_observe_property(mpv_, 0, "eof-reached",        MPV_FORMAT_FLAG);
    mpv_observe_property(mpv_, 0, "width",              MPV_FORMAT_INT64);
    mpv_observe_property(mpv_, 0, "height",             MPV_FORMAT_INT64);

    connect(this, &MpvObject::onMpvEvents, this, &MpvObject::handleMpvEvents, Qt::QueuedConnection);
    mpv_set_wakeup_callback(mpv_, &MpvObject::onMpvWakeup, this);
}

MpvObject::~MpvObject() {
    if (mpv_) mpv_terminate_destroy(mpv_);
}

QQuickFramebufferObject::Renderer* MpvObject::createRenderer() const {
    return new MpvRenderer(const_cast<MpvObject*>(this));
}

void MpvObject::setSource(const QString& source) {
    if (source == source_) return;
    source_ = source;
    emit sourceChanged();
    load(source);
}

void MpvObject::load(const QString& path) {
    loading_ = true;
    emit loadingChanged();

    const QByteArray utf8 = path.toUtf8();
    const char* cmd[] = {"loadfile", utf8.constData(), nullptr};
    mpv_command_async(mpv_, 0, cmd);
}

void MpvObject::stop() {
    const char* cmd[] = {"stop", nullptr};
    mpv_command_async(mpv_, 0, cmd);
    if (hasMedia_) {
        hasMedia_ = false;
        emit hasMediaChanged();
    }
}

void MpvObject::playPause() { setPausedState(!paused_); }
void MpvObject::play() { setPausedState(false); }
void MpvObject::pause() { setPausedState(true); }

void MpvObject::setPausedState(bool paused) {
    int flag = paused ? 1 : 0;
    mpv_set_property_async(mpv_, 0, "pause", MPV_FORMAT_FLAG, &flag);
}

void MpvObject::seek(double seconds) {
    const QByteArray s = QByteArray::number(seconds);
    const char* cmd[] = {"seek", s.constData(), "absolute", nullptr};
    mpv_command_async(mpv_, 0, cmd);
}

void MpvObject::seekRelative(double deltaSeconds) {
    const QByteArray s = QByteArray::number(deltaSeconds);
    const char* cmd[] = {"seek", s.constData(), "relative", nullptr};
    mpv_command_async(mpv_, 0, cmd);
}

void MpvObject::seekPercent(double percent) {
    const QByteArray s = QByteArray::number(qBound(0.0, percent, 100.0));
    const char* cmd[] = {"seek", s.constData(), "absolute-percent", nullptr};
    mpv_command_async(mpv_, 0, cmd);
}

void MpvObject::setVolume(double v) {
    v = qBound(0.0, v, 100.0);
    mpv_set_property_async(mpv_, 0, "volume", MPV_FORMAT_DOUBLE, &v);
}

void MpvObject::setMuted(bool m) {
    int flag = m ? 1 : 0;
    mpv_set_property_async(mpv_, 0, "mute", MPV_FORMAT_FLAG, &flag);
}

void MpvObject::toggleMute() { setMuted(!muted_); }

void MpvObject::setSpeed(double s) {
    s = qBound(0.25, s, 4.0);
    mpv_set_property_async(mpv_, 0, "speed", MPV_FORMAT_DOUBLE, &s);
}

void MpvObject::setLoop(bool l) {
    loop_ = l;
    mpv_set_property_string(mpv_, "loop-file", l ? "inf" : "no");
    emit loopChanged();
}

void MpvObject::toggleLoop() { setLoop(!loop_); }

void MpvObject::setAudioTrack(int id) {
    const QByteArray s = id > 0 ? QByteArray::number(id) : QByteArray("no");
    mpv_set_property_string(mpv_, "aid", s.constData());
}

void MpvObject::setSubtitleTrack(int id) {
    const QByteArray s = id > 0 ? QByteArray::number(id) : QByteArray("no");
    mpv_set_property_string(mpv_, "sid", s.constData());
}

void MpvObject::frameStep() {
    const char* cmd[] = {"frame-step", nullptr};
    mpv_command_async(mpv_, 0, cmd);
}

void MpvObject::frameBackStep() {
    const char* cmd[] = {"frame-back-step", nullptr};
    mpv_command_async(mpv_, 0, cmd);
}

QString MpvObject::takeScreenshot() {
    if (!hasMedia_) return {};

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QDir().mkpath(dir);
    const QString path = QDir(dir).filePath(
        QStringLiteral("plazma-%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss"))
    );

    const QByteArray utf8 = path.toUtf8();
    const char* cmd[] = {"screenshot-to-file", utf8.constData(), "video", nullptr};
    if (mpv_command(mpv_, cmd) >= 0) {
        emit screenshotSaved(path);
        return path;
    }
    return {};
}

void MpvObject::refreshTrackList() {
    mpv_node node;
    if (mpv_get_property(mpv_, "track-list", MPV_FORMAT_NODE, &node) < 0) return;

    QVariantList tracks = mpvNodeToVariant(node).toList();
    mpv_free_node_contents(&node);

    audioTracks_.clear();
    subtitleTracks_.clear();

    for (const QVariant& item : tracks) {
        QVariantMap m = item.toMap();
        const QString type = m.value("type").toString();
        QVariantMap track;
        track["id"] = m.value("id");
        track["title"] = m.value("title", m.value("lang", QStringLiteral("Track"))).toString();
        track["lang"] = m.value("lang");
        track["selected"] = m.value("selected").toBool();
        if (type == "audio") audioTracks_.append(track);
        else if (type == "sub") subtitleTracks_.append(track);
    }

    emit tracksChanged();
}

void MpvObject::onMpvWakeup(void* ctx) {
    emit static_cast<MpvObject*>(ctx)->onMpvEvents();
}

void MpvObject::handleMpvEvents() {
    while (mpv_) {
        mpv_event* ev = mpv_wait_event(mpv_, 0);
        if (ev->event_id == MPV_EVENT_NONE) break;

        switch (ev->event_id) {
            case MPV_EVENT_FILE_LOADED: {
                if (!hasMedia_) { hasMedia_ = true; emit hasMediaChanged(); }
                if (loading_)   { loading_  = false; emit loadingChanged(); }
                refreshTrackList();
                emit fileLoaded();
                break;
            }
            case MPV_EVENT_END_FILE: {
                auto* ef = static_cast<mpv_event_end_file*>(ev->data);
                if (ef && ef->reason == MPV_END_FILE_REASON_ERROR) {
                    emit playbackError(QString::fromUtf8(mpv_error_string(ef->error)));
                } else if (ef && ef->reason == MPV_END_FILE_REASON_EOF) {
                    emit endReached();
                }
                if (loading_) { loading_ = false; emit loadingChanged(); }
                break;
            }
            case MPV_EVENT_PROPERTY_CHANGE: {
                auto* prop = static_cast<mpv_event_property*>(ev->data);
                if (prop->format == MPV_FORMAT_NONE) continue;

                const QByteArray name(prop->name);
                if (name == "pause" && prop->format == MPV_FORMAT_FLAG) {
                    paused_ = *static_cast<int*>(prop->data) != 0;
                    emit pausedChanged();
                } else if (name == "duration" && prop->format == MPV_FORMAT_DOUBLE) {
                    duration_ = *static_cast<double*>(prop->data);
                    emit durationChanged();
                } else if (name == "time-pos" && prop->format == MPV_FORMAT_DOUBLE) {
                    position_ = *static_cast<double*>(prop->data);
                    emit positionChanged();
                } else if (name == "demuxer-cache-time" && prop->format == MPV_FORMAT_DOUBLE) {
                    bufferedPosition_ = *static_cast<double*>(prop->data);
                    emit bufferedPositionChanged();
                } else if (name == "volume" && prop->format == MPV_FORMAT_DOUBLE) {
                    volume_ = *static_cast<double*>(prop->data);
                    emit volumeChanged();
                } else if (name == "mute" && prop->format == MPV_FORMAT_FLAG) {
                    muted_ = *static_cast<int*>(prop->data) != 0;
                    emit mutedChanged();
                } else if (name == "speed" && prop->format == MPV_FORMAT_DOUBLE) {
                    speed_ = *static_cast<double*>(prop->data);
                    emit speedChanged();
                } else if (name == "width" && prop->format == MPV_FORMAT_INT64) {
                    videoWidth_ = static_cast<int>(*static_cast<int64_t*>(prop->data));
                    emit videoSizeChanged();
                } else if (name == "height" && prop->format == MPV_FORMAT_INT64) {
                    videoHeight_ = static_cast<int>(*static_cast<int64_t*>(prop->data));
                    emit videoSizeChanged();
                } else if (name == "core-idle" && prop->format == MPV_FORMAT_FLAG) {
                    const bool idle = *static_cast<int*>(prop->data) != 0;
                    const bool shouldLoad = idle && !paused_ && hasMedia_;
                    if (shouldLoad != loading_) {
                        loading_ = shouldLoad;
                        emit loadingChanged();
                    }
                } else if (name == "eof-reached" && prop->format == MPV_FORMAT_FLAG) {
                    if (*static_cast<int*>(prop->data)) emit endReached();
                }
                break;
            }
            default:
                break;
        }
    }
}
