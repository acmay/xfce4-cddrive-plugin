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

#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4panel/xfce-panel-convenience.h> /* for xfce_create_panel_button() */
#include <libxfce4panel/xfce-hvbox.h>
#define EXO_API_SUBJECT_TO_CHANGE
#include <exo/exo.h> /* for exo_gdk_pixbuf_lucent() */
#include "cddrive.h"
#include "cddrive-error.h"
#include "cddrive-cddb.h"
#include "cddrive-dialogs.h"


#define CDDRIVE_ICON_PADDING                2

#define CDDRIVE_CONF_ID_DEVICE              "device"
#define CDDRIVE_CONF_ID_NAME                "name"
#define CDDRIVE_CONF_ID_MOUNT_FALLBACK      "mount_fallback"
#define CDDRIVE_CONF_ID_UNMOUNT_FALLBACK    "unmount_fallback"
#define CDDRIVE_CONF_ID_USE_NAME_IN_PANEL   "use_name_in_panel"
#define CDDRIVE_CONF_ID_USE_NAME_IN_TIP     "use_name_in_tip"
#define CDDRIVE_CONF_ID_USE_MOUNTED_COLOR   "use_mounted_color"
#define CDDRIVE_CONF_ID_MOUNTED_COLOR       "mounted_color"
#define CDDRIVE_CONF_ID_USE_UNMOUNTED_COLOR "use_unmounted_color"
#define CDDRIVE_CONF_ID_UNMOUNTED_COLOR     "unmounted_color"
#define CDDRIVE_CONF_ID_USE_TRANSLUCENCY    "use_translucency"
#define CDDRIVE_CONF_ID_TRANSLUCENCY        "translucency"
#define CDDRIVE_CONF_ID_USE_CDDB            "use_cddb"

#define CDDRIVE_DEFAULT_DEVICE              NULL
#define CDDRIVE_DEFAULT_NAME                NULL
#define CDDRIVE_DEFAULT_USE_NAME_IN_PANEL   FALSE
#define CDDRIVE_DEFAULT_USE_NAME_IN_TIP     FALSE
#define CDDRIVE_DEFAULT_COLOR               NULL
#define CDDRIVE_DEFAULT_USE_MOUNTED_COLOR   FALSE
#define CDDRIVE_DEFAULT_USE_UNMOUNTED_COLOR FALSE
#define CDDRIVE_DEFAULT_USE_TRANSLUCENCY    TRUE
#define CDDRIVE_DEFAULT_TRANSLUCENCY        50
#define CDDRIVE_DEFAULT_USE_CDDB            TRUE

#define CDDRIVE_LOCK_EMBLEM "stock_lock"


/* Default fallback mount and unmount commands sorted in preference order.
   Last element must be NULL.
   Use CddriveCommandType enum (in cddrive.h) to access one of the command sub-array). */
static const gchar* const cddrive_default_fallback_commands[2][4] = 
{
  {
    "exo-mount %d",
    "pmount %d",
    "mount %d",
    NULL
  },
  {
    "exo-mount -u %d",
    "pumount %d",
    "umount %d",
    NULL
  }
};



static void
cddrive_set_button_icon (CddrivePlugin *cddrive,
                         const gchar   *name,
                         GdkColor      *color,
                         gboolean       use_translucency)
{
  GdkPixbuf *ico, *tmp;
  gint       size;

  g_assert (cddrive->button != NULL);
  g_assert (cddrive->image != NULL);
  g_assert (name != NULL);

  size = xfce_panel_plugin_get_size (cddrive->plugin) - CDDRIVE_ICON_PADDING * 2;

  ico = xfce_themed_icon_load (name, size);

  if (color != NULL)
    {
      tmp = exo_gdk_pixbuf_colorize (ico, color);
      g_object_unref (G_OBJECT (ico));
      ico = tmp;
    }
  
  if (use_translucency)
    {
      tmp = exo_gdk_pixbuf_lucent (ico, cddrive->translucency);
      g_object_unref (G_OBJECT (ico));
      ico = tmp;
    }
  
  gtk_image_clear (GTK_IMAGE (cddrive->image));
  gtk_image_set_from_pixbuf (GTK_IMAGE (cddrive->image), ico);
 
  g_object_unref (G_OBJECT (ico));
}



static void
cddrive_update_icon_from_status (CddrivePlugin *cddrive, CddriveStatus *status)
{
  GdkColor *c = NULL;
  gboolean  use_trans = FALSE;

  if (status != NULL)
    {
      use_trans = (! (cddrive_status_is_empty (status)   ||
                      cddrive_status_is_mounted (status) ||
                      cddrive_status_is_audio (status)));
  
      if (cddrive_status_is_mounted (status))
        c = cddrive->use_mounted_color ? cddrive->mounted_color : NULL;
      else
        {
          if (! (cddrive_status_is_empty (status) ||
                 cddrive_status_is_audio (status)))
            c = cddrive->use_unmounted_color ? cddrive->unmounted_color : NULL;
        }
    }
  
  cddrive_set_button_icon (cddrive,
                           cddrive_status_get_icon_name (status),
                           c, use_trans);
}



void
cddrive_update_icon (CddrivePlugin *cddrive)
{
  CddriveStatus *stat;
  GError        *error = NULL;

  stat = cddrive_status_new (cddrive->monitor, &error);
  cddrive_update_icon_from_status (cddrive, stat);

  if (stat != NULL)
    cddrive_status_free (stat);

  if (error != NULL)
    {
      cddrive_show_error (error);
      cddrive_clear_error (&error);
    }
}



/* The result must be freed when no longer needed. */
static gchar*
cddrive_get_unnamed_tip_text_from_status (CddriveStatus *status)
{
  const gchar *title, *type;

  /* Even if it result in a clumsy code, try to write the messages without using
     concatenation tricks, to preserve their meaning in the po files. */

  if (status == NULL)
    return g_strdup (_("Drive status is unavailable... is HAL installed and hald running ?"));
  
  if (cddrive_status_is_empty (status))
    {
      /* no CD title in the message */
    
      if (cddrive_status_is_ejectable (status))
        {
          if (cddrive_status_is_open (status))
            return g_strdup (_("Close tray"));
          
          return g_strdup (_("Open tray"));
        }
              
      /* drive is not ejectable */
      return g_strdup (_("No disc in the drive"));
    }
  
  /* There is a disc in the drive : use title whenever it is available.
     Otherwise, use disc type. */
  
  title = cddrive_status_get_title (status);
  type  = cddrive_status_get_type (status);
  
  if (cddrive_status_is_ejectable (status))
    {
      if (cddrive_status_is_blank (status))
        /* translation note: "Eject blank <disc type>" (e.g. "Eject blank cd-rw") */
        return g_strdup_printf (_("Eject blank %s"), type);
      
      if (title != NULL)
        /* translation note: "Eject \"<disc title>\"" */
        return g_strdup_printf (_("Eject \"%s\""), title);

      if (cddrive_status_is_audio (status))
        /* translation note: "Eject audio <disc type>" */
        return g_strdup_printf (_("Eject audio %s"), type);
        
      /* translation note: "Eject <disc type>" (e.g. "Eject dvd") */
      return g_strdup_printf (_("Eject %s"), type);
    }
  
  /* unejectable drive with disc inside */
  
  if (cddrive_status_is_blank (status))
    /* translation note: "Blank <disc type>" (e.g. "Blank cd-rw") */
    return g_strdup_printf (_("Blank %s"), type);
      
  if (title != NULL)
    /* translation note: "\"<disc title>\" (made translatable in case translation
                         do not use the '"' character to enclose the title) */
    return g_strdup_printf (_("\"%s\""), title);

  if (cddrive_status_is_audio (status))
    /* translation note: "Audio <disc type>" */
    return g_strdup_printf (_("Audio %s"), type);
        
  return g_strdup (type);
}



/* The result must be freed when no longer needed. */
static gchar*
cddrive_get_named_tip_text_from_status (CddriveStatus *status,
                                        const gchar   *drive_name)
{
  const gchar *title, *type;
  
  g_assert (drive_name != NULL);

  /* Even if it result in a clumsy code, try to write the messages without using
     concatenation tricks, to preserve their meaning in the po files. */

  if (status == NULL)
    return g_strdup_printf (_("%s status is unavailable... is HAL installed and hald running ?"),
                     drive_name);
  
  if (cddrive_status_is_empty (status))
    {
      /* no CD title in the message */
    
      if (cddrive_status_is_ejectable (status))
        {
          if (cddrive_status_is_open (status))
            /* translation note: "Close <drive name>" */
            return g_strdup_printf (_("Close %s"), drive_name);
          
          /* translation note: "Open <drive name>" */
          return g_strdup_printf (_("Open %s"), drive_name);
        }
              
      /* drive is not ejectable */
      
      /* translation note: "No disc in <drive name>" */
      return g_strdup_printf (_("No disc in %s"), drive_name);
    }
  
  /* There is a disc in the drive : use title whenever it is available.
     Otherwise, use disc type. */
  
  title = cddrive_status_get_title (status);
  type  = cddrive_status_get_type (status);
  
  if (cddrive_status_is_ejectable (status))
    {
      if (cddrive_status_is_blank (status))
        /* translation note: "Eject blank <disc type> from <drive name>" (e.g. "Eject blank cd-rw from cdrom1") */
        return g_strdup_printf (_("Eject blank %s from %s"), type, drive_name);
      
      if (title != NULL)
        /* translation note: "Eject \"<disc title>\" from <drive name>" */
        return g_strdup_printf (_("Eject \"%s\" from %s"), title, drive_name);

      if (cddrive_status_is_audio (status))
        /* translation note: "Eject audio <disc type> from <drive name>" */
        return g_strdup_printf (_("Eject audio %s from %s"), type, drive_name);
        
      /* translation note: "Eject <disc type> from <drive name>" (e.g. "Eject dvd from my-dvd-drive") */
      return g_strdup_printf (_("Eject %s from %s"), type, drive_name);
    }
  
  /* unejectable drive with disc inside */
    
  if (cddrive_status_is_blank (status))
    /* translation note: "Blank <disc type> in <drive name>" (e.g. "Blank cd-rw in cdrom1") */
    return g_strdup_printf (_("Blank %s in %s"), type, drive_name);
      
  if (title != NULL)
    /* translation note: "\"<disc title>\" in <drive name>" (e.g. ""Backup #36" in cdrom1") */
    return g_strdup_printf (_("\"%s\" in %s"), title, drive_name);

  if (cddrive_status_is_audio (status))
    /* translation note: "Audio <disc type> in <drive name>" */
    return g_strdup_printf (_("Audio %s in %s"), type, drive_name);
        
  /* translation note: "<disc type> in <drive name>" (e.g. "dvd in cdrom1") */
  return g_strdup_printf (_("%s in %s"), type, drive_name);
}



/* note: 'status' can be NULL, to set an "unknown status" message */
static void
cddrive_update_tip_from_status (CddrivePlugin *cddrive, CddriveStatus *status)
{
  gchar *txt;

  gtk_tooltips_disable (cddrive->tips);

  if (cddrive->use_name_in_tip && cddrive->name != NULL)
    txt = cddrive_get_named_tip_text_from_status (status, cddrive->name);
  else
    txt = cddrive_get_unnamed_tip_text_from_status (status);
        
  gtk_tooltips_set_tip (cddrive->tips,
                        cddrive->ebox,
                        txt,
                        NULL);
  
  g_free (txt);
  gtk_tooltips_enable (cddrive->tips);
}



void
cddrive_update_tip (CddrivePlugin *cddrive)
{
  CddriveStatus *stat;
  GError        *error = NULL;

  stat = cddrive_status_new (cddrive->monitor, &error);
  cddrive_update_tip_from_status (cddrive, stat);
  
  if (stat != NULL)
    cddrive_status_free (stat);

  if (error != NULL)
    {
      cddrive_show_error (error);
      cddrive_clear_error (&error);
    }
}



void
on_menu_item_activated (GtkMenuItem *menu_item, CddrivePlugin *cddrive)
{
  CddriveStatus *stat;
  GError        *err = NULL;

  g_assert (cddrive->monitor != NULL);

  stat = cddrive_status_new (cddrive->monitor, &err);
  if (stat != NULL)
    {
      if (cddrive_status_is_mounted (stat))
        cddrive_monitor_unmount (cddrive->monitor, &err);
      else
        cddrive_monitor_mount (cddrive->monitor, &err);
    }

  if (err != NULL)
    {
      cddrive_show_error (err);
      cddrive_clear_error (&err);
    }
}



/* note: 'status' can be NULL, which hides menu_item. */
void
cddrive_update_menu_from_status (CddrivePlugin *cddrive, CddriveStatus *status)
{
  GtkLabel    *label;
  gchar       *txt;
  const gchar *title;

  if (cddrive->menu_item == NULL)
    /* mount and unmount are both unavailable command */
    return;

  xfce_panel_plugin_block_menu (cddrive->plugin);

  if (status == NULL ||
      cddrive_status_is_empty (status) ||
      cddrive_status_is_blank (status) ||
      cddrive_status_is_audio (status))
    gtk_widget_hide (cddrive->menu_item);
  else
    {
      label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (cddrive->menu_item)));
      title = cddrive_status_get_title (status);
      if (title == NULL)
        {
          if (cddrive_status_is_mounted (status))
            gtk_label_set_text (label, _("Unmount disc"));
          else
            gtk_label_set_text (label, _("Mount disc"));
        }
      else
        {
          if (cddrive_status_is_mounted (status))
            txt = g_strdup_printf (_("Unmount \"%s\""), title);
          else
            txt = g_strdup_printf (_("Mount \"%s\""), title);
          
          gtk_label_set_text (label, txt);
          g_free (txt);
        }
      
      gtk_widget_show (cddrive->menu_item);
    }
  
  xfce_panel_plugin_unblock_menu (cddrive->plugin);
}



void
cddrive_update_menu (CddrivePlugin *cddrive)
{
  CddriveStatus *stat;
  GError        *error = NULL;

  stat = cddrive_status_new (cddrive->monitor, &error);
  cddrive_update_menu_from_status (cddrive, stat);
  if (stat != NULL)
    cddrive_status_free (stat);

  if (error != NULL)
    {
      cddrive_show_error (error);
      cddrive_clear_error (&error);
    }
}



/* Set the icon, tip and menu according to CD-ROM drive status. */
static void
cddrive_update_interface (CddrivePlugin *cddrive)
{
  CddriveStatus *stat;
  GError        *error = NULL;

  stat = cddrive_status_new (cddrive->monitor, &error);
  cddrive_update_icon_from_status (cddrive, stat);
  cddrive_update_tip_from_status (cddrive, stat);
  cddrive_update_menu_from_status (cddrive, stat);

  if (stat != NULL)
    cddrive_status_free (stat);
  
  if (error != NULL)
    {
      cddrive_show_error (error);
      cddrive_clear_error (&error);
    }
}



/* Return the first default fallback command found in user's path, NULL otherwise. */
static const gchar*
cddrive_get_default_fallback_command (CddriveCommandType command_type)
{
  gchar  *p = NULL;
  gchar* *cmd_args;
  gint    i;

  for (i=0; cddrive_default_fallback_commands [command_type][i] != NULL; i++)
    {
      /* extract command name and check if it is in user's path */
      cmd_args = g_strsplit (cddrive_default_fallback_commands [command_type][i], " ", 2);
      p = g_find_program_in_path (cmd_args [0]);
      g_strfreev (cmd_args);
    
      if (p != NULL)
        {
          g_free (p);
          break;
        }
    }
  
  return cddrive_default_fallback_commands [command_type][i];
}



static gchar*
cddrive_get_filtered_command (const gchar *command)
{
  gchar *res;

  if (command == NULL)
    return NULL;

  res = g_strdup (command);
  g_strstrip (res);
  
  if (g_utf8_strlen (res, 1) == 0)
    {
      g_free (res);
      return NULL;
    }
  
  return res;
}



void
cddrive_set_mount_fallback (CddrivePlugin *cddrive,
                            const gchar   *value)
{
  g_free (cddrive->mount_fallback);
  cddrive->mount_fallback = cddrive_get_filtered_command (value);
}



void
cddrive_set_unmount_fallback (CddrivePlugin *cddrive,
                              const gchar   *value)
{
  g_free (cddrive->unmount_fallback);
  cddrive->unmount_fallback = cddrive_get_filtered_command (value);
}



static void
cddrive_free_settings (CddrivePlugin *cddrive)
{
  g_free (cddrive->device);
  g_free (cddrive->mount_fallback);
  g_free (cddrive->unmount_fallback);
  g_free (cddrive->name);
  if (cddrive->mounted_color != NULL)
    gdk_color_free (cddrive->mounted_color);
  if (cddrive->unmounted_color != NULL)
    gdk_color_free (cddrive->unmounted_color);
}



static void
cddrive_free (XfcePanelPlugin *plugin,
              CddrivePlugin   *cddrive)
{
  GtkWidget *dialog;

  /* check if the dialog is still open. if so, destroy it */
  dialog = g_object_get_data (G_OBJECT (plugin), "dialog");
  if (G_UNLIKELY (dialog != NULL))
    gtk_widget_destroy (dialog);

  /* destroy the panel widgets */
  gtk_widget_destroy (cddrive->ebox);

  /* cleanup the settings */
  cddrive_free_settings (cddrive);
  
  /* free internals */
  if (G_LIKELY (cddrive->monitor != NULL))
    cddrive_monitor_free (cddrive->monitor);
  
  cddrive_audio_free_globals ();
  cddrive_error_free_globals ();

  /* free the plugin structure */
  panel_slice_free (CddrivePlugin, cddrive);
}



static GdkColor*
cddrive_rc_copy_fallback_color (const GdkColor *fallback)
{
  if (fallback == NULL)
    return NULL;
  
  return gdk_color_copy (fallback);
}



/* The result must be freed using 'gdk_color_free()'. */
static GdkColor*
cddrive_rc_read_color_entry (const XfceRc   *rc,
                             const gchar    *key,
                             const GdkColor *fallback)
{
  /* To be sure the result can be freed with 'gdk_color_free()', this function
     only return colors provided by 'gdk_color_copy()'.
  
     GDK API only state that 'gdk_color_free()' is used to free colors which
     are set with 'gdk_color_copy()', nothing else.
     (No, read 'gdk_color_free()' code is not the answer, because the implementation
     could change without contradicting the current API). */

  GdkColor *res, res_src;
  gchar    *v;

  g_assert (rc != NULL);
  g_assert (key != NULL);

  v = g_strdup (xfce_rc_read_entry (rc, key, NULL));
  if (v == NULL)
    return cddrive_rc_copy_fallback_color (fallback);
  
  if (! gdk_colormap_alloc_color (gdk_colormap_get_system(), &res_src, TRUE, TRUE))
    {
      g_warning ("Failed to allocate color for config key '%s'. Using fallback color.",
                 key);
      g_free (v);
      return cddrive_rc_copy_fallback_color (fallback);
    }
  
  if (! gdk_color_parse (v, &res_src))
    {
      g_warning ("Failed to parse color from '%s' key's value '%s'. Using fallback color.",
                 key,
                 v);
      gdk_colormap_free_colors (gdk_colormap_get_system(), &res_src, 1);
      g_free (v);
      return cddrive_rc_copy_fallback_color (fallback);
    }
  
  g_free (v);
  res = gdk_color_copy (&res_src);
  gdk_colormap_free_colors (gdk_colormap_get_system(), &res_src, 1);
  
  return res;
}



static void
cddrive_rc_write_color_entry (XfceRc         *rc,
                              const gchar    *key,
                              const GdkColor *color)
{
  gchar v[8];

  g_assert (rc != NULL);
  g_assert (key != NULL);
  g_assert (color != NULL);

  /* I don't know if a string with 16 bits colors would be accepted by
     gdk_color_parse(). I don't care. This is a mere plugin, not the gimp. */
  g_snprintf(v, 8, "#%02X%02X%02X",
             (guint)(color->red >> 8),
             (guint)(color->green >> 8),
             (guint)(color->blue >> 8));
  xfce_rc_write_entry (rc, key, v);
}



void
cddrive_save (XfcePanelPlugin *plugin,
              CddrivePlugin   *cddrive)
{
  XfceRc *rc;
  gchar  *rc_path;

  /* get the config file location */
  rc_path = xfce_panel_plugin_save_location (plugin, TRUE);

  if (G_UNLIKELY (rc_path == NULL))
    {
       g_warning ("failed to open config file");
       return;
    }

  /* open the config file, read/write */
  rc = xfce_rc_simple_open (rc_path, FALSE);
  g_free (rc_path);

  if (G_LIKELY (rc != NULL))
    {
      /* save the settings */
      if (cddrive->device == NULL)
        xfce_rc_delete_entry (rc, CDDRIVE_CONF_ID_DEVICE, FALSE);
      else
        xfce_rc_write_entry (rc, CDDRIVE_CONF_ID_DEVICE, cddrive->device);
      
      if (cddrive->mount_fallback == NULL)
        xfce_rc_write_entry (rc, CDDRIVE_CONF_ID_MOUNT_FALLBACK, "");
      else
        xfce_rc_write_entry (rc,
                             CDDRIVE_CONF_ID_MOUNT_FALLBACK,
                             cddrive->mount_fallback);

       if (cddrive->unmount_fallback == NULL)
        xfce_rc_write_entry (rc, CDDRIVE_CONF_ID_UNMOUNT_FALLBACK, "");
      else
        xfce_rc_write_entry (rc,
                             CDDRIVE_CONF_ID_UNMOUNT_FALLBACK,
                             cddrive->unmount_fallback);

     if (cddrive->name == NULL)
        xfce_rc_delete_entry (rc, CDDRIVE_CONF_ID_NAME, FALSE);
      else
        xfce_rc_write_entry (rc, CDDRIVE_CONF_ID_NAME, cddrive->name);

      xfce_rc_write_bool_entry (rc, CDDRIVE_CONF_ID_USE_NAME_IN_PANEL,
                                cddrive->use_name_in_panel);
      xfce_rc_write_bool_entry (rc, CDDRIVE_CONF_ID_USE_NAME_IN_TIP,
                                cddrive->use_name_in_tip);
      
      xfce_rc_write_bool_entry (rc, CDDRIVE_CONF_ID_USE_MOUNTED_COLOR,
                                cddrive->use_mounted_color);
      if (cddrive->mounted_color != NULL)
        cddrive_rc_write_color_entry (rc,
                                      CDDRIVE_CONF_ID_MOUNTED_COLOR,
                                      cddrive->mounted_color);
      
      xfce_rc_write_bool_entry (rc, CDDRIVE_CONF_ID_USE_UNMOUNTED_COLOR,
                                cddrive->use_unmounted_color);
      if (cddrive->unmounted_color != NULL)
        cddrive_rc_write_color_entry (rc,
                                      CDDRIVE_CONF_ID_UNMOUNTED_COLOR,
                                      cddrive->unmounted_color);
      
      xfce_rc_write_bool_entry (rc, CDDRIVE_CONF_ID_USE_TRANSLUCENCY,
                                cddrive->use_translucency);
      xfce_rc_write_int_entry (rc, CDDRIVE_CONF_ID_TRANSLUCENCY,
                               cddrive->translucency);

      xfce_rc_write_bool_entry (rc, CDDRIVE_CONF_ID_USE_CDDB,
                                cddrive->use_cddb);
      
      /* close the rc file */
      xfce_rc_close (rc);
    }
}



static void
cddrive_read (CddrivePlugin *cddrive)
{
  XfceRc *rc;
  gchar  *rc_path;

  cddrive_free_settings (cddrive);

  /* get the plugin config file location */
  rc_path = xfce_panel_plugin_save_location (cddrive->plugin, TRUE);

  if (G_LIKELY (rc_path != NULL))
    {
      /* open the config file, readonly */
      rc = xfce_rc_simple_open (rc_path, TRUE);

      /* cleanup */
      g_free (rc_path);

      if (G_LIKELY (rc != NULL))
        {
          /* read the settings */          
          cddrive->device               = g_strdup (xfce_rc_read_entry (rc,
                                                    CDDRIVE_CONF_ID_DEVICE,
                                                    CDDRIVE_DEFAULT_DEVICE));
        
          cddrive->name                 = g_strdup (xfce_rc_read_entry (rc,
                                                    CDDRIVE_CONF_ID_NAME,
                                                    CDDRIVE_DEFAULT_NAME));
        
          cddrive_set_mount_fallback (cddrive,
                                      xfce_rc_read_entry (rc,
                                                          CDDRIVE_CONF_ID_MOUNT_FALLBACK,
                                                          cddrive_get_default_fallback_command (CDDRIVE_MOUNT)));
                                      
          cddrive_set_unmount_fallback (cddrive,
                                        xfce_rc_read_entry (rc,
                                                            CDDRIVE_CONF_ID_UNMOUNT_FALLBACK,
                                                            cddrive_get_default_fallback_command (CDDRIVE_UNMOUNT)));
                                      
          cddrive->use_name_in_panel    = xfce_rc_read_bool_entry (rc,
                                              CDDRIVE_CONF_ID_USE_NAME_IN_PANEL,
                                              CDDRIVE_DEFAULT_USE_NAME_IN_PANEL);
        
          cddrive->use_name_in_tip      = xfce_rc_read_bool_entry (rc,
                                              CDDRIVE_CONF_ID_USE_NAME_IN_TIP,
                                              CDDRIVE_DEFAULT_USE_NAME_IN_TIP);
        
          cddrive->use_mounted_color    = xfce_rc_read_bool_entry (rc,
                                              CDDRIVE_CONF_ID_USE_MOUNTED_COLOR,
                                              CDDRIVE_DEFAULT_USE_MOUNTED_COLOR);
        
          cddrive->mounted_color        = cddrive_rc_read_color_entry (rc,
                                              CDDRIVE_CONF_ID_MOUNTED_COLOR,
                                              CDDRIVE_DEFAULT_COLOR);
                                          
          cddrive->use_unmounted_color  = xfce_rc_read_bool_entry (rc,
                                              CDDRIVE_CONF_ID_USE_UNMOUNTED_COLOR,
                                              CDDRIVE_DEFAULT_USE_UNMOUNTED_COLOR);
        
          cddrive->unmounted_color      = cddrive_rc_read_color_entry (rc,
                                              CDDRIVE_CONF_ID_UNMOUNTED_COLOR,
                                              CDDRIVE_DEFAULT_COLOR);
                                          
          cddrive->use_translucency     = xfce_rc_read_bool_entry (rc,
                                              CDDRIVE_CONF_ID_USE_TRANSLUCENCY,
                                              CDDRIVE_DEFAULT_USE_TRANSLUCENCY);
                                          
          cddrive->translucency         = xfce_rc_read_int_entry (rc,
                                              CDDRIVE_CONF_ID_TRANSLUCENCY,
                                              CDDRIVE_DEFAULT_TRANSLUCENCY);
          if (cddrive->translucency < 0)
            cddrive->translucency = 0;
          if (cddrive->translucency > 100)
            cddrive->translucency = 100;
                                          
          cddrive->use_cddb             = xfce_rc_read_bool_entry (rc,
                                              CDDRIVE_CONF_ID_USE_CDDB,
                                              CDDRIVE_DEFAULT_USE_CDDB);

          /* cleanup */
          xfce_rc_close (rc);

          /* leave the function, everything went well */
          return;
        }
    }

  /* something went wrong, apply default values */
  g_warning ("failed to open config file. Applying default settings.");
  cddrive->device               = g_strdup (CDDRIVE_DEFAULT_DEVICE);
  cddrive->name                 = g_strdup (CDDRIVE_DEFAULT_NAME);
  cddrive->use_name_in_panel    = CDDRIVE_DEFAULT_USE_NAME_IN_PANEL;
  cddrive->use_name_in_tip      = CDDRIVE_DEFAULT_USE_NAME_IN_TIP;
  cddrive->use_mounted_color    = CDDRIVE_DEFAULT_USE_MOUNTED_COLOR;
  cddrive->mounted_color        = cddrive_rc_copy_fallback_color (CDDRIVE_DEFAULT_COLOR);
  cddrive->use_unmounted_color  = CDDRIVE_DEFAULT_USE_UNMOUNTED_COLOR;
  cddrive->unmounted_color      = cddrive_rc_copy_fallback_color (CDDRIVE_DEFAULT_COLOR);
  cddrive->use_translucency     = CDDRIVE_DEFAULT_USE_TRANSLUCENCY;
  cddrive->translucency         = CDDRIVE_DEFAULT_TRANSLUCENCY;
  cddrive->use_cddb             = CDDRIVE_DEFAULT_USE_CDDB;
  
  cddrive_set_mount_fallback (cddrive,
                              cddrive_get_default_fallback_command (CDDRIVE_MOUNT));
  cddrive_set_unmount_fallback (cddrive,
                                cddrive_get_default_fallback_command (CDDRIVE_UNMOUNT));
}



void
cddrive_update_label (CddrivePlugin *cddrive)
{
  if (cddrive->use_name_in_panel && cddrive->name != NULL)
    {
      if (cddrive->label == NULL)
        {
          cddrive->label = gtk_label_new (cddrive->name);
          gtk_box_pack_end (GTK_BOX (cddrive->hvbox),
                            cddrive->label,
                            FALSE, FALSE, 0);
        }
      else
        gtk_label_set_text (GTK_LABEL (cddrive->label), cddrive->name);
    }
  else
    {
      if (cddrive->label != NULL)
        {
          gtk_widget_destroy (cddrive->label);
          cddrive->label = NULL;
        }
    }
}



static gboolean
cddrive_on_enter (GtkWidget        *hvbox,
                  GdkEventCrossing *event,
                  CddrivePlugin    *cddrive)
{
  cddrive_update_tip (cddrive);
  return FALSE;
}



static void
cddrive_on_click (GtkButton     *button,
                  CddrivePlugin *cddrive)
{
  CddriveStatus *s;
  GError        *err = NULL;

  /* to avoid spurious clicks while waiting the completion of the operation. */
  gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);

  if (G_LIKELY (cddrive->monitor != NULL))
    {
      s = cddrive_status_new (cddrive->monitor, &err);
      if (G_LIKELY (s != NULL ))
        {
          if (cddrive_status_is_open (s))
            /* close the tray */
            cddrive_monitor_close (cddrive->monitor, &err);
          else
            {
              if (cddrive_status_is_mounted (s))
                /* try to unmount the disc */
                cddrive_monitor_unmount (cddrive->monitor, &err);        

              if (G_LIKELY (err == NULL))
                /* open the tray */
                cddrive_monitor_eject (cddrive->monitor, &err);
            }
      
          cddrive_status_free (s);
      
          if (G_LIKELY (err == NULL))
            {
              /* force update, to keep the button pushed down while waiting for
                 HAL response. */
              cddrive_update_monitor (cddrive);
        
              /* show the new status after HAL response. */
              gtk_widget_show_all (cddrive->ebox);
            }
        }
  
      if (G_UNLIKELY (err != NULL))
        {    
          cddrive_show_error (err);
          cddrive_clear_error (&err);
        }
    }
  
  /* re-enable the button disabled at the beginning. */
  gtk_widget_set_sensitive (GTK_WIDGET (button), TRUE);
}



static void
cddrive_on_disc_change (CddriveMonitor *monitor,
                        CddrivePlugin  *cddrive)
{
  cddrive_update_interface (cddrive);
  gtk_widget_show (cddrive->image);
}



static void
cddrive_on_disc_inserted (CddriveMonitor *monitor,
                          gpointer        callback_data)
{
  cddrive_on_disc_change (monitor, (CddrivePlugin*) callback_data);
}



static void
cddrive_on_disc_removed (CddriveMonitor *monitor,
                         gpointer        callback_data)
{
  cddrive_on_disc_change (monitor, (CddrivePlugin*) callback_data);
}



static void
cddrive_on_disc_modified (CddriveMonitor *monitor,
                          gpointer        callback_data)
{
  cddrive_on_disc_change (monitor, (CddrivePlugin*) callback_data);
}



static void
on_icon_theme_changed (GtkIconTheme *icon_theme,
                       CddrivePlugin *cddrive)
{
  cddrive_update_icon (cddrive);
}



void
cddrive_update_monitor (CddrivePlugin *cddrive)
{
  GError *error = NULL;

  if (cddrive->monitor != NULL)
    cddrive_monitor_free (cddrive->monitor);

  if (cddrive->device == NULL)
    cddrive->monitor = NULL;
  else
    {
      cddrive->monitor = cddrive_monitor_new (cddrive->device,
                                              cddrive,
                                              cddrive_on_disc_inserted,
                                              cddrive_on_disc_removed,
                                              cddrive_on_disc_modified,
                                              cddrive->mount_fallback,
                                              cddrive->unmount_fallback,
                                              cddrive->use_cddb,
                                              &error);
      if (cddrive->monitor == NULL)
        {
          cddrive_show_error (error);
          cddrive_clear_error (&error);
        }
    }
    
  /* set the icon and menu according to CD-ROM drive status */
  cddrive_update_interface (cddrive);
}



static CddrivePlugin *
cddrive_new (XfcePanelPlugin *plugin)
{
  CddrivePlugin *res;

  cddrive_error_init_globals ();
  cddrive_audio_init_globals ();

  res = panel_slice_new0 (CddrivePlugin);

  res->plugin = plugin;
  xfce_panel_plugin_set_expand (res->plugin, FALSE);
 
  /* create plugin widgets */
  res->ebox = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (plugin), res->ebox);
  res->hvbox = xfce_hvbox_new (xfce_panel_plugin_get_orientation (plugin),
                               FALSE, 0);
  gtk_container_add (GTK_CONTAINER (res->ebox), res->hvbox);

  res->button = xfce_create_panel_button ();
  gtk_box_pack_end (GTK_BOX (res->hvbox), res->button, FALSE, FALSE, 0);

  res->image = gtk_image_new ();
  gtk_button_set_image (GTK_BUTTON (res->button), res->image);

  res->tips = gtk_tooltips_new ();
  
  /* set the panel's right-click menu */
  xfce_panel_plugin_add_action_widget (plugin, res->ebox);
  xfce_panel_plugin_add_action_widget (plugin, res->button);
  xfce_panel_plugin_menu_show_configure (plugin);
  xfce_panel_plugin_menu_show_about (plugin);
  
  /* create and insert the mount/unmount item */
  res->menu_item = gtk_menu_item_new_with_label (""); /* empty text to create item's label widget */
  xfce_panel_plugin_menu_insert_item (plugin,
                                      GTK_MENU_ITEM (res->menu_item));
  
  /* update from user settings */
  cddrive_read (res);
  cddrive_update_monitor (res);
  cddrive_update_label (res);

  return res;
}



static void
cddrive_orientation_changed (XfcePanelPlugin *plugin,
                             GtkOrientation   orientation,
                             CddrivePlugin   *cddrive)
{
  xfce_hvbox_set_orientation (XFCE_HVBOX (cddrive->hvbox), orientation);
}



static gboolean
cddrive_size_changed (XfcePanelPlugin *plugin,
                      gint             size,
                      CddrivePlugin   *cddrive)
{
  GtkOrientation o;

  o = xfce_panel_plugin_get_orientation (plugin);

  if (o == GTK_ORIENTATION_HORIZONTAL)
    gtk_widget_set_size_request (GTK_WIDGET (plugin), -1, size);
  else
    gtk_widget_set_size_request (GTK_WIDGET (plugin), size, -1);

  cddrive_update_icon (cddrive);

  return TRUE;
}



static void
cddrive_construct (XfcePanelPlugin *plugin)
{
  CddrivePlugin *cddrive;

  /* setup transation domain */
  xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  /* create the plugin */
  cddrive = cddrive_new (plugin);

  /* show the plugin widgets */
  gtk_widget_show_all (cddrive->ebox);

  /* as we cannot be notified of the open/close tray event, we update the tooltip
     each time the mouse pointer enters the plugin */
  g_signal_connect (cddrive->ebox,
                    "enter-notify-event",
                    G_CALLBACK (cddrive_on_enter), 
                    cddrive);
                    
  /* to open or close the drive */
  g_signal_connect (cddrive->button,
                    "clicked",
                    G_CALLBACK (cddrive_on_click),
                    cddrive);
      
  /* connect plugin signals */
  g_signal_connect (G_OBJECT (plugin), "free-data",
                    G_CALLBACK (cddrive_free),
                    cddrive);

  g_signal_connect (G_OBJECT (plugin), "save",
                    G_CALLBACK (cddrive_save),
                    cddrive);

  g_signal_connect (G_OBJECT (plugin), "size-changed",
                    G_CALLBACK (cddrive_size_changed),
                    cddrive);

  g_signal_connect (G_OBJECT (plugin), "orientation-changed",
                    G_CALLBACK (cddrive_orientation_changed),
                    cddrive);

  /* connect configure and about menu items */
  g_signal_connect (G_OBJECT (plugin), "configure-plugin",
                    G_CALLBACK (cddrive_configure),
                    cddrive);
  g_signal_connect (G_OBJECT (plugin), "about",
                    G_CALLBACK (cddrive_about),
                    NULL);

  /* connect the mount/unmount menu item */
  g_signal_connect (cddrive->menu_item,
                    "activate",
                    G_CALLBACK (on_menu_item_activated),
                    cddrive);
                    
  /* to handle icon theme change */
  g_signal_connect (gtk_icon_theme_get_default (),
                    "changed",
                    G_CALLBACK (on_icon_theme_changed),
                    cddrive);
}


/* register the plugin */
XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL (cddrive_construct)
