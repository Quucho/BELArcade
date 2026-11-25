#!/bin/sh
# BELArcade - restaura la biblioteca de juegos a los incluidos en la imagen,
# eliminando los que se agregaron desde USB

LISTA=/etc/belarcade/roms_originales.txt
ROMS=/home/pi/roms

[ -r "$LISTA" ] || { echo "No existe $LISTA"; exit 1; }
for f in "$ROMS"/*; do
    [ -f "$f" ] || continue
    nombre=$(basename "$f")
    grep -qxF "$nombre" "$LISTA" || rm -v "$f"     # elimina lo que no estaba en la imagen
done
: > /home/pi/.belarcade_nuevos                      # vacia la lista de juegos NUEVO
sync
echo "Listo: quedan $(ls "$ROMS" | wc -l) juegos."
