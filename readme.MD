# BELArcade

Consola de videojuegos retro para Raspberry Pi 4, basada en Buildroot 2025.02, Mednafen y una interfaz propia en C/SDL2.

## Autores
- García Vázquez Jesús Alejandro.
- Elena.
- Brandon.

Asignatura: Fundamentos de Sistemas Embebidos, grupo 1.
Fecha: 24 de noviembre de 2026.
Desarrollo: del 17 al 23 de noviembre de 2026.

## Contenido
- `doc/BELArcade_Reporte_Integral.pdf`: reporte técnico con construcción, configuración, integración, manual de operación, diagnóstico, resultados, cronograma, costos, conclusiones, cuestionario y referencias.
- `src/`: árbol externo de Buildroot con código fuente, configuración, servicios, scripts y recursos del proyecto.
- `vid/enlace_video.txt`: enlace al video de funcionamiento.
- `LICENSE`: información de licencias y avisos de terceros.

## Video
https://youtu.be/d4zTpowfDdM

## Construcción
Consultar el procedimiento detallado en el reporte. Colocar `src` junto a `buildroot-2025.02` y ejecutar desde la carpeta de Buildroot:

```sh
make BR2_EXTERNAL=../src belarcade_defconfig
make
```

El resultado se genera en `output/images/sdcard.img`. Grabar esa imagen en la microSD y conectar el monitor, el DualSense y la salida de audio antes de alimentar la Raspberry Pi.

Las ROMs se incorporan a `src/board/belarcade/rootfs-overlay/home/pi/roms/` antes de compilar. El paquete no contiene ROMs, el árbol completo de Buildroot ni la imagen compilada.

## Operación
La consola arranca directamente en el catálogo NES, SNES y GBA. Usar el gamepad para seleccionar y abrir juegos. El reporte incluye las instrucciones para importar títulos desde USB y apagar el sistema.
