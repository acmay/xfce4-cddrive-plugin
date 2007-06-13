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

#ifndef __CDDRIVE_ERROR_H__
#define __CDDRIVE_ERROR_H__

#include <glib.h>


#define CDDRIVE_ERROR_DOMAIN          g_quark_from_static_string ("CDDRIVE_ERROR")


G_BEGIN_DECLS

/* IMPORTANT: when adding an error code to the following enum, do not forget to
   add the corresponding label (even a NULL one) to cddrive_error_label in cddrive-error.c */
typedef enum
{
  CDDRIVE_ERROR_FAILED  = 0x00, /* external config error (due to hald not running for example) */
  CDDRIVE_ERROR_SYSTEM  = 0x01,
  CDDRIVE_ERROR_EJECT   = 0x02,
  CDDRIVE_ERROR_CLOSE   = 0x03,
  CDDRIVE_ERROR_MOUNT   = 0x04,
  CDDRIVE_ERROR_UNMOUNT = 0x05,
  CDDRIVE_ERROR_BUSY    = 0x06  /* disc busy */
}
CddriveError;



/* To call at program start */
void
cddrive_error_init_globals ();



/* Free error global resource (i.e. 'cddrive_no_memory_error').
   To call at program termination. */
void
cddrive_error_free_globals ();



/* Set a 'CDDRIVE_ERROR_*' error (slower than a macro, but type safe for 'code') */
void
cddrive_set_error          (GError       **error,
                            CddriveError   code,
                            const gchar   *format,
                            ...);
                            
                            

/* Set 'error' code with 'code' (leave 'error' message unchanged).
   '*error' must not be NULL. */
void
cddrive_set_error_code     (GError **error, CddriveError code);



/* Return an "out of memory" error... even if there is no more free memory */
GError*
cddrive_no_memory_error    ();



/* Safe way to free an error set with a 'cddrive_*()' function.
   Act the same as 'g_clear_error'. */
void
cddrive_clear_error        (GError **error);



/* Return the label corresponding to the code of 'error'.
   'error' must have been set with a CddriveError code. */
const gchar*
cddrive_error_get_label    (CddriveError code);

G_END_DECLS
#endif
