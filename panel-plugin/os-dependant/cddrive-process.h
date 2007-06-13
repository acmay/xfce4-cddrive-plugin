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

#include <glib.h>



/* Return the name of the program associated with the pid 'process_pid',
   or NULL if it failed to retrieve it.
   The result must be freed after use. */
gchar*
cddrive_get_process_name (guint64 process_pid);



/* Return the pid of a process currently using the directory of path 'dir_path',
   or zero if failed. */
guint64
cddrive_find_dir_locker_pid (const gchar *dir_path);
