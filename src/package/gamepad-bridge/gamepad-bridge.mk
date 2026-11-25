################################################################################
#
# gamepad-bridge
#
################################################################################

GAMEPAD_BRIDGE_VERSION = 1.0
GAMEPAD_BRIDGE_SITE = $(BR2_EXTERNAL_BELARCADE_PATH)/package/gamepad-bridge/src
GAMEPAD_BRIDGE_SITE_METHOD = local

define GAMEPAD_BRIDGE_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -Wall -O2 \
		-o $(@D)/gamepad-bridge $(@D)/gamepad-bridge.c
endef

define GAMEPAD_BRIDGE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/gamepad-bridge $(TARGET_DIR)/usr/bin/gamepad-bridge
endef

$(eval $(generic-package))
