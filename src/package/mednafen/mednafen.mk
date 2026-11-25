################################################################################
#
# mednafen
#
################################################################################

MEDNAFEN_VERSION = 1.32.1
MEDNAFEN_SOURCE = mednafen-$(MEDNAFEN_VERSION).tar.xz
MEDNAFEN_SITE = https://mednafen.github.io/releases/files
MEDNAFEN_LICENSE = GPL-2.0+
MEDNAFEN_LICENSE_FILES = COPYING
MEDNAFEN_DEPENDENCIES = sdl2 zlib alsa-lib flac

MEDNAFEN_CONF_ENV = SDL2_CONFIG="$(STAGING_DIR)/usr/bin/sdl2-config"

# Solo se compilan los sistemas usados por BELArcade (menor tiempo y memoria de compilacion)
MEDNAFEN_CONF_OPTS = \
	--with-sdl2-prefix="$(STAGING_DIR)/usr" \
	--disable-apple2 --disable-cdplay --disable-lynx --disable-md \
	--disable-ngp --disable-pce --disable-pce-fast --disable-pcfx \
	--disable-psx --disable-sasplay --disable-sms --disable-ss \
	--disable-ssfplay --disable-vb --disable-wswan \
	--enable-nes --enable-snes --enable-snes-faust \
	--enable-gb --enable-gba

$(eval $(autotools-package))
