/*  $Id$
 *
 *  Copyright (c) 2007 The Xfce development team. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libintl.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "cddrive-monitor.h"
#include "cddrive-error.h"
#include "os-dependant/cddrive-process.h"
#include "os-dependant/cddrive-cdrom.h"
#include "cddrive-cddb.h"


#define _(message) gettext (message)
#define gettext_noop(message) message

#define CDDRIVE_DBUS_REPLY_TIMEOUT  -1 /* in milliseconds (-1 = default timeout value) */
#define CDDRIVE_HAL_DEST            "org.freedesktop.Hal"
#define CDDRIVE_HAL_IFACE_STORAGE   "org.freedesktop.Hal.Device.Storage"
#define CDDRIVE_HAL_IFACE_VOLUME    "org.freedesktop.Hal.Device.Volume"

#define CDDRIVE_UNKNOWN_STATUS_ICON_NAME "stock_unknown"
#define CDDRIVE_EMPTY_DRIVE_ICON_NAME    "gnome-dev-removable"

/* commented as long as the open/close event is not notified */
/* #define CDDRIVE_OPEN_DRIVE_ICON_NAME     "gnome-dev-removable" */



/* HAL icon names mapping                                     */
/*(originaly borrowed from gnome-vfs-hal-mounts.c (rev 5309)) */
/* Remove this once exo-hal replace the direct use of HAL.    */
#define CDDRIVE_AUDIO_DISC_ICON_NAME  "gnome-dev-cdrom-audio"
static const gchar* const cddrive_disc_icon_name [] =
{
  "gnome-dev-disc-cdr",
  "gnome-dev-disc-cdr",
  "gnome-dev-disc-cdrw",
  "gnome-dev-disc-dvdrom",
  "gnome-dev-disc-dvdram",
  "gnome-dev-disc-dvdr",
  "gnome-dev-disc-dvdrw",
  "gnome-dev-disc-dvdr-plus",
  "gnome-dev-disc-dvdrw",
  "gnome-dev-disc-dvdr-plus"
};

/* note: do these strings need to be gettextized ? */
static const gchar* const cddrive_disc_type_name [] =
{
	gettext_noop ("cd-rom"),
	gettext_noop ("cd-r"),
	gettext_noop ("cd-rw"),
	gettext_noop ("dvd"),     /*dvd-rom */
	gettext_noop ("dvd-ram"),
	gettext_noop ("dvd-r"),
	gettext_noop ("dvd-rw"),
	gettext_noop ("dvd+r"),
	gettext_noop ("dvd+rw"),
	gettext_noop ("dvd+r dl"),
	gettext_noop ("bd-rom"),
	gettext_noop ("bd-r"),
	gettext_noop ("bd-re"),
	gettext_noop ("hddvd"),   /* hddvd-rom */
	gettext_noop ("hddvd-r"),
	gettext_noop ("hddvd-rw")
};



/* Set a GError from a DBusError. */
static void
cddrive_store_dbus_error (GError       **error,
                          CddriveError   code,
                          DBusError     *dbus_error)
{
  g_assert (error == NULL || *error == NULL);
  g_assert (dbus_error_is_set (dbus_error));

  g_warning (dbus_error->message);
  if (error != NULL)
    cddrive_set_error (error,
                       code,
                       dbus_error->message);
}



static LibHalVolume*
cddrive_monitor_get_hal_disc (CddriveMonitor *monitor, GError **error);



/* Set 'error' with 'code' and a message indicating the locking process, if possible.
   If a problem occured, then only the code of error will be changed
   (that's why error must be set before the call, to provide a message as precise
   as possible, or be NULL to set nothing). */
static void
cddrive_set_disc_busy_error (GError         **error,
                             CddriveError     code,
                             CddriveMonitor  *monitor)
{
  LibHalVolume *v;
  guint64       pid;
  gchar        *name;

  g_assert (error == NULL || *error != NULL);
  g_assert (monitor != NULL);
  g_assert (monitor->udi != NULL);
  
  if (error == NULL)
    /* nothing to set */
    return;
  
  v = cddrive_monitor_get_hal_disc (monitor, NULL);
  if (v == NULL)
    {
      g_warning ("failed to retrieve disc with UDI '%s'", monitor->udi);
      cddrive_set_error_code (error, code);
      return;
    }
  
  pid = cddrive_find_dir_locker_pid (libhal_volume_get_mount_point (v));
  libhal_volume_free (v);
  
	if (pid == 0)
    {
      g_warning ("unable to retrieve the disc lock");
      /* just set the code of the error, leaving the original message unchanged */
      cddrive_set_error_code (error, code);
      return;
    }
    
  /* we know the locker's pid; enough to set a new error */
  cddrive_clear_error (error);
  
  name = cddrive_get_process_name (pid);
  if (name == NULL)
    cddrive_set_error (error,
                       code,
                       _("The disc is locked by a process (pid: %llu)."),
                       pid);
  else
    {
      cddrive_set_error (error,
                         code,
                         _("The disc is locked by the program '%s' (pid: %llu)."),
                         name,
                         pid);
      g_free (name);
    }
}



static void
cddrive_close_hal_context (LibHalContext *context)
{
  libhal_ctx_shutdown (context, NULL);
  libhal_ctx_free (context);
}



static LibHalContext*
cddrive_get_new_hal_context (GError **error)
{
  LibHalContext  *res = NULL;
  DBusConnection *con;
  DBusError       derr;

  g_assert (error == NULL || *error == NULL);

  dbus_error_init (&derr);

  con = dbus_bus_get (DBUS_BUS_SYSTEM, &derr);
  if (G_UNLIKELY (dbus_error_is_set (&derr)))
    {
      cddrive_store_dbus_error (error, CDDRIVE_ERROR_FAILED, &derr);
      dbus_error_free (&derr);
      return NULL;
    }
  
  dbus_connection_setup_with_g_main (con, NULL);  
  res = libhal_ctx_new ();
  
  if (G_UNLIKELY (! libhal_ctx_set_dbus_connection (res, con)))
    {
      cddrive_set_error (error,
                         CDDRIVE_ERROR_FAILED,
                         _("Failed to set the D-BUS connection of HAL context."));
      libhal_ctx_free (res);
      return NULL;
    }
  
  if (G_UNLIKELY (! libhal_ctx_init (res, &derr)))
    {
      cddrive_store_dbus_error (error, CDDRIVE_ERROR_FAILED, &derr);
      dbus_error_free (&derr);
      cddrive_close_hal_context (res);
      return NULL;
    }

  return res;
}



/* Return a NULL terminated array of all detected CD drives */
CddriveDriveInfo**
cddrive_cdrom_drive_infos_new (GError **error)
{
  CddriveDriveInfo* *res;
  LibHalContext     *ctx;
  LibHalDrive       *drv;
  DBusError          derr;
  gchar*            *udis;
  gint               i, count;

  g_assert (error == NULL || *error == NULL);

  ctx = cddrive_get_new_hal_context (error);

  if (ctx == NULL)
    return NULL;

  dbus_error_init (&derr);
  udis = libhal_find_device_by_capability (ctx,
                                           "storage.cdrom",
                                           &count,
                                           &derr);
  if (dbus_error_is_set (&derr))
    {
      cddrive_store_dbus_error (error, CDDRIVE_ERROR_FAILED, &derr);
      dbus_error_free (&derr);
      cddrive_close_hal_context (ctx);
      return NULL;
    }
  
  res = (CddriveDriveInfo**) g_malloc0 ((count + 1) * sizeof (CddriveDriveInfo*));
  
  for (i=0; i < count; i++)
    {
      drv = libhal_drive_from_udi (ctx, udis [i]);

      res [i] = (CddriveDriveInfo*) g_malloc0 (sizeof (CddriveDriveInfo));      
      res [i]->device = g_strdup (libhal_drive_get_device_file (drv));
      res [i]->model  = g_strdup (libhal_drive_get_model (drv));
      
      libhal_drive_free (drv);
    }
  res [count] = NULL;
  
  libhal_free_string_array (udis);
  cddrive_close_hal_context (ctx);
  
  return res;
}



void
cddrive_cdrom_drive_infos_free (CddriveDriveInfo* *infos)
{
  gint i;

  g_assert (infos != NULL);

  i = 0;
  while (infos [i] != NULL)
    {
      g_free (infos[i]->device);
      g_free (infos[i]->model);
      g_free (infos [i]);
      i++;
    }
  
  g_free (infos);
}



/* Return the monitored drive from stored device path or NULL if retrieve failed. */
static LibHalDrive*
cddrive_monitor_get_hal_drive (CddriveMonitor *monitor, GError **error)
{
  LibHalDrive *res;

  g_assert (monitor != NULL);
  g_assert (monitor->dev != NULL);
  g_assert (error == NULL || *error == NULL);

  res = libhal_drive_from_device_file (monitor->ctx, monitor->dev);
  if (G_UNLIKELY (res == NULL))
    {
      cddrive_set_error (error,
                         CDDRIVE_ERROR_FAILED,
                         _("Failed to retrieve drive from device path '%s'."),
                         monitor->dev);
    }
  
  return res;
}



/* Return the monitored disc from stored UDI, NULL if there is no disc or
  retrieve failed. */
static LibHalVolume*
cddrive_monitor_get_hal_disc (CddriveMonitor *monitor, GError **error)
{
  LibHalVolume *res;

  g_assert (monitor != NULL);
  g_assert (error == NULL || *error == NULL);

  if (monitor->udi == NULL)
    return NULL;
  
  res = libhal_volume_from_udi (monitor->ctx, monitor->udi);
  if (G_UNLIKELY (res == NULL))
    cddrive_set_error (error,
                       CDDRIVE_ERROR_FAILED,
                       _("Failed to retrieve disc from UDI '%s'."),
                       monitor->udi);
  return res;
}



static void
cddrive_monitor_hal_callback_property_modified (LibHalContext *ctx,
                                                const char    *udi,
                                                const char    *key,
                                                dbus_bool_t    is_removed,
                                                dbus_bool_t    is_added)
{
  CddriveMonitor *m;

  g_assert (ctx != NULL);

  m = (CddriveMonitor*) libhal_ctx_get_user_data (ctx);
  g_assert (m != NULL);

  /* filter the properties we are interested in */
  if (g_str_equal (key, "volume.is_mounted") ||
      g_str_equal (key, "volume.ignore"))
    {
      (*((CddriveOnDiscModified) m->on_disc_modified)) (m, m->dat);
    }
}




static void
cddrive_monitor_register_disc (CddriveMonitor  *monitor,
                               LibHalVolume    *disc)
{
  DBusError derr;

  g_assert (monitor != NULL);
  g_assert (monitor->udi == NULL); /* if not NULL, need to unregister first */
  g_assert (disc != NULL);

  monitor->udi = g_strdup (libhal_volume_get_udi (disc));
  
  dbus_error_init (&derr);

  if (G_UNLIKELY (! libhal_device_add_property_watch (monitor->ctx, monitor->udi, &derr)))
    {
      g_warning (derr.message);
      dbus_error_free (&derr);
      return;
    }
  
  if (G_UNLIKELY (! libhal_ctx_set_device_property_modified (
                              monitor->ctx,
                              cddrive_monitor_hal_callback_property_modified)))
    {
      g_warning (_("Failed to set drive's status change callback."));
      if (G_UNLIKELY (! libhal_device_remove_property_watch (monitor->ctx,
                                                             monitor->udi,
                                                             &derr)))
        g_warning (derr.message);
        dbus_error_free (&derr);
    }
}



static void
cddrive_monitor_unregister_disc (CddriveMonitor *monitor)
{
  DBusError derr;

  g_assert (monitor != NULL);
  g_assert (monitor->udi != NULL);

  dbus_error_init (&derr);
  if (! libhal_device_remove_property_watch (monitor->ctx, monitor->udi, &derr))
    {
      g_warning (derr.message);
      dbus_error_free (&derr);
    }
  
  g_free (monitor->udi);
  monitor->udi = NULL;
  
  if (monitor->audio_title != NULL)
    {
      g_free (monitor->audio_title);
      monitor->audio_title = NULL;
    }
}



static void
cddrive_monitor_update_disc (CddriveMonitor *monitor)
{
  LibHalVolume *v;

  g_assert (monitor != NULL);
  g_assert (monitor->dev != NULL);

  if (monitor->udi != NULL)
    cddrive_monitor_unregister_disc (monitor);

  v = libhal_volume_from_device_file (monitor->ctx, monitor->dev);
  if (v != NULL)
    {
      cddrive_monitor_register_disc (monitor, v);
      libhal_volume_free (v);
    }
}



static void
cddrive_monitor_hal_callback_device_added (LibHalContext *ctx, const char *udi)
{
  CddriveMonitor *m;

  g_assert (ctx != NULL);
  g_assert (udi != NULL);

  m = (CddriveMonitor*) libhal_ctx_get_user_data (ctx);
  g_assert (m != NULL);

  cddrive_monitor_update_disc (m);
  if (m->udi != NULL && g_str_equal (m->udi, udi))
      (*((CddriveOnDiscInserted) m->on_disc_inserted)) (m, m->dat);
}



static void
cddrive_monitor_hal_callback_device_removed (LibHalContext *ctx, const char *udi)
{
  CddriveMonitor *m;

  g_assert (ctx != NULL);
  g_assert (udi != NULL);

  m = (CddriveMonitor*) libhal_ctx_get_user_data (ctx);
  g_assert (m != NULL);

  if (m->udi != NULL && g_str_equal (m->udi, udi))
    {
      cddrive_monitor_unregister_disc (m);
      (*((CddriveOnDiscRemoved) m->on_disc_removed)) (m, m->dat);
    }
}



gboolean
cddrive_monitor_enable_callbacks (CddriveMonitor *monitor,
                                  gboolean enable,
                                  GError **error)
{
  DBusError derr;

  g_assert (monitor != NULL);

  if (monitor->callbacks_enabled == enable)
    return TRUE;
  
  if (enable)
    {
      /*enable callbacks */
    
      if (G_UNLIKELY (! libhal_ctx_set_device_added (monitor->ctx,
                                                     cddrive_monitor_hal_callback_device_added)))
        {
          cddrive_set_error (error,
                             CDDRIVE_ERROR_FAILED,
                             _("Failed to register drive addition callback."));
          return FALSE;
        }
  
      if (G_UNLIKELY (! libhal_ctx_set_device_removed (monitor->ctx,
                                                       cddrive_monitor_hal_callback_device_removed)))
        {
          cddrive_set_error (error,
                             CDDRIVE_ERROR_FAILED,
                             _("Failed to register drive removal callback."));
          return FALSE;
        }
      
      if (monitor->udi != NULL)
        {
          dbus_error_init (&derr);
          if (! libhal_device_remove_property_watch (monitor->ctx, monitor->udi, &derr))
            {
              cddrive_store_dbus_error (error, CDDRIVE_ERROR_FAILED, &derr);
              dbus_error_free (&derr);
              return FALSE;
            }
        }
    }
  else 
    {
      /* disable callbacks */
    
      if (G_UNLIKELY (! libhal_ctx_set_device_added (monitor->ctx, NULL)))
        {
          cddrive_set_error (error,
                             CDDRIVE_ERROR_FAILED,
                             _("Failed to unregister drive addition callback."));
          return FALSE;
        }
  
      if (G_UNLIKELY (! libhal_ctx_set_device_removed (monitor->ctx, NULL)))
        {
          cddrive_set_error (error,
                             CDDRIVE_ERROR_FAILED,
                             _("Failed to unregister drive removal callback."));
          return FALSE;
        }
      
      if (monitor->udi != NULL)
        {
          dbus_error_init (&derr);
          if (! libhal_device_remove_property_watch (monitor->ctx, monitor->udi, &derr))
            {
              cddrive_store_dbus_error (error, CDDRIVE_ERROR_FAILED, &derr);
              dbus_error_free (&derr);
              return FALSE;
            }
        }
    }
  
  monitor->callbacks_enabled = enable;
  return TRUE;
}



gboolean
cddrive_monitor_callbacks_enabled (CddriveMonitor *monitor)
{
  g_assert (monitor != NULL);

  return monitor->callbacks_enabled;
}



static void
cddrive_monitor_set_is_ejectable (CddriveMonitor *monitor, GError **error)
{
  LibHalDrive *drv;

  g_assert (monitor != NULL);
  g_assert (monitor->dev != NULL);
  g_assert (error == NULL || *error == NULL);

  monitor->is_ejectable = FALSE;
  drv = cddrive_monitor_get_hal_drive (monitor, error);
  if (drv != NULL)
    {
      monitor->is_ejectable = libhal_drive_requires_eject (drv);
      libhal_drive_free (drv);
    }
}



/* Return a copy of a fallback command with each "%'symbol'" occurences replaced
   with ''. Symbol is either "d", "m" or "u". The result must be freed after use. */
static gchar*
cddrive_monitor_get_command_with_value (const gchar *custom_command,
                                        const gchar  symbol,
                                        const gchar *value)
{
  gchar *res, *tmp, *s;
  gchar ss [] = {'%', symbol, '\0'};

  g_assert (custom_command != NULL);
  g_assert (symbol =='d' || symbol == 'm' || symbol == 'u');
  g_assert (value != NULL);
  
  /* replace symbol string with "%s" in the command string */
  tmp = g_strdup (custom_command);
  for (s = g_strrstr (tmp, ss); s != NULL; s = g_strrstr (tmp, ss))
    s[1] = 's';
  
  res = g_strdup_printf (tmp, value);
  g_free (tmp);
  return res;
}



CddriveMonitor*
cddrive_monitor_new (gchar                  *device,
                     gpointer                callback_data,
                     CddriveOnDiscInserted   on_disc_inserted_callback,
                     CddriveOnDiscRemoved    on_disc_removed_callback,
                     CddriveOnDiscModified   on_disc_modified_callback,
                     const gchar            *mount_fallback,
                     const gchar            *unmount_fallback,
                     gboolean                use_cddb,
                     GError                **error)
{
  CddriveMonitor *res;

  g_assert (device != NULL);
  g_assert (on_disc_inserted_callback != NULL);
  g_assert (on_disc_removed_callback  != NULL);
  g_assert (on_disc_modified_callback != NULL);
  g_assert (mount_fallback == NULL || g_utf8_strlen (mount_fallback, 1) != 0);
  g_assert (unmount_fallback == NULL || g_utf8_strlen (unmount_fallback, 1) != 0);
  g_assert (error == NULL || *error == NULL);

  res = (CddriveMonitor*) g_try_malloc (sizeof (CddriveMonitor));
  if (res == NULL)
    {
      if (error != NULL)
        *error = cddrive_no_memory_error ();
      return NULL;
    }

  res->dev                  = device;
  res->dat                  = callback_data;
  res->on_disc_inserted     = on_disc_inserted_callback;
  res->on_disc_removed      = on_disc_removed_callback;
  res->on_disc_modified     = on_disc_modified_callback;
  res->callbacks_enabled    = FALSE; /* set to FALSE to enable callbacks effectively below */
  res->udi                  = NULL;
  res->is_ejectable         = FALSE;
  res->audio_title          = NULL;
  res->use_cddb             = use_cddb;

  res->mount                = NULL;
  res->mount_needs_update   = FALSE;
  res->unmount              = NULL;
  res->unmount_needs_update = FALSE;

  res->ctx = cddrive_get_new_hal_context (error);
  if (G_UNLIKELY (res->ctx == NULL))
    {
      cddrive_monitor_free (res);
      return NULL;
    }

  if (G_UNLIKELY (! libhal_ctx_set_user_data (res->ctx, res)))
    {
      cddrive_set_error (error,
                         CDDRIVE_ERROR_FAILED,
                         _("Failed to store monitor in HAL context."));
      cddrive_monitor_free (res);
      return NULL;
    }
  
  if (G_UNLIKELY (! cddrive_monitor_enable_callbacks (res, TRUE, error)))
    {
      cddrive_monitor_free (res);
      return NULL;
    }
  
  cddrive_monitor_set_is_ejectable (res, error);
  
  if (G_UNLIKELY (error != NULL && *error != NULL))
    {
      cddrive_monitor_free (res);
      return NULL;
    }
  
  /* register disc if any */
  cddrive_monitor_update_disc (res);
  
  /* store the custom commands with device inserted once for all */
  if (mount_fallback != NULL)
    {
      res->mount = cddrive_monitor_get_command_with_value (mount_fallback,
                                                           'd',
                                                           res->dev);
      /* because %d is more likely to be used than %m and %u, we detect once for all
         if these two are present in the command, implying an update each time
         the mount command is used */
      res->mount_needs_update = (g_strrstr (res->mount, "%m") != NULL ||
                                 g_strrstr (res->mount, "%u") != NULL);
    }
  if (unmount_fallback != NULL)
    {
      res->unmount = cddrive_monitor_get_command_with_value (unmount_fallback,
                                                             'd',
                                                             res->dev);
      /* same as above, for unmount this time */
      res->unmount_needs_update = (g_strrstr (res->unmount, "%m") != NULL ||
                                   g_strrstr (res->unmount, "%u") != NULL);
    }
  
  return res;
}



void
cddrive_monitor_free (CddriveMonitor * monitor)
{
  g_assert (monitor != NULL);

  /* (monitor->ctx == NULL) implies (monitor->udi == NULL) */
  g_assert ((monitor->ctx != NULL) || (monitor->udi == NULL));

  if (G_LIKELY (monitor->ctx != NULL))
    {
      if (monitor->udi != NULL)
        cddrive_monitor_unregister_disc (monitor);
      cddrive_close_hal_context (monitor->ctx);
    }

  g_free (monitor->mount);
  g_free (monitor->unmount);
  g_free (monitor);
}



/* Send a command (created with 'cddrive_new_command_message()') to hald.
   'command_message' is unreferenced by this function. */
static void
cddrive_monitor_send_command (CddriveMonitor  *monitor,
                              DBusMessage     *command_message,
                              DBusError       *dbus_error)
{
  DBusMessage    *reply;
  DBusConnection *con;

  g_assert (monitor != NULL);
  g_assert (monitor->ctx != NULL);
  g_assert (command_message != NULL);
  
  /* send D-BUS message and wait for reply */
  con = libhal_ctx_get_dbus_connection (monitor->ctx);
  reply = dbus_connection_send_with_reply_and_block (con,
                                                     command_message,
                                                     CDDRIVE_DBUS_REPLY_TIMEOUT,
                                                     dbus_error);
  /* cleanup messages */
  dbus_message_unref (command_message);
  if (G_LIKELY (reply != NULL))
    dbus_message_unref (reply);  
}



/* Launch a command on the drive (to open or close the tray) */
static void
cddrive_monitor_command_drive (CddriveMonitor  *monitor,
                               const gchar     *command,
                               GError         **error,
                               CddriveError     code)
{
  LibHalDrive  *drv;
  DBusMessage  *msg;
  DBusError     derr;
  const gchar*  msg_opts [0]; /* no special option */

  g_assert (monitor != NULL);
  g_assert (monitor->ctx != NULL);
  g_assert (monitor->dev != NULL);
  g_assert (error == NULL || *error == NULL);

  /* retrieve drive UDI */
  drv = cddrive_monitor_get_hal_drive (monitor, error);
  if (drv == NULL)
    return;

  msg = dbus_message_new_method_call (CDDRIVE_HAL_DEST,
                                      libhal_drive_get_udi (drv),
                                      CDDRIVE_HAL_IFACE_STORAGE,
                                      command);
  libhal_drive_free (drv);
  
  if (G_UNLIKELY (msg == NULL))
    {
      if (error != NULL)
        *error = cddrive_no_memory_error ();
      return;
    }  

  dbus_message_append_args (msg,
                            DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &msg_opts, 0,
                            DBUS_TYPE_INVALID);

  dbus_error_init (&derr);
  cddrive_monitor_send_command (monitor, msg, &derr); /* note: msg is unrefed by this call */

  /* set 'error' if necessary */
  if (dbus_error_is_set (&derr))
    {
      cddrive_store_dbus_error (error, code, &derr);
      dbus_error_free (&derr);
    }
}



void
cddrive_monitor_eject (CddriveMonitor *monitor, GError **error)
{
  g_assert (monitor != NULL);
  g_assert (monitor->ctx != NULL);
  g_assert (monitor->dev != NULL);
  g_assert (error == NULL || *error == NULL);

  cddrive_monitor_command_drive (monitor,
                                 "Eject",
                                 error,
                                 CDDRIVE_ERROR_EJECT);
}



void
cddrive_monitor_close (CddriveMonitor *monitor, GError **error)
{
  g_assert (monitor != NULL);
  g_assert (monitor->ctx != NULL);
  g_assert (monitor->dev != NULL);
  g_assert (error == NULL || *error == NULL);

  cddrive_monitor_command_drive (monitor,
                                 "CloseTray",
                                 error,
                                 CDDRIVE_ERROR_CLOSE);
}



/* Return TRUE if mount succeeded, FALSE otherwise. */
static gboolean
cddrive_monitor_hal_mount (CddriveMonitor *monitor, GError **error)
{
  DBusMessage *msg;
  DBusError    derr;

  /* mount arguments */
  const gchar*  mount_opts [0];     /* no special option */
  const gchar  *fs_type     = "";   /* use default fs type */
  const gchar  *mount_point = "";   /* use default mount point
                                       (note: HAL spec says NULL is valid,
                                        but it crash the plugin ) */
  
  g_assert (monitor != NULL);
  g_assert (monitor->ctx != NULL);
  g_assert (monitor->udi != NULL);
  g_assert (error == NULL || *error == NULL);
  
  /* create command message */
  msg = dbus_message_new_method_call (CDDRIVE_HAL_DEST,
                                      monitor->udi,
                                      CDDRIVE_HAL_IFACE_VOLUME,
                                      "Mount");
  if (msg == NULL)
    {
      if (error != NULL)
        *error = cddrive_no_memory_error ();
      return FALSE;
    }
  
  dbus_message_append_args (msg,
                            DBUS_TYPE_STRING, &mount_point,
                            DBUS_TYPE_STRING, &fs_type,
                            DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &mount_opts, 0,
                            DBUS_TYPE_INVALID);  

  dbus_error_init (&derr);
  cddrive_monitor_send_command (monitor, msg, &derr); /* note: msg is unrefed by this call */
  
  /* set 'error' if necessary */
  if (dbus_error_is_set (&derr))
    {
      cddrive_store_dbus_error (error, CDDRIVE_ERROR_MOUNT, &derr);
      dbus_error_free (&derr);
      return FALSE;
    }
  
  return TRUE;
}



/* Return TRUE if unmount succeeded, FALSE otherwise. */
static gboolean
cddrive_monitor_hal_unmount (CddriveMonitor *monitor, GError **error)
{
  DBusMessage  *msg;
  DBusError    derr;

  /* unmount arguments */
  const gchar*  unmount_opts [0]; /* no special option */
  
  g_assert (monitor != NULL);
  g_assert (monitor->ctx != NULL);
  g_assert (monitor->udi != NULL);
  g_assert (error == NULL || *error == NULL);

  /* create command message */
  msg = dbus_message_new_method_call (CDDRIVE_HAL_DEST,
                                      monitor->udi,
                                      CDDRIVE_HAL_IFACE_VOLUME,
                                      "Unmount");
  if (msg == NULL)
    {
      if (error != NULL)
        *error = cddrive_no_memory_error ();
      return FALSE;
    }

  dbus_message_append_args (msg,
                            DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &unmount_opts, 0,
                            DBUS_TYPE_INVALID);  

  dbus_error_init (&derr);
  cddrive_monitor_send_command (monitor, msg, &derr); /* note: msg is unrefed by this call */
  
  /* set 'error' if necessary */
  if (dbus_error_is_set (&derr))
    {
      if (dbus_error_has_name (&derr, "org.freedesktop.Hal.Device.Volume.Busy"))
        /* filter the "busy" error to provide a more useful message to the user */
        cddrive_store_dbus_error (error, CDDRIVE_ERROR_BUSY, &derr);
      else
        cddrive_store_dbus_error (error, CDDRIVE_ERROR_UNMOUNT, &derr);
      
      dbus_error_free (&derr);
      return FALSE;
    }
  
  return TRUE;
}



static void
cddrive_monitor_run_updated_command (CddriveMonitor  *monitor,
                                     const gchar     *command,
                                     GError         **error)
{
  gchar        *cmd, *tmp;
  LibHalVolume *disc;

  g_assert (monitor != NULL);
  g_assert (monitor->udi != NULL);
  g_assert (command != NULL);
  g_assert (error == NULL || *error == NULL);

  cmd = g_strdup (command);
  if (g_strrstr (cmd, "%m"))
    {
      disc = cddrive_monitor_get_hal_disc (monitor, error);
      if (disc == NULL)
        return;
      tmp = cmd;
      cmd = cddrive_monitor_get_command_with_value (tmp,
                                                    'm',
                                                    libhal_volume_get_mount_point (disc));
      g_free (tmp);
      libhal_volume_free (disc);
    }
  
  if (g_strrstr (cmd, "%u"))
    {
      tmp = cmd;
      cmd = cddrive_monitor_get_command_with_value (tmp,
                                                    'u',
                                                    monitor->udi);
      g_free (tmp);
    }
  
  g_spawn_command_line_sync (cmd, NULL, NULL, NULL, error);
  g_free (cmd);
}



void
cddrive_monitor_mount (CddriveMonitor *monitor, GError **error)
{
  g_assert (monitor != NULL);
  g_assert (error == NULL || *error == NULL);

  if (! cddrive_monitor_hal_mount (monitor, error) && monitor->mount != NULL)
    {
      /* HAL failed, let's try the fallback */
      if (error != NULL)
        {
          g_assert (*error != NULL);
          g_message ("HAL mount failed : %s... Trying fallback command.",
                     (*error)->message);
          cddrive_clear_error (error);
        }
      
      if (monitor->mount_needs_update)
        cddrive_monitor_run_updated_command (monitor, monitor->mount, error);
      else
        g_spawn_command_line_sync (monitor->mount, NULL, NULL, NULL, error);

      if (error != NULL && *error != NULL)
        cddrive_set_error_code (error, CDDRIVE_ERROR_MOUNT);
    }
}



void
cddrive_monitor_unmount (CddriveMonitor *monitor, GError **error)
{
  g_assert (monitor != NULL);
  g_assert (error == NULL || *error == NULL);

  if (! cddrive_monitor_hal_unmount (monitor, error))
    {
      if (error != NULL && (*error)->code == CDDRIVE_ERROR_BUSY)
        {
          /* The disc is busy. No need to try the fallback command,
             but we can try to find the culprit and set the error message accordingly. */
          cddrive_set_disc_busy_error (error, CDDRIVE_ERROR_UNMOUNT, monitor);
          return;
        }
      
      if (monitor->unmount != NULL)
        {
          /* HAL failed, let's try the fallback */
          if (error != NULL)
            {
              g_message ("HAL unmount failed : %s... Trying fallback command.",
                         (*error)->message);
              cddrive_clear_error (error);
            }
          
          if (monitor->unmount_needs_update)
            cddrive_monitor_run_updated_command (monitor, monitor->unmount, error);
          else
            g_spawn_command_line_sync (monitor->unmount, NULL, NULL, NULL, error);
      
          if (error != NULL && *error != NULL)
            cddrive_set_error_code (error, CDDRIVE_ERROR_UNMOUNT);
        }
    }
}



static const gchar*
cddrive_status_get_disc_icon_name (LibHalVolume *disc)
{
  g_assert (disc != NULL);

  if (libhal_volume_disc_has_audio (disc))
    return CDDRIVE_AUDIO_DISC_ICON_NAME;
  else
    return cddrive_disc_icon_name [libhal_volume_get_disc_type (disc) - LIBHAL_VOLUME_DISC_TYPE_CDROM];
}



static const gchar*
cddrive_monitor_get_audio_disc_title (CddriveMonitor *monitor)
{
  g_assert (monitor != NULL);
  g_assert (monitor->dev != NULL);

  if (monitor->audio_title != NULL)
    /* title has been previously cached */
      return monitor->audio_title;

  monitor->audio_title = cddrive_cddb_get_title (monitor->dev, monitor->use_cddb);
  return monitor->audio_title;
}



CddriveStatus*
cddrive_status_new (CddriveMonitor *monitor, GError **error)
{
  CddriveStatus *res;
  LibHalVolume  *vol;
  gint           tray_stat;

  /* note: monitor can be NULL */

  g_assert (error != NULL);
  g_assert (*error == NULL);

  if (monitor == NULL)
    return NULL;

  vol = cddrive_monitor_get_hal_disc (monitor, error);

  if (G_UNLIKELY (*error != NULL))
    {
      if (vol != NULL)
        libhal_volume_free (vol);
      return NULL;
    }
  
  res = (CddriveStatus*) g_try_malloc (sizeof (CddriveStatus));
  if (res == NULL)
    {
      *error = cddrive_no_memory_error ();
      return NULL;
    }

  res->mon = monitor;
  /* set flags for empty drive */
  res->is_open      = FALSE;
  res->is_empty     = TRUE;
  res->is_mounted   = FALSE;
  res->is_blank     = FALSE;
  res->is_audio     = FALSE;
  res->type         = NULL;
  res->title        = NULL;

  if (vol == NULL)
    {
      /* drive is empty */
      if (monitor->is_ejectable)
        {
          tray_stat = cddrive_get_tray_status (monitor->dev);
          if (tray_stat == -1)
            res->icon_name  = CDDRIVE_UNKNOWN_STATUS_ICON_NAME;
          else
            {
              res->is_open = (tray_stat == 1);
            
              /* commented as long as the open/close event is not notified */
              /*
              if (res->is_open)
                res->icon_name  = CDDRIVE_OPEN_DRIVE_ICON_NAME;
              else
              */
                res->icon_name  = CDDRIVE_EMPTY_DRIVE_ICON_NAME;
            }
        }
      else
        res->icon_name  = CDDRIVE_EMPTY_DRIVE_ICON_NAME;
    }
  else
    {
      /* drive has a disc */
      res->is_empty   = FALSE;
      res->is_mounted = libhal_volume_is_mounted (vol);
      res->is_blank   = libhal_volume_disc_is_blank (vol);
      res->is_audio   = libhal_volume_disc_has_audio (vol);
      res->icon_name  = cddrive_status_get_disc_icon_name (vol);
        
      res->type  = cddrive_disc_type_name [libhal_volume_get_disc_type (vol) - LIBHAL_VOLUME_DISC_TYPE_CDROM];
      res->title = g_strdup (libhal_volume_get_label (vol));
      
      libhal_volume_free (vol);
    }
  
  g_assert (res != NULL);
  g_assert (res->icon_name != NULL);
  return res;
}
  


void
cddrive_status_free (CddriveStatus *status)
{
  g_assert (status != NULL);

  g_free (status->title);
  g_free (status);
}



gboolean
cddrive_status_is_ejectable (CddriveStatus *status)
{
  g_assert (status != NULL);
  
  return status->mon->is_ejectable;
}



gboolean
cddrive_status_is_open (CddriveStatus *status)
{
  g_assert (status != NULL);
  
  return status->is_open;
}



gboolean
cddrive_status_is_empty (CddriveStatus *status)
{
  g_assert (status != NULL);

  return status->is_empty;
}



gboolean
cddrive_status_is_mounted (CddriveStatus *status)
{
  g_assert (status != NULL);

  return status->is_mounted;
}



gboolean
cddrive_status_is_blank (CddriveStatus *status)
{
  g_assert (status != NULL);

  return status->is_blank;
}



gboolean
cddrive_status_is_audio (CddriveStatus *status)
{
  g_assert (status != NULL);

  return status->is_audio;
}



const gchar*
cddrive_status_get_title (CddriveStatus *status)
{
  g_assert (status != NULL);

  if (status->title == NULL && status->is_audio)
    return cddrive_monitor_get_audio_disc_title (status->mon);
  
  return status->title;  
}



const gchar*
cddrive_status_get_type (CddriveStatus *status)
{
  g_assert (status != NULL);

  return _(status->type);
}



const gchar*
cddrive_status_get_icon_name (CddriveStatus *status)
{
  g_assert (status == NULL || status->icon_name != NULL);

  if (status == NULL)
    return CDDRIVE_UNKNOWN_STATUS_ICON_NAME;
  
  return status->icon_name;
}
