/*
 * BELArcade - gamepad-bridge
 * Lee el control (DualSense, /dev/input/js0) y emite teclas en un teclado
 * virtual (uinput). Asi el frontend y Mednafen solo tienen que entender teclado.
 *
 * Mapeo (coincide con mednafen.cfg del proyecto):
 *   Cruz      -> KEY_A        Cuadrado -> KEY_F
 *   Triangulo -> KEY_S        Circulo  -> KEY_D
 *   L1        -> KEY_Q        R1       -> KEY_W
 *   Options   -> KEY_ENTER    Share    -> KEY_RIGHTSHIFT
 *   PS        -> KEY_F12 (salir de Mednafen)
 *   Cruceta / stick izquierdo -> flechas
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include <linux/uinput.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define JS_DEV     "/dev/input/js0"
#define READY_FLAG "/tmp/gamepad-bridge-ready"
#define UMBRAL     16000                       /* desplazamiento minimo del eje para generar una flecha */

static volatile int corriendo = 1;

/* Numeros de boton que asigna el driver hid_playstation al DualSense */
static int boton_a_tecla(int n)
{
    switch (n) {
    case 0:  return KEY_A;          /* Cruz */
    case 1:  return KEY_D;          /* Circulo */
    case 2:  return KEY_S;          /* Triangulo */
    case 3:  return KEY_F;          /* Cuadrado */
    case 4:  return KEY_Q;          /* L1 */
    case 5:  return KEY_W;          /* R1 */
    case 8:  return KEY_RIGHTSHIFT; /* Share / Create */
    case 9:  return KEY_ENTER;      /* Options */
    case 10: return KEY_F12;        /* Boton PS */
    default: return 0;
    }
}

static void emitir(int ufd, int tipo, int codigo, int valor)
{
    struct input_event ie;
    memset(&ie, 0, sizeof(ie));
    ie.type = tipo;
    ie.code = codigo;
    ie.value = valor;
    if (write(ufd, &ie, sizeof(ie)) < 0) { /* ignorar */ }
}

static void tecla(int ufd, int codigo, int presionada)
{
    emitir(ufd, EV_KEY, codigo, presionada ? 1 : 0);
    emitir(ufd, EV_SYN, SYN_REPORT, 0);
}

/* Eje -> dos teclas (negativo/positivo) con estado para no repetir eventos */
static void eje(int ufd, int *estado, int valor, int tecla_neg, int tecla_pos)
{
    int nuevo = (valor < -UMBRAL) ? -1 : (valor > UMBRAL) ? 1 : 0;
    if (nuevo == *estado) return;
    if (*estado == -1) tecla(ufd, tecla_neg, 0);
    if (*estado == 1)  tecla(ufd, tecla_pos, 0);
    if (nuevo == -1) tecla(ufd, tecla_neg, 1);
    if (nuevo == 1)  tecla(ufd, tecla_pos, 1);
    *estado = nuevo;
}

static int crear_teclado(void)
{
    int teclas[] = { KEY_A, KEY_D, KEY_S, KEY_F, KEY_Q, KEY_W, KEY_ENTER,
                     KEY_RIGHTSHIFT, KEY_F12, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT };
    struct uinput_setup us;
    int ufd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (ufd < 0) { perror("uinput"); return -1; }

    ioctl(ufd, UI_SET_EVBIT, EV_KEY);
    ioctl(ufd, UI_SET_EVBIT, EV_SYN);
    for (size_t i = 0; i < sizeof(teclas) / sizeof(teclas[0]); i++)
        ioctl(ufd, UI_SET_KEYBIT, teclas[i]);

    memset(&us, 0, sizeof(us));
    us.id.bustype = BUS_USB;
    us.id.vendor = 0x1209;                     /* id generico para proyectos propios */
    us.id.product = 0xBE1A;
    strcpy(us.name, "BELArcade Gamepad Bridge");
    ioctl(ufd, UI_DEV_SETUP, &us);
    ioctl(ufd, UI_DEV_CREATE);
    return ufd;
}

static void salir(int s) { (void)s; corriendo = 0; }

int main(void)
{
    signal(SIGTERM, salir);
    signal(SIGINT, salir);

    int ufd = crear_teclado();
    if (ufd < 0) return 1;
    sleep(1);                                  /* dar tiempo a que el sistema registre el teclado */
    FILE *f = fopen(READY_FLAG, "w");          /* indica a mednafen-wrapper.sh que el bridge esta listo */
    if (f) fclose(f);

    while (corriendo) {
        int jfd = open(JS_DEV, O_RDONLY);      /* espera a que conecten el control */
        if (jfd < 0) { sleep(1); continue; }
        fprintf(stderr, "control conectado\n");

        int hx = 0, hy = 0, sx = 0, sy = 0;    /* estado de cruceta y stick */
        struct js_event je;
        while (corriendo) {
            ssize_t n = read(jfd, &je, sizeof(je));
            if (n != sizeof(je)) break;        /* se desconecto el control */
            if (je.type & JS_EVENT_INIT) continue;   /* ignorar el estado inicial */

            if (je.type == JS_EVENT_BUTTON) {
                int k = boton_a_tecla(je.number);
                if (k) tecla(ufd, k, je.value);
            } else if (je.type == JS_EVENT_AXIS) {
                if (je.number == 6)      eje(ufd, &hx, je.value, KEY_LEFT, KEY_RIGHT);  /* cruceta horizontal */
                else if (je.number == 7) eje(ufd, &hy, je.value, KEY_UP, KEY_DOWN);     /* cruceta vertical */
                else if (je.number == 0) eje(ufd, &sx, je.value, KEY_LEFT, KEY_RIGHT);  /* stick izq. horizontal */
                else if (je.number == 1) eje(ufd, &sy, je.value, KEY_UP, KEY_DOWN);     /* stick izq. vertical */
            }
        }
        close(jfd);
        fprintf(stderr, "control desconectado, esperando...\n");
        sleep(1);
    }

    ioctl(ufd, UI_DEV_DESTROY);
    close(ufd);
    unlink(READY_FLAG);
    return 0;
}
