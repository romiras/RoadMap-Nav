/* roadmap_pointer.c - Manage mouse/pointer events
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
 *   See roadmap_pointer.h.
 */

#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "roadmap.h"
#include "roadmap_pointer.h"
#include "roadmap_gui.h"
#include "roadmap_canvas.h"
#include "roadmap_main.h"

#define LONG_CLICK_TIMEOUT 350
#define DRAG_FLOW_CONTROL_TIMEOUT 30

static int is_button_down = 0;
static int cancel_dragging = 0;
static int is_dragging = 0;
static int is_drag_flow_control_on = 0;

static RoadMapGuiPoint last_pointer_point;

enum POINTER_EVENT {SHORT_CLICK = 0, LONG_CLICK, PRESSED, RELEASED, DRAG_START, DRAG_MOTION, DRAG_END, MAX_EVENTS};

#define MAX_CALLBACKS 10

typedef struct pointer_callback {
   RoadMapPointerHandler handler;
   int                   priority;
} PointerCallback;

static PointerCallback pointer_callbacks[MAX_EVENTS][MAX_CALLBACKS];


static int exec_callbacks (int event, RoadMapGuiPoint *point) {
   int i = 0;
   int res = 0;

   while ((i<MAX_CALLBACKS) && pointer_callbacks[event][i].handler) {
      res = (pointer_callbacks[event][i].handler) (point);
      if (res) break;
      i++;
   }

   return res;
}


static void roadmap_pointer_button_timeout(void) {

   roadmap_main_remove_periodic(roadmap_pointer_button_timeout);

   exec_callbacks (LONG_CLICK, &last_pointer_point);
   is_button_down = 0;
}
 
/* Instead of calling the drag motion event with every mouse move,
 * we use this timer as a flow control. It may take time for the
 * application to finish the task of drawing the screen and we don't
 * want to lag.
 */
static void roadmap_pointer_drag_flow_control(void) {

   roadmap_main_remove_periodic(roadmap_pointer_drag_flow_control);

   exec_callbacks (DRAG_MOTION, &last_pointer_point);
   is_drag_flow_control_on = 0;
}
   

static void roadmap_pointer_button_pressed (RoadMapGuiPoint *point) {
   last_pointer_point = *point;

   if (exec_callbacks (PRESSED, point)) {

      /* If a handler returns true dragging event is off.
       */
      cancel_dragging = 1;
   } else {
      cancel_dragging = 0;
   }

   is_button_down = 1;    
   roadmap_main_set_periodic
      (LONG_CLICK_TIMEOUT, roadmap_pointer_button_timeout);
}


static void roadmap_pointer_button_released (RoadMapGuiPoint *point) {
    
   if (is_dragging) {
      if (is_drag_flow_control_on) {
         roadmap_main_remove_periodic(roadmap_pointer_drag_flow_control);
         is_drag_flow_control_on = 0;
      }

      exec_callbacks (DRAG_END, point);

      is_dragging = 0;
      is_button_down = 0;
   } else if (is_button_down) {
      roadmap_main_remove_periodic(roadmap_pointer_button_timeout);
      
      exec_callbacks (SHORT_CLICK, point);
      is_button_down = 0;
   }

   exec_callbacks (RELEASED, point);
}

static void roadmap_pointer_moved (RoadMapGuiPoint *point) {
   if (cancel_dragging || (!is_button_down && !is_dragging)) return;

   if (!is_dragging) {

      /* Less sensitive, since a car is not a quiet environment... */
      if ((abs(point->x - last_pointer_point.x) <= 3) &&
          (abs(point->y - last_pointer_point.y) <= 3)) return;

      roadmap_main_remove_periodic(roadmap_pointer_button_timeout);
      
      exec_callbacks (DRAG_START, &last_pointer_point);

      last_pointer_point = *point;
      is_drag_flow_control_on = 1;
      roadmap_main_set_periodic
         (DRAG_FLOW_CONTROL_TIMEOUT, roadmap_pointer_drag_flow_control);
      is_dragging = 1;
   } else {
      /* the flow control timer will execute the handler */
      last_pointer_point = *point;
      if (!is_drag_flow_control_on) {
         is_drag_flow_control_on = 1;
         roadmap_main_set_periodic
            (DRAG_FLOW_CONTROL_TIMEOUT, roadmap_pointer_drag_flow_control);
      }
   }
}


static void remove_callback (int event, void *handler) {
   int i=0;
   for (i=0; i<MAX_CALLBACKS; i++) {
      if (pointer_callbacks[event][i].handler == handler) break;
   }

   if (i==MAX_CALLBACKS) {
      roadmap_log (ROADMAP_FATAL, "Can't find a callback to remove. Event: %d",
      event);
   }

   memmove (&pointer_callbacks[event][i], &pointer_callbacks[event][i+1],
            sizeof(PointerCallback) * (MAX_CALLBACKS - i - 1));

   pointer_callbacks[event][MAX_CALLBACKS - 1].handler = NULL;
}


static void queue_callback (int event, void *handler, int priority) {
   int i;

   if (pointer_callbacks[event][MAX_CALLBACKS-1].handler) {
      roadmap_log (ROADMAP_FATAL, "Too many callbacks for event: %d", event);
   }

   for (i=0; i<MAX_CALLBACKS; i++) {
      if (pointer_callbacks[event][i].priority <= priority) break;
   }

   
   memmove (&pointer_callbacks[event][i+1], &pointer_callbacks[event][i],
            sizeof(PointerCallback) * (MAX_CALLBACKS - i - 1));

   pointer_callbacks[event][i].handler = handler;
   pointer_callbacks[event][i].priority = priority;
}


void roadmap_pointer_initialize (void) {

   roadmap_canvas_register_button_pressed_handler
      (&roadmap_pointer_button_pressed);
   roadmap_canvas_register_button_released_handler
      (&roadmap_pointer_button_released);
   roadmap_canvas_register_mouse_move_handler
      (&roadmap_pointer_moved);
}


const RoadMapGuiPoint *roadmap_pointer_position (void) {

   return &last_pointer_point;
}


void roadmap_pointer_register_short_click (RoadMapPointerHandler handler,
                                           int priority) {

   queue_callback (SHORT_CLICK, handler, priority);
}


void roadmap_pointer_register_long_click (RoadMapPointerHandler handler,
                                          int priority) {

   queue_callback (LONG_CLICK, handler, priority);
}


void roadmap_pointer_register_pressed (RoadMapPointerHandler handler,
                                       int priority) {

   queue_callback (PRESSED, handler, priority);
}


void roadmap_pointer_register_released (RoadMapPointerHandler handler,
                                        int priority) {

   queue_callback (RELEASED, handler, priority);
}


void roadmap_pointer_register_drag_start (RoadMapPointerHandler handler,
                                          int priority) {

   queue_callback (DRAG_START, handler, priority);
}


void roadmap_pointer_register_drag_motion (RoadMapPointerHandler handler,
                                           int priority) {

   queue_callback (DRAG_MOTION, handler, priority);
}


void roadmap_pointer_register_drag_end (RoadMapPointerHandler handler,
                                        int priority) {

   queue_callback (DRAG_END, handler, priority);
}


void roadmap_pointer_unregister_short_click (RoadMapPointerHandler handler) {

   remove_callback (SHORT_CLICK, handler);
}


void roadmap_pointer_unregister_long_click (RoadMapPointerHandler handler) {

   remove_callback (LONG_CLICK, handler);
}


void roadmap_pointer_unregister_pressed (RoadMapPointerHandler handler) {

   remove_callback (PRESSED, handler);
}


void roadmap_pointer_unregister_released (RoadMapPointerHandler handler) {

   remove_callback (RELEASED, handler);
}


void roadmap_pointer_unregister_drag_start (RoadMapPointerHandler handler) {

   remove_callback (DRAG_START, handler);
}


void roadmap_pointer_unregister_drag_motion (RoadMapPointerHandler handler) {

   remove_callback (DRAG_MOTION, handler);
}


void roadmap_pointer_unregister_drag_end (RoadMapPointerHandler handler) {

   remove_callback (DRAG_END, handler);
}


