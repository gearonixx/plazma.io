# Plazma

Desktop video client that signs you in through your Telegram account and plays content from a self-hosted media backend.

**Status:** early development (`v0.0.1`). APIs, schemas and on-disk layout change without notice.

## What it is

Plazma is a native Qt Quick desktop application written in C++20. It pairs:

- **Telegram-account authentication** via TDLib — the same login flow you know from Telegram Desktop, no separate password for Plazma;
- **Hardware-accelerated playback** via libmpv (VA-API, NVDEC, software fallback), with an honest in-UI report of the decoder actually in use;
- **A self-hosted backend** (see [PlazmaServer](../PlazmaServer)) for the video feed, uploads and object storage (MinIO).

The UI lives in QML, the engine in C++. The player surface (`MpvObject`) wraps libmpv as a `QQuickFramebufferObject`, so videos render straight into the Qt scene graph.

## Supported systems

Linux is the primary target. The app is known to build and run on:

- Arch Linux (x86_64)
- Any glibc-based distro with Qt 6.5+, libmpv 0.36+ and TDLib 1.8+

Windows and macOS ports are planned but not validated.

## Third-party

- [Qt 6](https://www.qt.io/) — LGPLv3 (Core, Quick, Qml, Widgets, OpenGL, LinguistTools)
- [TDLib](https://github.com/tdlib/td) — Boost Software License (Telegram authentication and client)
- [libmpv](https://mpv.io/) — LGPLv2.1+ (video decoding, rendering, hwdec)
- [FFmpeg](https://ffmpeg.org/) — LGPLv2.1+ (pulled in transitively by libmpv)
- [MinIO client](https://min.io/) — AGPLv3 (server-side object storage, via PlazmaServer)

## Build

### Dependencies (Arch)

```
sudo pacman -S --needed cmake ninja pkgconf \
    qt6-base qt6-declarative qt6-tools \
    mpv tdlib
```

For hardware-accelerated decoding, install the driver matching your GPU

- Intel iGPU: `intel-media-driver`
- AMD: `libva-mesa-driver`
- NVIDIA (proprietary): `libva-nvidia-driver`

