/* roadmap_screen.h - draw the map on the screen.
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
 */

#ifndef INCLUDE__ROADMAP_SCREEN__H
#define INCLUDE__ROADMAP_SCREEN__H

#include "roadmap_types.h"
#include "roadmap_canvas.h"

enum { VIEW_MODE_2D = 0,
       VIEW_MODE_3D
};

enum { ORIENTATION_DYNAMIC = 0,
       ORIENTATION_FIXED
};

void roadmap_screen_initialize (void);
void roadmap_screen_shutdown   (void);

void roadmap_screen_set_initial_position (void);

void roadmap_screen_zoom_in    (void);
void roadmap_screen_zoom_out   (void);
void roadmap_screen_zoom_reset (void);

void roadmap_screen_move_up    (void);
void roadmap_screen_move_down  (void);
void roadmap_screen_move_right (void);
void roadmap_screen_move_left  (void);

void roadmap_screen_move_center (int dy);

void roadmap_screen_toggle_view_mode (void);
void roadmap_screen_toggle_orientation_mode (void);
void roadmap_screen_increase_horizon (void);
void roadmap_screen_decrease_horizon (void);

void roadmap_screen_rotate (int delta);

void roadmap_screen_refresh (void); /* Conditional: only if needed. */
void roadmap_screen_redraw  (void); /* Force a screen redraw, no move. */

void roadmap_screen_hold     (void); /* Hold on at the current position. */
void roadmap_screen_freeze   (void); /* Forbid any screen refresh. */
void roadmap_screen_unfreeze (void); /* Enable screen refresh. */

void roadmap_screen_get_center (RoadMapPosition *center);

typedef void (*RoadMapScreenSubscriber) (void);

RoadMapScreenSubscriber roadmap_screen_subscribe_after_refresh
                                    (RoadMapScreenSubscriber handler);

void roadmap_screen_draw_one_line (RoadMapPosition *from,
                                   RoadMapPosition *to,
                                   int fully_visible,
                                   RoadMapPosition *first_shape_pos,
                                   int first_shape,
                                   int last_shape,
                                   RoadMapShapeItr shape_itr,
                                   RoadMapPen *pens,
                                   int num_pens,
                                   int *total_length,
                                   RoadMapGuiPoint *middle,
                                   int *angle);

void roadmap_screen_draw_line_direction (RoadMapPosition *from,
                                         RoadMapPosition *to,
                                         RoadMapPosition *first_shape_pos,
                                         int first_shape,
                                         int last_shape,
                                         RoadMapShapeItr shape_itr,
                                         int width,
                                         int direction);

int roadmap_screen_fast_refresh (void);

int roadmap_screen_height (void);

#define DBG_TIME_FULL 0
#define DBG_TIME_DRAW_SQUARE 1
#define DBG_TIME_DRAW_ONE_LINE 2
#define DBG_TIME_SELECT_PEN 3
#define DBG_TIME_DRAW_LINES 4
#define DBG_TIME_CREATE_PATH 5
#define DBG_TIME_ADD_PATH 6
#define DBG_TIME_FLIP 7
#define DBG_TIME_TEXT_FULL 8
#define DBG_TIME_TEXT_CNV 9
#define DBG_TIME_TEXT_LOAD 10
#define DBG_TIME_TEXT_ONE_LETTER 11
#define DBG_TIME_TEXT_GET_GLYPH 12
#define DBG_TIME_TEXT_ONE_RAS 13
#define DBG_TIME_DRAW_LONG_LINES 14
#define DBG_TIME_FIND_LONG_LINES 15
#define DBG_TIME_ADD_SEGMENT 16
#define DBG_TIME_FLUSH_LINES 17
#define DBG_TIME_FLUSH_POINTS 18
#define DBG_TIME_T1 19
#define DBG_TIME_T2 20
#define DBG_TIME_T3 21
#define DBG_TIME_T4 22
#define DBG_TIME_LAST_COUNTER 23

void dbg_time_start(int type);
void dbg_time_end(int type);

#endif // INCLUDE__ROADMAP_SCREEN__H
