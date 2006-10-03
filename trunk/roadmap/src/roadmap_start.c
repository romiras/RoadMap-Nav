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
#include "roadmap_string.h"
#include "roadmap_config.h"
#include "roadmap_history.h"

#include "roadmap_spawn.h"
#include "roadmap_path.h"
#include "roadmap_io.h"

#include "roadmap_object.h"
#include "roadmap_voice.h"
#include "roadmap_gps.h"

#include "roadmap_canvas.h"
#include "roadmap_time.h"
#include "roadmap_preferences.h"
#include "roadmap_address.h"
#include "roadmap_coord.h"
#include "roadmap_crossing.h"
#include "roadmap_sprite.h"
#include "roadmap_gpx.h"
#include "roadmap_trip.h"
#include "roadmap_track.h"
#include "roadmap_landmark.h"
#include "roadmap_features.h"
#include "roadmap_adjust.h"
#include "roadmap_screen.h"
#include "roadmap_fuzzy.h"
#include "roadmap_navigate.h"
#include "roadmap_label.h"
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
#include "roadmap_pointer.h"
#include "roadmap_layer.h"

#include "roadmap_start.h"

static const char *RoadMapMainTitle = "RoadMap";

static int RoadMapStartFrozen = 0;

static RoadMapDynamicString RoadMapStartGpsID;

static RoadMapConfigDescriptor RoadMapConfigGeneralUnit =
                        ROADMAP_CONFIG_ITEM("General", "Unit");

static RoadMapConfigDescriptor RoadMapConfigGeneralKeyboard =
                        ROADMAP_CONFIG_ITEM("General", "Keyboard");

static RoadMapConfigDescriptor RoadMapConfigGeometryMain =
                        ROADMAP_CONFIG_ITEM("Geometry", "Main");

static RoadMapConfigDescriptor RoadMapConfigMapPath =
                        ROADMAP_CONFIG_ITEM("Map", "Path");

static RoadMapConfigDescriptor RoadMapConfigDisplayRefresh =
                        ROADMAP_CONFIG_ITEM("Display", "Refresh Period");


#define ROADMAP_DEFAULT_REFRESH_INTERVAL 200


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

static void roadmap_start_show_start (void) {
    roadmap_trip_set_focus ("Start");
    roadmap_screen_refresh ();
}

static void roadmap_start_show_departure (void) {
    roadmap_trip_set_focus ("Departure");
    roadmap_screen_refresh ();
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
   roadmap_screen_hold ();
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

#ifdef ROADMAP_USES_EXPAT
static void roadmap_start_open_trip (void) {
    roadmap_trip_load_ask (0);
}

static void roadmap_start_merge_trip (void) {
    roadmap_trip_load_ask (1);
}

static void roadmap_start_save_trip (void) {
    roadmap_trip_save (1);
}

static void roadmap_start_save_trip_as (void) {
    roadmap_trip_save_as (1);
}

static void roadmap_start_save_track (void) {
    roadmap_track_save();
}

static void roadmap_start_merge_landmark (void) {
    roadmap_landmark_merge ();
}

#else // ROADMAP_USES_EXPAT

static void roadmap_start_no_expat (void) {
    roadmap_log (ROADMAP_ERROR,
                 "This feature is not available (no expat library)");
}

static void roadmap_start_open_trip (void) { roadmap_start_no_expat(); }

static void roadmap_start_merge_trip (void) { roadmap_start_no_expat(); }

static void roadmap_start_save_trip (void) { roadmap_start_no_expat(); }

static void roadmap_start_save_trip_as (void) { roadmap_start_no_expat(); }

static void roadmap_start_save_track (void) { roadmap_start_no_expat(); }

static void roadmap_start_merge_landmark (void) { roadmap_start_no_expat(); }

#endif // ROADMAP_USES_EXPAT

static void roadmap_start_route (void) {
    
    roadmap_trip_route_start ();
}

static void roadmap_start_route_resume (void) {
    
    roadmap_trip_route_resume ();
}

static void roadmap_start_route_reverse (void) {
    
    roadmap_trip_route_reverse ();
}

static void roadmap_start_route_return (void) {

    roadmap_trip_route_return ();
}

static void roadmap_start_create_route (void) {

    roadmap_trip_new_route ();
}

static void roadmap_start_create_waypoint (void) {

    roadmap_trip_create_selection_waypoint ();
    roadmap_screen_refresh();
}

static void roadmap_start_create_gps_waypoint (void) {

    roadmap_trip_create_gps_waypoint ();
    roadmap_screen_refresh();
}

static int roadmap_start_no_download (int fips) {

   if (! roadmap_download_blocked (fips)) {
      roadmap_log (ROADMAP_WARNING, "cannot open map database usc%05d", fips);
      roadmap_download_block (fips);
   }
   return 0;
}

static void roadmap_start_toggle_download (void) {

   if (roadmap_download_enabled()) {

      roadmap_download_subscribe_when_done (NULL);
      roadmap_locator_declare (&roadmap_start_no_download);

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
      roadmap_download_unblock_all ();
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

   {"startpoint", "Route Start", "Start", NULL,
      "Show the current route starting point", roadmap_start_show_start},

   {"departure", "Departure", "Dep", NULL,
      "Show the route's point of departure", roadmap_start_show_departure},

   {"gps", "GPS Position", "GPS", "G",
      "Show the current GPS position", roadmap_start_show_gps},

   {"location", "Location", "L", NULL,
      "Show the last selected address, crossroads, or position",
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

   {"mergetrip", "Merge Trip", "Merge", "M",
      "Merge a trip with the current trip", roadmap_start_merge_trip},

   {"savetrip", "Save Trip", "Save", "S",
      "Save the current trip", roadmap_start_save_trip},

   {"savescreenshot", "Make a screenshot of the map", "Screenshot", "Y",
      "Make a screenshot of the current map under the trip name",
      roadmap_trip_save_screenshot},

   {"savetripas", "Save Trip As...", "Save As", "As",
      "Save the current trip under a different name",
      roadmap_start_save_trip_as},

   {"starttrip", "Start Route", "Start", NULL,
      "Start following the current route", roadmap_start_route},

   {"stoptrip", "Stop Route", "Stop", NULL,
      "Stop following the current route", roadmap_trip_route_stop},

   {"toggleview", "2D/3D View", "M", NULL,
      "Toggle view mode 2D / 3D", roadmap_screen_toggle_view_mode},

   {"togglelabels", "Show/Hide Street Labels", "Labels", NULL,
      "Show or Hide the names of streets", roadmap_screen_toggle_labels},

   {"toggleorientation", "Dynamic/Fixed Orientation", "", NULL,
      "Toggle orientation mode dynamic / fixed",
      roadmap_screen_toggle_orientation_mode},

   {"increasehorizon", "Increase Horizon", "I", NULL,
      "Increase the 3D horizon", roadmap_screen_increase_horizon},

   {"decreasehorizon", "Decrease Horizon", "DI", NULL,
      "Decrease the 3D horizon", roadmap_screen_decrease_horizon},

   {"tracktoggle", "Show/Hide Track", "Track", NULL,
      "Show or Hide the GPS breadcrumb track", roadmap_track_toggle_display},

   {"tracksave", "Save Current Track", "Save Track", NULL,
      "Save the current GPS breadcrumb track", roadmap_start_save_track},

   {"trackclear", "Clear Current Track", "Clear Track", NULL,
      "Clear the current GPS breadcrumb track", roadmap_track_clear},

   {"backtrackroute", "Create Backtrack Route", "BackTrack", NULL,
      "Convert the current GPS breadcrumb track to a new route",
      roadmap_trip_currenttrack_to_route },

   {"trackroute", "Convert Track to Route", "RouteToTrack", NULL,
      "Create a new route from the currently selected track",
      roadmap_trip_track_to_route },

   {"addtrack", "Add Current Track to Trip", "AddTrack", NULL,
      "Add a copy of the current GPS breadcrumb track to the trip",
      roadmap_trip_currenttrack_to_track },

   {"resumeroute", "Resume Route", "Resume", NULL,
      "Resume following (resync with) the current route",
      roadmap_start_route_resume},

   {"returnroute", "Return Route", "Return", NULL,
      "Start the route back to the departure point",
      roadmap_start_route_return},

   {"reverseroute", "Reverse Route", "Reverse", NULL,
      "Reverse the current route",
       roadmap_start_route_reverse},

   {"simplifyroute", "Simplify Route", NULL, NULL,
      "Create simplified version of current route",
      roadmap_trip_route_simplify },

   {"createroute", "New route using selection", NULL, NULL,
      "Start new route using last selected street or place",
      roadmap_start_create_route},

#if WGET_GOOGLE_ROUTE
   {"getgoogleroute", "Fetch route from google", NULL, NULL,
      "Fetch google route for current route's start/dest",
        roadmap_trip_replace_with_google_route},
#endif

   {"setasdestination", "Goto selection", NULL, NULL,
      "Show distance and direction to the last selected street or place",
      roadmap_trip_set_as_destination},

   {"manageroutes", "Manage Current Routes...", "Manage", NULL,
      "Select, rename, or delete routes", roadmap_trip_route_manage_dialog},

   {"allroutetoggle", "Show/Hide Inactive Routes", "AllRoutes", NULL,
      "Show or Hide currently inactive routes",
      roadmap_trip_toggle_show_inactive},

   {"gpsaswaypoint", "New place from GPS...", "GPS Place", "GW",
      "Create new place using current GPS position",
      roadmap_start_create_gps_waypoint},

   {"addaswaypoint", "New place from selected...", "Place", "W",
      "Create new place using last selected street or place",
      roadmap_start_create_waypoint},

   {"editroutewaypoints", "Route Waypoints...", NULL, NULL,
      "Edit current route's waypoints", roadmap_trip_route_waypoint_manage_dialog },

   {"addroutewaypoint", "Add Route Waypoint", NULL, NULL,
      "Add selection to the current route's waypoints", 
      roadmap_trip_insert_routepoint_best },

   {"edittripwaypoints", "Trip Landmarks...", NULL, NULL,
      "Edit landmarks associated with this trip", roadmap_trip_trip_waypoint_manage_dialog },

   {"editpersonalwaypoints", "Personal Landmarks...", NULL, NULL,
      "Edit personal landmarks", roadmap_trip_personal_waypoint_manage_dialog },

   {"mergepersonalwaypoints", "Load more Personal Landmarks...", NULL, NULL,
      "Merge personal landmarks from file", roadmap_start_merge_landmark },

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
   "savescreenshot",

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
   "togglelabels",
   "toggleorientation",
   "toggleview",
   "increasehorizon",
   "decreasehorizon",
   "full",

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

   "startpoint",
   "destination",
   "departure",
   "gps",

   RoadMapFactorySeparator,

   "mapdownload",
   "mapdiskspace",
   "deletemaps",


   ROADMAP_MENU "Trip",

   "newtrip",
   "opentrip",
   "mergetrip",
   "savetrip",
   "savetripas",

   ROADMAP_MENU "Routes",

   "manageroutes",
   "starttrip",
   "stoptrip",
   "resumeroute",
   "reverseroute",
   "simplifyroute",
   "trackroute",
   "createroute",
   "setasdestination",
   "allroutetoggle", 
#if WGET_GOOGLE_ROUTE
   "getgoogleroute", 
#endif

   ROADMAP_MENU "Places",

   "addaswaypoint",
   "gpsaswaypoint", 
   "addroutewaypoint",
   "editroutewaypoints",
   "edittripwaypoints",
   "editpersonalwaypoints",
   "mergepersonalwaypoints",

   RoadMapFactorySeparator,

   "tracksave",
   "trackclear",
   "backtrackroute",
   "addtrack",
   "tracktoggle",


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

   "Enter"           ROADMAP_MAPPED_TO "resumeroute",

   /* These binding are for the iPAQ buttons: */
   "Button-Menu"     ROADMAP_MAPPED_TO "zoom1",
   "Button-Contact"  ROADMAP_MAPPED_TO "zoomin",
   "Button-Calendar" ROADMAP_MAPPED_TO "zoomout",
   "Button-Start"    ROADMAP_MAPPED_TO "quit",

   /* These binding are for regular keyboards (case unsensitive !): */
   "+"               ROADMAP_MAPPED_TO "zoomin",
   "="               ROADMAP_MAPPED_TO "zoomin",
   "-"               ROADMAP_MAPPED_TO "zoomout",
   "A"               ROADMAP_MAPPED_TO "address",
   "B"               ROADMAP_MAPPED_TO "returnroute",
   /* C Unused. */
   "D"               ROADMAP_MAPPED_TO "destination",
   "E"               ROADMAP_MAPPED_TO "deletemaps",
   "F"               ROADMAP_MAPPED_TO "full",
   "G"               ROADMAP_MAPPED_TO "gps",
   "H"               ROADMAP_MAPPED_TO "hold",
   "I"               ROADMAP_MAPPED_TO "intersection",
   "J"               ROADMAP_MAPPED_TO "counterclockwise",
   "K"               ROADMAP_MAPPED_TO "clockwise",
   "L"               ROADMAP_MAPPED_TO "location",
   "M"               ROADMAP_MAPPED_TO "mapdownload",
   "N"               ROADMAP_MAPPED_TO "newtrip",
   "O"               ROADMAP_MAPPED_TO "opentrip",
   "P"               ROADMAP_MAPPED_TO "stoptrip",
   "Q"               ROADMAP_MAPPED_TO "quit",
   "R"               ROADMAP_MAPPED_TO "zoom1",
   "S"               ROADMAP_MAPPED_TO "starttrip",
   "T"               ROADMAP_MAPPED_TO "tracktoggle",
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

static void roadmap_gps_update
               (int gps_time,
                const RoadMapGpsPrecision *dilution,
                const RoadMapGpsPosition  *gps_position) {

   static int RoadMapSynchronous = -1;

   if (RoadMapStartFrozen) {

      RoadMapStartGpsRefresh = 0;

   } else {

      roadmap_object_move (RoadMapStartGpsID, gps_position);

      roadmap_trip_set_gps (gps_time, gps_position);
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
}


void roadmap_start_error (const char *text) {
   roadmap_messagebox ("Error", text);
}

void roadmap_start_fatal (const char *text) {
   roadmap_messagebox_wait ("Fatal Error", text);
}


static void roadmap_start_periodic (void) {

   roadmap_spawn_check ();

   if (RoadMapStartGpsRefresh) {

      if (roadmap_main_flush_synchronous (ROADMAP_DEFAULT_REFRESH_INTERVAL)) {

         RoadMapStartGpsRefresh = 0;

         roadmap_screen_refresh();
         roadmap_log_reset_stack ();
      }
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

char * roadmap_start_now() {

   return roadmap_time_get_hours_minutes (time(NULL));

}

void roadmap_start (int argc, char **argv) {

   int period;

#ifdef ROADMAP_DEBUG_HEAP
   /* Do not forget to set the trace file using the env. variable MALLOC_TRACE,
    * then use the mtrace tool to analyze the output.
    */
   mtrace();
#endif

   roadmap_log (ROADMAP_WARNING, "RoadMap starting, time %s", roadmap_start_now());
   roadmap_log_redirect (ROADMAP_MESSAGE_ERROR, roadmap_start_error);
   roadmap_log_redirect (ROADMAP_MESSAGE_FATAL, roadmap_start_fatal);

   roadmap_config_initialize ();

   roadmap_config_declare_enumeration
      ("preferences", &RoadMapConfigGeneralUnit, "imperial", "metric", NULL);
   roadmap_config_declare_enumeration
      ("preferences", &RoadMapConfigGeneralKeyboard, "yes", "no", NULL);

   roadmap_config_declare
      ("preferences", &RoadMapConfigGeometryMain, "800x600");

   roadmap_config_declare
      ("preferences", &RoadMapConfigDisplayRefresh, "");

   roadmap_config_declare
      ("preferences", &RoadMapConfigMapPath, "");

   roadmap_option_initialize   ();
   roadmap_math_initialize     ();
   roadmap_trip_initialize     ();
   roadmap_track_initialize    ();
   roadmap_landmark_initialize ();
   roadmap_features_initialize ();
   roadmap_pointer_initialize  ();
   roadmap_screen_initialize   ();
   roadmap_fuzzy_initialize    ();
   roadmap_navigate_initialize ();
   roadmap_label_initialize    ();
   roadmap_display_initialize  ();
   roadmap_voice_initialize    ();
   roadmap_gps_initialize      ();
   roadmap_history_initialize  ();
   roadmap_download_initialize ();
   roadmap_adjust_initialize   ();
   roadmap_driver_initialize   ();
   roadmap_layer_initialize    ();

   roadmap_config_load ();

   roadmap_gps_register_listener (&roadmap_gps_update);

   RoadMapStartGpsID = roadmap_string_new("GPS");

   roadmap_object_add (roadmap_string_new("RoadMap"),
                       RoadMapStartGpsID,
                       NULL,
                       NULL);

   if (roadmap_config_get(&RoadMapConfigMapPath)[0] != 0) {
      roadmap_path_set("maps", roadmap_config_get(&RoadMapConfigMapPath));
   }

   roadmap_factory_keymap (RoadMapStartActions, RoadMapStartKeyBinding);

   roadmap_option (argc, argv, roadmap_start_usage);

   roadmap_start_set_unit ();
   
   roadmap_math_restore_zoom ();
   roadmap_start_window      ();
   roadmap_label_activate    ();
   roadmap_sprite_load       ();
   roadmap_layer_load        ();

   roadmap_screen_set_initial_position ();

   roadmap_history_load ();

#ifdef ROADMAP_USES_EXPAT
   roadmap_track_autoload ();
   roadmap_landmark_load ();
   roadmap_features_load ();
#endif

   roadmap_spawn_initialize (argv[0]);
   
   roadmap_driver_activate ();
   roadmap_gps_open ();

   roadmap_help_initialize ();

   roadmap_screen_subscribe_after_refresh (roadmap_start_after_refresh);

   roadmap_trip_restore_focus ();

#ifdef ROADMAP_USES_EXPAT
   if ( ! roadmap_trip_load (1, 0)) {
      roadmap_start_create_trip ();
   }
#else
   roadmap_start_create_trip ();
#endif

   roadmap_locator_declare (&roadmap_start_no_download);

   period = atoi(roadmap_config_get(&RoadMapConfigDisplayRefresh));
   if (period <= 10) {
      period = ROADMAP_DEFAULT_REFRESH_INTERVAL;
   }
   roadmap_main_set_periodic (period, roadmap_start_periodic);
}


void roadmap_start_exit (void) {
    
    roadmap_driver_shutdown ();
    roadmap_history_save();
#ifdef ROADMAP_USES_EXPAT
    roadmap_track_autosave ();
    roadmap_landmark_save ();
    roadmap_trip_save (0);
#endif
    roadmap_config_save (0);
    roadmap_log (ROADMAP_WARNING, "RoadMap exiting, time %s", roadmap_start_now());
}
