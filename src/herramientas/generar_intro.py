#!/usr/bin/env python3
# BELArcade - genera splash.png (intro) y splash_copiando.png (copia desde USB), 720x480
import math
from PIL import Image, ImageDraw, ImageFont, ImageFilter

W, H = 720, 480                      # tamano de las imagenes
BG     = (18, 8, 31)                 # fondo morado casi negro
ROSA   = (255, 63, 164)              # rosa neon (logo, linea del horizonte)
ROSA_C = (255, 209, 234)             # rosa muy claro (centro de las letras)
CIAN   = (41, 227, 255)              # cian neon (bordes, cuadricula)
AMAR   = (255, 225, 77)              # amarillo (detalles)

FD = "../board/belarcade/rootfs-overlay/usr/share/belarcade/fonts/"
PS2P  = FD + "PressStart2P.ttf"
VT323 = FD + "VT323.ttf"


def capa():
    return Image.new("RGBA", (W, H), (0, 0, 0, 0))


def con_brillo(base, dibujar, radios=(10, 4)):
    """Dibuja algo con efecto neon: copias borrosas detras y la version nitida encima."""
    nitida = capa()
    dibujar(ImageDraw.Draw(nitida))
    for r in radios:
        halo = nitida.filter(ImageFilter.GaussianBlur(r))
        base.alpha_composite(halo)
        base.alpha_composite(halo)                   # dos veces para que el neon se note
    base.alpha_composite(nitida)


def texto_centrado(d, y, txt, font, color, x=None):
    w = d.textlength(txt, font=font)
    d.text(((W - w) / 2 if x is None else x, y), txt, font=font, fill=color)


def estrella(d, cx, cy, r, color):
    pts = []
    for i in range(10):
        ang = -math.pi / 2 + i * math.pi / 5
        rr = r if i % 2 == 0 else r * 0.45
        pts.append((cx + rr * math.cos(ang), cy + rr * math.sin(ang)))
    d.polygon(pts, fill=color)


def logo(d, y, size):
    """BEL en rosa neon + Arcade en rosa claro, centrado."""
    f = ImageFont.truetype(PS2P, size)
    w1, w2 = d.textlength("BELA", font=f), d.textlength("rcade", font=f)
    x = (W - w1 - w2) / 2
    d.text((x, y), "BELA", font=f, fill=ROSA)
    d.text((x + w1, y), "rcade", font=f, fill=ROSA_C)


def horizonte():
    img = Image.new("RGBA", (W, H), BG + (255,))
    hy = 300                                            # altura del horizonte
    # sol rosa con franjas
    con_brillo(img, lambda d: d.ellipse((260, hy - 100, 460, hy + 100), fill=ROSA), (14,))
    d0 = ImageDraw.Draw(img)
    for y0, alto in ((240, 5), (256, 7), (273, 9), (290, 11)):
        d0.rectangle((255, y0, 465, y0 + alto), fill=BG)    # franjas del sol
    d0.rectangle((0, hy, W, H), fill=BG)                    # tapa la mitad de abajo del sol
    # cuadricula cian en perspectiva
    def grid(d):
        for y in (318, 345, 385, 445):
            d.line((0, y, W, y), fill=CIAN, width=2)
        for dx in (0, 40, 80, 120, 160):
            for s in (-1, 1):
                x0 = 360 + s * dx
                x1 = 360 + s * dx * 4
                d.line((x0, hy, x1, H), fill=CIAN, width=2)
    con_brillo(img, grid, (4,))
    con_brillo(img, lambda d: d.line((0, hy, W, hy), fill=ROSA, width=3), (6,))
    # titulo
    con_brillo(img, lambda d: logo(d, 85, 38), (12, 5))
    f = ImageFont.truetype(VT323, 28)
    con_brillo(img, lambda d: texto_centrado(d, 140, "NES  -  SNES  -  GBA", f, CIAN), (5,))
    return img.convert("RGB")


def copiando():
    img = Image.new("RGBA", (W, H), BG + (255,))
    con_brillo(img, lambda d: logo(d, 60, 22), (8, 3))
    f = ImageFont.truetype(PS2P, 22)
    con_brillo(img, lambda d: texto_centrado(d, 190, "USB DETECTADA", f, CIAN), (8, 3))
    f2 = ImageFont.truetype(VT323, 40)
    con_brillo(img, lambda d: texto_centrado(d, 240, "copiando juegos...", f2, ROSA_C), (6,))

    def barra(d):
        d.rectangle((180, 305, 540, 335), outline=ROSA, width=3)
        for i in range(7):
            d.rectangle((188 + i * 49, 312, 188 + i * 49 + 42, 328), fill=ROSA)
    con_brillo(img, barra, (6,))
    f3 = ImageFont.truetype(VT323, 30)
    con_brillo(img, lambda d: texto_centrado(d, 400, "no retires la memoria", f3, AMAR), (5,))
    return img.convert("RGB")


if __name__ == "__main__":
    import sys
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    horizonte().save(f"{out}/splash.png")
    copiando().save(f"{out}/splash_copiando.png")
    print("listo")
