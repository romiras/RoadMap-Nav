/* roadmap_canvas_agg.c - manage the canvas for agg.
 *
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
 *   Copyright 2006 Ehud Shabtai
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
#include <wchar.h>

#include <agg_rendering_buffer.h>
#include <agg_curves.h>
#include <agg_conv_stroke.h>
#include "util/agg_color_conv_rgb8.h"
#include <agg_conv_contour.h>
#include <agg_conv_transform.h>
#include <agg_rasterizer_scanline_aa.h>
#include <agg_rasterizer_outline_aa.h>
#include <agg_rasterizer_outline.h>
#include <agg_renderer_primitives.h>
#include <agg_renderer_scanline.h>
#include <agg_scanline_p.h>
#include <agg_renderer_outline_aa.h>
#include <agg_pixfmt_rgb.h>
#include <agg_pixfmt_rgb_packed.h>

#include "../agg/font_freetype/agg_font_freetype.h"

#ifdef USE_FRIBIDI
#include <fribidi.h>
#define MAX_STR_LEN 65000
#endif

extern "C" {
#include "roadmap.h"
#include "roadmap_config.h"
#include "roadmap_messagebox.h"
#include "roadmap_path.h"

#include "roadmap_canvas.h"
#include "roadmap_gtkcanvas.h"
}

#include "roadmap_libpng.h"
#include "roadmap_canvas_agg.h"

#define GetRValue(x) x.red
#define GetGValue(x) x.green
#define GetBValue(x) x.blue

static GtkWidget  *RoadMapDrawingArea;
static GdkImage   *RoadMapDrawingBuffer;

int roadmap_canvas_agg_to_wchar (const char *text, wchar_t *output, int size) {

   int length = mbstowcs(output, text, size - 1);
   output[length] = 0;

   return length;
}
                                     

agg::rgba8 roadmap_canvas_agg_parse_color (const char *color) {

   GdkColor native_color;

	if (*color == '#') {
		int r, g, b, a;
      int count;
      
		count = sscanf(color, "#%2x%2x%2x%2x", &r, &g, &b, &a);

      if (count == 4) {    
         return agg::rgba8(r, g, b, a);
      } else {
         return agg::rgba8(r, g, b);
      }

	}

   gdk_color_parse (color, &native_color);
   gdk_color_alloc (gdk_colormap_get_system(), &native_color);

   return agg::rgba8(GetRValue(native_color), GetGValue(native_color),
            GetBValue(native_color));
}


static RoadMapImage load_bmp (const char *full_name) {

   GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file(full_name, NULL);

   if (pixbuf == NULL) {
      return NULL;
   }

   if (gdk_pixbuf_get_colorspace (pixbuf) != GDK_COLORSPACE_RGB) {
      g_object_unref (pixbuf);
      return NULL;
   }
   
   int n_channels = gdk_pixbuf_get_n_channels (pixbuf);
   int bbs = gdk_pixbuf_get_bits_per_sample (pixbuf);
   int width = gdk_pixbuf_get_width (pixbuf);
   int height = gdk_pixbuf_get_height (pixbuf);

   unsigned char *buf = (unsigned char *)malloc (width*height*4);

   agg::rendering_buffer tmp_rbuf (gdk_pixbuf_get_pixels (pixbuf),
                                  width, height,
                                  gdk_pixbuf_get_rowstride (pixbuf));

   agg::pixfmt_rgb24 tmp_pixfmt (tmp_rbuf);

   RoadMapImage image =  new roadmap_canvas_image();
   
   image->rbuf.attach (buf,
                       width, height,
                       width * 4);
   agg::color_conv(&image->rbuf, &tmp_rbuf, agg::color_conv_rgb24_to_rgba32());
   
   g_object_unref (pixbuf);

   return image;
}


static RoadMapImage load_png (const char *full_name) {

   int width;
   int height;
   int stride;

   unsigned char *buf = read_png_file(full_name, &width, &height, &stride);

   if (!buf) return NULL;

   RoadMapImage image =  new roadmap_canvas_image();
   image->rbuf.attach (buf, width, height, stride);

   return image;
}


RoadMapImage roadmap_canvas_agg_load_image (const char *path,
                                            const char *file_name) {

   char *full_name = roadmap_path_join (path, file_name);
   RoadMapImage image;

   if ((strlen(file_name) > 4) &&
      !strcasecmp (file_name + strlen(file_name) - 4, ".png")) {

      image = load_png (full_name);
   } else {
      image = load_bmp (full_name);
   }

   free (full_name);

   return image;
}


void roadmap_canvas_agg_free_image (RoadMapImage image) {
   
   free (image->rbuf.buf());
   delete image;
}


static gint roadmap_canvas_configure
               (GtkWidget *widget, GdkEventConfigure *event) {

   static GdkPixmap *tmp_buf;

   if (tmp_buf != NULL) {
      gdk_pixmap_unref (tmp_buf);
   }

   tmp_buf =
      gdk_pixmap_new (widget->window,
            widget->allocation.width,
            widget->allocation.height,
            -1);

   if (RoadMapDrawingBuffer != NULL) {
      gdk_image_unref (RoadMapDrawingBuffer);
   }

   RoadMapDrawingBuffer = gdk_drawable_get_image (tmp_buf, 0, 0,
                           widget->allocation.width, widget->allocation.height);

   if (RoadMapDrawingBuffer->visual->type != GDK_VISUAL_TRUE_COLOR) {
      exit(-1);
   }

   roadmap_canvas_agg_configure ((unsigned char*)RoadMapDrawingBuffer->mem,
                                 RoadMapDrawingBuffer->width,
                                 RoadMapDrawingBuffer->height,
                                 RoadMapDrawingBuffer->bpl);

   (*RoadMapCanvasConfigure) ();

   return TRUE;
}


static gint roadmap_canvas_expose (GtkWidget *widget, GdkEventExpose *event) {

   gdk_draw_image (widget->window,
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

   point.x = (short)event->x;
   point.y = (short)event->y;

   switch ((int) data) {
      case 1: (*RoadMapCanvasMouseButtonPressed) (&point);  break;
      case 2: (*RoadMapCanvasMouseButtonReleased) (&point); break;
      case 3: (*RoadMapCanvasMouseMoved) (&point);          break;
   }

   return FALSE;
}


void roadmap_canvas_refresh (void) {

   GdkRectangle update;

   update.x = 0;
   update.y = 0;
   update.width  = RoadMapDrawingArea->allocation.width;
   update.height = RoadMapDrawingArea->allocation.height;

   gtk_widget_queue_draw_area
       (RoadMapDrawingArea, 0, 0,
        RoadMapDrawingArea->allocation.width,
        RoadMapDrawingArea->allocation.height);
}


GtkWidget *roadmap_canvas_new (void) {

   RoadMapDrawingArea = gtk_drawing_area_new ();

   gtk_widget_set_double_buffered (RoadMapDrawingArea, FALSE);

   gtk_widget_set_events
      (RoadMapDrawingArea,
       GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_POINTER_MOTION_MASK);


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

   return RoadMapDrawingArea;
}


