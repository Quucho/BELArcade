# BELArcade - compila el frontend (en Buildroot o de forma nativa con SDL2 y SDL2_ttf)
CC ?= gcc
PKG_CONFIG ?= pkg-config
SDL_CFLAGS = $(shell $(PKG_CONFIG) --cflags sdl2 SDL2_ttf)
SDL_LIBS = $(shell $(PKG_CONFIG) --libs sdl2 SDL2_ttf) -lm

belarcade: belarcade.c
	$(CC) $(CFLAGS) -O2 -Wall $(SDL_CFLAGS) $(LDFLAGS) -o $@ $< $(SDL_LIBS)

clean:
	rm -f belarcade
