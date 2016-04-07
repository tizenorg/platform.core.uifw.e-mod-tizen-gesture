MAINTAINERCLEANFILES = Makefile.in
MODULE = e-mod-tizen-gesture

LDFLAGS +=

pkgdir                 = $(libdir)/enlightenment/modules/$(MODULE)/$(MODULE_ARCH)
pkg_LTLIBRARIES        = module.la
module_la_SOURCES      = e_mod_main.c \
                         e_mod_main.h \
                         e_mod_gesture_events.c \
                         e_mod_gesture_device.c
module_la_CFLAGS       = @ENLIGHTENMENT_CFLAGS@ @WAYLAND_CFLAGS@ -DHAVE_WAYLAND_ONLY
module_la_LDFLAGS      = -module -avoid-version @WAYLAND_LIBS@ @ENLIGHTENMENT_LIBS@

module_la_DEPENDENCIES = $(top_builddir)/config.h
