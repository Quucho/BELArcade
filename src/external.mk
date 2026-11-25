# BELArcade - incluye los paquetes propios del arbol externo
include $(sort $(wildcard $(BR2_EXTERNAL_BELARCADE_PATH)/package/*/*.mk))

# Compatibilidad de host-m4 con compiladores de host recientes
HOST_M4_CONF_ENV += CFLAGS="-O2 -std=gnu17"
