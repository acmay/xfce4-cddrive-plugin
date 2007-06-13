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
#include "cddrive-error.h"
#include "cddrive.h"
#include "cddrive-dialogs.h"
#include "cddrive-monitor.h"


#define CDDRIVE_TITLE            _("CD Drive Monitor")
#define CDDRIVE_VERSION          "0.0.1"
#define CDDRIVE_COPYRIGHT_YEARS  "2007"
#define CDDRIVE_COPYRIGHT_OWNERS "The Xfce development team. All rights reserved."

#define CDDRIVE_SECTION_PADDING             5
#define CDDRIVE_SECTION_VBOX_SPACING        5
#define CDDRIVE_SECTION_VBOX_PADDING        5
#define CDDRIVE_VBOX_SPACING                5
#define CDDRIVE_VBOX_PADDING                5
#define CDDRIVE_NAME_TOGGLES_LEFT_PADDING   20
#define CDDRIVE_TABLE_SETTINGS  GTK_SHRINK|GTK_FILL, GTK_SHRINK|GTK_FILL, 5, 5

#define CDDRIVE_NAME_ENTRY_MAX_LEN    10
#define CDDRIVE_COMMAND_ENTRY_MAX_LEN 300 /* hope it is enough to store a command line */

#define CDDRIVE_DRIVE_INFOS_ID            "drv_infos"
#define CDDRIVE_TOGGLE_BUTTON_WIDGET_ID   "wid"
#define CDDRIVE_MOUNT_ENTRY_ID            "mount_entry"
#define CDDRIVE_UNMOUNT_ENTRY_ID          "unmount_entry"



/* Read fallback command from corresponding entry,
  and clear the data pointing to the entry */
static void
cddrive_configure_pick_fallback_command (CddrivePlugin      *cddrive,
                                         CddriveCommandType  command_type)
{
  gchar     *eid;
  GtkEntry  *e;

  eid = (command_type == CDDRIVE_MOUNT) ? CDDRIVE_MOUNT_ENTRY_ID : CDDRIVE_UNMOUNT_ENTRY_ID;
  e = GTK_ENTRY (g_object_get_data (G_OBJECT (cddrive->plugin), eid));
  
  if (command_type == CDDRIVE_MOUNT)
    cddrive_set_mount_fallback (cddrive, gtk_entry_get_text (e));
  else
    cddrive_set_unmount_fallback (cddrive, gtk_entry_get_text (e));

  g_object_set_data (G_OBJECT (cddrive->plugin), eid, NULL);
}



static void
cddrive_configure_response (GtkWidget     *dialog,
                            gint           response,
                            CddrivePlugin *cddrive)
{
  CddriveDriveInfo* *nfo;

  /* retrieve and free drives infos list */
  nfo = (CddriveDriveInfo**) g_object_get_data (G_OBJECT (cddrive->plugin),
                                                CDDRIVE_DRIVE_INFOS_ID);
  if (nfo != NULL)
    cddrive_cdrom_drive_infos_free (nfo);
  
  /* set mount and unmount commands options */
  cddrive_configure_pick_fallback_command (cddrive, CDDRIVE_MOUNT);
  cddrive_configure_pick_fallback_command (cddrive, CDDRIVE_UNMOUNT);
  
  /* end dialog */
  g_object_set_data (G_OBJECT (cddrive->plugin), CDDRIVE_DRIVE_INFOS_ID, NULL);
  g_object_set_data (G_OBJECT (cddrive->plugin), "dialog", NULL);
  gtk_widget_destroy (dialog);
  xfce_panel_plugin_unblock_menu (cddrive->plugin);

  cddrive_save (cddrive->plugin, cddrive);
  cddrive_update_monitor (cddrive);
}



static void
cddrive_toggle_button_set_widget (GtkToggleButton *button, GtkWidget *widget)
{
  g_object_set_data (G_OBJECT (button),
                     CDDRIVE_TOGGLE_BUTTON_WIDGET_ID,
                     widget);
  gtk_widget_set_sensitive (widget, gtk_toggle_button_get_active (button));
}



static GtkWidget*
cddrive_toggle_button_get_widget (GtkToggleButton *button)
{
  return GTK_WIDGET (g_object_get_data (G_OBJECT (button),
                                        CDDRIVE_TOGGLE_BUTTON_WIDGET_ID));
}



/* Enable/disable the widget associated with a toggle button */
static void
cddrive_toggle_button_toggle_widget (GtkToggleButton *button)
{
  gtk_widget_set_sensitive (cddrive_toggle_button_get_widget (button),
                            gtk_toggle_button_get_active (button));
}



static void
cddrive_configure_change_device (GtkComboBox   *device_combo_box,
                                 CddrivePlugin *cddrive)
{
  CddriveDriveInfo* *drv_infos;
  gint              i;

  /* retrieve drives infos list */
  drv_infos = (CddriveDriveInfo**) g_object_get_data (G_OBJECT (cddrive->plugin),
                                                      CDDRIVE_DRIVE_INFOS_ID);
  i = gtk_combo_box_get_active (device_combo_box);

  g_free (cddrive->device);
  
  cddrive->device = g_strdup (drv_infos [i]->device);
  cddrive_update_monitor (cddrive);
  gtk_widget_show (cddrive->image);
}



static void
cddrive_configure_change_name (GtkEntry      *name_entry,
                               CddrivePlugin *cddrive)
{
  
  if (cddrive->name != NULL)
    g_free (cddrive->name);
  
  cddrive->name = g_strdup (gtk_entry_get_text (GTK_ENTRY (name_entry)));
  cddrive_update_label (cddrive);
  if (cddrive->label != NULL)
    gtk_widget_show (cddrive->label);
}



static void
cddrive_configure_toggle_panel_name (GtkButton     *toggle,
                                     CddrivePlugin *cddrive)
{
  GtkWidget *name_entry = cddrive_toggle_button_get_widget (GTK_TOGGLE_BUTTON (toggle));

  cddrive->use_name_in_panel = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle));
  gtk_widget_set_sensitive (name_entry,
                            cddrive->use_name_in_panel || cddrive->use_name_in_tip);

  cddrive_update_label (cddrive);
  if (cddrive->label != NULL)
    gtk_widget_show (cddrive->label);
}



static void
cddrive_configure_toggle_tip_name (GtkButton     *toggle,
                                   CddrivePlugin *cddrive)
{
  GtkWidget *name_entry = cddrive_toggle_button_get_widget (GTK_TOGGLE_BUTTON (toggle));

  cddrive->use_name_in_tip = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle));
  gtk_widget_set_sensitive (name_entry,
                            cddrive->use_name_in_panel || cddrive->use_name_in_tip);
  cddrive_update_tip (cddrive);
}



static void
cddrive_configure_toggle_mounted_color (GtkButton     *toggle,
                                        CddrivePlugin *cddrive)
{
  cddrive->use_mounted_color = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle));
  cddrive_toggle_button_toggle_widget (GTK_TOGGLE_BUTTON (toggle));
  cddrive_update_icon (cddrive);
}



static void
cddrive_configure_toggle_unmounted_color (GtkButton     *toggle,
                                          CddrivePlugin *cddrive)
{
  cddrive->use_unmounted_color = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle));
  cddrive_toggle_button_toggle_widget (GTK_TOGGLE_BUTTON (toggle));
  cddrive_update_icon (cddrive);
}



static GdkColor*
cddrive_configure_get_color_copy (GtkColorButton  *button)
{
  GdkColor *res, c;

  gtk_color_button_get_color (button, &c);
  
  /* to be sure 'color_to_update' can later be freed with 'gdk_color_free()'.
  
     May be 'color_to_update' can be set directly with 'gtk_color_button_get_color()',
     but GDK API only state that 'gdk_color_free()' is used to free colors which
     are set with 'gdk_color_copy()'.
     As, in the plugin, 'gdk_color_free()' must be used to free colors which are
     set from the config file, 'color_to_update' is here set as a copy to avoid
     any problem in other parts of the plugin. */
  res = gdk_color_copy (&c);
  
  /* again no clear info about the color map used by 'gtk_color_button_get_color ()',
     and how the color set with 'gtk_color_button_get_color' should be freed. */
  gdk_colormap_free_colors (gdk_colormap_get_system(), &c, 1);

  return res;
}



static void
cddrive_configure_change_mounted_color (GtkColorButton *button,
                                        CddrivePlugin  *cddrive)
{
  if (cddrive->mounted_color != NULL)
    gdk_color_free (cddrive->mounted_color);
  cddrive->mounted_color = cddrive_configure_get_color_copy (button);
  cddrive_update_icon (cddrive);
}



static void
cddrive_configure_change_unmounted_color (GtkColorButton *button,
                                          CddrivePlugin  *cddrive)
{
  if (cddrive->unmounted_color != NULL)
    gdk_color_free (cddrive->unmounted_color);
  cddrive->unmounted_color = cddrive_configure_get_color_copy (button);
  cddrive_update_icon (cddrive);
}



static void
cddrive_configure_toggle_translucency (GtkButton     *toggle,
                                       CddrivePlugin *cddrive)
{
  cddrive->use_translucency = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle));
  cddrive_toggle_button_toggle_widget (GTK_TOGGLE_BUTTON (toggle));
  cddrive_update_icon (cddrive);
}



static void
cddrive_configure_change_translucency (GtkSpinButton *spin_button,
                                       CddrivePlugin *cddrive)
{
  cddrive->translucency = gtk_spin_button_get_value (spin_button);
  cddrive_update_icon (cddrive);
}



static GtkBox*
cddrive_create_section_vbox (CddrivePlugin *cddrive,
                             GtkDialog     *dialog,
                             gchar         *title)
{
  GtkWidget *res, *frm;

  res = gtk_vbox_new (FALSE, CDDRIVE_SECTION_VBOX_SPACING);
  frm = xfce_create_framebox_with_content (title, res);
  gtk_box_pack_start (GTK_BOX (dialog->vbox), frm,
                      FALSE, FALSE, CDDRIVE_SECTION_PADDING);
  return GTK_BOX (res);
}



static void
cddrive_add_fallback_entry (CddrivePlugin      *cddrive,
                            GtkTable           *table,
                            CddriveCommandType  command_type)
{
  GtkWidget   *align, *label, *entry;
  GtkTooltips *tip;
  gchar       *cmd ,*label_txt, *tip_txt, *entry_id, *tmp;
  gint         table_row;

  if (command_type == CDDRIVE_MOUNT)
    {
      cmd       = cddrive->mount_fallback;
      label_txt = _("Mounting");
      tip_txt   = _("Enter a command to use if HAL system fails to mount the disc.");
      entry_id  = CDDRIVE_MOUNT_ENTRY_ID;
      table_row = 0;
    }
  else
    {
      cmd       = cddrive->unmount_fallback;
      label_txt = _("Unmounting");
      tip_txt   = _("Enter a command to use if the HAL system fails to unmount the disc.");
      entry_id  = CDDRIVE_UNMOUNT_ENTRY_ID;
      table_row = 1;
    }

  align = gtk_alignment_new (0, 0.5, 0, 0);
  /*gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 0, 0);*/  
  gtk_table_attach (table, align, 0, 1, table_row, table_row + 1, CDDRIVE_TABLE_SETTINGS);
  label = gtk_label_new (label_txt);
  gtk_container_add (GTK_CONTAINER (align), label);
    
  entry = gtk_entry_new_with_max_length (CDDRIVE_COMMAND_ENTRY_MAX_LEN);
  if (cmd != NULL)
    gtk_entry_set_text (GTK_ENTRY (entry), cmd);
  gtk_table_attach (table, entry, 1, 2, table_row, table_row + 1, CDDRIVE_TABLE_SETTINGS);
  /* to retrieve entry the response callback */
  g_object_set_data (G_OBJECT (cddrive->plugin), entry_id, entry);

  tmp = g_strjoin ("\n\n", tip_txt, _("You can use \"%d\", \"%m\" and \"%u\" \
character sequences as arguments for your command. These will be replaced respectively \
with the device path, the disc mount point and the disc UDI."), NULL);
  tip = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tip, entry, tmp, NULL);
  g_free (tmp);
}



static void
cddrive_add_icon_color_option (CddrivePlugin *cddrive,
                               GtkTable *table,
                               CddriveCommandType status_type)
{
  GtkWidget *tb, *cb;
  gchar     *label_txt;
  gint       table_row;
  gboolean   toggled;
  GdkColor  *color;
  GCallback  toggle_callback, color_callback;

  if (status_type == CDDRIVE_MOUNT)
    {
      label_txt       = _("Mounted disc icon color");
      table_row       = 0;
      toggled         = cddrive->use_mounted_color;
      color           = cddrive->mounted_color;
      toggle_callback = G_CALLBACK (cddrive_configure_toggle_mounted_color);
      color_callback  = G_CALLBACK (cddrive_configure_change_mounted_color);
    }
  else
    {
      label_txt       = _("Unmounted disc icon color");
      table_row       = 1;
      toggled         = cddrive->use_unmounted_color;
      color           = cddrive->unmounted_color;
      toggle_callback = G_CALLBACK (cddrive_configure_toggle_unmounted_color);
      color_callback  = G_CALLBACK (cddrive_configure_change_unmounted_color);
    }
  
  tb = gtk_check_button_new_with_label (label_txt);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tb), toggled);
  gtk_table_attach (table, tb, 0, 1, table_row, table_row + 1,
                    CDDRIVE_TABLE_SETTINGS);
  g_signal_connect (tb, "clicked", toggle_callback, cddrive);
      
  cb = gtk_color_button_new ();
  if (color != NULL)
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cb), color);
  gtk_table_attach (table, cb, 1, 2, table_row, table_row + 1,
                    CDDRIVE_TABLE_SETTINGS);
  g_signal_connect (cb, "color-set", color_callback, cddrive);
  /* to enable/disable the color button when the toggle is selected/deselected */
  cddrive_toggle_button_set_widget (GTK_TOGGLE_BUTTON (tb), cb);
}



void
cddrive_configure (XfcePanelPlugin  *plugin,
                   CddrivePlugin    *cddrive)
{
  GtkWidget         *drv_wid, *hbox, *align, *table,
                    *label, *entry, *button, *spin_button;
  GtkDialog         *dialog;
  GtkBox            *section_vbox, *vbox;
  CddriveDriveInfo* *drv_infos;
  gint               i;
  gchar             *combo_txt;
  GError            *err = NULL;

  xfce_panel_plugin_block_menu (plugin);

  dialog = GTK_DIALOG (xfce_titled_dialog_new_with_buttons (CDDRIVE_TITLE,
                                                NULL,
                                                GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
                                                GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
                                                NULL));

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

  gtk_window_set_icon_name (GTK_WINDOW (dialog), "xfce4-settings");

  /* link the dialog to the plugin, so we can destroy it when the plugin
   * is closed, but the dialog is still open */
  g_object_set_data (G_OBJECT (plugin), CDDRIVE_DRIVE_INFOS_ID, dialog);

  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK(cddrive_configure_response), cddrive);
                    
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 2);
  

  /* --- "Drive" section  --- */
  section_vbox = cddrive_create_section_vbox (cddrive, dialog, _("Drive"));
  
  /* get infos (device path and model) for all detected CD-ROM drives */
  drv_infos = cddrive_cdrom_drive_infos_new (&err);
  
  if (drv_infos == NULL)
    drv_wid = gtk_label_new (_("CD-ROM drive detection failed !"));
    /* 'err' is displayed after the dialog is shown; see at the end of this function */
  else
    {
      if (drv_infos [0] == NULL)
        drv_wid = gtk_label_new (_("No CD-ROM drive detected"));
      else
        {
          /* create the CD-ROM drivers combo box */
          drv_wid = gtk_combo_box_new_text ();
        
          /* to get selected device in change callback and free devices infos
             in response callback */
          g_object_set_data (G_OBJECT (cddrive->plugin),
                             CDDRIVE_DRIVE_INFOS_ID,
                             drv_infos);
       
          /* fill the combo box */
          i = 0;
          while (drv_infos [i] != NULL)
            {
              combo_txt = g_strconcat (drv_infos [i]->model,
                                       " ( ",
                                       drv_infos [i]->device,
                                       " )",
                                       NULL);
              gtk_combo_box_append_text (GTK_COMBO_BOX (drv_wid), combo_txt);
              if (cddrive->device != NULL &&
                  g_str_equal (cddrive->device, drv_infos [i]->device))
                gtk_combo_box_set_active (GTK_COMBO_BOX (drv_wid), i);
              
              i++;
            }
          
          g_signal_connect (drv_wid,
                            "changed",
                            G_CALLBACK (cddrive_configure_change_device), 
                            cddrive);
          
          if (gtk_combo_box_get_active (GTK_COMBO_BOX (drv_wid)) == -1)
            /* None of the driver match the configured device path.
               Select the first one in the list. */
            gtk_combo_box_set_active (GTK_COMBO_BOX (drv_wid), 0);
       }
    }
  gtk_box_pack_start (section_vbox, drv_wid,
                      FALSE, FALSE, CDDRIVE_SECTION_VBOX_PADDING);

  if (drv_infos != NULL && drv_infos [0] != NULL)
    {
      /* if some drives have been detected... */
    

      /* --- "Commands" section --- */    
      section_vbox = cddrive_create_section_vbox (cddrive, dialog, _("Fallback Commands"));
      table = gtk_table_new (2, 2, FALSE);
      gtk_box_pack_start (section_vbox, table,
                          FALSE, FALSE, CDDRIVE_SECTION_VBOX_PADDING);
      cddrive_add_fallback_entry (cddrive, GTK_TABLE (table), CDDRIVE_MOUNT);
      cddrive_add_fallback_entry (cddrive, GTK_TABLE (table), CDDRIVE_UNMOUNT);
    
     
      /* --- "Display" section --- */
    
      /* -- name config -- */
      section_vbox = cddrive_create_section_vbox (cddrive, dialog, _("Display"));
    
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (section_vbox, hbox, FALSE, FALSE, 0);
    
      label = gtk_label_new (_("Name to display"));
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);
    
      entry = gtk_entry_new ();
      if (cddrive->name != NULL)
        gtk_entry_set_text (GTK_ENTRY (entry), cddrive->name);
      gtk_entry_set_max_length (GTK_ENTRY (entry), CDDRIVE_NAME_ENTRY_MAX_LEN);
      
      gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, FALSE, 5);
      g_signal_connect (entry,
                        "changed",
                        G_CALLBACK (cddrive_configure_change_name),
                        cddrive);
                       
      align = gtk_alignment_new (0, 0, 0, 0);
      gtk_alignment_set_padding (GTK_ALIGNMENT (align),
                                 0, 0,
                                 CDDRIVE_NAME_TOGGLES_LEFT_PADDING, 0);
      gtk_box_pack_start (section_vbox, align,
                          FALSE, FALSE, CDDRIVE_VBOX_PADDING);
                          
      vbox = GTK_BOX (gtk_vbox_new (FALSE, CDDRIVE_VBOX_SPACING));
      gtk_container_add (GTK_CONTAINER (align), GTK_WIDGET (vbox));
      
      button = gtk_check_button_new_with_label (_("display in panel"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
                                    cddrive->use_name_in_panel);
      gtk_box_pack_start (vbox, button, FALSE, FALSE, 0);
      g_signal_connect (button,
                        "clicked",
                        G_CALLBACK (cddrive_configure_toggle_panel_name),
                        cddrive);
      /* to enable/disable the name entry when the toggle button is clicked */
      cddrive_toggle_button_set_widget (GTK_TOGGLE_BUTTON (button), entry);
      
      button = gtk_check_button_new_with_label (_("use in tooltip"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
                                    cddrive->use_name_in_tip);
      gtk_box_pack_start (vbox, button, FALSE, FALSE, 0);
      g_signal_connect (button,
                        "clicked",
                        G_CALLBACK (cddrive_configure_toggle_tip_name),
                        cddrive);
      /* to enable/disable the name entry when the toggle button is clicked */
      cddrive_toggle_button_set_widget (GTK_TOGGLE_BUTTON (button), entry);
    
      /* -- mounted/unmounted disc icon config -- */
      table = gtk_table_new (2, 2, FALSE);
      gtk_box_pack_start (section_vbox, table,
                          FALSE, FALSE, CDDRIVE_SECTION_VBOX_PADDING);
                          
      cddrive_add_icon_color_option (cddrive, GTK_TABLE (table), CDDRIVE_MOUNT);
      cddrive_add_icon_color_option (cddrive, GTK_TABLE (table), CDDRIVE_UNMOUNT);
    
      button = gtk_check_button_new_with_label (_("Unmounted disc icon opacity"));
        /* "opacity" is shorter than "translucency", and, i think, more commonly used */
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
                                    cddrive->use_translucency);
      gtk_table_attach (GTK_TABLE (table), button, 0, 1, 2, 3,
                        CDDRIVE_TABLE_SETTINGS);
      g_signal_connect (button,
                        "clicked",
                        G_CALLBACK (cddrive_configure_toggle_translucency),
                        cddrive);
      
      spin_button = gtk_spin_button_new_with_range (0, 100, 1);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_button), cddrive->translucency);
      gtk_table_attach (GTK_TABLE (table), spin_button, 1, 2, 2, 3,
                        CDDRIVE_TABLE_SETTINGS);
      g_signal_connect (spin_button,
                        "value-changed",
                        G_CALLBACK (cddrive_configure_change_translucency),
                        cddrive);
      /* to enable/disable the opacity spin button when the toggle button is clicked */
      cddrive_toggle_button_set_widget (GTK_TOGGLE_BUTTON (button), spin_button);
    }
  
  gtk_widget_show_all (GTK_WIDGET (dialog));
  
  if (err != NULL)
    {
      cddrive_show_error (err);
      cddrive_clear_error (&err); /* useless with _current_ implementation */
    }
}



void
cddrive_about (XfcePanelPlugin *plugin)
{
  XfceAboutInfo *nfo;
  GtkWidget     *dialog;

  nfo = xfce_about_info_new (CDDRIVE_TITLE,
                             CDDRIVE_VERSION,
                             _("CD-ROM drive tray and content control"),
                             XFCE_COPYRIGHT_TEXT (CDDRIVE_COPYRIGHT_YEARS,
                                                  CDDRIVE_COPYRIGHT_OWNERS),
                             XFCE_LICENSE_GPL);
  xfce_about_info_add_credit (nfo, "Sylvain Reynal", "sreynal@nerim.net", "author");

  dialog = xfce_about_dialog_new_with_values (NULL, nfo, NULL);
  xfce_about_info_free (nfo);

  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy (dialog);
}



void
cddrive_show_error_message (CddriveError error_group,
                            const gchar *error_message)
{
  g_assert (error_message != NULL);

  /* xfce_err ("%s : %s", cddrive_error_get_label (error_group), error_message); */
  xfce_message_dialog (NULL,
                       _("Error"),
                       GTK_STOCK_DIALOG_ERROR,
                       cddrive_error_get_label (error_group),
                       error_message,
                       GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                       NULL);
}



void
cddrive_show_error (const GError *error)
{
  g_assert (error != NULL);

  if (error->message != NULL)
    cddrive_show_error_message (error->code,
                                error->message);
  else
    cddrive_show_error_message (error->code,
                                _("No error description available."));
}
