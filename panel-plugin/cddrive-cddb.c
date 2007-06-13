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

#include "cddrive-cddb.h"

#if defined (HAVE_LIBCDIO) && defined (HAVE_LIBCDDB)

#include <cdio/cdio.h>
#include <cddb/cddb.h>
#include <libxfce4util/libxfce4util.h>


#ifndef PACKAGE
#define PACKAGE "xfce4-cddrive-plugin"
#endif
#ifndef VERSION
#define VERSION "unknown"
#endif

#define CDDRIVE_CDDB_CACHE_PATH           "xfce4/panel/cddrive"
#define CDDRIVE_CDDB_CACHE_GROUP          "Cddb"
#define CDDRIVE_CDDB_CACHE_SIZE           20
#define cddrive_cddb_get_key_for_id(id)   g_strdup_printf ("%08x", id)



/* Save a pair (CDDB id, CD title) in the cache file only if it is not already
   stored. Do nothing otherwise.
   
   If it does not yet exist, create the cache file before saving.
   
   The cache file is a Xfce rc file, containing a group called "Cddb" with
   at most CDDRIVE_CDDB_CACHE_SIZE entries, each having a CDDB id as the key,
   and a CD title as the value.
*/
static void
cddrive_cddb_cache_save (guint id, const gchar *title)
{
  XfceRc *cache;
  gchar* *keys;
  gchar  *k;
  guint   nb;

  cache = xfce_rc_config_open (XFCE_RESOURCE_CACHE,
                               CDDRIVE_CDDB_CACHE_PATH,
                               FALSE);
  if (cache == NULL)
    {
      g_warning ("unable to open CDDB cache file.");
      return;
    }
  
  xfce_rc_set_group (cache, CDDRIVE_CDDB_CACHE_GROUP);
  
  k = cddrive_cddb_get_key_for_id (id);
  if (! xfce_rc_has_entry (cache, k))
    {
      /* check if the cache limit is reached. If so, remove the first entry. */
      keys = xfce_rc_get_entries (cache, CDDRIVE_CDDB_CACHE_GROUP);
      nb = g_strv_length (keys);
    
      if (nb >= CDDRIVE_CDDB_CACHE_SIZE)
        xfce_rc_delete_entry (cache, keys [0], FALSE);
      
      g_strfreev (keys);
      
      /* write the new entry */
      xfce_rc_write_entry (cache, k, title);
    }
  
  xfce_rc_close (cache);
  g_free (k);
}



/* Return the title corresponding to the CDDB id 'id' in the cache, or NULL
   if not found. */
gchar*
cddrive_cddb_cache_read (guint id)
{
  XfceRc *cache;
  gchar  *k, *res;

  cache = xfce_rc_config_open (XFCE_RESOURCE_CACHE,
                               CDDRIVE_CDDB_CACHE_PATH,
                               TRUE);
  if (cache == NULL)
    {
      g_warning ("unable to open CDDB cache file.");
      return NULL;
    }
  
  xfce_rc_set_group (cache, CDDRIVE_CDDB_CACHE_GROUP);
  k = cddrive_cddb_get_key_for_id (id);
  res = g_strdup (xfce_rc_read_entry (cache, k, NULL));
  xfce_rc_close (cache);
  g_free (k);
  
  return res;
}



/* Set the cddb id and the length of the audio CD in 'cdda', as well as each
   track and its frame offset.
   Return TRUE on success, FALSE otherwise. */
static gboolean
cddrive_cddb_fill_cdda (CdIo_t *cdio, cddb_disc_t *cdda)
{
  track_t       first, last, i; /* first, last and current track numbers */
  lba_t         lba;
  cddb_track_t *track;
  
  /* set CDDA's length */
  lba = cdio_get_track_lba (cdio, CDIO_CDROM_LEADOUT_TRACK);
  if (lba == CDIO_INVALID_LBA)
    {
      g_warning ("leadout track has invalid Logical Block Address.");
      return FALSE;
    }
  cddb_disc_set_length (cdda, FRAMES_TO_SECONDS (lba));

  /* get number of the first track (should be 1, but just in case...) */
  first = cdio_get_first_track_num (cdio);
  if (first == CDIO_INVALID_TRACK)
    {
      g_warning ("unable to get first track number.");
      return FALSE;
    }

  /* get number of the last track */
  last = cdio_get_last_track_num (cdio);
  if (last == CDIO_INVALID_TRACK)
    {
      g_warning ("unable to get last track number.");
      return FALSE;
    }
  
  /* set frame offset for each track of the CDDA */
  for (i = first; i <= last; i++)
    {
      track = cddb_track_new ();
      if (track == NULL)
        {
          g_warning ("not enough memory to get CDDA title.");
          return FALSE;
        }
      
      cddb_disc_add_track (cdda, track);
      
      lba = cdio_get_track_lba (cdio, i);
      if (lba == CDIO_INVALID_LBA)
        {
          g_warning ("invalid Logical Block Adress for track %d.", i);
          return FALSE;
        }
      
      cddb_track_set_frame_offset (track, lba);
    }
  
  if (! cddb_disc_calc_discid (cdda))
    {
      g_warning ("failed to compute the CDDB disc id.");
      return FALSE;
    }
  
  return TRUE;
}



static cddb_disc_t*
cddrive_cddb_new_cdda (const gchar* device)
{
  cddb_disc_t *res;
  CdIo_t      *cdio;

  cdio = cdio_open_cd (device);
  if (cdio == NULL)
  {
    g_warning ("unable to open CDDA in drive '%s'.", device);
    return NULL;
  }

  res = cddb_disc_new ();
  if (res == NULL)
    g_warning ("not enough memory to get CDDA title.");
  else
    {
      if (! cddrive_cddb_fill_cdda (cdio, res))
        {
          cddb_disc_destroy (res);
          res = NULL;
        }
    }
  
  cdio_destroy (cdio);
  return res;
}



static cddb_conn_t*
cddrive_cddb_new_connection ()
{
  cddb_conn_t *res;

  res = cddb_new ();
  if (res == NULL)
    {
      g_warning ("failed to create CDDB connection.");
      return NULL;
    }
  
  /* Use HTTP rather than CDDB to avoid firewall headaches...
     as soon as freedb.org accept HTTP requests again. */
  /*cddb_http_enable (res);*/
  
  cddb_set_client (res, PACKAGE, VERSION);
  
  /* cache disabled (cache does not work for me, and we only need to cache the title anyway) */
  cddb_cache_disable (res);
  
  return res;
}



static gpointer
cddrive_cddb_cache_title_from_server (gpointer data)
{
  cddb_disc_t *cdda = (cddb_disc_t*) data;
  cddb_conn_t *conn;
  static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

  /* we don't want two connections at the same time, have mercy for freedb.org,
     nor we want two thread writing in the cache together; hence the mutex. */
  if (g_static_mutex_trylock (&mutex))
    {
      conn = cddrive_cddb_new_connection ();
      if (conn != NULL)
        {
          if (cddb_query (conn, cdda) == -1)
            g_warning ("query on server '%s' failed (%s).",
                       cddb_get_server_name (conn),
                       cddb_error_str (cddb_errno (conn)));
          else
            {
              if (cddb_disc_get_title (cdda) != NULL)
                cddrive_cddb_cache_save (cddb_disc_get_discid (cdda),
                                         cddb_disc_get_title (cdda));
            }
      
          cddb_destroy (conn);
        }
      g_static_mutex_unlock (&mutex);
    }
  
  cddb_disc_destroy (cdda);
  
  return NULL; /* dummy return value */
}



gchar*
cddrive_cddb_get_title (const gchar* device)
{
  gchar       *res = NULL;
  cddb_disc_t *cdda;

  cdda = cddrive_cddb_new_cdda (device);
  if (cdda != NULL)
    {
      res = cddrive_cddb_cache_read (cddb_disc_get_discid (cdda));
      
      if (res == NULL)
        {
          /* the CDDB disc id was not found in cache. Try to fetch it on freedb.org */
        
#ifdef HAVE_GTHREAD
          /* if possible, fetch the title in a thread, so the plugin do not freeze
             while attempting to connect to the server */          
          g_thread_create (cddrive_cddb_cache_title_from_server,
                           cdda,
                           FALSE,
                           NULL);
          /* note: cdda is destroyed in the thread function */
        
#else
          /* no thread support, plugin freeze will depend on the connection quality */
          cddrive_cddb_cache_title_from_server (cdda);
          res = g_strdup (cddb_disc_get_title (cdda));
          cddb_disc_destroy (cdda);
#endif
        }
      else
        cddb_disc_destroy (cdda);
    }

  return res;
}



void
cddrive_cddb_init_globals ()
{
#ifdef HAVE_GTHREAD
  if (! g_thread_supported ())
    g_thread_init (NULL);
#endif
}



void
cddrive_cddb_free_globals ()
{
  libcddb_shutdown ();
}



#else /* ---------- NO CDDB SUPPORT ---------- */

gchar*
cddrive_cddb_get_title (const gchar* device)
{
  return NULL;
}



void
cddrive_cddb_init_globals ()
{
}



void
cddrive_cddb_free_globals ()
{
}

#endif
