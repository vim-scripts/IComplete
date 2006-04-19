CFLAGS		+= -I$(top_builddir)

ifeq ($(PKG_CONFIG_PATH),)
  PKG_CONFIG_PATH := $(top_builddir)
else
  PKG_CONFIG_PATH := $(PKG_CONFIG_PATH):$(top_builddir)
endif
