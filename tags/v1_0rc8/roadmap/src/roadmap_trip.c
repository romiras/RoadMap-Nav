/* roadmap_trip.c - Manage a trip: destination & waypoints.
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
 *   See roadmap_trip.h.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "roadmap.h"
#include "roadmap_types.h"
#include "roadmap_time.h"
#include "roadmap_file.h"
#include "roadmap_gui.h"
#include "roadmap_math.h"
#include "roadmap_gps.h"
#include "roadmap_config.h"
#include "roadmap_dialog.h"
#include "roadmap_fileselection.h"
#include "roadmap_canvas.h"
#include "roadmap_sprite.h"
#include "roadmap_screen.h"
#include "roadmap_message.h"
#include "roadmap_messagebox.h"
#include "roadmap_preferences.h"
#include "roadmap_display.h"

#include "roadmap_trip.h"

static RoadMapConfigDescriptor RoadMapConfigTripName =
                        ROADMAP_CONFIG_ITEM("Trip", "Name");

static RoadMapConfigDescriptor RoadMapConfigFocusName =
                        ROADMAP_CONFIG_ITEM("Focus", "Name");

static RoadMapConfigDescriptor RoadMapConfigFocusRotate =
                        ROADMAP_CONFIG_ITEM("Focus", "Rotate");


/* Default location is: 1 market St, san Francisco, California. */
#define ROADMAP_INITIAL_LONGITUDE -122394181
#define ROADMAP_INITIAL_LATITUDE    37794928


static int RoadMapTripRotate   = 1;
static int RoadMapTripModified = 0; /* List needs to be saved ? */
static int RoadMapTripRefresh  = 1; /* Screen needs to be refreshed ? */
static int RoadMapTripFocusChanged = 1;
static int RoadMapTripFocusMoved   = 1;


typedef struct roadmap_trip_point {

    RoadMapListItem link;
    
    char *id;
    const char *sprite;
    
    char predefined;
    char mobile;
    char in_trip;
    char has_value;

    RoadMapPosition position;

    RoadMapConfigDescriptor config_position;
    RoadMapConfigDescriptor config_direction;

    int speed;
    int direction;
    int distance;   /* .. from the destination point. */

} RoadMapTripPoint;


#define ROADMAP_TRIP_ITEM(id,sprite,mobile,in_trip, has_value) \
    {{NULL, NULL}, id, sprite, 1, mobile, in_trip, has_value, \
     {ROADMAP_INITIAL_LONGITUDE, ROADMAP_INITIAL_LATITUDE}, \
     ROADMAP_CONFIG_ITEM(id,"Position"), \
     ROADMAP_CONFIG_ITEM(id,"Direction"), \
     0, 0, 0 \
    }

RoadMapTripPoint RoadMapTripPredefined[] = {
    ROADMAP_TRIP_ITEM("GPS",         "GPS",         1, 0, 1),
    ROADMAP_TRIP_ITEM("Destination", "Destination", 0, 1, 0),
    ROADMAP_TRIP_ITEM("Address",     NULL,          0, 0, 1),
    ROADMAP_TRIP_ITEM("Selection",   NULL,          0, 0, 1),
    {{NULL, NULL}, NULL, NULL}
};


static RoadMapTripPoint *RoadMapTripGps = NULL;
static RoadMapTripPoint *RoadMapTripFocus = NULL;
static RoadMapTripPoint *RoadMapTripAddress = NULL;
static RoadMapTripPoint *RoadMapTripDeparture = NULL;
static RoadMapTripPoint *RoadMapTripDestination = NULL;
static RoadMapTripPoint *RoadMapTripNextWaypoint = NULL;

static RoadMapList RoadMapTripWaypoints = ROADMAP_LIST_EMPTY;

static RoadMapPosition RoadMapTripLastPosition;


static void roadmap_trip_unfocus (void) {
    
    if (RoadMapTripFocus != NULL) {
        
        RoadMapTripLastPosition = RoadMapTripFocus->position;
        RoadMapTripFocus = NULL;
    }
    RoadMapTripDeparture = NULL;
    RoadMapTripDestination = NULL;
}


static RoadMapTripPoint *roadmap_trip_search (const char *name) {
    
    RoadMapTripPoint *item;
    
    for (item = (RoadMapTripPoint *)RoadMapTripWaypoints.first;
         item != NULL;
         item = (RoadMapTripPoint *)item->link.next) {

        if (strcmp (item->id, name) == 0) {
            return item;
        }
    }
    return NULL;
}


static void roadmap_trip_coordinate (const RoadMapPosition *position, RoadMapGuiPoint *point) {
    
    if (roadmap_math_point_is_visible (position)) {
        roadmap_math_coordinate (position, point);
    } else {
        point->x = 32767;
        point->y = 32767;
    }
}


static RoadMapTripPoint *roadmap_trip_update (const char *name, const RoadMapPosition *position, const char *sprite) {

    RoadMapTripPoint *result;
    
    
    if (strchr (name, ',') != NULL) {
        
        /* Because we use a 'simplified' CSV format, we cannot have
         * commas in the name.
         */
        char *to;
        char *from;
        char *expurged = strdup (name);
            
        for (from = expurged, to = expurged; *from != 0; ++from) {
            if (*from != ',') {
                if (to != from) {
                    *to = *from;
                }
                ++to;
            }
        }
        *to = 0;
            
        result = roadmap_trip_update (expurged, position, sprite);
        free (expurged);
        
        return result;
    }
        
    result = roadmap_trip_search (name);
    if (result == NULL) {
        
        /* A new point: refresh is needed only if this point
         * is visible.
         */
        result = malloc (sizeof(RoadMapTripPoint));
        roadmap_check_allocated(result);

        result->id = strdup(name);
        roadmap_check_allocated(result->id);

        result->predefined = 0;
        result->in_trip = 1;
        result->sprite = sprite;
        result->mobile = 0;
        result->speed  = 0;
        result->direction = 0;
        
        if (roadmap_math_point_is_visible (position)) {
            RoadMapTripRefresh = 1;
        }
        
        roadmap_list_append (&RoadMapTripWaypoints, &result->link);
        RoadMapTripModified = 1;
        
    } else {
        
        /* An existing point: refresh is needed only if the point
         * moved in a visible fashion.
         */
        RoadMapGuiPoint point1;
        RoadMapGuiPoint point2;
        
        if (result->position.latitude != position->latitude ||
            result->position.longitude != position->longitude) {
            roadmap_trip_coordinate (position, &point1);
            roadmap_trip_coordinate (&result->position, &point2);
            if (point1.x != point2.x || point1.y != point2.y) {
                RoadMapTripRefresh = 1;
            }
            RoadMapTripModified = 1;
        
            if (result == RoadMapTripFocus) {
                RoadMapTripFocusMoved = 1;
            }
            result->distance = 0;
        }
    }
    
    result->position = *position;
    result->has_value = 1;

    if (result->predefined && (! result->in_trip)) {
       roadmap_config_set_position
           (&result->config_position, &result->position);
    }
    return result;
}


static void roadmap_trip_dialog_cancel (const char *name, void *data) {
    
    roadmap_dialog_hide (name);
}


static void roadmap_trip_file_dialog_ok (const char *filename, const char *mode) {
    
    if (mode[0] == 'w') {
        roadmap_trip_save (filename);
    } else {
        roadmap_trip_load (filename);
    }
}


static void roadmap_trip_file_dialog (const char *mode) {
    
    roadmap_fileselection_new ("RoadMap Trip",
                                NULL, /* no filter. */
                                roadmap_file_trips(),
                                mode,
                                roadmap_trip_file_dialog_ok);
}


static void roadmap_trip_set_dialog_ok (const char *name, void *data) {
    
    char *point_name = (char *) roadmap_dialog_get_data ("Name", "Name:");
    
    if (point_name[0] != 0) {
        roadmap_trip_set_point (point_name, (RoadMapPosition *)data);
        roadmap_dialog_hide (name);
    }
}


static void roadmap_trip_set_dialog (RoadMapPosition *position) {

    static RoadMapPosition point_position;

    point_position = *position;
    
    if (roadmap_dialog_activate ("Add Waypoint", &point_position)) {

        roadmap_dialog_new_entry  ("Name", "Name:");
        roadmap_dialog_add_button ("OK", roadmap_trip_set_dialog_ok);
        roadmap_dialog_add_button ("Cancel", roadmap_trip_dialog_cancel);

        roadmap_dialog_complete (roadmap_preferences_use_keyboard());
    }
}


static void roadmap_trip_remove_dialog_populate (int count);

static void roadmap_trip_remove_dialog_delete (const char *name, void *data) {
    
    int count;
    char *point_name = (char *) roadmap_dialog_get_data ("Names", ".Waypoints");
    
    if (point_name[0] != 0) {
        
        roadmap_trip_remove_point (point_name);
        
        count = roadmap_list_count (&RoadMapTripWaypoints);
        if (count > 0) {
            roadmap_trip_remove_dialog_populate (count);
        } else {
            roadmap_dialog_hide (name);
        }
    }
}


static void roadmap_trip_remove_dialog_selected (const char *name, void *data) {

   char *point_name = (char *) roadmap_dialog_get_data ("Names", ".Waypoints");

   if (point_name != NULL) {

      roadmap_trip_set_focus (point_name, 0);
      roadmap_screen_refresh ();
   }
}


static void roadmap_trip_remove_dialog_populate (int count) {

    static char **Names = NULL;

    int i;
    RoadMapListItem *item;
    RoadMapTripPoint *point;
    
    if (Names != NULL) {
       free (Names);
    }
    Names = calloc (count, sizeof(*Names));
    roadmap_check_allocated(Names);

    for (i = 0, item = RoadMapTripWaypoints.first; item != NULL; item = item->next) {
        point = (RoadMapTripPoint *)item;
        if (! point->predefined) {
            Names[i++] = point->id;
        }
    }
    
    roadmap_dialog_show_list
        ("Names", ".Waypoints", count, Names, (void **)Names,
         roadmap_trip_remove_dialog_selected);
}


static void roadmap_trip_remove_dialog (void) {
    
    int count;
    
    count = roadmap_list_count (&RoadMapTripWaypoints);
    if (count <= 0) {
        return; /* Nothing to delete. */
    }
    
    if (roadmap_dialog_activate ("Delete Waypoints", NULL)) {

        roadmap_dialog_new_list   ("Names", ".Waypoints");
        roadmap_dialog_add_button ("Delete", roadmap_trip_remove_dialog_delete);
        roadmap_dialog_add_button ("Done", roadmap_trip_dialog_cancel);

        roadmap_dialog_complete (0); /* No need for a keyboard. */
    }

    roadmap_trip_remove_dialog_populate (count);
}


static FILE *roadmap_trip_fopen (const char *name, const char *mode) {

    FILE *file;

    if (name == NULL) {
        roadmap_trip_file_dialog (mode);
        return NULL;
    }

    if (name [0] == '/') {
        file = roadmap_file_open (NULL, name, mode);
    } else {
        file = roadmap_file_open (roadmap_file_trips(), name, mode);
    }

    if (file != NULL) {
        roadmap_config_set (&RoadMapConfigTripName, name);
    }
    return file;
}


static void roadmap_trip_set_point_focus (RoadMapTripPoint *point, int rotate) {
    
    if (point == NULL) return;
        
    if (! point->mobile) {
        rotate = 0; /* Fixed point, no rotation. */
        RoadMapTripRefresh = 1;
        RoadMapTripFocusChanged = 1;
    }
    
    if (RoadMapTripRotate != rotate) {
        roadmap_config_set_integer (&RoadMapConfigFocusRotate, rotate);
        RoadMapTripRotate = rotate;
        RoadMapTripRefresh = 1;
    }
    if (RoadMapTripFocus != point) {
        roadmap_config_set (&RoadMapConfigFocusName, point->id);
        RoadMapTripFocus = point;
        RoadMapTripRefresh = 1;
        RoadMapTripFocusChanged = 1;
        roadmap_display_page (point->id);
    }
    if (point != RoadMapTripGps && RoadMapTripDestination != NULL) {
        RoadMapTripRefresh = 1;
        RoadMapTripDestination = NULL;
    }
}


static void roadmap_trip_activate (int rotate) {

    RoadMapTripPoint *destination;
    RoadMapTripPoint *waypoint;

 
    destination = RoadMapTripDestination;
    if (destination == NULL) return;

    /* Compute the distances to the destination. */
    
    for (waypoint = (RoadMapTripPoint *)RoadMapTripWaypoints.first;
         waypoint != NULL;
         waypoint = (RoadMapTripPoint *)waypoint->link.next) {

        if (! waypoint->predefined) {
            
            waypoint->distance =
                roadmap_math_distance
                    (&destination->position, &waypoint->position);
            
            roadmap_log (ROADMAP_DEBUG,
                            "Waypoint %s: distance to destination = %d %s",
                            waypoint->id,
                            waypoint->distance,
                            roadmap_math_distance_unit());
        }
    }
    destination->distance = 0;
    
    roadmap_trip_set_focus ("GPS", rotate);
    roadmap_screen_refresh ();
}


static void roadmap_trip_clear (void) {
    
    RoadMapTripPoint *point;
    RoadMapTripPoint *next;
    
    for (point = (RoadMapTripPoint *)RoadMapTripWaypoints.first;
         point != NULL;
         point = next) {
        
        next = (RoadMapTripPoint *)point->link.next;
        
        if (! point->predefined) {
        
            if (RoadMapTripFocus == point) {
                roadmap_trip_unfocus ();
            }
            if (roadmap_math_point_is_visible (&point->position)) {
                RoadMapTripRefresh = 1;
            }
            roadmap_list_remove (&RoadMapTripWaypoints, &point->link);
            free (point->id);
            free(point);
            RoadMapTripModified = 1;

        } else {

            point->has_value = 0;
        }
    }
    
    if (RoadMapTripModified) {
        roadmap_config_set (&RoadMapConfigTripName, "default");
    }
}


void roadmap_trip_set_point (const char *name, RoadMapPosition *position) {

    if (name == NULL) {
        roadmap_trip_set_dialog (position);
    } else {
        roadmap_trip_update (name, position, "Waypoint");
    }
}


void roadmap_trip_set_selection_as (const char *name) {

   static RoadMapTripPoint *Selection = NULL;

   if (Selection == NULL) {
      Selection = roadmap_trip_search ("Selection");
   }

   roadmap_trip_update (name, &Selection->position, "Waypoint");
}


void roadmap_trip_set_mobile (const char *name,
                              RoadMapPosition *position,
                              int speed,
                              int direction) {

    RoadMapTripPoint *result = roadmap_trip_update (name, position, "Mobile");

    result->mobile = 1;
    result->speed  = speed;

    if (speed > roadmap_gps_speed_accuracy()) {

       result->direction = direction;

       if (result->predefined && (! result->in_trip)) {
           roadmap_config_set_integer (&result->config_direction, direction);
       }
    }

    roadmap_screen_refresh();
}


void roadmap_trip_remove_point (const char *name) {
    
    RoadMapTripPoint *result;

    if (name == NULL) {
        roadmap_trip_remove_dialog ();
        return;
    }
    
    result = roadmap_trip_search (name);
    
    if (result == NULL) {
        roadmap_log (ROADMAP_ERROR, "cannot delete: point %s not found", name);
        return;
    }
    if (result->predefined) {
        roadmap_log (ROADMAP_ERROR, "cannot delete: point %s is predefined", name);
        return;
    }

    if (RoadMapTripFocus == result ||
        RoadMapTripDeparture == result ||
        RoadMapTripDestination == result) {
        roadmap_trip_unfocus ();
    }
    
    if (roadmap_math_point_is_visible (&result->position)) {
        RoadMapTripRefresh = 1;
    }
    
    roadmap_list_remove (&RoadMapTripWaypoints, &result->link);
    free (result->id);
    free(result);
    
    RoadMapTripModified = 1;

    roadmap_screen_refresh();
}


void  roadmap_trip_restore_focus (void) {

    int i;
    RoadMapTripPoint *focus;

    for (i = 0; RoadMapTripPredefined[i].id != NULL; ++i) {

        roadmap_list_append
            (&RoadMapTripWaypoints, &RoadMapTripPredefined[i].link);

        if (! RoadMapTripPredefined[i].in_trip) {

            roadmap_config_get_position
                (&RoadMapTripPredefined[i].config_position,
                 &RoadMapTripPredefined[i].position);

            if (RoadMapTripPredefined[i].mobile) {
                RoadMapTripPredefined[i].direction =
                    roadmap_config_get_integer
                        (&RoadMapTripPredefined[i].config_direction);
            }
        }
    }
    RoadMapTripGps     = roadmap_trip_search ("GPS");
    RoadMapTripAddress = roadmap_trip_search ("Address");

    focus = roadmap_trip_search (roadmap_config_get (&RoadMapConfigFocusName));
    if (focus == NULL) {
        focus = RoadMapTripGps;
    }
    roadmap_trip_set_point_focus (focus, (focus->mobile) ? 1 : 0);
    RoadMapTripFocusChanged = 0;
}


void roadmap_trip_set_focus (const char *name, int rotate) {
    
    RoadMapTripPoint *point = roadmap_trip_search (name);
    
    if (point == NULL) {
        roadmap_log
            (ROADMAP_ERROR, "cannot activate: point %s not found", name);
        return;
    }

    roadmap_trip_set_point_focus (point, rotate);
}

int roadmap_trip_is_focus_changed (void) {
    
    if (RoadMapTripFocusChanged) {
        
        RoadMapTripFocusChanged = 0;
        return 1;
    }
    return 0;
}

int roadmap_trip_is_focus_moved (void) {
    
    if (RoadMapTripFocusMoved) {
        
        RoadMapTripFocusMoved = 0;
        return 1;
    }
    return 0;
}

int roadmap_trip_is_refresh_needed (void) {
    
    if (RoadMapTripRefresh ||
        RoadMapTripFocusChanged || RoadMapTripFocusMoved) {
    
        RoadMapTripFocusChanged = 0;
        RoadMapTripFocusMoved = 0;
        RoadMapTripRefresh = 0;

        return 1;
    }
    return 0;
}


int roadmap_trip_get_orientation (void) {
    
    if (RoadMapTripRotate && (RoadMapTripFocus != NULL)) {
        return 360 - RoadMapTripFocus->direction;
    }
    
    return 0;
}


const char *roadmap_trip_get_focus_name (void) {

    if (RoadMapTripFocus != NULL) {
        return RoadMapTripFocus->id;
    }
    
    return NULL;
}


const RoadMapPosition *roadmap_trip_get_focus_position (void) {
    
    if (RoadMapTripFocus != NULL) {
        return &RoadMapTripFocus->position;
    }
    
    return &RoadMapTripLastPosition;
}


void  roadmap_trip_start (int rotate) {

    RoadMapTripDestination = roadmap_trip_search ("Destination");

    if (RoadMapTripDestination != NULL) {

       roadmap_trip_set_point ("Departure", &RoadMapTripGps->position);
       RoadMapTripDeparture = roadmap_trip_search ("Departure");

       roadmap_trip_activate (rotate);
    }
}

    
void roadmap_trip_resume (int rotate) {

    RoadMapTripDeparture = roadmap_trip_search ("Departure");

    if (RoadMapTripDeparture != NULL) {

       RoadMapTripDestination = roadmap_trip_search ("Destination");

       if (RoadMapTripDestination != NULL) {
          roadmap_trip_activate (rotate);
       }
    }
}


void roadmap_trip_reverse (int rotate) {
    
    RoadMapTripDestination = roadmap_trip_search ("Departure");

    if (RoadMapTripDestination != NULL) {

       RoadMapTripDeparture = roadmap_trip_search ("Destination");

       if (RoadMapTripDeparture != NULL) {
          roadmap_trip_activate (rotate);
       } else {
          RoadMapTripDestination = NULL;
       }
    }
}


void roadmap_trip_stop (void) {

    RoadMapTripDestination = NULL;
    RoadMapTripRefresh = 1;
    roadmap_screen_refresh();
}


void roadmap_trip_format_messages (void) {
    
    int distance_to_destination;
    char *unit;
    RoadMapTripPoint *gps = RoadMapTripGps;
    RoadMapTripPoint *waypoint;


    if (RoadMapTripFocus == gps &&
        RoadMapTripDestination != NULL &&
        RoadMapTripDestination->has_value) {
    
        roadmap_message_set ('T', roadmap_time_get_hours_minutes());
        
        distance_to_destination =
            roadmap_math_distance
                (&gps->position, &RoadMapTripDestination->position);
    
        roadmap_log (ROADMAP_DEBUG,
                        "GPS: distance to destination = %d %s",
                        distance_to_destination,
                        roadmap_math_distance_unit());
        
        roadmap_message_set ('D', "%d %s",
                             roadmap_math_to_trip_distance
                                        (distance_to_destination),
                             roadmap_math_trip_unit());
        
        RoadMapTripNextWaypoint = RoadMapTripDestination;
        
        for (waypoint = (RoadMapTripPoint *)RoadMapTripWaypoints.first;
             waypoint != NULL;
             waypoint = (RoadMapTripPoint *)waypoint->link.next) {

            if (waypoint->in_trip && waypoint != RoadMapTripDeparture) {
                if ((waypoint->distance < distance_to_destination) &&
                    (waypoint->distance > RoadMapTripNextWaypoint->distance)) {
                    RoadMapTripNextWaypoint = waypoint;
                }
            }
        }
        
        unit = roadmap_math_trip_unit();
        
        if (RoadMapTripNextWaypoint != RoadMapTripDestination) {
            
            int distance_to_waypoint =
                    roadmap_math_distance (&gps->position,
                                           &RoadMapTripNextWaypoint->position);
            
            roadmap_log (ROADMAP_DEBUG,
                            "GPS: distance to next waypoint %s = %d %s",
                            RoadMapTripNextWaypoint->id,
                            distance_to_waypoint,
                            roadmap_math_distance_unit());
            
            roadmap_message_set ('W', "%d %s",
                                 roadmap_math_to_trip_distance
                                            (distance_to_waypoint),
                                 roadmap_math_trip_unit());
            
        } else {
            roadmap_message_unset ('W');
        }
        
        roadmap_message_set ('S', "%3d %s",
                             roadmap_math_to_speed_unit(gps->speed),
                             roadmap_math_speed_unit());
        
    } else {

        RoadMapTripNextWaypoint = NULL;

        roadmap_message_unset ('A');
        roadmap_message_unset ('D');
        roadmap_message_unset ('S');
        roadmap_message_unset ('T');
        roadmap_message_unset ('W');
    }
}


void roadmap_trip_display (void) {

    int azymuth;
    RoadMapGuiPoint point;
    RoadMapTripPoint *gps = RoadMapTripGps;
    RoadMapTripPoint *waypoint;


    for (waypoint = (RoadMapTripPoint *)RoadMapTripWaypoints.first;
         waypoint != NULL;
         waypoint = (RoadMapTripPoint *)waypoint->link.next) {

        if (waypoint->sprite == NULL) continue;
        if (! waypoint->has_value) continue;

        if (roadmap_math_point_is_visible (&waypoint->position)) {

            roadmap_math_coordinate (&waypoint->position, &point);
            roadmap_math_rotate_coordinates (1, &point);
            roadmap_sprite_draw (waypoint->sprite, &point, waypoint->direction);
        }
    }

    if (RoadMapTripNextWaypoint != NULL) {
    
        azymuth = roadmap_math_azymuth (&gps->position,
                                        &RoadMapTripNextWaypoint->position);
        roadmap_math_coordinate (&gps->position, &point);
        roadmap_sprite_draw ("Direction", &point, azymuth);
    }
}

    
const char *roadmap_trip_current (void) {
    return roadmap_config_get (&RoadMapConfigTripName);
}


void roadmap_trip_new (void) {

    roadmap_trip_clear ();
    roadmap_config_set (&RoadMapConfigTripName, "default");
    RoadMapTripModified = 1;

    roadmap_screen_refresh();
}


void roadmap_trip_initialize (void) {
    
    int i;
    
    for (i = 0; RoadMapTripPredefined[i].id != NULL; ++i) {

        if (! RoadMapTripPredefined[i].in_trip) {

            roadmap_config_declare
                ("session",
                 &RoadMapTripPredefined[i].config_position, "0,0");

            if (RoadMapTripPredefined[i].mobile) {
                roadmap_config_declare
                    ("session",
                     &RoadMapTripPredefined[i].config_direction, "0");
            }
        }
        if (strcmp (RoadMapTripPredefined[i].id, "GPS") == 0) {
            RoadMapTripGps = &(RoadMapTripPredefined[i]);
        }
    }
    roadmap_config_declare
        ("session", &RoadMapConfigTripName, "default");
    roadmap_config_declare
        ("session", &RoadMapConfigFocusName, "GPS");
    roadmap_config_declare
        ("session", &RoadMapConfigFocusRotate, "1");
}


int roadmap_trip_load (const char *name) {
    
    FILE *file;
    int   i;
    char *p;
    char *argv[8];
    char  line[1024];
    
    RoadMapPosition position;


    /* Load waypoints from the user environment. */
    
    file = roadmap_trip_fopen (name, "r");
    
    if (file == NULL) {
       if (name == NULL) {
          return 1; /* Delay the answer: file selection has been activated. */
       }
       roadmap_messagebox ("Warning",
                           "An error occurred when opening the selected trip");
       return 0;
    }

    roadmap_trip_clear ();

    while (! feof(file)) {

       if (fgets (line, sizeof(line), file) == NULL) break;

       p = roadmap_config_extract_data (line, sizeof(line));

       if (p == NULL) continue;
         
       argv[0] = p; /* Not used at that time. */
             
       for (p = strchr (p, ','), i = 0; p != NULL && i < 8; p = strchr (p, ',')) {
           *p = 0;
           argv[++i] = ++p;
       }
         
       if (i != 3) {
           roadmap_log (ROADMAP_ERROR, "erroneous trip line (%d fields)", i);
           continue;
       }
       position.longitude = atoi(argv[2]);
       position.latitude  = atoi(argv[3]);
       roadmap_trip_update (argv[1], &position, "Waypoint");
    }
      
    fclose (file);
    RoadMapTripModified = 0;
      
    roadmap_screen_refresh();
    return 1;
}


static void roadmap_trip_printf (FILE *file, const RoadMapTripPoint *point) {
    
    fprintf (file, "%s,%s,%d,%d\n",
                point->sprite,
                point->id,
                point->position.longitude,
                point->position.latitude);
}

void roadmap_trip_save (const char *name) {
    
    RoadMapTripPoint *point;

    /* Save all existing points, if the list was ever modified, or if
     * the user wants to specify another name.
     */
    if (name == NULL) {
        RoadMapTripModified = 1;
    }
        
    if (RoadMapTripModified) {
        
        FILE *file;
        
        file = roadmap_trip_fopen (name, "w");

        if (file == NULL) {
            return;
        }
        
        for (point = (RoadMapTripPoint *)RoadMapTripWaypoints.first;
             point != NULL;
             point = (RoadMapTripPoint *)point->link.next) {
                  
            if (point->in_trip && point->has_value) {
                roadmap_trip_printf (file, point);
            }
        }

        fclose (file);
        RoadMapTripModified = 0;
    }
}