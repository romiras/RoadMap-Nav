/* editor_main.c - main plugin file
 *
 * LICENSE:
 *
 *   Copyright 2005 Ehud Shabtai
 *
 *   This file is part of RoadMap.
 *
 *   RoadMap is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   RoadMap is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with RoadMap; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * SYNOPSYS:
 *
 *   See editor_main.h
 */

#include <stdlib.h>
#include <assert.h>

#include "../roadmap.h"
#include "../roadmap_pointer.h"
#include "../roadmap_plugin.h"
#include "../roadmap_layer.h"
#include "../roadmap_locator.h"
#include "../roadmap_metadata.h"
#include "../roadmap_messagebox.h"

#include "editor_screen.h"
#include "static/update_range.h"
#include "static/notes.h"
#include "track/editor_track_main.h"
#include "track/editor_gps_data.h"
#include "export/editor_upload.h"
#include "export/editor_export.h"
#include "editor_plugin.h"
#include "db/editor_db.h"
#include "editor_main.h"

int EditorEnabled = 0;
int EditorPluginID = -1;

const char *EDITOR_VERSION = "0.10.0 rc5";

void editor_main_check_map (void) {

   int fips;
   time_t now_t;
   time_t map_time_t;

   fips = roadmap_locator_active ();

   if (fips < 0) {
      fips = 77001;
   }

   if (roadmap_locator_activate (fips) != ROADMAP_US_OK) {
      roadmap_messagebox ("Error.", "Can't load map data.");
      return;
   }

   now_t = time (NULL);
   map_time_t = atoi(roadmap_metadata_get_attribute ("Version", "UnixTime"));

   if ((map_time_t + 3600*24) < now_t) {
      roadmap_messagebox
         ("Warning", "Your map is not updated. Please synchronize.");
   }
}


int editor_is_enabled (void) {
   return EditorEnabled;
}


void editor_main_initialize (void) {

   editor_upload_initialize   ();
   editor_gps_data_initialize ();
   editor_export_initialize   ();
   editor_screen_initialize   ();
   editor_track_initialize    ();
   update_range_initialize    ();
   editor_notes_initialize    ();

   EditorPluginID = editor_plugin_register ();
   /* This is due to the WinCE auto sync */
   assert(EditorPluginID == 1);

   roadmap_layer_adjust ();
}


void editor_main_shutdown (void) {
   editor_gps_data_shutdown ();
   editor_db_close (roadmap_locator_active ());
}

void editor_main_set (int status) {

   if (status && EditorEnabled) {
      return;
   } else if (!status && !EditorEnabled) {
      return;
   }

   EditorEnabled = status;

   if (!EditorEnabled) {

      editor_track_end ();
   }

   editor_screen_set (status);
}


const char *editor_main_get_version (void) {

   return EDITOR_VERSION;
}
