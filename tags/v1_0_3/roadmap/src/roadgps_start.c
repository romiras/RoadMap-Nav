/* roadgps_start.c - A small tool to show GPS status and log NMEA messages.
 *
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
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
 *   see roadmap_start.h
 */

#include <stdlib.h>
#include <string.h>

#ifdef ROADMAP_DEBUG_HEAP
#include <mcheck.h>
#endif

#include "roadmap.h"
#include "roadmap_config.h"
#include "roadmap_gps.h"
#include "roadmap_factory.h"
#include "roadmap_math.h"
#include "roadmap_main.h"

#include "roadgps_logger.h"
#include "roadgps_screen.h"

#include "roadmap_start.h"


static const char *RoadGpsMainTitle = "GPS Console";

static RoadMapConfigDescriptor RoadMapConfigGeneralToolbar =
                        ROADMAP_CONFIG_ITEM("General", "Toolbar");

/* The RoadGps menu and toolbar items: ----------------------------------- */

static RoadMapFactory RoadGpsStartMenu[] = {

   {"File", NULL, NULL, NULL},
   {"Quit", NULL, "Quit RoadMap", roadmap_main_exit},

   {"Log", NULL, NULL, NULL},
   {"Start", NULL, "Start recording GPS messages", roadgps_logger_start},
   {"Stop",  NULL, "Stop recording GPS messages",  roadgps_logger_stop},

   {NULL, NULL, NULL, NULL}   
};

static RoadMapFactory RoadGpsStartToolbar[] = {

   {"Quit", NULL, "Quit RoadMap", roadmap_main_exit},
   {"Start", NULL, "Start recording GPS messages", roadgps_logger_start},
   {"Stop",  NULL, "Stop recording GPS messages",  roadgps_logger_stop},

   {NULL, NULL, NULL, NULL}   
};

static RoadMapFactory RoadGpsStartKeyBinding[] = {

   {"Button-Start", NULL, "Quit RoadGps", roadmap_main_exit},

   {"Q", NULL, "Quit RoadGps", roadmap_main_exit},
   {"R", NULL, "Start recording GPS messages", roadgps_logger_start},
   {"S", NULL, "Stop recording GPS messages",  roadgps_logger_stop},

   {NULL, NULL, NULL, NULL}
};


static void roadgps_start_usage (void) {

   RoadMapFactory *cursor;

   printf ("RoadGps button & key bindings:\n\n");
   for (cursor = RoadGpsStartKeyBinding; cursor->name != NULL; ++cursor) {
      printf ("  %-20.20s\t%s.\n", cursor->name, cursor->tip);
   }
}


static void roadgps_start_add_gps (int fd) {

   roadmap_main_set_input (fd, roadmap_gps_input);
}


static void roadgps_start_set_unit (void) {

   static RoadMapConfigDescriptor RoadMapConfigGeneralUnit =
                            ROADMAP_CONFIG_ITEM("General", "Unit");

   const char *unit = roadmap_config_get (&RoadMapConfigGeneralUnit);

   if (strcmp (unit, "imperial") == 0) {

      roadmap_math_use_imperial();

   } else if (strcmp (unit, "metric") == 0) {

      roadmap_math_use_metric();

   } else {
      roadmap_log (ROADMAP_ERROR, "%s is not a supported unit", unit);
      roadmap_math_use_imperial();
   }
}


static void roadgps_start_set_timeout (RoadMapCallback callback) {

   roadmap_main_set_periodic (3000, callback);
}


static void roadgps_start_window (void) {

   roadmap_main_new (RoadGpsMainTitle, 300, 420);

   roadmap_factory (RoadGpsStartMenu,
                    RoadGpsStartToolbar,
                    RoadGpsStartKeyBinding);

   roadmap_main_add_canvas ();
   roadmap_main_add_status ();

   roadmap_main_show ();

   roadmap_gps_register_link_control
      (roadgps_start_add_gps, roadmap_main_remove_input);

   roadmap_gps_register_periodic_control
      (roadgps_start_set_timeout, roadmap_main_remove_periodic);
}


const char *roadmap_start_get_title (const char *name) {

   static char *RoadGpsMainTitleBuffer = NULL;

   int length;


   if (name == NULL) {
      return RoadGpsMainTitle;
   }

   length = strlen(RoadGpsMainTitle) + strlen(name) + 4;

   if (RoadGpsMainTitleBuffer != NULL) {
         free(RoadGpsMainTitleBuffer);
   }
   RoadGpsMainTitleBuffer = malloc (length);

   if (RoadGpsMainTitleBuffer != NULL) {

      strcpy (RoadGpsMainTitleBuffer, RoadGpsMainTitle);
      strcat (RoadGpsMainTitleBuffer, ": ");
      strcat (RoadGpsMainTitleBuffer, name);
      return RoadGpsMainTitleBuffer;
   }

   return name;
}


void roadmap_start_exit (void) {
    
    roadmap_config_save (0);
}


void roadmap_start (int argc, char **argv) {

#ifdef ROADMAP_DEBUG_HEAP
   // Do not forget to set the trace file using the env. variable MALLOC_TRACE.
   mtrace();
#endif

   roadmap_config_declare_enumeration
      ("preferences", &RoadMapConfigGeneralToolbar, "yes", "no", NULL);

   roadmap_gps_initialize    (NULL);
   roadgps_screen_initialize ();
   roadmap_config_initialize ();

   roadmap_option (argc, argv, roadgps_start_usage);

   roadgps_logger_initialize ();

   roadgps_start_set_unit ();

   roadgps_start_window ();

   roadmap_gps_open ();
}
