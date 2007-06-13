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

#ifndef __CDDRIVE_H__
#define __CDDRIVE_H__

#include <libxfce4panel/xfce-panel-plugin.h>
#include "cddrive-monitor.h"


G_BEGIN_DECLS

/* plugin structure */
typedef struct
{
  XfcePanelPlugin *plugin;

  /* panel widgets */
  GtkWidget       *ebox;
  GtkWidget       *hvbox;
  GtkWidget       *button;
  GtkWidget       *image;            /* button icon */
  GtkWidget       *label;            /* to display 'name' value */
  GtkTooltips     *tips;
  GtkWidget       *menu_item;
  /*GtkWidget       *menu_sep;*/

  /* settings */
  gchar           *device;
  gchar*           mount_fallback;       /* fallback mount command */
  gchar*           unmount_fallback;     /* fallback unmount command */
  gchar           *name;
  gboolean         use_name_in_panel;    /* show name in panel ? */
  gboolean         use_name_in_tip;      /* show name in tooltip ? */
  gboolean         use_mounted_color;    /* colorize the unmounted disc icons ? */
  GdkColor        *mounted_color;
  gboolean         use_unmounted_color;  /* colorize the unmounted disc icons ? */
  GdkColor        *unmounted_color;
  gboolean         use_translucency;     /* fade the unmounted disc icons ? */
  gint             translucency;         /* in percent */

  /* internals */
  CddriveMonitor  *monitor;
}
CddrivePlugin;



typedef enum
{
  CDDRIVE_MOUNT   = 0,
  CDDRIVE_UNMOUNT = 1
}
CddriveCommandType;



void
cddrive_save                 (XfcePanelPlugin *plugin,
                              CddrivePlugin    *sample);



void
cddrive_set_mount_fallback   (CddrivePlugin *cddrive,
                              const gchar   *value);
                            
                            
                            
void
cddrive_set_unmount_fallback (CddrivePlugin *cddrive,
                              const gchar   *value);



void
cddrive_update_monitor       (CddrivePlugin *cddrive);



void
cddrive_update_label         (CddrivePlugin *cddrive);



void
cddrive_update_icon          (CddrivePlugin *cddrive);



void
cddrive_update_tip           (CddrivePlugin *cddrive);
                            
G_END_DECLS
#endif /* !__CDDRIVE_H__ */
