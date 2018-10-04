plugin_LTLIBRARIES = libgstaudioeventemitters.la

libgstaudioeventemitters_la_SOURCES = plugin.c \
	gstpowereventemitter.c gstpowereventemitter.h \
	gstentropyeventemitter.c gstentropyeventemitter.h

libgstaudioeventemitters_la_CFLAGS = $(GST_PLUGINS_BAD_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) \
	$(GST_CFLAGS)
libgstaudioeventemitters_la_LIBADD = \
	$(GST_PLUGINS_BASE_LIBS) -lgstaudio-$(GST_API_VERSION) \
	-lgstvideo-$(GST_API_VERSION) -lgstfft-$(GST_API_VERSION) \
	-lgstpbutils-$(GST_API_VERSION) $(GST_BASE_LIBS)  $(GST_LIBS) $(LIBM)
libgstaudioeventemitters_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

noinst_HEADERS = gstpowereventemitter.h gstentropyeventemitter.h
