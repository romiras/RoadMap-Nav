/* roadmap_start.c - The main function of the RoadMap application.
 *
 * LICENSE:
 *
 *   (c) Copyright 2002, 2003 Pascal F. Martin
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
 *   void roadmap_start (int argc, char **argv);
 */

#include <stdlib.h>
#include <string.h>

#ifdef ROADMAP_DEBUG_HEAP
#include <mcheck.h>
#endif

#include "roadmap.h"
#include "roadmap_copyright.h"
#include "roadmap_dbread.h"
#include "roadmap_math.h"
#include "roadmap_config.h"
#include "roadmap_history.h"

#include "roadmap_spawn.h"
#include "roadmap_path.h"
#include "roadmap_io.h"

#include "roadmap_voice.h"
#include "roadmap_gps.h"

#include "roadmap_preferences.h"
#include "roadmap_address.h"
#include "roadmap_coord.h"
#include "roadmap_crossing.h"
#include "roadmap_sprite.h"
#include "roadmap_object.h"
#include "roadmap_trip.h"
#include "roadmap_adjust.h"
#include "roadmap_screen.h"
#include "roadmap_fuzzy.h"
#include "roadmap_navigate.h"
#include "roadmap_display.h"
#include "roadmap_locator.h"
#include "roadmap_copy.h"
#include "roadmap_httpcopy.h"
#include "roadmap_download.h"
#include "roadmap_driver.h"
#include "roadmap_factory.h"
#include "roadmap_main.h"
#include "roadmap_messagebox.h"
#include "roadmap_help.h"


static const char *RoadMapMainTitle = "RoadMap";

static int RoadMapStartFrozen = 0;


static RoadMapConfigDescriptor RoadMapConfigGeneralUnit =
                        ROADMAP_CONFIG_ITEM("General", "Unit");

static RoadMapConfigDescriptor RoadMapConfigGeneralKeyboard =
                        ROADMAP_CONFIG_ITEM("General", "Keyboard");

static RoadMapConfigDescriptor RoadMapConfigGeometryMain =
                        ROADMAP_CONFIG_ITEM("Geometry", "Main");

static RoadMapConfigDescriptor RoadMapConfigMapPath =
                        ROADMAP_CONFIG_ITEM("Map", "Path");


/* The menu and toolbar callbacks: --------------------------------------- */

static void roadmap_start_periodic (void);

static void roadmap_start_console (void) {

   const char *url = roadmap_gps_source();

   if (url == NULL) {
      roadmap_spawn ("roadgps", "");
   } else {
      char arguments[1024];
      snprintf (arguments, sizeof(arguments), "--gps=%s", url);
      roadmap_spawn ("roadgps", arguments);
   }
}

static void roadmap_start_purge (void) {
   roadmap_history_purge (10);
}

static void roadmap_start_show_destination (void) {
    roadmap_trip_set_focus ("Destination");
    roadmap_screen_refresh ();
}

static void roadmap_start_show_location (void) {
    roadmap_trip_set_focus ("Address");
    roadmap_screen_refresh ();
}

static void roadmap_start_show_gps (void) {
    roadmap_trip_set_focus ("GPS");
    roadmap_screen_refresh ();
}

static void roadmap_start_hold_map (void) {
   roadmap_start_periodic (); /* To make sure the map is current. */
   roadmap_trip_copy_focus ("Hold");
   roadmap_trip_set_focus ("Hold");
}

static void roadmap_start_rotate (void) {
    roadmap_screen_rotate (10);
}

static void roadmap_start_counter_rotate (void) {
    roadmap_screen_rotate (-10);
}

static void roadmap_start_about (void) {

   roadmap_messagebox ("About",
                       "RoadMap " ROADMAP_VERSION "\n"
                       "(c) " ROADMAP_YEAR " Pascal Martin\n"
                       "<pascal.martin@iname.com>\n"
                       "A Street navigation system\n"
                       "for Linux & UNIX");
}

static void roadmap_start_create_trip (void) {
    
    roadmap_trip_new ();
}

static void roadmap_start_open_trip (void) {
    
    roadmap_trip_load (NULL, 0);
}

static void roadmap_start_save_trip (void) {
    
    roadmap_trip_save (roadmap_trip_current());
}

static void roadmap_start_save_trip_as (void) {
    
    roadmap_trip_save (NULL);
}

static void roadmap_start_trip (void) {
    
    roadmap_trip_start ();
}

static void roadmap_start_trip_resume (void) {
    
    roadmap_trip_resume ();
}

static void roadmap_start_trip_reverse (void) {
    
    roadmap_trip_reverse ();
}

static void roadmap_start_set_destination (void) {

    roadmap_trip_set_selection_as ("Destination");
    roadmap_screen_refresh();
}

static void roadmap_start_set_waypoint (void) {

    const char *id = roadmap_display_get_id ("Selected Street");

    if (id != NULL) {
       roadmap_trip_set_selection_as (id);
       roadmap_screen_refresh();
    }
}

static void roadmap_start_delete_waypoint (void) {
    
    roadmap_trip_remove_point (NULL);
}

static void roadmap_start_toggle_download (void) {

   if (roadmap_download_enabled()) {

      roadmap_download_subscribe_when_done (NULL);
      roadmap_locator_declare (NULL);

   } else {

      static int ProtocolInitialized = 0;

      if (! ProtocolInitialized) {

         /* PLUGINS NOT SUPPORTED YET.
          * roadmap_plugin_load_all
          *      ("download", roadmap_download_subscribe_protocol);
          */

         roadmap_copy_init (roadmap_download_subscribe_protocol);
         roadmap_httpcopy_init (roadmap_download_subscribe_protocol);

         ProtocolInitialized = 1;
      }

      roadmap_download_subscribe_when_done (roadmap_screen_redraw);
      roadmap_locator_declare (roadmap_download_get_county);
   }

   roadmap_screen_redraw ();
}


/* The RoadMap menu and toolbar items: ----------------------------------- */

/* This table lists all the RoadMap actions that can be initiated
 * fom the user interface (a sort of symbol table).
 * Any other part of the user interface (menu, toolbar, etc..)
 * will reference an action.
 */
static RoadMapAction RoadMapStartActions[] = {

   {"preferences", "Preferences", "Preferences", "P",
      "Open the preferences editor", roadmap_preferences_edit},

   {"gpsconsole", "GPS Console", "Console", "C",
      "Start the GPS console application", roadmap_start_console},

   {"mutevoice", "Mute Voice", "Mute", NULL,
      "Mute all voice annoucements", roadmap_voice_mute},

   {"enablevoice", "Enable Voice", "Mute Off", NULL,
      "Enable all voice annoucements", roadmap_voice_enable},

   {"nonavigation", "Disable Navigation", "Nav Off", NULL,
      "Disable all navigation functions", roadmap_navigate_disable},

   {"navigation", "Enable Navigation", "Nav On", NULL,
      "Enable all GPS-based navigation functions", roadmap_navigate_enable},

   {"logtofile", "Log to File", "Log", NULL,
      "Save future log messages to the postmortem file",
      roadmap_log_save_all},

   {"nolog", "Disable Log", "No Log", NULL,
      "Do not save future log messages to the postmortem file",
      roadmap_log_save_all},

   {"purgelogfile", "Purge Log File", "Purge", NULL,
      "Delete the current postmortem log file", roadmap_log_purge},

   {"purgehistory", "Purge History", "Forget", NULL,
      "Remove all but the 10 most recent addresses", roadmap_start_purge},

   {"quit", "Quit", NULL, NULL,
      "Quit RoadMap", roadmap_main_exit},

   {"zoomin", "Zoom In", "+", NULL,
      "Enlarge the central part of the map", roadmap_screen_zoom_in},

   {"zoomout", "Zoom Out", "-", NULL,
      "Show a larger area", roadmap_screen_zoom_out},

   {"zoom1", "Normal Size", ":1", NULL,
      "Set the map back to the default zoom level", roadmap_screen_zoom_reset},

   {"up", "Up", "N", NULL,
      "Move the map view upward", roadmap_screen_move_up},

   {"left", "Left", "W", NULL,
      "Move the map view to the left", roadmap_screen_move_left},

   {"right", "Right", "E", NULL,
      "Move the map view to the right", roadmap_screen_move_right},

   {"down", "Down", "S", NULL,
      "Move the map view downward", roadmap_screen_move_down},

   {"clockwise", "Rotate Clockwise", "R+", NULL,
      "Rotate the map view clockwise", roadmap_start_rotate},

   {"counterclockwise", "Rotate Counter-Clockwise", "R-", NULL,
      "Rotate the map view counter-clockwise", roadmap_start_counter_rotate},

   {"hold", "Hold Map", "Hold", "H",
      "Hold the map view in its current position", roadmap_start_hold_map},

   {"address", "Address...", "Addr", "A",
      "Show a specified address", roadmap_address_location_by_city},

   {"intersection", "Intersection...", "X", NULL,
      "Show a specified street intersection", roadmap_crossing_dialog},

   {"position", "Position...", "P", NULL,
      "Show a position at the specified coordinates", roadmap_coord_dialog},

   {"destination", "Destination", "D", NULL,
      "Show the current destination point", roadmap_start_show_destination},

   {"gps", "GPS Position", "GPS", "G",
      "Center the map on the current GPS position", roadmap_start_show_gps},

   {"location", "Location", "L", NULL,
      "Center the map on the last selected location",
      roadmap_start_show_location},

   {"mapdownload", "Map Download", "Download", NULL,
      "Enable/Disable the map download mode", roadmap_start_toggle_download},

   {"mapdiskspace", "Map Disk Space", "Disk", NULL,
      "Show the amount of disk space occupied by the maps",
      roadmap_download_show_space},

   {"deletemaps", "Delete Maps...", "Delete", "Del",
      "Delete maps that are currently visible", roadmap_download_delete},

   {"newtrip", "New Trip", "New", NULL,
      "Create a new trip", roadmap_start_create_trip},

   {"opentrip", "Open Trip", "Open", "O",
      "Open an existing trip", roadmap_start_open_trip},

   {"savetrip", "Save Trip", "Save", "S",
      "Save the current trip", roadmap_start_save_trip},

   {"savescreenshot", "Make a screenshot of the map", "Screenshot", "Y",
      "Make a screenshot of the current map under the trip name",
      roadmap_trip_save_screenshot},

   {"savetripas", "Save Trip As...", "Save As", "As",
      "Save the current trip under a different name",
      roadmap_start_save_trip_as},

   {"starttrip", "Start Trip", "Start", NULL,
      "Start tracking the current trip", roadmap_start_trip},

   {"stoptrip", "Stop Trip", "Stop", NULL,
      "Stop tracking the current trip", roadmap_trip_stop},

   {"resumetrip", "Resume Trip", "Resume", NULL,
      "Resume the trip (keep the existing departure point)",
      roadmap_start_trip_resume},

   {"returntrip", "Return Trip", "Return", NULL,
      "Start the trip back to the departure point",
      roadmap_start_trip_reverse},

   {"setasdestination", "Set as Destination", NULL, NULL,
      "Set the selected street block as the trip's destination",
      roadmap_start_set_destination},

   {"addaswaypoint", "Add as Waypoint", "Waypoint", "W",
      "Set the selected street block as waypoint", roadmap_start_set_waypoint},

   {"deletewaypoints", "Delete Waypoints...", "Delete...", NULL,
      "Delete selected waypoints", roadmap_start_delete_waypoint},

   {"full", "Full Screen", "Full", "F",
      "Toggle the window full screen mode (depends on the window manager)",
      roadmap_main_toggle_full_screen},

   {"about", "About", NULL, NULL,
      "Show information about RoadMap", roadmap_start_about},

   {NULL, NULL, NULL, NULL, NULL, NULL}
};


static const char *RoadMapStartMenu[] = {

   ROADMAP_MENU "File",

   "preferences",
   "gpsconsole",

   RoadMapFactorySeparator,

   "mutevoice",
   "enablevoice",
   "nonavigation",
   "navigation",

   RoadMapFactorySeparator,

   "logtofile",
   "nolog",
   "purgehistory",

   RoadMapFactorySeparator,

   "quit",


   ROADMAP_MENU "View",

   "zoomin",
   "zoomout",
   "zoom1",

   RoadMapFactorySeparator,

   "up",
   "left",
   "right",
   "down",

   RoadMapFactorySeparator,

   "clockwise",
   "counterclockwise",

   RoadMapFactorySeparator,

   "hold",


   ROADMAP_MENU "Find",

   "address",
   "intersection",
   "position",

   RoadMapFactorySeparator,

   "destination",
   "gps",

   RoadMapFactorySeparator,

   "mapdownload",
   "mapdiskspace",
   "deletemaps",


   ROADMAP_MENU "Trip",

   "newtrip",
   "opentrip",
   "savetrip",
   "savetripas",
   "savescreenshot",

   RoadMapFactorySeparator,

   "starttrip",
   "stoptrip",
   "resumetrip",
   "resumetripnorthup",
   "returntrip",

   RoadMapFactorySeparator,

   "setasdestination",
   "addaswaypoint",
   "deletewaypoints",


   ROADMAP_MENU "Help",

   RoadMapFactoryHelpTopics,

   RoadMapFactorySeparator,

   "about",

   NULL
};


static char const *RoadMapStartToolbar[] = {

   "destination",
   "location",
   "gps",
   "hold",

   RoadMapFactorySeparator,

   "counterclockwise",
   "clockwise",

   "zoomin",
   "zoomout",
   "zoom1",

   RoadMapFactorySeparator,

   "up",
   "left",
   "right",
   "down",

   RoadMapFactorySeparator,

   "full",
   "quit",

   NULL,
};


static char const *RoadMapStartKeyBinding[] = {

   "Button-Left"     ROADMAP_MAPPED_TO "left",
   "Button-Right"    ROADMAP_MAPPED_TO "right",
   "Button-Up"       ROADMAP_MAPPED_TO "up",
   "Button-Down"     ROADMAP_MAPPED_TO "down",

   /* These binding are for the iPAQ buttons: */
   "Button-Menu"     ROADMAP_MAPPED_TO "zoom1",
   "Button-Contact"  ROADMAP_MAPPED_TO "zoomin",
   "Button-Calendar" ROADMAP_MAPPED_TO "zoomout",
   "Button-Start"    ROADMAP_MAPPED_TO "quit",

   /* These binding are for regular keyboards (case unsensitive !): */
   "+"               ROADMAP_MAPPED_TO "zoomin",
   "-"               ROADMAP_MAPPED_TO "zoomout",
   "A"               ROADMAP_MAPPED_TO "address",
   "B"               ROADMAP_MAPPED_TO "returntrip",
   /* C Unused. */
   "D"               ROADMAP_MAPPED_TO "destination",
   "E"               ROADMAP_MAPPED_TO "deletemaps",
   "F"               ROADMAP_MAPPED_TO "full",
   "G"               ROADMAP_MAPPED_TO "gps",
   "H"               ROADMAP_MAPPED_TO "hold",
   "I"               ROADMAP_MAPPED_TO "intersection",
   /* J Unused. */
   /* K Unused. */
   "L"               ROADMAP_MAPPED_TO "location",
   "M"               ROADMAP_MAPPED_TO "mapdownload",
   "N"               ROADMAP_MAPPED_TO "newtrip",
   "O"               ROADMAP_MAPPED_TO "opentrip",
   "P"               ROADMAP_MAPPED_TO "stoptrip",
   "Q"               ROADMAP_MAPPED_TO "quit",
   "R"               ROADMAP_MAPPED_TO "zoom1",
   "S"               ROADMAP_MAPPED_TO "starttrip",
   /* T Unused. */
   "U"               ROADMAP_MAPPED_TO "gpsnorthup",
   /* V Unused. */
   "W"               ROADMAP_MAPPED_TO "addaswaypoint",
   "X"               ROADMAP_MAPPED_TO "intersection",
   "Y"               ROADMAP_MAPPED_TO "savesscreenshot",
   /* Z Unused. */
   NULL
};


static void roadmap_start_set_unit (void) {

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


static int RoadMapStartGpsRefresh = 0;

static void roadmap_gps_update (const RoadMapGpsPosition *gps_position) {

   static int RoadMapSynchronous = -1;

   if (RoadMapStartFrozen) {

      RoadMapStartGpsRefresh = 0;

   } else {

      roadmap_trip_set_mobile ("GPS", gps_position);
      roadmap_log_reset_stack ();

      roadmap_navigate_locate (gps_position);
      roadmap_log_reset_stack ();

      if (RoadMapSynchronous) {

         if (RoadMapSynchronous < 0) {
            RoadMapSynchronous = roadmap_option_is_synchronous ();
         }

         RoadMapStartGpsRefresh = 0;

         roadmap_screen_refresh();
         roadmap_log_reset_stack ();

      } else {

         RoadMapStartGpsRefresh = 1;
      }
   }

   roadmap_driver_publish (gps_position);
}


static void roadmap_start_periodic (void) {

   roadmap_spawn_check ();

   if (RoadMapStartGpsRefresh) {

      RoadMapStartGpsRefresh = 0;

      roadmap_screen_refresh();
      roadmap_log_reset_stack ();
   }
}


static void roadmap_start_add_gps (RoadMapIO *io) {

   roadmap_main_set_input (io, roadmap_gps_input);
}

static void roadmap_start_remove_gps (RoadMapIO *io) {

   roadmap_main_remove_input(io);
}


static void roadmap_start_add_driver (RoadMapIO *io) {

   roadmap_main_set_input (io, roadmap_driver_input);
}

static void roadmap_start_remove_driver (RoadMapIO *io) {

   roadmap_main_remove_input(io);
}


static void roadmap_start_add_driver_server (RoadMapIO *io) {

   roadmap_main_set_input (io, roadmap_driver_accept);
}

static void roadmap_start_remove_driver_server (RoadMapIO *io) {

   roadmap_main_remove_input(io);
}


static void roadmap_start_set_timeout (RoadMapCallback callback) {

   roadmap_main_set_periodic (3000, callback);
}


static void roadmap_start_window (void) {

   roadmap_main_new (RoadMapMainTitle,
                     roadmap_option_width("Main"),
                     roadmap_option_height("Main"));

   roadmap_factory ("roadmap",
                    RoadMapStartActions,
                    RoadMapStartMenu,
                    RoadMapStartToolbar);

   roadmap_main_add_canvas ();

   roadmap_main_show ();

   roadmap_gps_register_link_control
      (roadmap_start_add_gps, roadmap_start_remove_gps);

   roadmap_gps_register_periodic_control
      (roadmap_start_set_timeout, roadmap_main_remove_periodic);

   roadmap_driver_register_link_control
      (roadmap_start_add_driver, roadmap_start_remove_driver);

   roadmap_driver_register_server_control
      (roadmap_start_add_driver_server, roadmap_start_remove_driver_server);
}


const char *roadmap_start_get_title (const char *name) {

   static char *RoadMapMainTitleBuffer = NULL;

   int length;


   if (name == NULL) {
      return RoadMapMainTitle;
   }

   length = strlen(RoadMapMainTitle) + strlen(name) + 4;

   if (RoadMapMainTitleBuffer != NULL) {
         free(RoadMapMainTitleBuffer);
   }
   RoadMapMainTitleBuffer = malloc (length);

   if (RoadMapMainTitleBuffer != NULL) {

      strcpy (RoadMapMainTitleBuffer, RoadMapMainTitle);
      strcat (RoadMapMainTitleBuffer, ": ");
      strcat (RoadMapMainTitleBuffer, name);
      return RoadMapMainTitleBuffer;
   }

   return name;
}


static void roadmap_start_after_refresh (void) {

   if (roadmap_download_enabled()) {

      RoadMapGuiPoint download_point = {0, 20};

      download_point.x = roadmap_canvas_width() - 20;
      if (download_point.x < 0) {
         download_point.x = 0;
      }
      roadmap_sprite_draw
         ("Download", &download_point, 0 - roadmap_math_get_orientation());
   }
}


static void roadmap_start_usage (const char *section) {

   roadmap_factory_usage (section, RoadMapStartActions);
}


void roadmap_start_freeze (void) {

   RoadMapStartFrozen = 1;
   RoadMapStartGpsRefresh = 0;

   roadmap_screen_freeze ();
}

void roadmap_start_unfreeze (void) {

   RoadMapStartFrozen = 0;
   roadmap_screen_unfreeze ();
}


void roadmap_start (int argc, char **argv) {

#ifdef ROADMAP_DEBUG_HEAP
   /* Do not forget to set the trace file using the env. variable MALLOC_TRACE,
    * then use the mtrace tool to analyze the output.
    */
   mtrace();
#endif

   roadmap_config_declare_enumeration
      ("preferences", &RoadMapConfigGeneralUnit, "imperial", "metric", NULL);
   roadmap_config_declare_enumeration
      ("preferences", &RoadMapConfigGeneralKeyboard, "yes", "no", NULL);

   roadmap_config_declare
      ("preferences", &RoadMapConfigGeometryMain, "800x600");

   roadmap_option_initialize   ();
   roadmap_math_initialize     ();
   roadmap_trip_initialize     ();
   roadmap_screen_initialize   ();
   roadmap_fuzzy_initialize    ();
   roadmap_navigate_initialize ();
   roadmap_display_initialize  ();
   roadmap_voice_initialize    ();
   roadmap_gps_initialize      (&roadmap_gps_update);
   roadmap_history_initialize  ();
   roadmap_download_initialize ();
   roadmap_adjust_initialize   ();
   roadmap_driver_initialize   ();
   roadmap_config_initialize   ();

   roadmap_path_set("maps", roadmap_config_get(&RoadMapConfigMapPath));

   roadmap_factory_keymap (RoadMapStartActions, RoadMapStartKeyBinding);

   roadmap_option (argc, argv, roadmap_start_usage);

   roadmap_start_set_unit ();
   
   roadmap_math_restore_zoom ();
   roadmap_start_window      ();
   roadmap_sprite_initialize ();
   roadmap_object_initialize ();

   roadmap_screen_set_initial_position ();

   roadmap_history_load ();
   
   roadmap_driver_activate ();
   roadmap_gps_open ();

   roadmap_spawn_initialize (argv[0]);

   roadmap_help_initialize ();

   roadmap_screen_subscribe_after_refresh (roadmap_start_after_refresh);

   roadmap_trip_restore_focus ();

   if (! roadmap_trip_load (roadmap_trip_current(), 1)) {
      roadmap_start_create_trip ();
   }

   roadmap_main_set_periodic (200, roadmap_start_periodic);
}


void roadmap_start_exit (void) {
    
    roadmap_driver_shutdown ();
    roadmap_history_save();
    roadmap_config_save (0);
    roadmap_start_save_trip ();
}
