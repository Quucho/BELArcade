/*
 * BELArcade - frontend grafico (SDL2 + SDL2_ttf)
 * Galeria de juegos por consola con estilo neon 90s, menu de opciones,
 * etiqueta NUEVO y lanzamiento de Mednafen.
 *
 * Controles (el gamepad-bridge convierte el control en estas teclas):
 *   Flechas arriba/abajo : elegir juego          Flechas izq/der : cambiar consola
 *   A (Cruz)             : jugar / aceptar       D (Circulo)     : regresar
 *   Enter (Options)      : menu de opciones      Esc             : salir (modo ventana)
 *
 * Variables de entorno opcionales:
 *   BELARCADE_ROMS, BELARCADE_FONTS, BELARCADE_EMU, BELARCADE_NUEVOS
 *   BELARCADE_WINDOWED=1 (ejecucion en ventana, fuera de la consola)
 */
#include <SDL.h>
#include <SDL_ttf.h>
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define ANCHO 720                             /* resolucion logica de la interfaz */
#define ALTO  480
#define MAX_ROMS 512
#define FILAS_VISIBLES 8                      /* juegos que caben en la lista */

/* ---------- Paleta neon ---------- */
static const SDL_Color C_FONDO  = { 18,   8,  31, 255 };   /* morado casi negro */
static const SDL_Color C_ROSA   = { 255,  63, 164, 255 };  /* rosa neon: logo y seleccion */
static const SDL_Color C_ROSA_C = { 255, 209, 234, 255 };  /* rosa claro: "rcade" del logo */
static const SDL_Color C_CIAN   = { 41,  227, 255, 255 };  /* cian: pestanas y botones */
static const SDL_Color C_AMAR   = { 255, 225,  77, 255 };  /* amarillo: etiqueta NUEVO */
static const SDL_Color C_TEXTO  = { 217, 200, 240, 255 };  /* lila claro: texto normal */
static const SDL_Color C_TENUE  = { 138, 111, 184, 255 };  /* morado medio: pistas */
static const SDL_Color C_AMAR_O = { 61,   46,   0, 255 };  /* texto oscuro sobre amarillo */

/* ---------- Consolas ---------- */
typedef struct {
    const char *nombre;        /* como se muestra */
    const char *bits;          /* "8 bits", etc. */
    const char *modulo;        /* -force_module de Mednafen */
    const char *exts[4];       /* extensiones validas */
} Consola;

static const Consola CONSOLAS[] = {
    { "NES",  "8 bits",  "nes",        { ".nes", ".unf", NULL } },
    { "SNES", "16 bits", "snes_faust", { ".sfc", ".smc", NULL } },
    { "GBA",  "32 bits", "gba",        { ".gba", NULL } },
};
#define N_CONSOLAS 3

typedef struct {
    char archivo[256];         /* nombre de archivo real */
    char titulo[128];          /* nombre limpio para mostrar */
    int nuevo;                 /* llego por USB y no se ha jugado */
} Rom;

static Rom roms[N_CONSOLAS][MAX_ROMS];
static int n_roms[N_CONSOLAS];

/* ---------- Estado ---------- */
enum { PANTALLA_GALERIA, PANTALLA_SISTEMA };
static int pantalla = PANTALLA_GALERIA;
static int pestana = 0, sel[N_CONSOLAS], scroll[N_CONSOLAS];
static int sel_sistema = 0;
static char aviso[160] = "";              /* mensaje temporal abajo de la pantalla */
static Uint32 aviso_hasta = 0;

static const char *dir_roms, *dir_fuentes, *emulador;
static const char *ARCH_ESTADO = "/tmp/belarcade.estado";
static const char *ARCH_NUEVOS = "/home/pi/.belarcade_nuevos";

static SDL_Renderer *ren;
static TTF_Font *f_logo, *f_logo_brillo, *f_lista, *f_lista_brillo, *f_tab, *f_pie, *f_titulo, *f_titulo_brillo, *f_chica;

/* ================================================================
 *  Utilidades de texto y dibujo
 * ================================================================ */
static const char *env_o(const char *k, const char *def)
{
    const char *v = getenv(k);
    return (v && *v) ? v : def;
}

static int texto(TTF_Font *f, const char *s, int x, int y, SDL_Color c, Uint8 alfa)
{
    if (!s || !*s) return 0;
    SDL_Surface *sf = TTF_RenderUTF8_Blended(f, s, c);
    if (!sf) return 0;
    SDL_Texture *tx = SDL_CreateTextureFromSurface(ren, sf);
    SDL_SetTextureAlphaMod(tx, alfa);
    SDL_Rect r = { x, y, sf->w, sf->h };
    SDL_RenderCopy(ren, tx, NULL, &r);
    int w = sf->w;
    SDL_DestroyTexture(tx);
    SDL_FreeSurface(sf);
    return w;
}

/* Texto con brillo neon: primero el contorno grueso transparente, luego el texto */
static int texto_neon(TTF_Font *f, TTF_Font *f_brillo, const char *s, int x, int y, SDL_Color c, SDL_Color brillo)
{
    int out = TTF_GetFontOutline(f_brillo);
    texto(f_brillo, s, x - out, y - out, brillo, 70);
    return texto(f, s, x, y, c, 255);
}

static int ancho_texto(TTF_Font *f, const char *s)
{
    int w = 0, h = 0;
    TTF_SizeUTF8(f, s, &w, &h);
    return w;
}

static void color(SDL_Color c, Uint8 a) { SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, a); }

static void rect_lleno(int x, int y, int w, int h, SDL_Color c, Uint8 a)
{
    SDL_Rect r = { x, y, w, h };
    color(c, a);
    SDL_RenderFillRect(ren, &r);
}

static void rect_borde(int x, int y, int w, int h, int grosor, SDL_Color c, Uint8 a)
{
    for (int i = 0; i < grosor; i++) {
        SDL_Rect r = { x + i, y + i, w - 2 * i, h - 2 * i };
        color(c, a);
        SDL_RenderDrawRect(ren, &r);
    }
}

/* Triangulo relleno; dir: 0 arriba, 1 abajo, 2 izquierda, 3 derecha */
static void triangulo(int cx, int cy, int t, int dir, SDL_Color c)
{
    SDL_Vertex v[3];
    float x = cx, y = cy, s = t;
    SDL_FPoint p[3];
    if (dir == 0)      { p[0] = (SDL_FPoint){x, y - s}; p[1] = (SDL_FPoint){x - s, y + s}; p[2] = (SDL_FPoint){x + s, y + s}; }
    else if (dir == 1) { p[0] = (SDL_FPoint){x, y + s}; p[1] = (SDL_FPoint){x - s, y - s}; p[2] = (SDL_FPoint){x + s, y - s}; }
    else if (dir == 2) { p[0] = (SDL_FPoint){x - s, y}; p[1] = (SDL_FPoint){x + s, y - s}; p[2] = (SDL_FPoint){x + s, y + s}; }
    else               { p[0] = (SDL_FPoint){x + s, y}; p[1] = (SDL_FPoint){x - s, y - s}; p[2] = (SDL_FPoint){x - s, y + s}; }
    for (int i = 0; i < 3; i++) {
        v[i].position = p[i];
        v[i].color = c;
        v[i].tex_coord = (SDL_FPoint){0, 0};
    }
    SDL_RenderGeometry(ren, NULL, v, 3, NULL, 0);
}

/* Icono de la cruz del DualSense */
static void icono_cruz(int cx, int cy, int t, SDL_Color c)
{
    color(c, 255);
    for (int d = -1; d <= 1; d++) {
        SDL_RenderDrawLine(ren, cx - t + d, cy - t, cx + t + d, cy + t);
        SDL_RenderDrawLine(ren, cx - t + d, cy + t, cx + t + d, cy - t);
    }
}

/* Icono del circulo del DualSense */
static void icono_circulo(int cx, int cy, int r, SDL_Color c)
{
    color(c, 255);
    for (int g = 0; g < 2; g++)
        for (int a = 0; a < 360; a += 4) {
            float rad = a * 3.14159f / 180.0f;
            SDL_RenderDrawPointF(ren, cx + (r - g) * SDL_cosf(rad), cy + (r - g) * SDL_sinf(rad));
        }
}

/* ================================================================
 *  ROMs
 * ================================================================ */
static void limpiar_titulo(const char *archivo, char *out, size_t n)
{
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s", archivo);
    char *punto = strrchr(tmp, '.');
    if (punto) *punto = 0;                         /* quita la extension */
    size_t j = 0;
    int dentro = 0;
    for (size_t i = 0; tmp[i] && j + 1 < n; i++) {
        char ch = tmp[i];
        if (ch == '(' || ch == '[') { dentro++; continue; }   /* quita (USA), [!], etc. */
        if (ch == ')' || ch == ']') { if (dentro) dentro--; continue; }
        if (dentro) continue;
        if (ch == '_') ch = ' ';
        if (ch == ' ' && (j == 0 || out[j - 1] == ' ')) continue;  /* sin espacios dobles */
        out[j++] = ch;
    }
    while (j > 0 && out[j - 1] == ' ') j--;
    out[j] = 0;
    if (!out[0]) snprintf(out, n, "%s", archivo);
}

static int es_nuevo(const char *archivo)
{
    FILE *f = fopen(ARCH_NUEVOS, "r");
    if (!f) return 0;
    char linea[300];
    int si = 0;
    while (fgets(linea, sizeof(linea), f)) {
        linea[strcspn(linea, "\r\n")] = 0;
        if (strcmp(linea, archivo) == 0) { si = 1; break; }
    }
    fclose(f);
    return si;
}

static void quitar_nuevo(const char *archivo)
{
    FILE *f = fopen(ARCH_NUEVOS, "r");
    if (!f) return;
    char lineas[MAX_ROMS][300];
    int n = 0;
    while (n < MAX_ROMS && fgets(lineas[n], sizeof(lineas[n]), f)) {
        lineas[n][strcspn(lineas[n], "\r\n")] = 0;
        if (strcmp(lineas[n], archivo) != 0 && lineas[n][0]) n++;
    }
    fclose(f);
    f = fopen(ARCH_NUEVOS, "w");
    if (!f) return;
    for (int i = 0; i < n; i++) fprintf(f, "%s\n", lineas[i]);
    fclose(f);
}

static int cmp_rom(const void *a, const void *b)
{
    return strcasecmp(((const Rom *)a)->titulo, ((const Rom *)b)->titulo);
}

static void cargar_roms(void)
{
    for (int c = 0; c < N_CONSOLAS; c++) n_roms[c] = 0;
    DIR *d = opendir(dir_roms);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        const char *ext = strrchr(e->d_name, '.');
        if (!ext) continue;
        for (int c = 0; c < N_CONSOLAS; c++)
            for (int k = 0; CONSOLAS[c].exts[k]; k++)
                if (strcasecmp(ext, CONSOLAS[c].exts[k]) == 0 && n_roms[c] < MAX_ROMS) {
                    Rom *r = &roms[c][n_roms[c]++];
                    snprintf(r->archivo, sizeof(r->archivo), "%s", e->d_name);
                    limpiar_titulo(e->d_name, r->titulo, sizeof(r->titulo));
                    r->nuevo = es_nuevo(e->d_name);
                }
    }
    closedir(d);
    for (int c = 0; c < N_CONSOLAS; c++)
        qsort(roms[c], n_roms[c], sizeof(Rom), cmp_rom);
}

static int total_roms(void) { return n_roms[0] + n_roms[1] + n_roms[2]; }

/* ================================================================
 *  Estado (para volver al mismo juego despues de jugar)
 * ================================================================ */
static void guardar_estado(void)
{
    FILE *f = fopen(ARCH_ESTADO, "w");
    if (!f) return;
    fprintf(f, "%d %d %d %d\n", pestana, sel[0], sel[1], sel[2]);
    fclose(f);
}

static void cargar_estado(void)
{
    FILE *f = fopen(ARCH_ESTADO, "r");
    if (!f) return;
    if (fscanf(f, "%d %d %d %d", &pestana, &sel[0], &sel[1], &sel[2]) != 4) pestana = 0;
    fclose(f);
    if (pestana < 0 || pestana >= N_CONSOLAS) pestana = 0;
    for (int c = 0; c < N_CONSOLAS; c++) {
        if (sel[c] >= n_roms[c]) sel[c] = n_roms[c] > 0 ? n_roms[c] - 1 : 0;
        if (sel[c] < 0) sel[c] = 0;
        scroll[c] = sel[c] >= FILAS_VISIBLES ? sel[c] - FILAS_VISIBLES + 1 : 0;
    }
}

static void poner_aviso(const char *msg)
{
    snprintf(aviso, sizeof(aviso), "%s", msg);
    aviso_hasta = SDL_GetTicks() + 2500;
}

/* ================================================================
 *  Dibujo de pantallas
 * ================================================================ */
static void dibujar_logo(int x, int y)
{
    int w = texto_neon(f_logo, f_logo_brillo, "BELA", x, y, C_ROSA, C_ROSA);     /* BEL(A): Brandon, Elena y Alejandro */
    texto_neon(f_logo, f_logo_brillo, "rcade", x + w, y, C_ROSA_C, C_ROSA);
}

static void dibujar_pie(int sistema)
{
    rect_lleno(24, ALTO - 44, ANCHO - 48, 2, C_ROSA, 255);
    int y = ALTO - 34, x = 40;
    if (!sistema) {
        triangulo(x, y + 6, 6, 0, C_CIAN); triangulo(x + 16, y + 10, 6, 1, C_CIAN);
        x += 30; x += texto(f_pie, "elegir", x, y - 4, C_TEXTO, 255) + 28;
        triangulo(x, y + 8, 6, 2, C_CIAN); triangulo(x + 16, y + 8, 6, 3, C_CIAN);
        x += 30; x += texto(f_pie, "consola", x, y - 4, C_TEXTO, 255) + 28;
        icono_cruz(x + 4, y + 8, 6, C_CIAN);
        x += 18; x += texto(f_pie, "jugar", x, y - 4, C_TEXTO, 255) + 28;
        x += texto(f_pie, "OPTIONS", x, y - 4, C_CIAN, 255) + 8;
        texto(f_pie, "men\xC3\xBA", x, y - 4, C_TEXTO, 255);
    } else {
        triangulo(x, y + 6, 6, 0, C_CIAN); triangulo(x + 16, y + 10, 6, 1, C_CIAN);
        x += 30; x += texto(f_pie, "elegir", x, y - 4, C_TEXTO, 255) + 32;
        icono_cruz(x + 4, y + 8, 6, C_CIAN);
        x += 18; x += texto(f_pie, "aceptar", x, y - 4, C_TEXTO, 255) + 32;
        icono_circulo(x + 5, y + 8, 7, C_CIAN);
        x += 20; texto(f_pie, "regresar", x, y - 4, C_TEXTO, 255);
    }
}

static void dibujar_galeria(void)
{
    char buf[200];
    dibujar_logo(24, 18);

    /* Pestanas por consola */
    int x = 24, y = 56;
    for (int c = 0; c < N_CONSOLAS; c++) {
        snprintf(buf, sizeof(buf), "%s \xC2\xB7 %s (%d)", CONSOLAS[c].nombre, CONSOLAS[c].bits, n_roms[c]);   /* nombre, bits y numero de juegos */
        int w = ancho_texto(f_tab, buf);
        if (c == pestana) {
            rect_borde(x - 3, y - 3, w + 22, 36, 6, C_CIAN, 35);       /* brillo */
            rect_borde(x, y, w + 16, 30, 2, C_CIAN, 255);
            texto(f_tab, buf, x + 8, y + 1, C_CIAN, 255);
        } else {
            texto(f_tab, buf, x + 8, y + 1, C_TENUE, 255);
        }
        x += w + 30;
    }

    /* Lista de juegos */
    int n = n_roms[pestana];
    y = 104;
    if (n == 0) {
        snprintf(buf, sizeof(buf), "Sin juegos de %s.", CONSOLAS[pestana].nombre);
        texto(f_lista, buf, 48, y + 60, C_TEXTO, 255);
        texto(f_pie, "Conecta una memoria USB con juegos para agregarlos.", 48, y + 100, C_TENUE, 255);
        return;
    }
    int s = sel[pestana];
    if (s < scroll[pestana]) scroll[pestana] = s;
    if (s >= scroll[pestana] + FILAS_VISIBLES) scroll[pestana] = s - FILAS_VISIBLES + 1;
    for (int i = 0; i < FILAS_VISIBLES && scroll[pestana] + i < n; i++) {
        Rom *r = &roms[pestana][scroll[pestana] + i];
        int yy = y + i * 36;
        int w;
        if (scroll[pestana] + i == s) {
            triangulo(36, yy + 17, 8, 3, C_ROSA);
            w = texto_neon(f_lista, f_lista_brillo, r->titulo, 56, yy, C_ROSA, C_ROSA);
        } else {
            w = texto(f_lista, r->titulo, 56, yy, C_TEXTO, 255);
        }
        if (r->nuevo) {
            int bx = 56 + w + 14, bw = ancho_texto(f_chica, "NUEVO") + 12;
            rect_lleno(bx, yy + 8, bw, 22, C_AMAR, 255);
            texto(f_chica, "NUEVO", bx + 6, yy + 7, C_AMAR_O, 255);
        }
    }
    int restantes = n - (scroll[pestana] + FILAS_VISIBLES);
    if (restantes > 0) {
        triangulo(64, ALTO - 66, 6, 1, C_TENUE);
        snprintf(buf, sizeof(buf), "%d m\xC3\xA1s", restantes);
        texto(f_pie, buf, 78, ALTO - 78, C_TENUE, 255);
    }
}

static const char *OPC_SISTEMA[] = { "Regresar", "Reiniciar", "Apagar" };

/* Menu de Options: solo las tres opciones, centradas */
static void dibujar_sistema(void)
{
    dibujar_logo(24, 18);
    for (int i = 0; i < 3; i++) {
        int yy = 170 + i * 48;
        int w = ancho_texto(f_lista, OPC_SISTEMA[i]);
        int x = (ANCHO - w) / 2;
        if (i == sel_sistema) {
            triangulo(x - 22, yy + 17, 8, 3, C_ROSA);
            texto_neon(f_lista, f_lista_brillo, OPC_SISTEMA[i], x, yy, C_ROSA, C_ROSA);
        } else {
            texto(f_lista, OPC_SISTEMA[i], x, yy, C_TEXTO, 255);
        }
    }
}

static void dibujar(void)
{
    color(C_FONDO, 255);
    SDL_RenderClear(ren);
    if (pantalla == PANTALLA_GALERIA) dibujar_galeria();
    else dibujar_sistema();
    dibujar_pie(pantalla == PANTALLA_SISTEMA);
    if (aviso[0] && SDL_GetTicks() < aviso_hasta) {
        int w = ancho_texto(f_pie, aviso);
        rect_lleno((ANCHO - w) / 2 - 12, ALTO - 96, w + 24, 32, C_FONDO, 255);
        rect_borde((ANCHO - w) / 2 - 12, ALTO - 96, w + 24, 32, 2, C_AMAR, 255);
        texto(f_pie, aviso, (ANCHO - w) / 2, ALTO - 94, C_AMAR, 255);
    }
    SDL_RenderPresent(ren);
}

/* ================================================================
 *  Acciones
 * ================================================================ */
static void lanzar_juego(void)
{
    if (n_roms[pestana] == 0) return;
    Rom *r = &roms[pestana][sel[pestana]];
    char ruta[600];
    snprintf(ruta, sizeof(ruta), "%s/%s", dir_roms, r->archivo);

    if (access(emulador, X_OK) != 0) {                 /* sin emulador disponible: solo se muestra un aviso */
        char msg[160];
        snprintf(msg, sizeof(msg), "Emulador no disponible: %s (%s)", r->titulo, CONSOLAS[pestana].modulo);
        poner_aviso(msg);
        fprintf(stderr, "%s -force_module %s \"%s\"\n", emulador, CONSOLAS[pestana].modulo, ruta);
        if (r->nuevo) { quitar_nuevo(r->archivo); r->nuevo = 0; }
        return;
    }

    if (r->nuevo) quitar_nuevo(r->archivo);
    guardar_estado();
    for (int i = 0; i < 3; i++) {                       /* deja la pantalla en negro para la transicion */
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        SDL_RenderPresent(ren);
    }
    TTF_Quit();
    SDL_Quit();                                         /* libera la pantalla para Mednafen */
    execl(emulador, emulador, "-force_module", CONSOLAS[pestana].modulo, ruta, (char *)NULL);
    perror("execl");                                    /* si execl falla, inittab vuelve a lanzar el frontend */
    exit(1);
}

static void accion_sistema(int windowed)
{
    if (sel_sistema == 0) { pantalla = PANTALLA_GALERIA; return; }
    if (windowed) { poner_aviso("Opci\xC3\xB3n desactivada en modo ventana"); return; }
    color(C_FONDO, 255);
    SDL_RenderClear(ren);
    texto_neon(f_titulo, f_titulo_brillo, sel_sistema == 1 ? "Reiniciando..." : "Apagando...", 200, 220, C_ROSA, C_ROSA);
    SDL_RenderPresent(ren);
    sync();
    SDL_Delay(800);
    if (system(sel_sistema == 1 ? "reboot" : "poweroff") != 0) { /* error ignorado */ }
}

/* ================================================================
 *  Programa principal
 * ================================================================ */
static TTF_Font *fuente(const char *archivo, int tam, int contorno)
{
    char ruta[512];
    snprintf(ruta, sizeof(ruta), "%s/%s", dir_fuentes, archivo);
    TTF_Font *f = TTF_OpenFont(ruta, tam);
    if (!f) { fprintf(stderr, "No se pudo abrir la fuente %s: %s\n", ruta, TTF_GetError()); exit(1); }
    if (contorno) TTF_SetFontOutline(f, contorno);
    return f;
}

int main(void)
{
    dir_roms    = env_o("BELARCADE_ROMS", "/home/pi/roms");
    dir_fuentes = env_o("BELARCADE_FONTS", "/usr/share/belarcade/fonts");
    emulador    = env_o("BELARCADE_EMU", "/usr/local/bin/mednafen-wrapper.sh");
    ARCH_NUEVOS = env_o("BELARCADE_NUEVOS", ARCH_NUEVOS);
    int windowed = getenv("BELARCADE_WINDOWED") != NULL;

    /* Mientras el daemon USB copia juegos, el frontend espera antes de tomar la pantalla */
    while (access("/tmp/belarcade_pausa.flag", F_OK) == 0) SDL_Delay(500);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    if (TTF_Init() != 0) { fprintf(stderr, "TTF_Init: %s\n", TTF_GetError()); return 1; }

    SDL_Window *win = SDL_CreateWindow("BELArcade", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       ANCHO, ALTO, windowed ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!win) { fprintf(stderr, "Ventana: %s\n", SDL_GetError()); return 1; }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { fprintf(stderr, "Renderer: %s\n", SDL_GetError()); return 1; }
    SDL_RenderSetLogicalSize(ren, ANCHO, ALTO);          /* la interfaz se dibuja a 720x480 y se escala a la pantalla */
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_ShowCursor(SDL_DISABLE);

    f_logo          = fuente("PressStart2P.ttf", 18, 0);
    f_logo_brillo   = fuente("PressStart2P.ttf", 18, 3);
    f_titulo        = fuente("PressStart2P.ttf", 24, 0);
    f_titulo_brillo = fuente("PressStart2P.ttf", 24, 4);
    f_lista         = fuente("VT323.ttf", 32, 0);
    f_lista_brillo  = fuente("VT323.ttf", 32, 3);
    f_tab           = fuente("VT323.ttf", 28, 0);
    f_pie           = fuente("VT323.ttf", 24, 0);
    f_chica         = fuente("VT323.ttf", 20, 0);

    cargar_roms();
    cargar_estado();

    /* Modo captura: guarda la pantalla actual en un archivo BMP y termina */
    const char *captura = getenv("BELARCADE_CAPTURA");
    if (captura) {
        if (getenv("BELARCADE_PANTALLA_SISTEMA")) pantalla = PANTALLA_SISTEMA;
        dibujar();
        SDL_Surface *sf = SDL_CreateRGBSurfaceWithFormat(0, ANCHO, ALTO, 32, SDL_PIXELFORMAT_ARGB8888);
        SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, sf->pixels, sf->pitch);
        SDL_SaveBMP(sf, captura);
        SDL_FreeSurface(sf);
        SDL_Quit();
        return 0;
    }

    int corriendo = 1, sucio = 1;
    Uint32 ultima_recarga = SDL_GetTicks();
    while (corriendo) {
        SDL_Event e;
        if (SDL_WaitEventTimeout(&e, 30)) {
            do {
                if (e.type == SDL_QUIT) corriendo = 0;
                if (e.type != SDL_KEYDOWN) continue;
                SDL_Keycode k = e.key.keysym.sym;
                sucio = 1;
                if (k == SDLK_ESCAPE && windowed) { corriendo = 0; break; }
                if (pantalla == PANTALLA_GALERIA) {
                    int n = n_roms[pestana];
                    if (k == SDLK_UP && n)    sel[pestana] = (sel[pestana] + n - 1) % n;
                    if (k == SDLK_DOWN && n)  sel[pestana] = (sel[pestana] + 1) % n;
                    if (k == SDLK_LEFT)       pestana = (pestana + N_CONSOLAS - 1) % N_CONSOLAS;
                    if (k == SDLK_RIGHT)      pestana = (pestana + 1) % N_CONSOLAS;
                    if (k == SDLK_a)          lanzar_juego();
                    if (k == SDLK_RETURN)     { pantalla = PANTALLA_SISTEMA; sel_sistema = 0; }
                } else {
                    if (k == SDLK_UP)         sel_sistema = (sel_sistema + 2) % 3;
                    if (k == SDLK_DOWN)       sel_sistema = (sel_sistema + 1) % 3;
                    if (k == SDLK_a)          accion_sistema(windowed);
                    if (k == SDLK_d || k == SDLK_RETURN) pantalla = PANTALLA_GALERIA;
                }
            } while (SDL_PollEvent(&e));
        }
        /* Cada 3 s se revisa la carpeta de ROMs para detectar juegos copiados desde USB */
        if (SDL_GetTicks() - ultima_recarga > 3000) {
            int antes = total_roms();
            cargar_roms();
            if (total_roms() != antes) sucio = 1;
            ultima_recarga = SDL_GetTicks();
        }
        if (aviso[0] && SDL_GetTicks() >= aviso_hasta) { aviso[0] = 0; sucio = 1; }
        dibujar(); sucio = 0;                         /* redibuja siempre: KMS/DRM necesita cuadros continuos */
    }

    guardar_estado();
    TTF_Quit();
    SDL_Quit();
    return 0;
}
