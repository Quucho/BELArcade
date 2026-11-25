# BELArcade - árbol externo de Buildroot (BR2_EXTERNAL)

Consola retro para Raspberry Pi 4 basada en Buildroot 2025.02 y Mednafen,
con frontend propio en C/SDL2.

## Contenido
- configs/belarcade_defconfig ........ configuración completa de la imagen
- package/mednafen ................... emulador (NES, SNES, GBA)
- package/gamepad-bridge ............. control -> teclado virtual (C, uinput)
- package/belarcade-frontend ......... galería de juegos (C + SDL2)
- board/belarcade .................... config.txt, cmdline, scripts y rootfs-overlay
- herramientas ....................... generación de la imagen y el sonido de arranque

## Compilación
    cd buildroot-2025.02
    make BR2_EXTERNAL=/ruta/a/src belarcade_defconfig
    make

La imagen resultante queda en output/images/sdcard.img

## ROMs
Se copian (sin subcarpetas) en board/belarcade/rootfs-overlay/home/pi/roms/
Extensiones reconocidas: .nes / .sfc .smc / .gba

## Recompilar solo el frontend
    make belarcade-frontend-rebuild all
