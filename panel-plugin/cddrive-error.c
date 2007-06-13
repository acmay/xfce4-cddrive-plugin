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

#include <stdarg.h>
#include <libintl.h>
#include "cddrive-error.h"

#define _(message) gettext (message)
#define gettext_noop(message) message


/* To be able to report an "out of memory" error.
   Freed with 'cddrive_error_free_globals()'. */
static GError *_cddrive_no_memory_error;



/*#define CDDRIVE_ERROR_LABEL_COUNT 7*/
static const gchar* const cddrive_error_label [] =
{
  gettext_noop ("Configuration error"),
  gettext_noop ("System error"),
  gettext_noop ("Eject failed"),
  gettext_noop ("Close failed"),
  gettext_noop ("Mount failed"),
  gettext_noop ("Unmount failed"),
  gettext_noop ("Busy disc"),
};



void
cddrive_error_init_globals ()
{
  _cddrive_no_memory_error = g_error_new (CDDRIVE_ERROR_DOMAIN,
                                          CDDRIVE_ERROR_SYSTEM,
                                          _("Not enough memory to perform the operation."));
}



void
cddrive_error_free_globals ()
{
  g_error_free (_cddrive_no_memory_error);
}



void
cddrive_set_error (GError       **error,
                   CddriveError   code,
                   const gchar   *format,
                   ...)
{
  /* Slower than a macro, but type safe (ensure code is right at compilation time) */
  va_list  vl;
  gchar   *msg;

  va_start (vl, format);
  msg = g_strdup_vprintf (format, vl);
  va_end (vl);

  g_set_error (error,
               CDDRIVE_ERROR_DOMAIN,
               code,
               msg);
  g_free (msg);
}



void
cddrive_set_error_code (GError **error, CddriveError code)
{
  GError *new = NULL;

  g_assert (*error != NULL);
  
  if (*error != _cddrive_no_memory_error && (*error)->code != code)
    {
      g_set_error (&new, CDDRIVE_ERROR_DOMAIN, code, (*error)->message);
      g_clear_error (error);
      g_propagate_error (error, new);
    }
}



GError*
cddrive_no_memory_error ()
{
  g_assert (_cddrive_no_memory_error != NULL);
  return _cddrive_no_memory_error;
}



void
cddrive_clear_error (GError **error)
{
  if (error != NULL)
    {
      if (*error != _cddrive_no_memory_error)
        g_clear_error (error);
      else
        *error = NULL;
    }
}



const gchar*
cddrive_error_get_label (CddriveError code)
{
  g_assert (code >= 0);
  g_assert (code < g_strv_length ((gchar**) cddrive_error_label));

  return _(cddrive_error_label [code]);
}
