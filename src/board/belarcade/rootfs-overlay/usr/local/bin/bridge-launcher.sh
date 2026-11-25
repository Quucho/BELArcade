#!/bin/sh
# BELArcade - lanza el bridge que convierte el control en teclado virtual

exec /usr/bin/gamepad-bridge >> /var/log/gamepad-bridge.log 2>&1
