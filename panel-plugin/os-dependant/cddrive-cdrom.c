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

#include "cddrive-cdrom.h"


#ifdef __linux__

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>
#include <errno.h>



gint
cddrive_get_tray_status (const gchar *device)
{
  gint res = 0;
  gint fd, s;

  g_assert (device != NULL);

  fd = open (device, O_RDONLY|O_NONBLOCK);
  
	if (fd == -1)
    {
      g_warning ("Cannot open file '%s'. Failed to get tray status.",
                 device);
      return -1;
    }

	/* get CD drive status */  
  s = ioctl (fd, CDROM_DRIVE_STATUS, CDSL_CURRENT);
	if (s == -1)
    {
      g_warning ("%s. Failed to get tray status.", g_strerror (errno));
      if (close (fd) != 0)
        g_warning (g_strerror (errno));
      return -1;
    }
  
  /* inspect status */
  switch (s)
    {
      case CDS_DRIVE_NOT_READY:
        g_warning ("Drive '%s' is not ready. Failed to get tray status.",
                   device);
        break;
      
      case CDS_NO_INFO:
        g_warning ("Drive status not available for device '%s' (not implemented in kernel).",
                   device);
        break;
      
      case CDS_TRAY_OPEN:
        res = 1;
    }
  
  /* cleanup */
  if (close (fd) != 0)
    g_warning (g_strerror (errno));
  
  return res;
}


#else /* ------- Unsupported Operating System or missing libs/headers ------- */
#warning "compiling without tray status support"

gint
cddrive_get_tray_status (const gchar *device)
{
  g_warning ("Plugin compiled without tray status support.");
  return -1;
}
#endif
