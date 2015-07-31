/* roadmap_trip.h - Manage a trip: destination & waypoints.
 *
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
 *   Copyright (c) 2011 Danny Backx
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
 */

#ifndef INCLUDE__ROADMAP_TRIP__H
#define INCLUDE__ROADMAP_TRIP__H

#include "roadmap_types.h"
#include "roadmap_gui.h"
#include "roadmap_gps.h"
#include "roadmap_plugin.h"

void  roadmap_trip_set_point (const char *name, RoadMapPosition *position);
void  roadmap_trip_add_waypoint
    (const char *name, RoadMapPosition *position, int where);
void  roadmap_trip_create_selection_waypoint(void);
void  roadmap_trip_create_gps_waypoint(void);

void  roadmap_trip_set_gps
    (int gps_time, const RoadMapGpsPosition *gps_position);

void  roadmap_trip_copy_focus (const char *name);

void  roadmap_trip_lost_waypoint_manage_dialog(void);
void  roadmap_trip_route_waypoint_manage_dialog(void);
void  roadmap_trip_trip_waypoint_manage_dialog(void);
void  roadmap_trip_personal_waypoint_manage_dialog(void);


void  roadmap_trip_restore_focus (void);
void  roadmap_trip_set_focus (const char *name);

void  roadmap_trip_preserve_focus (void);

int   roadmap_trip_is_focus_changed  (void);
int   roadmap_trip_is_focus_moved    (void);
int   roadmap_trip_is_refresh_needed (void);

int   roadmap_trip_get_orientation (void);
int   roadmap_trip_get_speed (void);
const char *roadmap_trip_get_focus_name (void);

const RoadMapPosition *roadmap_trip_get_focus_position (void);
void roadmap_trip_set_focus_position (RoadMapPosition *pos );

void  roadmap_trip_route_start   (void);
void  roadmap_trip_route_resume  (void);
void  roadmap_trip_route_stop    (void);
void  roadmap_trip_route_reverse (void);
void  roadmap_trip_route_return  (void);
void  roadmap_trip_new_route     (void);
void  roadmap_trip_set_as_destination (void);


void  roadmap_trip_format_messages (void);
void  roadmap_trip_display (void);
void  roadmap_trip_toggle_show_inactive_routes(void);
void  roadmap_trip_toggle_show_inactive_tracks(void);

void  roadmap_trip_show_nextpoint(void);
void  roadmap_trip_show_2ndnextpoint(void);

void  roadmap_trip_new (void);

void  roadmap_trip_initialize (void);
void  roadmap_trip_shutdown (void);

/* In the two primitives that follow, the name is either NULL (i.e.
 * open a dialog to let the user enter one), or an explicit name.
 */
int  roadmap_trip_load (int silent, int merge);
void roadmap_trip_load_ask (void);
void roadmap_trip_merge_ask (void);
void roadmap_trip_save_manual (void);
int  roadmap_trip_save (void);
void roadmap_trip_save_as (void);

void roadmap_trip_save_screenshot (void);

void roadmap_trip_route_manage_dialog (void);
void roadmap_trip_lost_route_manage_dialog (void);

void roadmap_trip_track_to_route (void);
void roadmap_trip_route_simplify (void);
void roadmap_trip_currenttrack_to_route (void);
void roadmap_trip_currenttrack_to_track (void);

void roadmap_trip_insert_routepoint_best(void);
void roadmap_trip_insert_routepoint_dest(void);
void roadmap_trip_insert_routepoint_start(void);
void roadmap_trip_insert_trip_point(void);
void roadmap_trip_insert_personal_point(void);

int roadmap_trip_retrieve_area_points
        (RoadMapArea *area, RoadMapPosition *position);

void roadmap_trip_delete_last_place(void);
void roadmap_trip_edit_last_place(void);
void roadmap_trip_move_last_place(void);
void roadmap_trip_move_routepoint_ahead (void);
void roadmap_trip_move_routepoint_back (void);
void roadmap_trip_view_next_routepoint (void);
void roadmap_trip_view_prev_routepoint (void);
void roadmap_trip_complete (void);

int roadmap_trip_move_last_place_callback
        (int action, const RoadMapGuiPoint *point);

#if WGET_GOOGLE_ROUTE
void roadmap_trip_replace_with_google_route(void);
#endif

void roadmap_trip_set_selection_as (const char *name);

#ifdef HAVE_NAVIGATE_PLUGIN
void roadmap_trip_set_selection (int lon, int lat);
#endif

void roadmap_trip_enable (int status);
void roadmap_trip_refresh (void);
void roadmap_trip_refresh_needed(void);
void roadmap_trip_set_modified(int modified);
void roadmap_trip_unset_route_focii (void);

/* Add Waypoint dialog */
#define TRIP_PLACE_PERSONAL_MARK         0
#define TRIP_PLACE_TRIP_MARK             1
#define TRIP_PLACE_NEW_ROUTE             2
#define TRIP_PLACE_ROUTE_MARK_DEST       3
#define TRIP_PLACE_ROUTE_MARK_INSERT     4
#define TRIP_PLACE_ROUTE_MARK_START      5

#define TRIP_NUM_PLACEMENTS           3
#define TRIP_NUM_ROUTE_PLACEMENTS     6


void roadmap_trip_departure_waypoint(void);
void roadmap_trip_destination_waypoint(void);

RoadMapPosition *roadmap_trip_get_position (const char *name);
#endif // INCLUDE__ROADMAP_TRIP__H
