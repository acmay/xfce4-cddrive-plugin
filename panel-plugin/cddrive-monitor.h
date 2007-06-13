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

#ifndef __CDDRIVE_MONITOR_H__
#define __CDDRIVE_MONITOR_H__

#include "cddrive-monitor-private.h"


G_BEGIN_DECLS

typedef _CddriveMonitor CddriveMonitor;


typedef _CddriveStatus CddriveStatus;


typedef struct
{
  gchar *device;
  gchar *model;
}
CddriveDriveInfo;



typedef void (*CddriveOnDiscInserted) (CddriveMonitor *monitor,
                                       gpointer        callback_data);

typedef void (*CddriveOnDiscRemoved)  (CddriveMonitor *monitor,
                                       gpointer        callback_data);

typedef void (*CddriveOnDiscModified) (CddriveMonitor *monitor,
                                       gpointer        callback_data);



/* Return an array with the infos of each CD-ROM drive, or NULL if it failed.
   The last element of the array is NULL. */
CddriveDriveInfo**
cddrive_cdrom_drive_infos_new  (GError **error);



void
cddrive_cdrom_drive_infos_free (CddriveDriveInfo* *infos);



/* Create a monitor for the CD-ROM drive of 'device' device path.
   'device' must not be NULL.
   If the creation failed, set 'error' and return NULL. */
CddriveMonitor*
cddrive_monitor_new             (gchar                  *device,
                                 gpointer                callback_data,
                                 CddriveOnDiscInserted   on_disc_inserted_callback,
                                 CddriveOnDiscRemoved    on_disc_removed_callback,
                                 CddriveOnDiscModified   on_disc_modified_callback,
                                 const gchar*            mount_fallback,
                                 const gchar*            unmount_fallback,
                                 GError                **error);



void
cddrive_monitor_free            (CddriveMonitor *monitor);



/* Enable/disable callbacks on changes of drive state.
   Return FALSE if the operation failed, TRUE otherwise. */
gboolean
cddrive_monitor_enable_callbacks    (CddriveMonitor  *monitor,
                                     gboolean         enable,
                                     GError         **error);



/* Are the callbacks enabled ? */
gboolean
cddrive_monitor_callbacks_enabled   (CddriveMonitor *monitor);



/* Open the tray of the CD-ROM drive */
void
cddrive_monitor_eject           (CddriveMonitor *monitor, GError **error);



/* Close the tray of the CD-ROM drive */
void
cddrive_monitor_close           (CddriveMonitor *monitor, GError **error);



/* Mount disc */
void
cddrive_monitor_mount           (CddriveMonitor *monitor, GError **error);



/* Unmount disc */
void
cddrive_monitor_unmount         (CddriveMonitor *monitor, GError **error);



/* Various infos about the CD-ROM drive, and the disc that might be in.
   If a problem occured while retrieving an info, the function set 'error'
   and return NULL.
   A NULL value as 'monitor' is not an error. In this case the function return
   NULL without setting 'error'.
   It is up to the programmer to free the result with 'cddrive_status_free'. */
CddriveStatus*
cddrive_status_new  (CddriveMonitor *monitor, GError **error);



void
cddrive_status_free (CddriveStatus *status);



/*  Can we eject the disc when there is one ?
   'status' must not be NULL. */
gboolean
cddrive_status_is_ejectable  (CddriveStatus *status);



/* TRUE if there is no disc in the CD-ROM drive, otherwise FALSE.
   'status' must not be NULL. */
gboolean
cddrive_status_is_empty      (CddriveStatus *status);



/* TRUE if the tray of the CD-ROM drive is open, otherwise FALSE.
   'status' must not be NULL. */
gboolean
cddrive_status_is_open       (CddriveStatus *status);



/* TRUE if there is a mounted disc in the CD-ROM drive, otherwise FALSE.
   'status' must not be NULL. */
gboolean
cddrive_status_is_mounted    (CddriveStatus *status);



/* TRUE if there is a blank disc in the CD-ROM drive, otherwise FALSE.
   'status' must not be NULL. */
gboolean
cddrive_status_is_blank      (CddriveStatus *status);



/* TRUE if there is a audio disc in the CD-ROM drive, otherwise FALSE.
   'status' must not be NULL. */
gboolean
cddrive_status_is_audio      (CddriveStatus *status);



/* TRUE if the disc is locked by an application, otherwise FALSE.
   'status' must not be NULL. */
gboolean
cddrive_status_is_locked     (CddriveStatus *status);



/* Title of the disc. NULL if there is none or the disc is audio or blank.
   'status' must not be NULL. */
const gchar*
cddrive_status_get_title     (CddriveStatus *status);



/* Type of the disc. NULL if there is none or the disc is audio or blank.
   'status' must not be NULL. */
const gchar*
cddrive_status_get_type      (CddriveStatus *status);



/* Name of icon showing the status of the drive (never NULL).
   If 'status' is NULL, returns an icon for unknown status. */
const gchar*
cddrive_status_get_icon_name (CddriveStatus *status);

G_END_DECLS
#endif
