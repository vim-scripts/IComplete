#
# Copyright 2005 Timo Hirvonen <tihirvon@ee.oulu.fi>
#
# This file is licensed under the GPLv2.

ifndef top_srcdir
$(error top_srcdir not defined)
endif

ifndef top_builddir
$(error top_builddir not defined)
endif

ifndef scriptdir
$(error scriptdir not defined)
endif

BUILD_VERBOSITY := 1
ifeq ($(origin V),command line)
  BUILD_VERBOSITY := $(V)
endif
ifneq ($(findstring s,$(MAKEFLAGS)),)
  BUILD_VERBOSITY := 0
endif

export pkglibdir	:= $(libdir)/$(PACKAGE)
export pkgincludedir	:= $(includedir)/$(PACKAGE)
export pkgdatadir	:= $(datadir)/$(PACKAGE)
export install_log	:= $(top_builddir)/install.log
export BUILD_VERBOSITY

MAKEFLAGS	+= --no-print-directory

include $(scriptdir)/sub.mk

# NOTE: $restricted is defined in sub.mk

ifeq ($(restricted),0)
# topdir make

uninstall:
	@if [[ -f $(install_log) ]]; \
	then \
		sort $(install_log) | uniq > $(install_log).tmp || exit 1; \
		mv $(install_log).tmp $(install_log) || exit 1; \
		exec < $(install_log) || exit 1; \
		while read line; \
		do \
			echo "   RM     $$line"; \
			rm $$line || exit 1; \
		done; \
		rm $(install_log); \
	else \
		echo "Nothing to uninstall"; \
	fi

dist:
	@$(scriptdir)/dist $(top_srcdir) $(PACKAGE) $(VERSION)

ifeq ($(top_srcdir),$(top_builddir))
dist: clobber
endif

else
# subdir make

uninstall dist:
	@echo "can't make $@ in a subdir" >&2
	@/bin/false

endif

help:
	@echo "Main targets:"
	@echo "  all                 same as build (this is the default target)"
	@echo "  build               build main targets"
	@echo "  install             install main targets"
	@echo "  install-exec        install main targets (only executables)"
	@echo "  install-data        install main targets (only data)"
	@echo
	@echo "Extra targets"
	@echo "  extra               same as extra-build"
	@echo "  extra-build         build extra targets"
	@echo "  extra-install       install extra targets"
	@echo "  extra-install-exec  install extra targets (only executables)"
	@echo "  extra-install-data  install extra targets (only data)"
	@echo
	@echo "Cleaning targets:"
	@echo "  clean               remove temporary files"
	@echo "  clobber             remove temporary files and targets"
	@echo "  distclean           prepare for 'make dist'"
	@echo
	@echo "Other targets:"
	@echo "  uninstall           remove all files listed in 'install.log'"
	@echo "  dist                build \$$DISTDIR/$(PACKAGE)-$(VERSION).tar.bz2 file"
	@echo "  tags                generate tags (requires ctags)"
	@echo "  help                show this help"
	@echo
	@echo "Build verbosity:"
	@echo "  make V=0            quiet build"
	@echo "  make V=1            beautified build (default)"
	@echo "  make V=2            verbose build"
	@echo "  make -s             same as 'make V=0'"
	@echo
	@echo "Installed files are logged to $(install_log)."

tags:
	@$(scriptdir)/generate-tags

.PHONY: help uninstall tags dist
