#!/usr/bin/env python3
# BELArcade - daemon USB
# Detecta memorias USB, pausa el juego en curso, copia las ROMs sin duplicados
# (comparando su contenido) y deja anotadas las nuevas para que el frontend
# las marque como NUEVO.
import hashlib
import os
import shutil
import subprocess
import time

ARCH_CONF = "/etc/belarcade/belarcade.conf"


def leer_conf(ruta):
    """Lee el archivo de configuracion (formato CLAVE=valor)"""
    conf = {}
    try:
        with open(ruta, "r") as f:
            for linea in f:
                linea = linea.strip()
                if not linea or linea.startswith("#") or "=" not in linea:
                    continue
                clave, valor = linea.split("=", 1)
                conf[clave.strip()] = valor.strip()
    except FileNotFoundError:
        pass
    return conf


CONF = leer_conf(ARCH_CONF)
DIR_ROMS = CONF.get("ROMS_DIR", "/home/pi/roms")
DIR_USB = CONF.get("USB_MOUNT", "/mnt/usb")
IMAGEN_COPIA = CONF.get("SPLASH_COPIANDO", "/etc/belarcade/assets/splash_copiando.png")
VISOR = "/usr/bin/fbv"
BANDERA_PAUSA = "/tmp/belarcade_pausa.flag"
BANDERA_REFRESCO = "/tmp/belarcade_refresh.flag"
ARCH_NUEVOS = "/home/pi/.belarcade_nuevos"

EXTENSIONES = {".nes", ".unf", ".smc", ".sfc", ".gba"}   # extensiones que muestra el frontend
ESPERA = 2.0                                              # segundos entre revisiones de USB


def log(msg):
    print("[usb] %s" % msg, flush=True)


def pausar_juego():
    """Cierra Mednafen de forma normal (con autosave guarda la partida)
    y despues el frontend, para liberar la pantalla"""
    subprocess.call(["killall", "-TERM", "mednafen"], stderr=subprocess.DEVNULL)
    time.sleep(1.5)                                       # tiempo para guardar el estado
    subprocess.call(["killall", "-9", "mednafen"], stderr=subprocess.DEVNULL)
    subprocess.call(["killall", "-9", "belarcade"], stderr=subprocess.DEVNULL)
    time.sleep(0.5)


def mostrar_pantalla_copia():
    """Muestra la imagen de copia en curso; devuelve el proceso del visor"""
    if not os.path.exists(VISOR) or not os.path.exists(IMAGEN_COPIA):
        log("imagen de copia no disponible")
        return None
    try:
        return subprocess.Popen([VISOR, "-fic", IMAGEN_COPIA],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except OSError as e:
        log("error al mostrar la imagen de copia: %s" % e)
        return None


def ocultar_pantalla_copia(proceso):
    if proceso is None:
        return
    try:
        proceso.terminate()
        time.sleep(0.3)
        if proceso.poll() is None:
            proceso.kill()
    except OSError:
        pass


def particiones_usb():
    """Particiones de discos USB presentes (sda1, sdb1, ...)"""
    particiones = set()
    try:
        with open("/proc/partitions", "r") as f:
            for linea in f.readlines()[2:]:
                columnas = linea.split()
                if len(columnas) < 4:
                    continue
                nombre = columnas[3]
                if nombre.startswith("sd") and nombre[-1].isdigit():
                    particiones.add(nombre)
    except OSError:
        pass
    return particiones


def montar(particion):
    """Monta la particion en modo solo lectura"""
    disp = "/dev/" + particion
    os.makedirs(DIR_USB, exist_ok=True)
    for tipo in (None, "vfat", "exfat", "ext4", "ntfs"):
        orden = ["mount", "-o", "ro", disp, DIR_USB]
        if tipo:
            orden[1:1] = ["-t", tipo]
        if subprocess.call(orden, stderr=subprocess.DEVNULL) == 0:
            log("montada %s en %s" % (disp, DIR_USB))
            return True
    log("no se pudo montar %s" % disp)
    return False


def desmontar():
    subprocess.call(["sync"])
    if subprocess.call(["umount", DIR_USB], stderr=subprocess.DEVNULL) != 0:
        subprocess.call(["umount", "-l", DIR_USB])
    log("desmontada")


def buscar_roms_usb():
    """Busca ROMs en toda la memoria USB, incluyendo subcarpetas"""
    encontradas = []
    for raiz, _dirs, archivos in os.walk(DIR_USB):
        for nombre in archivos:
            if nombre.startswith("."):                    # archivos ocultos de macOS y otros
                continue
            if os.path.splitext(nombre)[1].lower() in EXTENSIONES:
                encontradas.append(os.path.join(raiz, nombre))
    return encontradas


def huella(ruta):
    """Hash del contenido: dos ROMs iguales tienen la misma huella aunque se llamen distinto"""
    h = hashlib.md5()
    with open(ruta, "rb") as f:
        for bloque in iter(lambda: f.read(1 << 16), b""):
            h.update(bloque)
    return h.hexdigest()


def copiar_roms():
    """Copia las ROMs de la USB sin duplicados y anota las nuevas; devuelve cuantas copio"""
    os.makedirs(DIR_ROMS, exist_ok=True)
    origen = buscar_roms_usb()
    log("ROMs en la USB: %d" % len(origen))

    existentes = {}
    for nombre in os.listdir(DIR_ROMS):
        ruta = os.path.join(DIR_ROMS, nombre)
        if os.path.isfile(ruta):
            try:
                existentes[huella(ruta)] = nombre
            except OSError:
                pass

    nuevas, omitidas = [], 0
    for ruta in origen:
        nombre = os.path.basename(ruta)
        try:
            h = huella(ruta)
            if h in existentes:
                omitidas += 1
                log("omitida (ya existe como %s): %s" % (existentes[h], nombre))
                continue
            destino = os.path.join(DIR_ROMS, nombre)
            base, ext = os.path.splitext(nombre)
            n = 2
            while os.path.exists(destino):                # mismo nombre, otro juego: no se sobrescribe
                destino = os.path.join(DIR_ROMS, "%s (%d)%s" % (base, n, ext))
                n += 1
            shutil.copy2(ruta, destino)
            existentes[h] = os.path.basename(destino)
            nuevas.append(os.path.basename(destino))
            log("copiada: %s" % os.path.basename(destino))
        except OSError as e:
            log("error al copiar %s: %s" % (nombre, e))

    if nuevas:
        with open(ARCH_NUEVOS, "a") as f:                 # el frontend las marca como NUEVO
            for nombre in nuevas:
                f.write(nombre + "\n")
    log("copia terminada: %d copiadas, %d omitidas por duplicadas" % (len(nuevas), omitidas))
    return len(nuevas)


def crear_bandera(ruta):
    try:
        with open(ruta, "w") as f:
            f.write(str(time.time()))
    except OSError:
        pass


def borrar_bandera(ruta):
    try:
        os.remove(ruta)
    except OSError:
        pass


def atender_usb(particion):
    """Secuencia completa al conectar una memoria USB"""
    log("USB detectada: %s" % particion)
    crear_bandera(BANDERA_PAUSA)                          # el frontend espera mientras exista
    pausar_juego()
    visor = mostrar_pantalla_copia()
    time.sleep(0.5)

    copiadas = 0
    try:
        if montar(particion):
            try:
                copiadas = copiar_roms()
            finally:
                desmontar()
    finally:
        time.sleep(1)                                     # la pantalla de copia queda visible un momento
        ocultar_pantalla_copia(visor)
        borrar_bandera(BANDERA_PAUSA)
        crear_bandera(BANDERA_REFRESCO)
        log("listo: %d ROMs nuevas; el frontend se vuelve a abrir" % copiadas)


def main():
    log("daemon USB iniciado")
    borrar_bandera(BANDERA_PAUSA)
    conocidas = particiones_usb()
    log("particiones al iniciar: %s" % (sorted(conocidas) or "ninguna"))
    while True:
        try:
            actuales = particiones_usb()
            for particion in sorted(actuales - conocidas):
                for _ in range(5):                        # espera a que aparezca el dispositivo
                    if os.path.exists("/dev/" + particion):
                        break
                    time.sleep(0.4)
                atender_usb(particion)
            for particion in sorted(conocidas - actuales):
                log("USB retirada: %s" % particion)
            conocidas = actuales
        except Exception as e:                            # el daemon no debe detenerse
            log("error: %s" % e)
        time.sleep(ESPERA)


if __name__ == "__main__":
    main()
