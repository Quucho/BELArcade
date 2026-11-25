#!/bin/sh
# BELArcade - lanza el frontend directamente sobre la pantalla (KMS/DRM, sin X11)

export SDL_VIDEODRIVER=kmsdrm
export SDL_RENDER_DRIVER=opengles2
export SDL_KMSDRM_REQUIRE_DRM_MASTER=0
export EGL_PLATFORM=drm
export HOME=/home/pi
exec /usr/bin/belarcade >> /var/log/belarcade.log 2>&1
