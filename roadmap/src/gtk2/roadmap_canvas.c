/* roadmap_canvas.c - manage the canvas that is used to draw the map.
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
 *   See roadmap_canvas.h.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "roadmap.h"
#include "roadmap_types.h"
#include "roadmap_gui.h"
#include "roadmap_math.h"

#include "roadmap_canvas.h"
#include "roadmap_gtkcanvas.h"
#include "gtk/gtk.h"


#define ROADMAP_CURSOR_SIZE           10
#define ROADMAP_CANVAS_POINT_BLOCK  1024


struct roadmap_canvas_pen {

   struct roadmap_canvas_pen *next;

   char  *name;
   GdkLineStyle style;
   GdkGC *gc;
};


static struct roadmap_canvas_pen *RoadMapPenList = NULL;

static GtkWidget  *RoadMapDrawingArea;
static GdkPixmap  *RoadMapDrawingBuffer;
static GdkGC      *RoadMapGc;

static RoadMapPen CurrentPen;

static PangoLayout  *RoadMapLayout = NULL;
static PangoContext  *RoadMapContext = NULL;


/* The canvas callbacks: all callbacks are initialized to do-nothing
 * functions, so that we don't care checking if one has been setup.
 */
static void roadmap_canvas_ignore_mouse (int button, RoadMapGuiPoint *point) {}

static RoadMapCanvasMouseHandler RoadMapCanvasMouseButtonPressed =
                                     roadmap_canvas_ignore_mouse;

static RoadMapCanvasMouseHandler RoadMapCanvasMouseButtonReleased =
                                     roadmap_canvas_ignore_mouse;

static RoadMapCanvasMouseHandler RoadMapCanvasMouseMoved =
                                     roadmap_canvas_ignore_mouse;

static RoadMapCanvasMouseHandler RoadMapCanvasMouseScroll =
                                     roadmap_canvas_ignore_mouse;


static void roadmap_canvas_ignore_configure (void) {}

static RoadMapCanvasConfigureHandler RoadMapCanvasConfigure =
                                     roadmap_canvas_ignore_configure;

static void roadmap_canvas_convert_points
                (GdkPoint *gdkpoints, RoadMapGuiPoint *points, int count) {

    RoadMapGuiPoint *end = points + count;

    while (points < end) {
        gdkpoints->x = points->x;
        gdkpoints->y = points->y;
        gdkpoints += 1;
        points += 1;
    }
}


void roadmap_canvas_get_text_extents 
        (const char *text, int size, int *width,
            int *ascent, int *descent, int *can_tilt) {

   PangoRectangle rectangle;

   if (RoadMapLayout == NULL) {
       PangoFontDescription *desc;
       RoadMapLayout = gtk_widget_create_pango_layout
                           (GTK_WIDGET(RoadMapDrawingArea), text);
       RoadMapContext =  gtk_widget_get_pango_context(GTK_WIDGET(RoadMapDrawingArea));
       pango_layout_set_width (RoadMapLayout, -1);

       desc = pango_font_description_from_string ("Sans Bold 15");
       pango_layout_set_font_description (RoadMapLayout, desc);
       pango_font_description_free (desc);
   }

   pango_layout_set_text (RoadMapLayout, text, -1);

   pango_layout_get_extents (RoadMapLayout, NULL, &rectangle);

   *width   = rectangle.width / PANGO_SCALE;
   *ascent  = PANGO_ASCENT(rectangle) / PANGO_SCALE;
   *descent = PANGO_DESCENT(rectangle) / PANGO_SCALE;
   if (can_tilt) *can_tilt = 1;
}


RoadMapPen roadmap_canvas_select_pen (RoadMapPen pen) {

   RoadMapPen old_pen = CurrentPen;
   CurrentPen = pen;
   RoadMapGc = pen->gc;
   return old_pen;
}


RoadMapPen roadmap_canvas_create_pen (const char *name) {

   struct roadmap_canvas_pen *pen;

   for (pen = RoadMapPenList; pen != NULL; pen = pen->next) {
      if (strcmp(pen->name, name) == 0) break;
   }

   if (pen == NULL) {

      /* This is a new pen: create it. */

      GdkGC *gc;

      gc = gdk_gc_new (RoadMapDrawingBuffer);

      gdk_gc_set_fill (gc, GDK_SOLID);

      pen = (struct roadmap_canvas_pen *)
                malloc (sizeof(struct roadmap_canvas_pen));
      roadmap_check_allocated(pen);

      pen->name = strdup (name);
      pen->gc   = gc;
      pen->style = GDK_LINE_SOLID;
      pen->next = RoadMapPenList;

      RoadMapPenList = pen;
   }

   roadmap_canvas_select_pen (pen);

   return pen;
}


void roadmap_canvas_set_foreground (const char *color) {

   static GdkColor *native_color;

   if (native_color == NULL) {
      native_color = (GdkColor *) g_malloc (sizeof(GdkColor));
   }

   gdk_color_parse (color, native_color);
   gdk_color_alloc (gdk_colormap_get_system(), native_color);

   gdk_gc_set_foreground (RoadMapGc, native_color);
}

void roadmap_canvas_set_label_font_color(const char *color) {
}

void roadmap_canvas_set_label_font_size(int size) {
}

void roadmap_canvas_set_linestyle (const char *style) {

   if (strcasecmp (style, "dashed") == 0) {
      CurrentPen->style = GDK_LINE_ON_OFF_DASH;
   } else {
      CurrentPen->style = GDK_LINE_SOLID;
   }
}

void roadmap_canvas_set_thickness  (int thickness) {

   gdk_gc_set_line_attributes
      (RoadMapGc, thickness, CurrentPen->style,
       CurrentPen->style == GDK_LINE_SOLID ? GDK_CAP_ROUND : GDK_CAP_BUTT,
       GDK_JOIN_ROUND);
}

/* this are stubs */
void roadmap_canvas_set_opacity (int opacity) {}

void roadmap_canvas_set_linejoin(const char *join) {}
void roadmap_canvas_set_linecap(const char *cap) {}

void roadmap_canvas_set_brush_color(const char *color) {}
void roadmap_canvas_set_brush_style(const char *style) {}
void roadmap_canvas_set_brush_isbackground(int isbackground) {}

void roadmap_canvas_set_label_font_name(const char *name) {}
void roadmap_canvas_set_label_font_spacing(int spacing) {}
void roadmap_canvas_set_label_font_weight(const char *weight) {}
void roadmap_canvas_set_label_font_style(int style) {}

void roadmap_canvas_set_label_buffer_color(const char *color) {}
void roadmap_canvas_set_label_buffer_size(int size) {}

void roadmap_canvas_erase (void) {

   gdk_draw_rectangle (RoadMapDrawingBuffer,
                       RoadMapGc,
                       TRUE,
                       0, 0,
                       RoadMapDrawingArea->allocation.width,
                       RoadMapDrawingArea->allocation.height);
}

void
roadmap_pango_matrix_rotate (PangoMatrix *matrix,
	int degrees, int sine, int cosine)
{
  PangoMatrix tmp;

  tmp.xx = cosine/32768.;
  tmp.xy = sine/32768.;
  tmp.yx = -sine/32768.;
  tmp.yy = cosine/32768.;
  tmp.x0 = 0;
  tmp.y0 = 0;

  pango_matrix_concat (matrix, &tmp);
}

void roadmap_canvas_draw_string (RoadMapGuiPoint *position,
                                 int corner, int size, const char *text) {

   int text_width;
   int text_ascent;
   int text_descent;
   RoadMapGuiPoint start[1];

   roadmap_canvas_get_text_extents 
        (text, size, &text_width, &text_ascent, &text_descent, NULL);

   start->x = position->x;
   start->y = position->y;

   if (corner & ROADMAP_CANVAS_RIGHT)
      start->x -= text_width;
   else if (corner & ROADMAP_CANVAS_CENTER_X)
      start->x -= text_width / 2;

   if (corner & ROADMAP_CANVAS_BOTTOM)
      start->y -= text_descent;
   else if (corner & ROADMAP_CANVAS_CENTER_Y)
      start->y = start->y - text_descent + ((text_descent + text_ascent) / 2);
   else /* TOP */
      start->y += text_ascent;

   gdk_draw_layout (RoadMapDrawingBuffer, RoadMapGc,
       start->x, start->y, RoadMapLayout);
}


void
roadmap_canvas_draw_string_angle(RoadMapGuiPoint *position,
				 int size, int angle, const char *text)
{
    int text_width;
    int text_height;
    int text_ascent;
    int text_descent;
    int width, height;
    int sine, cosine;
    RoadMapGuiPoint start[1];
    PangoMatrix matrix = PANGO_MATRIX_INIT;
    PangoMatrix init_matrix = PANGO_MATRIX_INIT;


    roadmap_canvas_get_text_extents 
        (text, size, &text_width, &text_ascent, &text_descent, NULL);
    text_height = (text_ascent + text_descent);

    angle = -angle;

    // fprintf(stderr, "position: x %d y %d\n", position->x, position->y);
    // fprintf(stderr, "%s @ %d degrees: width %d ascent %d descent %d\n",
    //	    text, angle, text_width, text_ascent, text_descent);

    start->x = position->x;
    start->y = position->y - text_height;
   

    roadmap_math_trigonometry (angle, &sine, &cosine);
    // fprintf(stderr, "a %d cos(a) %d  sin(a) %d", angle, cosine, sine);


    // scaling works fine, and obviously so does rotation.
    // but i can't get translations to have any affect whatsoever.  :-/
    pango_matrix_translate (&matrix, 0, (text_ascent + text_descent));
    // pango_matrix_rotate(&matrix, angle);
    roadmap_pango_matrix_rotate(&matrix, angle, sine, cosine);

    pango_context_set_matrix(RoadMapContext, &matrix);
    pango_layout_context_changed(RoadMapLayout);

    pango_layout_get_size (RoadMapLayout, &width, &height);
    width /= PANGO_SCALE;

#if 1
	/* major hacks ahead.  gdk_draw_layout doesn't fully honor
	 * the pango matrix transform.  we can't use 
	 * pango_renderer_draw_layout() instead, because it
	 * won't render to a GdkPixmap (that i can find).
	 * so we adjust the position of the rotated text in
	 * a few ad-hoc ways, and the result is reasonable.
	 * not great, just reasonable.
	 */
    if (angle < 0) {
	start->x = start->x - (width/2 * cosine / 32768);
	start->y = start->y + (width/2 * sine / 32768) -
			(text_height * angle / 90);
    } else {
	start->x = start->x - (width/2 * cosine / 32768) -
			(text_height * 120 * angle / 90 / 100);
	start->y = start->y - (width/2 * sine / 32768) +
			(text_height * angle / 90);
    }
#endif
    gdk_draw_layout(RoadMapDrawingBuffer, RoadMapGc,
		start->x, start->y, RoadMapLayout);


    // reset 
    pango_context_set_matrix(RoadMapContext, &init_matrix);
    pango_layout_context_changed(RoadMapLayout);
}


void roadmap_canvas_draw_multiple_points (int count, RoadMapGuiPoint *points) {

   GdkPoint gdkpoints[1024];

   while (count > 1024) {
       roadmap_canvas_convert_points (gdkpoints, points, 1024);
       gdk_draw_points (RoadMapDrawingBuffer, RoadMapGc, gdkpoints, 1024);
       points += 1024;
       count -= 1024;
   }
   roadmap_canvas_convert_points (gdkpoints, points, count);
   gdk_draw_points (RoadMapDrawingBuffer, RoadMapGc, gdkpoints, count);
}

void roadmap_canvas_draw_multiple_lines 
         (int count, int *lines, RoadMapGuiPoint *points, int fast_draw) {

   int i;
   int count_of_points;
   GdkPoint gdkpoints[1024];

   for (i = 0; i < count; ++i) {

      count_of_points = *lines;

      while (count_of_points > 1024) {
          roadmap_canvas_convert_points (gdkpoints, points, 1024);
          gdk_draw_lines (RoadMapDrawingBuffer, RoadMapGc, gdkpoints, 1024);

          /* We shift by 1023 only, because we must link the lines. */
          points += 1023;
          count_of_points -= 1023;
      }

      roadmap_canvas_convert_points (gdkpoints, points, count_of_points);
      gdk_draw_lines (RoadMapDrawingBuffer,
                      RoadMapGc, gdkpoints, count_of_points);

      points += count_of_points;
      lines += 1;
   }
}


void roadmap_canvas_draw_multiple_polygons
         (int count, int *polygons, RoadMapGuiPoint *points, int filled,
            int fast_draw) {

   int i;
   int count_of_points;
   GdkPoint gdkpoints[1024];

   for (i = 0; i < count; ++i) {

      count_of_points = *polygons;

      while (count_of_points > 1024) {
          roadmap_canvas_convert_points (gdkpoints, points, 1024);
          gdk_draw_polygon (RoadMapDrawingBuffer,
                            RoadMapGc, filled, gdkpoints, 1024);

          /* We shift by 1023 only, because we must link the lines. */
          points += 1023;
          count_of_points -= 1023;
      }

      roadmap_canvas_convert_points (gdkpoints, points, count_of_points);
      gdk_draw_polygon (RoadMapDrawingBuffer,
                        RoadMapGc, filled, gdkpoints, count_of_points);

      polygons += 1;
      points += count_of_points;
   }
}


void roadmap_canvas_draw_multiple_circles
        (int count, RoadMapGuiPoint *centers, int *radius, int filled,
            int fast_draw) {

   int i;

   for (i = 0; i < count; ++i) {

      int r = radius[i];

      gdk_draw_arc (RoadMapDrawingBuffer,
                    RoadMapGc,
                    filled,
                    centers[i].x - r,
                    centers[i].y - r,
                    2 * r,
                    2 * r,
                    0,
                    (360 * 64));
   }
}


static gint roadmap_canvas_configure
               (GtkWidget *widget, GdkEventConfigure *event) {

   if (RoadMapDrawingBuffer != NULL) {
      gdk_pixmap_unref (RoadMapDrawingBuffer);
   }

   RoadMapDrawingBuffer =
      gdk_pixmap_new (widget->window,
                      widget->allocation.width,
                      widget->allocation.height,
                      -1);

   (*RoadMapCanvasConfigure) ();

   return TRUE;
}


void roadmap_canvas_register_configure_handler
                    (RoadMapCanvasConfigureHandler handler) {

   RoadMapCanvasConfigure = handler;
}


static gint roadmap_canvas_expose (GtkWidget *widget, GdkEventExpose *event) {

   gdk_draw_pixmap (widget->window,
                    widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
                    RoadMapDrawingBuffer,
                    event->area.x, event->area.y,
                    event->area.x, event->area.y,
                    event->area.width, event->area.height);

   return FALSE;
}


static gint roadmap_canvas_mouse_event
               (GtkWidget *w, GdkEventButton *event, gpointer data) {

   RoadMapGuiPoint point;

   point.x = event->x;
   point.y = event->y;

   switch ((long) data) {
      case 1:
         (*RoadMapCanvasMouseButtonPressed) (event->button, &point);
         break;
      case 2:
         (*RoadMapCanvasMouseButtonReleased) (event->button, &point);
         break;
      case 3:
         (*RoadMapCanvasMouseMoved) (0, &point);
         break;
   }

   return FALSE;
}


static gboolean roadmap_canvas_scroll_event
               (GtkWidget *w, GdkEventScroll *event, gpointer data) {

   int direction = 0;
   RoadMapGuiPoint point;

   point.x = event->x;
   point.y = event->y;

   switch (event->direction) {
      case GDK_SCROLL_UP:    direction = 1;  break;
      case GDK_SCROLL_DOWN:  direction = -1; break;
      case GDK_SCROLL_LEFT:  direction = 2;  break;
      case GDK_SCROLL_RIGHT: direction = -2; break;
   }

   (*RoadMapCanvasMouseScroll) (direction, &point);

   return FALSE;
}


void roadmap_canvas_register_button_pressed_handler
                    (RoadMapCanvasMouseHandler handler) {

   RoadMapCanvasMouseButtonPressed = handler;
}


void roadmap_canvas_register_button_released_handler
                    (RoadMapCanvasMouseHandler handler) {
       
   RoadMapCanvasMouseButtonReleased = handler;
}


void roadmap_canvas_register_mouse_move_handler
                    (RoadMapCanvasMouseHandler handler) {

   RoadMapCanvasMouseMoved = handler;
}


void roadmap_canvas_register_mouse_scroll_handler
                    (RoadMapCanvasMouseHandler handler) {

   RoadMapCanvasMouseScroll = handler;
}


int roadmap_canvas_width (void) {

   if (RoadMapDrawingArea == NULL) {
      return 0;
   }
   return RoadMapDrawingArea->allocation.width;
}

int roadmap_canvas_height (void) {

   if (RoadMapDrawingArea == NULL) {
      return 0;
   }
   return RoadMapDrawingArea->allocation.height;
}


void roadmap_canvas_refresh (void) {

   gtk_widget_queue_draw_area
       (RoadMapDrawingArea, 0, 0,
        RoadMapDrawingArea->allocation.width,
        RoadMapDrawingArea->allocation.height);
}


GtkWidget *roadmap_canvas_new (void) {

   RoadMapDrawingArea = gtk_drawing_area_new ();

   gtk_widget_set_double_buffered (RoadMapDrawingArea, FALSE);

   gtk_widget_set_events (RoadMapDrawingArea,
                          GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK |
                          GDK_POINTER_MOTION_MASK |
                          GDK_SCROLL_MASK);


   g_signal_connect (RoadMapDrawingArea,
                     "expose_event",
                     (GCallback) roadmap_canvas_expose,
                     NULL);

   g_signal_connect (RoadMapDrawingArea,
                     "configure_event",
                     (GCallback) roadmap_canvas_configure,
                     NULL);

   g_signal_connect (RoadMapDrawingArea,
                     "button_press_event",
                     (GCallback) roadmap_canvas_mouse_event,
                     (gpointer)1);

   g_signal_connect (RoadMapDrawingArea,
                     "button_release_event",
                     (GCallback) roadmap_canvas_mouse_event,
                     (gpointer)2);

   g_signal_connect (RoadMapDrawingArea,
                     "motion_notify_event",
                     (GCallback) roadmap_canvas_mouse_event,
                     (gpointer)3);

   g_signal_connect (RoadMapDrawingArea,
                     "scroll_event",
                     (GCallback) roadmap_canvas_scroll_event,
                     (gpointer)0);

   RoadMapGc = RoadMapDrawingArea->style->black_gc;

   return RoadMapDrawingArea;
}


void roadmap_canvas_save_screenshot (const char* filename) {

   gint width,height;
   GdkColormap *colormap = gdk_colormap_get_system();
   GdkPixbuf *pixbuf;
   GError *error = NULL;


   gdk_drawable_get_size(RoadMapDrawingBuffer, &width, &height);

   pixbuf = gdk_pixbuf_get_from_drawable(NULL, // Create a new pixbuf.
                                         RoadMapDrawingBuffer,
                                         colormap,
                                         0,0,         // source
                                         0,0,         // destination
                                         width, height);  // size


   if (gdk_pixbuf_save(pixbuf, filename, "png", &error, NULL) == FALSE) {

      roadmap_log(ROADMAP_ERROR, "Failed to save image %s\n",filename);
   }

   gdk_pixbuf_unref(pixbuf);
}

