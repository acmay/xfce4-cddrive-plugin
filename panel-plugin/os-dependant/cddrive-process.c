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

#include "cddrive-process.h"


#ifdef __linux__

#include <errno.h>

/*
  The following code is based on linux procfs. I don't know how much this is portable.
  The functions use two procf files (are they linux only ?):
    - the /proc/<pid>/cwd link, to know if a process currently uses the disc.
    - the /proc/<pid>/exe link, to get the process name (i don't use /proc/<pid>/status file
      since it only provide a truncated name).
*/



gchar*
cddrive_get_process_name (guint64 process_pid)
{
  gchar *res = NULL;
  gchar *p1, *p2;
  gsize  w;

  g_assert (process_pid > 0);

  /* get the program path pointed by the exe link */
  p1 = g_strdup_printf ("/proc/%llu/exe", process_pid);
  p2 = g_file_read_link (p1, NULL);
  g_free (p1);
  p1 = g_filename_to_utf8 (p2, -1, NULL, &w, NULL);
  g_free (p2);

  if (p1 != NULL)
    {
      /* extract the name from the program path */
      p2 = g_utf8_strrchr (p1, -1, '/');
      if (p2 == NULL)
        res = p1;
      else
        {
          if (g_utf8_strlen (p2, -1) > 1)
            {
              /* remove the starting '/' */
              res = g_utf8_find_next_char (p2, NULL);
              if (res != NULL)
                res = g_strdup (res);
            }
        }
      g_free (p1);
    }
  
  return res;
}



/* Return the pid of a process currently using the directory of path 'dir_path',
   or zero if failed. */
guint64
cddrive_find_dir_locker_pid (const gchar *dir_path)
{
  guint64      res;
  GDir        *d;
  const gchar *f_name;
  gchar       *cwd_path = NULL;
  gchar       *cwd_lnk = NULL;
  gchar       *c;

  g_assert (dir_path != NULL);

  /* note: at first, i tried to look for file locks, but found that none of
           the blocking programs used locks, it's only that their cwd was the mount point
           or one of its subdirectories. */

  d = g_dir_open ("/proc", 0, NULL);
  if (d == NULL)
    {
      g_warning ("unable to open directory /proc");
      return 0;
    }
  
  for (f_name = g_dir_read_name (d);
       f_name != NULL;
       f_name = g_dir_read_name (d))
    {
      res = g_ascii_strtoull (f_name, &c, 10);
      if (errno != ERANGE && errno != EINVAL && *c == 0)
        {
          /* f_name is a number, i.e. the directory name of the process of pid 'res' */
          cwd_path = g_strconcat ("/proc/", f_name, "/cwd", NULL);
          if (g_file_test (cwd_path, G_FILE_TEST_IS_SYMLINK))
            {
              /* look if the cwd link point to the mount point or one of its subdirectories. */
              cwd_lnk = g_file_read_link (cwd_path, NULL);
              if (cwd_lnk != NULL && g_str_has_prefix (cwd_lnk, dir_path))
                {
                  /* locker found ! */
                  g_free (cwd_path);
                  g_free (cwd_lnk);
                  break;
                }
              
              g_free (cwd_lnk);
              cwd_lnk = NULL;
            }
          
          g_free (cwd_path);
          cwd_path = NULL;
        }      
    }
  
  if (f_name == NULL)
    res = 0;
    
  g_dir_close (d);
  
  return res;
}



#else /* ------- Unsupported Operating System or missing libs/headers ------- */
#warning "compiling without process informations support"

gchar*
cddrive_get_process_name (guint64 process_pid)
{
  g_warning ("Plugin compiled without process name reading from pid support.");
  return NULL;
}



guint64
cddrive_find_dir_locker_pid (const gchar *dir_path)
{
  g_warning ("Plugin compiled without locker pid search support.");
  return 0;
}
#endif
