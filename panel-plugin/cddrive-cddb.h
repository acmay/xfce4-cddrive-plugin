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

#ifndef __CDDRIVE_CDDB_H__
#define __CDDRIVE_CDDB_H__

#include <glib.h>


G_BEGIN_DECLS

/* freedb.org support (to get the title of an audio CD) */


/* To call at program start */
void
cddrive_cddb_init_globals ();



/* Free cddb global resources.
   To call at program termination. */
void
cddrive_cddb_free_globals ();



/* Return the title of the audio CD in the drive of device path 'device',
   or NULL if the drive have no audio CD, or if the operation failed.
   If 'connection_allowed' is FALSE, the function looks for the title in the
   cache only.
   The result must be freed after use. */
gchar*
cddrive_cddb_get_title (const gchar* device, gboolean connection_allowed);

G_END_DECLS
#endif
