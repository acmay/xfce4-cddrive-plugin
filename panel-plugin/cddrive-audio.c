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

#include "cddrive-audio.h"
#include <cdio/cdio.h>

#ifdef HAVE_LIBCDDB

#include <cddb/cddb.h>
#include <libxfce4util/libxfce4util.h>

#ifndef PACKAGE
#define PACKAGE "xfce4-cddrive-plugin"
#endif
#ifndef VERSION
#define VERSION "unknown"
#endif

#define CDDRIVE_CDDB_CACHE_PATH             "xfce4/panel/cddrive"
#define CDDRIVE_CDDB_CACHE_INFOS_GROUP_ID   "infos"
#define CDDRIVE_CDDB_CACHE_VERSION_ID       "version"
#define CDDRIVE_CDDB_CACHE_VERSION          "1"
#define CDDRIVE_CDDB_CACHE_SIZE             20
#define CDDRIVE_CDDB_CACHE_PERFORMERS_ID    "perf"
#define CDDRIVE_CDDB_CACHE_TITLE_ID         "title"

#define cddrive_audio_get_key_for_id(id)  g_strdup_printf ("%08x", id)

#endif



/* Return a structure containing the infos passed as parameters.
   Return NULL if there is not enough memory, or if all the parameters are NULL.
   The string parameters should not be freed, this is done by calling
   'cddrive_audio_free_infos'.*/
static CddriveAudioInfos*
cddrive_audio_new_infos (gchar *performers, gchar *title)
{
  CddriveAudioInfos *res;

  if (performers == NULL && title == NULL)
    return NULL;

  res = g_try_malloc (sizeof (CddriveAudioInfos));
  if (res == NULL)
    {
      g_warning ("not enough memory to store the audio CD infos.");
      return NULL;
    }
  
  res->performers = performers;
  res->title = title;

  return res;
}



void
cddrive_audio_free_infos (CddriveAudioInfos *infos)
{
  g_assert (infos != NULL);

  g_free (infos->performers);
  g_free (infos->title);
  g_free (infos);
}



/* Create a read access on the CD */
static CdIo_t*
cddrive_audio_new_cdio (const gchar* device)
{
  CdIo_t     *res;

  res = cdio_open (device, DRIVER_DEVICE);
  if (res == NULL)
    g_warning ("unable to open audio CD in drive '%s'.", device);
  
  else
    {
      if (cdio_get_discmode (res) == CDIO_DISC_MODE_ERROR ||
          cdio_get_discmode (res) == CDIO_DISC_MODE_NO_INFO)
        {
          cdio_destroy (res);
          res = NULL;
        }
    }
  
  return res;
}



/* Return the infos of the CD from CD-TEXT, or NULL if the CD does not contain
   such information. */
static CddriveAudioInfos*
cddrive_audio_get_cdtext_infos (CdIo_t *cdio)
{
  CddriveAudioInfos *res;
  cdtext_t          *cdtxt;

  cdtxt = cdio_get_cdtext (cdio, 0);
  res = cddrive_audio_new_infos (cdtext_get (CDTEXT_PERFORMER, cdtxt),
                                 cdtext_get (CDTEXT_TITLE, cdtxt));
  cdtext_destroy (cdtxt);

  return res;
}



#ifdef HAVE_LIBCDDB /* ---------- CDDB SUPPORT ---------- */

/* Open cache without checking version */
static XfceRc*
cddrive_audio_open_cache (gboolean readonly)
{
  XfceRc *res;

  res = xfce_rc_config_open (XFCE_RESOURCE_CACHE,
                             CDDRIVE_CDDB_CACHE_PATH,
                             readonly);
  if (res == NULL)
    g_warning ("unable to open CDDB cache file for %s.",
                 (readonly ? "reading" : "writing"));
  
  return res;
}



/* Clear cache and set version to 'CDDRIVE_CDDB_CACHE_VERSION' */
static void
cddrive_audio_reset_cddb_cache (XfceRc *cache)
{
  gchar* *l;
  gint    i;

  l = xfce_rc_get_groups (cache);
  for (i = 0; l [i] != NULL; i++)
    xfce_rc_delete_group (cache, l [i], FALSE);
  
  g_strfreev (l);
  
  xfce_rc_set_group (cache, CDDRIVE_CDDB_CACHE_INFOS_GROUP_ID);
  xfce_rc_write_entry (cache,
                       CDDRIVE_CDDB_CACHE_VERSION_ID,
                       CDDRIVE_CDDB_CACHE_VERSION);
}



/* Open cache, check it, and reset it if necessary */
static XfceRc*
cddrive_audio_get_cddb_cache (gboolean readonly)
{
  XfceRc      *res;
  const gchar *v;

  res = cddrive_audio_open_cache (readonly);
  if (res == NULL)
    return NULL;
  
  xfce_rc_set_group (res, CDDRIVE_CDDB_CACHE_INFOS_GROUP_ID);
  v = xfce_rc_read_entry (res, CDDRIVE_CDDB_CACHE_VERSION_ID, "not set");
  
  if (! g_str_equal (CDDRIVE_CDDB_CACHE_VERSION, v))
    {
      g_message ("current CDDB cache version is %s. Reseting cache to version %s",
                 v, CDDRIVE_CDDB_CACHE_VERSION);
      
      if (readonly)
        {
          /* reopen cache for writing */
          xfce_rc_close (res);
          res = cddrive_audio_open_cache (FALSE);
          if (res == NULL)
            return NULL;
          
          cddrive_audio_reset_cddb_cache (res);
          
          /* reopen cache for reading */
          xfce_rc_close (res);
          res = cddrive_audio_open_cache (TRUE);
          if (res == NULL)
            return NULL;
        }
      else
        cddrive_audio_reset_cddb_cache (res);      
    }
  
  return res;
}



/* Save a pair (CDDB id, CD title) in the cache file only if it is not already
   stored. Do nothing otherwise.
   
   If it does not yet exist, create the cache file before saving.
   
   The cache file is a Xfce rc file, containing a group called "Cddb" with
   at most CDDRIVE_CDDB_CACHE_SIZE entries, each having a CDDB id as the key,
   and a list (title, performers) as the value.
*/
static void
cddrive_audio_cache_save (guint id, CddriveAudioInfos *infos)
{
  XfceRc *cache = NULL;
  gchar* *keys;
  gchar  *k, *k2;
  guint   n;

  g_assert (infos != NULL);

  cache = cddrive_audio_get_cddb_cache (FALSE);
  if (cache == NULL)
    return;
  
  k = cddrive_audio_get_key_for_id (id);
  if (! xfce_rc_has_group (cache, k))
    {
      /* check if the cache size is over the limit. If so, remove the first groups
         until the limit is reached. */
      keys = xfce_rc_get_groups (cache);
      for (n = g_strv_length (keys); n > CDDRIVE_CDDB_CACHE_SIZE; n--)
        {
          k2 = keys [n - CDDRIVE_CDDB_CACHE_SIZE];
        
          /* do not delete the cache internal infos group */
          if (! g_str_equal (k2, CDDRIVE_CDDB_CACHE_INFOS_GROUP_ID))
            xfce_rc_delete_entry (cache, k2, FALSE);
        }
      
      g_strfreev (keys);
      
      /* write the new group */
      xfce_rc_set_group (cache, k);
      if (infos->performers != NULL)
        xfce_rc_write_entry (cache,
                             CDDRIVE_CDDB_CACHE_PERFORMERS_ID,
                             infos->performers);
      if (infos->performers != NULL)
        xfce_rc_write_entry (cache,
                             CDDRIVE_CDDB_CACHE_TITLE_ID,
                             infos->title);
    }
  
  xfce_rc_close (cache);
  g_free (k);
}



/* Return the infos corresponding to the CDDB id 'id' in the cache, or NULL
   if not found.
   If not NULL, free the result with 'cddrive_audio_free_infos'. */
static CddriveAudioInfos*
cddrive_audio_cache_read (guint id)
{
  CddriveAudioInfos *res = NULL;
  XfceRc            *cache;
  gchar             *k;

  cache = cddrive_audio_get_cddb_cache (TRUE);
  if (cache == NULL)
    return NULL;
  
  k = cddrive_audio_get_key_for_id (id);
  
  if (xfce_rc_has_group (cache, k))
    {
      xfce_rc_set_group (cache, k);
      res = cddrive_audio_new_infos (g_strdup (xfce_rc_read_entry (cache,
                                                  CDDRIVE_CDDB_CACHE_PERFORMERS_ID,
                                                  NULL)),
                                     g_strdup (xfce_rc_read_entry (cache,
                                                  CDDRIVE_CDDB_CACHE_TITLE_ID,
                                                  NULL)));
    }
  
  xfce_rc_close (cache);
  g_free (k);
  
  return res;
}



/* Set the cddb id and the length of the audio CD in 'cdda', as well as each
   track and its frame offset.
   Return TRUE on success, FALSE otherwise. */
static gboolean
cddrive_audio_fill_cdda (CdIo_t *cdio, cddb_disc_t *cdda)
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
cddrive_audio_new_cdda (CdIo_t *cdio)
{
  cddb_disc_t *res;

  res = cddb_disc_new ();
  if (res == NULL)
    g_warning ("not enough memory to get CDDA title.");
  else
    {
      if (! cddrive_audio_fill_cdda (cdio, res))
        {
          cddb_disc_destroy (res);
          res = NULL;
        }
    }
  
  return res;
}



static cddb_conn_t*
cddrive_audio_new_connection ()
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
cddrive_audio_cache_infos_from_server (gpointer data)
{
  cddb_disc_t       *cdda = (cddb_disc_t*) data;
  cddb_conn_t       *conn;
  CddriveAudioInfos *infos;
  static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

  /* we don't want two connections at the same time, have mercy for freedb.org,
     nor we want two thread writing in the cache together; hence the mutex. */
  if (g_static_mutex_trylock (&mutex))
    {
      conn = cddrive_audio_new_connection ();
      if (conn != NULL)
        {
          g_debug ("sending query to freedb.org");
          if (cddb_query (conn, cdda) == -1)
            g_warning ("query on server '%s' failed (%s).",
                       cddb_get_server_name (conn),
                       cddb_error_str (cddb_errno (conn)));
          else
            {
              infos = cddrive_audio_new_infos (g_strdup (cddb_disc_get_artist (cdda)),
                                               g_strdup (cddb_disc_get_title (cdda)));
                                               
              if (infos != NULL)
                {
                  cddrive_audio_cache_save (cddb_disc_get_discid (cdda), infos);
                  cddrive_audio_free_infos (infos);
                }
            }
      
          cddb_destroy (conn);
          g_debug ("freedb.org connection closed");
        }
      g_static_mutex_unlock (&mutex);
    }
  
#ifdef HAVE_GTHREAD
  cddb_disc_destroy (cdda);
#endif
  
  return NULL; /* dummy return value */
}



CddriveAudioInfos*
cddrive_audio_get_infos (const gchar* device, gboolean connection_allowed)
{
  CddriveAudioInfos *res = NULL;
  cddb_disc_t       *cdda;
  CdIo_t            *cdio;

  g_assert (device != NULL);

  cdio = cddrive_audio_new_cdio (device);
  if (cdio != NULL)
    {
      cdda = cddrive_audio_new_cdda (cdio);
      if (cdda != NULL)
        {
          res = cddrive_audio_cache_read (cddb_disc_get_discid (cdda));
      
          if (res == NULL)
            {
              res = cddrive_audio_get_cdtext_infos (cdio);
        
              if (res == NULL)
                {
                  if (connection_allowed)
                    {
                      /* the CDDB disc id was not found in cache. Try to fetch it on freedb.org */
        
#ifdef HAVE_GTHREAD
                      /* if possible, fetch the infos in a thread, so the plugin do not freeze
                         while attempting to connect to the server */          
                      g_thread_create (cddrive_audio_cache_infos_from_server,
                                       cdda,
                                       FALSE,
                                       NULL);
                      /* note: cdda is destroyed in the thread function */
        
#else
                      /* no thread support, plugin freeze will depend on the connection quality */
                      /* note: cdda is NOT destroyed in this function (disabled with no thread support) */
                      cddrive_audio_cache_infos_from_server (cdda);
                    
                      res = cddrive_audio_new_infos (g_strdup (cddb_disc_get_artist (cdda)),
                                                     g_strdup (cddb_disc_get_title (cdda)));
                      cddb_disc_destroy (cdda);
#endif
                    }
                }
              else
                {
                  cddrive_audio_cache_save (cddb_disc_get_discid (cdda), res);
                  cddb_disc_destroy (cdda);
                }
            }
        }
      
      cdio_destroy (cdio);
    }

  return res;
}



void
cddrive_audio_init_globals ()
{
#ifdef HAVE_GTHREAD
  if (! g_thread_supported ())
    g_thread_init (NULL);
#endif
}



void
cddrive_audio_free_globals ()
{
  libcddb_shutdown ();
}



#else /* ---------- NO CDDB SUPPORT ---------- */

CddriveAudioInfos*
cddrive_audio_get_infos (const gchar* device, gboolean connection_allowed)
{
  CddriveAudioInfos *res = NULL;
  CdIo_t            *cdio;

  g_assert (device != NULL);

  cdio = cddrive_audio_new_cdio (device);
  if (cdio != NULL)
    {
      res = cddrive_audio_get_cdtext_infos (cdio);
      cdio_destroy (cdio);
    }
  
  return res;
}



void
cddrive_audio_init_globals ()
{
}



void
cddrive_audio_free_globals ()
{
}

#endif
