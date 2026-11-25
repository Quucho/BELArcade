#!/bin/sh
# BELArcade - ajustes al sistema de archivos al final de la compilacion, antes de crear la imagen
set -e
TARGET_DIR="$1"
BOARD_DIR="$(dirname "$0")"

# inittab: bridge del control en tty4 y frontend en tty1, ambos se relanzan solos
cat > "${TARGET_DIR}/etc/inittab" << 'INITEOF'
::sysinit:/bin/mount -t proc proc /proc
::sysinit:/bin/mount -o remount,rw /
::sysinit:/bin/mkdir -p /dev/pts /dev/shm
::sysinit:/bin/mount -a
::sysinit:/bin/hostname -F /etc/hostname
::sysinit:/etc/init.d/rcS

tty4::respawn:/usr/local/bin/bridge-launcher.sh
tty1::respawn:/usr/local/bin/frontend-launcher.sh

# Consola serial de rescate
ttyAMA0::respawn:/sbin/getty -L ttyAMA0 115200 vt100

::shutdown:/etc/init.d/rcK
::shutdown:/bin/umount -a -r
INITEOF

# Grupo input
grep -q "^input:" "${TARGET_DIR}/etc/group" || echo "input:x:1001:" >> "${TARGET_DIR}/etc/group"

# Configuracion de Mednafen tambien para root; autosave guarda la partida al salir y la retoma al volver
sed -i 's/^autosave 0/autosave 1/' "${TARGET_DIR}/home/pi/.mednafen/mednafen.cfg"
mkdir -p "${TARGET_DIR}/root/.mednafen"
cp "${TARGET_DIR}/home/pi/.mednafen/mednafen.cfg" "${TARGET_DIR}/root/.mednafen/mednafen.cfg"

# Permisos de ejecucion
chmod +x "${TARGET_DIR}"/usr/local/bin/*.sh "${TARGET_DIR}"/etc/init.d/S* 2>/dev/null || true

mkdir -p "${TARGET_DIR}/home/pi/roms" "${TARGET_DIR}/var/log" "${TARGET_DIR}/mnt/usb"

# Lista de juegos incluidos en la imagen (la usa belarcade-restablecer.sh)
ls "${TARGET_DIR}/home/pi/roms" > "${TARGET_DIR}/etc/belarcade/roms_originales.txt"
exit 0
