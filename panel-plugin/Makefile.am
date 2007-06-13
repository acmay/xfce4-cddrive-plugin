# $Id$

INCLUDES =									              \
	-I$(top_srcdir)							            \
	-DG_LOG_DOMAIN=\"xfce4-cddrive-plugin\"	\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"	  \
	$(PLATFORM_CPPFLAGS)


plugin_PROGRAMS =			  \
	xfce4-cddrive-plugin

plugindir =								          \
	$(libexecdir)/xfce4/panel-plugins

xfce4_cddrive_plugin_SOURCES =	    \
  os-dependant/cddrive-cdrom.h      \
  os-dependant/cddrive-cdrom.c      \
  os-dependant/cddrive-process.h    \
  os-dependant/cddrive-process.c    \
	cddrive-error.h                   \
	cddrive-error.c                   \
  cddrive-cddb.h                    \
  cddrive-cddb.c                    \
	cddrive-monitor.h                 \
  cddrive-monitor-private.h         \
	cddrive-monitor.c                 \
	cddrive-dialogs.h			            \
	cddrive-dialogs.c			            \
	cddrive.h					                \
	cddrive.c

xfce4_cddrive_plugin_CFLAGS =	\
	-Wall						            \
	$(LIBXFCEGUI4_CFLAGS)				\
	$(LIBXFCE4PANEL_CFLAGS)			\
	$(EXO_CFLAGS)			          \
	$(HALSTORAGE_CFLAGS)				\
  $(DBUSGLIB_CFLAGS)          \
  $(LIBCDIO_CFLAGS)           \
  $(LIBCDDB_CFLAGS)           \
  $(GTHREAD_CFLAGS)           \
	$(PLATFORM_CFLAGS)

xfce4_cddrive_plugin_LDADD =	\
	$(LIBXFCEGUI4_LIBS)					\
	$(LIBXFCE4PANEL_LIBS)				\
	$(EXO_LIBS)  			          \
	$(HALSTORAGE_LIBS)	        \
  $(DBUSGLIB_LIBS)            \
  $(LIBCDIO_LIBS)             \
  $(LIBCDDB_LIBS)             \
  $(GTHREAD_LIBS)

#
# Desktop file
#
desktopdir =								      \
	$(datadir)/xfce4/panel-plugins

desktop_in_in_files =		\
	cddrive.desktop.in.in

desktop_in_files =							                    \
	$(desktop_in_in_files:.desktop.in.in=.desktop.in)

%.desktop.in: %.desktop.in.in
	sed -e "s,\@libexecdir\@,$(libexecdir),g" < $< > $@

desktop_DATA =								            \
	$(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =						  \
	$(desktop_in_in_files)

CLEANFILES =					\
	$(desktop_in_files) \
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
