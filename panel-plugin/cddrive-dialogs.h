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

#ifndef __CDDRIVE_DIALOGS_H__
#define __CDDRIVE_DIALOGS_H__

#include <libxfce4panel/xfce-panel-plugin.h>
#include "cddrive.h"


G_BEGIN_DECLS

void
cddrive_configure               (XfcePanelPlugin *plugin,
                                 CddrivePlugin   *cddrive);

void
cddrive_about                   (XfcePanelPlugin *plugin);


void
cddrive_show_error_message      (CddriveError  error_group,
                                 const gchar  *error_message);


void
cddrive_show_error              (const GError *error);

G_END_DECLS
#endif
