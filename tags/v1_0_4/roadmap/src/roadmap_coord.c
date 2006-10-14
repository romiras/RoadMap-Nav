/* roadmap_coord.c - manage the roadmap Coordinates dialog.
 *
 * LICENSE:
 *
 *   Copyright 2003 Latchesar Ionkov
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
 *   See roadmap_coord.h
 */

#include <string.h>
#include <stdlib.h>

#include "roadmap.h"
#include "roadmap_types.h"
#include "roadmap_math.h"
#include "roadmap_config.h"
#include "roadmap_gui.h"
#include "roadmap_street.h"
#include "roadmap_county.h"
#include "roadmap_locator.h"
#include "roadmap_trip.h"
#include "roadmap_screen.h"
#include "roadmap_messagebox.h"
#include "roadmap_dialog.h"
#include "roadmap_display.h"
#include "roadmap_history.h"
#include "roadmap_preferences.h"

#include "roadmap_coord.h"


static void *RoadMapCoordHistory;


static int roadmap_coord_to_binary (const char *image) {

   int value;
   char *s;

   if (strchr (image, '.') != NULL) {
      value = (int) (strtod(image, &s) * 1000000);
      if (s != NULL) {
         return 181000000; /* Invalid on purpose. */
      }
   } else {
      value = atoi(image);
   }

   return value;
}


static void roadmap_coord_set (void) {

   char *argv[2];

   roadmap_history_get ('C', RoadMapCoordHistory, argv);

   roadmap_dialog_set_data
      ("Coordinates", "Longitude (decimal degrees):", argv[0]);
   roadmap_dialog_set_data
      ("Coordinates", "Latitude (decimal degrees):", argv[1]);
}


static void roadmap_coord_before (const char *name, void *data) {

   RoadMapCoordHistory = roadmap_history_before ('C', RoadMapCoordHistory);

   roadmap_coord_set ();
}


static void roadmap_coord_after (const char *name, void *data) {

   RoadMapCoordHistory = roadmap_history_after ('C', RoadMapCoordHistory);

   roadmap_coord_set ();
}



static void roadmap_coord_ok (const char *name, void *data) {

   const char *argv[2];

   RoadMapPosition position;


   argv[0] = (const char*) roadmap_dialog_get_data
                              ("Coordinates", "Longitude (decimal degrees):");
   position.longitude = roadmap_coord_to_binary (argv[0]);
   if (position.longitude > 180000000 || position.longitude < -180000000) {
	roadmap_messagebox("Warning", "Invalid longitude value");
	return;
   }

   argv[1] = (const char*) roadmap_dialog_get_data
                              ("Coordinates", "Latitude (decimal degrees):");
   position.latitude = roadmap_coord_to_binary (argv[1]);
   if (position.latitude > 90000000 || position.latitude < -90000000) {
	roadmap_messagebox("Warning", "Invalid latitude value");
	return;
   }

   roadmap_trip_set_point ("Selection", &position);
   roadmap_trip_set_point ("Address", &position);
   roadmap_trip_set_focus ("Address", 0);
   roadmap_screen_refresh ();

   roadmap_history_add ('C', argv);
   roadmap_dialog_hide (name);
}


static void roadmap_coord_cancel (const char *name, void *data) {

   roadmap_dialog_hide (name);
}


void roadmap_coord_dialog (void) {

   if (roadmap_dialog_activate ("Position", NULL)) {

      roadmap_dialog_new_entry ("Coordinates", "Longitude (decimal degrees):");
      roadmap_dialog_new_entry ("Coordinates", "Latitude (decimal degrees):");

      roadmap_dialog_add_button ("Back", roadmap_coord_before);
      roadmap_dialog_add_button ("Next", roadmap_coord_after);
      roadmap_dialog_add_button ("OK", roadmap_coord_ok);
      roadmap_dialog_add_button ("Cancel", roadmap_coord_cancel);

      roadmap_dialog_complete (roadmap_preferences_use_keyboard());

      roadmap_history_declare ('C', 2);
   }

   RoadMapCoordHistory = roadmap_history_latest ('C');

   roadmap_coord_set ();
}
