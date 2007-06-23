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

#ifndef __CDDRIVE_MONITOR_PRIVATE_H__
#define __CDDRIVE_MONITOR_PRIVATE_H__

#include <hal/libhal-storage.h>


G_BEGIN_DECLS

typedef struct
{
  LibHalContext *ctx;
  gpointer       on_disc_inserted;
  gpointer       on_disc_removed;
  gpointer       on_disc_modified;
  gboolean       callbacks_enabled;

  gchar         *dev;                  /* CD-ROM drive's device path                       */
  gchar         *udi;                  /* CD-ROM disc's UDI (NULL if none)                 */
  gpointer       dat;                  /* data to use in callbacks                         */
  gchar*         mount;                /* fallback mount command (not empty if not NULL)   */
  gchar *        unmount;              /* fallback unmount command (not empty if not NULL) */

  /* to know if 'mount' and 'unmount' needs to be formatted before execution */
  gboolean       mount_needs_update;   
  gboolean       unmount_needs_update;

  /* special status infos */

  /* set once at init only (could have been set for each new CddriveStatus, but
     we don't really expect this property to change, do we ?) */
  gboolean       is_ejectable;

  /* audio disc title cache */
  gchar         *audio_title;
  
  gboolean       use_cddb; /* are freedb.org connections allowed ? */
} _CddriveMonitor;



typedef struct
{
  gboolean         is_empty;     /* is the drive empty (no disc inside) ?                 */
  gboolean         is_open;      /* is the tray open   ?                                  */
  gboolean         is_mounted;   /* is there a mounted disc in the drive ?                */
  gboolean         is_blank;     /* is there a blank disc in the drive ?                  */
  gboolean         is_audio;     /* is there an audio disc in the drive ?                 */
  gchar           *title;        /* title of disc if there is one, NULL otherwise         */
  const gchar     *type;         /* type of disc if there is one, NULL otherwise          */
  const gchar     *icon_name;    /* name of the icon showing the drive status, never NULL */

  _CddriveMonitor *mon;          /* to access cached datas */
} _CddriveStatus;

G_END_DECLS
#endif
