################################################################################
#
# belarcade-frontend
#
################################################################################

BELARCADE_FRONTEND_VERSION = 1.0
BELARCADE_FRONTEND_SITE = $(BR2_EXTERNAL_BELARCADE_PATH)/package/belarcade-frontend/src
BELARCADE_FRONTEND_SITE_METHOD = local
BELARCADE_FRONTEND_DEPENDENCIES = sdl2 sdl2_ttf host-pkgconf

define BELARCADE_FRONTEND_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D)
endef

define BELARCADE_FRONTEND_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/belarcade $(TARGET_DIR)/usr/bin/belarcade
endef

$(eval $(generic-package))
