#!/bin/sh
# BELArcade - lanza Mednafen con el entorno de video y entrada de la consola

for i in 1 2 3 4 5; do                             # espera a que el bridge del control este listo
    [ -f /tmp/gamepad-bridge-ready ] && break
    sleep 1
done

export SDL_INPUT_LINUXEV=1
export SDL_JOYSTICK_DISABLE_LINUX_HOTPLUG=0
export SDL_VIDEODRIVER=kmsdrm
export SDL_KMSDRM_REQUIRE_DRM_MASTER=0
export HOME=/home/pi
export MEDNAFEN_ALLOWMULTI=1

cat /dev/zero > /dev/fb0 2>/dev/null               # pantalla en negro mientras abre el juego
exec /usr/bin/mednafen "$@" > /dev/null 2>&1       # sin texto en pantalla
