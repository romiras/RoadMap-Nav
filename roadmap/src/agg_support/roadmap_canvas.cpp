/* roadmap_canvas.cpp - manage the canvas that is used to draw the map with agg
 *
 * LICENSE:
 *
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

#define PEN_DEBUG 0

#define FAST 0


#ifdef WIN32_PROFILE
#include <C:\Program Files\Windows CE Tools\Common\Platman\sdk\wce500\include\cecap.h>
#endif

#include <wchar.h>
#include "agg_rendering_buffer.h"
#include "agg_curves.h"
#include "agg_conv_stroke.h"
#include "agg_conv_contour.h"
#include "agg_conv_transform.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_rasterizer_outline_aa.h"
#include "agg_rasterizer_outline.h"
#include "agg_renderer_primitives.h"
#include "agg_ellipse.h"
#include "agg_renderer_scanline.h"
#include "agg_scanline_p.h"
#include "agg_renderer_outline_aa.h"
#include "agg_path_storage.h"

#include "agg_font_freetype.h"

#ifdef USE_FRIBIDI
#include <fribidi.h>
#define MAX_STR_LEN 65000
#endif

#define DEFAULT_FONT_SIZE 20

extern "C" {
#include "roadmap.h"
#include "roadmap_types.h"
#include "roadmap_gui.h"
#include "roadmap_screen.h"
#include "roadmap_canvas.h"
#include "roadmap_messagebox.h"
#include "roadmap_math.h"
#include "roadmap_config.h"
#include "roadmap_scan.h"
#include "roadmap_file.h"
#include "roadmap_path.h"
}
#include "roadmap_canvas_agg.h"

#include "agg_pixfmt_rgb_packed.h"
#include "agg_pixfmt_rgba.h"
#include "agg_pixfmt_rgb.h"


typedef agg::AGG_PIXFMT pixfmt;
typedef agg::renderer_base<pixfmt> renbase_type;
typedef agg::renderer_primitives<renbase_type> renderer_pr;
typedef agg::font_engine_freetype_int32 font_engine_type;
typedef agg::font_cache_manager<font_engine_type> font_manager_type;

agg::rendering_buffer agg_rbuf;

static pixfmt agg_pixf(agg_rbuf);
static agg::renderer_base<pixfmt> agg_renb;

static agg::line_profile_aa profile(2, agg::gamma_none());

static agg::renderer_outline_aa<renbase_type> reno(agg_renb, profile);
static agg::rasterizer_outline_aa< agg::renderer_outline_aa<renbase_type> >  raso(reno);

static agg::rasterizer_scanline_aa<> ras;
static agg::scanline_p8 sl;
static agg::renderer_scanline_aa_solid<agg::renderer_base<pixfmt> > ren_solid(agg_renb);

static font_engine_type             m_feng;
static font_manager_type            m_fman(m_feng);

static font_engine_type             m_image_feng;
static font_manager_type            m_image_fman(m_image_feng);

static RoadMapConfigDescriptor RoadMapConfigFont =
                        ROADMAP_CONFIG_ITEM("Labels", "FontName");

struct roadmap_canvas_pen {   
   struct roadmap_canvas_pen *next;
   char  *name;
   char  *color_name;
   agg::rgba8 color;
   char  *font_color_name;
   agg::rgba8 font_color;
   int thickness;
   int size;
};

static struct roadmap_canvas_pen *RoadMapPenList = NULL;
static RoadMapPen CurrentPen;
static int        RoadMapCanvasFontLoaded = 0;

/* The canvas callbacks: all callbacks are initialized to do-nothing
 * functions, so that we don't care checking if one has been setup.
 */
static void roadmap_canvas_ignore_mouse (int button, RoadMapGuiPoint *point) {}

RoadMapCanvasMouseHandler RoadMapCanvasMouseButtonPressed =
                                     roadmap_canvas_ignore_mouse;

RoadMapCanvasMouseHandler RoadMapCanvasMouseButtonReleased =
                                     roadmap_canvas_ignore_mouse;

RoadMapCanvasMouseHandler RoadMapCanvasMouseMoved =
                                     roadmap_canvas_ignore_mouse;

RoadMapCanvasMouseHandler RoadMapCanvasMouseScroll =
                                     roadmap_canvas_ignore_mouse;


static void roadmap_canvas_ignore_configure (void) {}

RoadMapCanvasConfigureHandler RoadMapCanvasConfigure =
                                     roadmap_canvas_ignore_configure;

static void roadmap_canvas_draw_string_worker (RoadMapGuiPoint *start,
                                       RoadMapGuiPoint *center,
                                       int width,
                                       int angle, const char *text);

void roadmap_canvas_get_text_extents 
        (const char *text, int *width,
            int *ascent, int *descent, int *can_tilt) {

   int size = CurrentPen->size;
   *ascent = 0;
   *descent = 0;
   if (can_tilt) *can_tilt = 1;

   wchar_t wstr[255];
   int length = roadmap_canvas_agg_to_wchar (text, wstr, 255);

   if (length <=0) {
      *width = 0;
      return;
   }

   double x  = 0;
   double y  = 0;
   const wchar_t* p = wstr;

   font_manager_type *fman;

   if (size == -1) {
      /* Use the regular font */
      *descent = abs((int)m_feng.descender());
      *ascent = (int)m_feng.ascender();
      fman = &m_fman;
   } else {

      m_image_feng.height(size);
      m_image_feng.width(size);
      *descent = abs((int)m_image_feng.descender());
      *ascent = (int)m_image_feng.ascender();
      fman = &m_image_fman;
   }

   while(*p) {
      const agg::glyph_cache* glyph = fman->glyph(*p);

      if(glyph) {
         x += glyph->advance_x;
         y += glyph->advance_y;
// (removed, per editor branch)         if (-glyph->bounds.y1 > *descent) *descent=-glyph->bounds.y1 - 1;
      }
      ++p;
   }

   *width = (int)x;
}


RoadMapPen roadmap_canvas_select_pen (RoadMapPen pen)
{
   RoadMapPen old_pen = CurrentPen;
   dbg_time_start(DBG_TIME_SELECT_PEN);
   if (pen && PEN_DEBUG) {
      roadmap_log(ROADMAP_DEBUG, "selecting pen %s color %s", pen->name, pen->color_name);
   }
   if (!CurrentPen || (pen->thickness != CurrentPen->thickness)) {
      profile.width(pen->thickness);
   }
   CurrentPen = pen;

   reno.color(pen->color);

   dbg_time_end(DBG_TIME_SELECT_PEN);

   return old_pen;
}


RoadMapPen roadmap_canvas_create_pen (const char *name)
{
   struct roadmap_canvas_pen *pen;
   
   for (pen = RoadMapPenList; pen != NULL; pen = pen->next) {
      if (strcmp(pen->name, name) == 0) break;
   }
   
   if (pen == NULL) {
      
      pen = (struct roadmap_canvas_pen *)
         malloc (sizeof(struct roadmap_canvas_pen));
      roadmap_check_allocated(pen);
      
      pen->name = strdup (name);
      pen->font_color_name =
	    pen->color_name = 0;
      pen->font_color =
	    pen->color = agg::rgba8(0, 0, 0);
      pen->thickness = 1;
      pen->size = DEFAULT_FONT_SIZE;
      pen->next = RoadMapPenList;
      
      RoadMapPenList = pen;
   }
   
   roadmap_canvas_select_pen (pen);
   
   return pen;
}


void roadmap_canvas_set_foreground (const char *color) {

   if (!CurrentPen) return;
   CurrentPen->color_name = strdup (color);
   CurrentPen->color = roadmap_canvas_agg_parse_color(color);
   roadmap_canvas_select_pen(CurrentPen);
}

void roadmap_canvas_set_label_font_color(const char *color) {

   if (!CurrentPen) return;
   CurrentPen->font_color_name = strdup (color);
   CurrentPen->font_color = roadmap_canvas_agg_parse_color(color);
   roadmap_canvas_select_pen(CurrentPen);
}

void roadmap_canvas_set_label_font_size(int size) {
   if (!CurrentPen) return;
   CurrentPen->size = size;
   roadmap_canvas_select_pen(CurrentPen);
}

void roadmap_canvas_set_linestyle (const char *style) {

   // unimplemented

}


int  roadmap_canvas_get_thickness  (RoadMapPen pen) {

   if (pen == NULL) return 0;

   return pen->thickness;
}


void roadmap_canvas_set_thickness (int thickness) {

   if (CurrentPen && (CurrentPen->thickness != thickness)) {
      CurrentPen->thickness = thickness;
      profile.width(thickness);
   }
}


void roadmap_canvas_set_opacity (int opacity) {

   if (!CurrentPen) return;
   CurrentPen->color.a = opacity;
   roadmap_canvas_select_pen(CurrentPen);
}

/* this are stubs */
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

   agg_renb.clear(CurrentPen->color);
}


void roadmap_canvas_draw_string (RoadMapGuiPoint *position,
                                 int corner,
                                 const char *text) {
   int text_width;
   int text_ascent;
   int text_descent;
   RoadMapGuiPoint start[1];
   
   roadmap_canvas_get_text_extents 
         (text, &text_width, &text_ascent, &text_descent, NULL);
   
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

   roadmap_canvas_draw_string_worker (start, position, 0, 0, text);
}

void roadmap_canvas_draw_string_angle
    (  RoadMapGuiPoint *center, int theta, const char *text) {
   int text_width;
   int text_ascent;
   int text_descent;
   RoadMapGuiPoint start[1];

   roadmap_canvas_get_text_extents 
      (text, &text_width, &text_ascent, &text_descent, NULL);

   start->x = center->x - text_width/2;
   start->y = center->y - text_descent;

   roadmap_canvas_draw_string_worker (start, center, text_width, theta, text);
}


void roadmap_canvas_draw_multiple_points (int count, RoadMapGuiPoint *points) {

   int i;

   for (i=0; i<count; i++) {

      agg_renb.copy_pixel(points[i].x, points[i].y, CurrentPen->color);
   }
}


void roadmap_canvas_draw_multiple_lines (int count, int *lines,
      RoadMapGuiPoint *points, int fast_draw) {

   int i;
   int count_of_points;
   
   dbg_time_start(DBG_TIME_DRAW_LINES);
#ifdef WIN32_PROFILE
   ResumeCAPAll();
#endif

   raso.round_cap(true);
   if (!(fast_draw || FAST)) {
      raso.line_join(agg::outline_miter_accurate_join);
   }

   agg::path_storage path;
   
   for (i = 0; i < count; ++i) {
      
      int first = 1;
      
      count_of_points = *lines;
      
      if (count_of_points < 2) continue;
      
      dbg_time_start(DBG_TIME_CREATE_PATH);

      for (int j=0; j<count_of_points; j++) {

         if (first) {
            first = 0;
            path.move_to(points->x, points->y);            
         } else {
            path.line_to(points->x, points->y);
         }

         points++;
      }

      dbg_time_end(DBG_TIME_CREATE_PATH);
      dbg_time_start(DBG_TIME_ADD_PATH);
      
      if (fast_draw || FAST) {
         renderer_pr ren_pr(agg_renb);
         agg::rasterizer_outline<renderer_pr> ras_line(ren_pr);
         ren_pr.line_color(CurrentPen->color);
         ras_line.add_path(path);
         
      } else {
         
         raso.add_path(path);
      }
      
      path.remove_all ();
      dbg_time_end(DBG_TIME_ADD_PATH);
      
      lines += 1;
   }
   
     
#ifdef WIN32_PROFILE
   SuspendCAPAll();
#endif
   
   dbg_time_end(DBG_TIME_DRAW_LINES);
}

void roadmap_canvas_draw_multiple_polygons
         (int count, int *polygons, RoadMapGuiPoint *points, int filled,
          int fast_draw) {

   int i;
   int count_of_points;

   agg::path_storage path;
   
   for (i = 0; i < count; ++i) {
      
      count_of_points = *polygons;
      
      int first = 1;
      
      for (int j=0; j<count_of_points; j++) {
         
         if (first) {
            first = 0;
            path.move_to(points->x, points->y);            
         } else {
            path.line_to(points->x, points->y);
         }
         points++;
      }
      
      path.close_polygon();
      
      if (filled) {
         
         ras.reset();
         ras.add_path(path);
         ren_solid.color(CurrentPen->color);
         agg::render_scanlines( ras, sl, ren_solid);
         
      } else if (fast_draw || FAST) {
         renderer_pr ren_pr(agg_renb);
         agg::rasterizer_outline<renderer_pr> ras_line(ren_pr);
         ren_pr.line_color(CurrentPen->color);
         ras_line.add_path(path);
         
      } else {
         
         raso.add_path(path);
      }
      
      path.remove_all ();
      
      polygons += 1;
   }
}


void roadmap_canvas_draw_multiple_circles
        (int count, RoadMapGuiPoint *centers, int *radius, int filled,
         int fast_draw) {

   int i;

   agg::path_storage path;
   
   for (i = 0; i < count; ++i) {
      
      int r = radius[i];
      
      int x = centers[i].x;
      int y = centers[i].y;
      
      agg::ellipse e( x, y, r, r);
      path.concat_path(e);
      
      if (filled) {
         
         ras.reset();
         ras.add_path(path);
         ren_solid.color(CurrentPen->color);
         agg::render_scanlines( ras, sl, ren_solid);
         
      } else if (fast_draw || FAST) {
         renderer_pr ren_pr(agg_renb);
         agg::rasterizer_outline<renderer_pr> ras_line(ren_pr);
         ren_pr.line_color(CurrentPen->color);
         ras_line.add_path(path);
         
      } else {
         
         raso.add_path(path);
      }
      
      path.remove_all ();
   }
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


void roadmap_canvas_register_configure_handler
                    (RoadMapCanvasConfigureHandler handler) {

   RoadMapCanvasConfigure = handler;
}


int roadmap_canvas_width (void) {
   return agg_renb.width();
}


int roadmap_canvas_height (void) {
   return agg_renb.height();
}


void roadmap_canvas_save_screenshot (const char* filename) {
   /* NOT IMPLEMENTED. */
}


/*
** Use FRIBIDI to encode the string.
** The return value must be freed by the caller.
*/

#ifdef USE_FRIBIDI
static wchar_t* bidi_string(wchar_t *logical) {
   
   FriBidiCharType base = FRIBIDI_TYPE_ON;
   size_t len;
   
   len = wcslen(logical);
   
   FriBidiChar *visual;
   
   FriBidiStrIndex *ltov, *vtol;
   FriBidiLevel *levels;
   FriBidiStrIndex new_len;
   fribidi_boolean log2vis;
   
   visual = (FriBidiChar *) malloc (sizeof (FriBidiChar) * (len + 1));
   ltov = NULL;
   vtol = NULL;
   levels = NULL;
   
   /* Create a bidi string. */
   log2vis = fribidi_log2vis ((FriBidiChar *)logical, len, &base,
      /* output */
      visual, ltov, vtol, levels);
   
   if (!log2vis) {
      //msSetError(MS_IDENTERR, "Failed to create bidi string.", 
      //"msGetFriBidiEncodedString()");
      return NULL;
   }
   
   new_len = len;
   
   return (wchar_t *)visual;
   
}
#endif


static void roadmap_canvas_draw_string_worker (RoadMapGuiPoint *start,
                                       RoadMapGuiPoint *center,
                                       int width,
                                       int angle, const char *text)
{
   int size;
   
   if (RoadMapCanvasFontLoaded != 1) return;
   
   dbg_time_start(DBG_TIME_TEXT_FULL);      
   dbg_time_start(DBG_TIME_TEXT_CNV);
   
   wchar_t wstr[255];
   int length = roadmap_canvas_agg_to_wchar (text, wstr, 255);
   if (length <=0) return;
   
#ifdef USE_FRIBIDI
   wchar_t *bidi_text = bidi_string(wstr);
   const wchar_t* p = bidi_text;
#else   
   const wchar_t* p = wstr;
#endif
   
   ren_solid.color(CurrentPen->font_color);
   dbg_time_end(DBG_TIME_TEXT_CNV);
   
   dbg_time_start(DBG_TIME_TEXT_LOAD);
   
   double x  = 0;
   double y  = 0;
   
   size = CurrentPen->size;

   if ((angle > -5) && (angle < 5)) {

      /* Use faster drawing for text with no angle */
      x  = start->x;
      y  = start->y;

      m_image_feng.height(size);
      m_image_feng.width(size);


      while(*p) {
         const agg::glyph_cache* glyph = m_image_fman.glyph(*p);

         if(glyph) {
            m_image_fman.init_embedded_adaptors(glyph, x, y);

            agg::render_scanlines(m_image_fman.gray8_adaptor(), 
                  m_image_fman.gray8_scanline(), 
                  ren_solid);      

            // increment pen psition
            x += glyph->advance_x;
            y += glyph->advance_y;
         }
         ++p;
      }

   } else {

      double scale = (double)size / (double)DEFAULT_FONT_SIZE;


      while(*p) {
         dbg_time_start(DBG_TIME_TEXT_ONE_LETTER);
         dbg_time_start(DBG_TIME_TEXT_GET_GLYPH);
         const agg::glyph_cache* glyph = m_fman.glyph(*p);
         dbg_time_end(DBG_TIME_TEXT_GET_GLYPH);

         if(glyph) {
            m_fman.init_embedded_adaptors(glyph, x, y);
            
            agg::trans_affine mtx;
            mtx *= agg::trans_affine_translation((start->x - center->x)/scale, (start->y - center->y)/scale);
            if (abs(angle) > 5) {
               mtx *= agg::trans_affine_rotation(agg::deg2rad(angle));
            }
            mtx *= agg::trans_affine_scaling(scale, scale);
            mtx *= agg::trans_affine_translation(center->x, center->y);
            
            agg::conv_transform<font_manager_type::path_adaptor_type> tr(m_fman.path_adaptor(), mtx);

            agg::conv_curve<agg::conv_transform<font_manager_type::path_adaptor_type> > fill(tr);

            dbg_time_start(DBG_TIME_TEXT_ONE_RAS);
            
#ifdef WIN32_PROFILE
            ResumeCAPAll();
#endif
            ras.reset();
            ras.add_path(tr);
            agg::render_scanlines(ras, sl, ren_solid);

#ifdef WIN32_PROFILE
            SuspendCAPAll();
#endif
            
            dbg_time_end(DBG_TIME_TEXT_ONE_RAS);
            
            // increment pen psition
            x += glyph->advance_x;
            y += glyph->advance_y;
            dbg_time_end(DBG_TIME_TEXT_ONE_LETTER);
         }
         ++p;
      }
   }  

   dbg_time_end(DBG_TIME_TEXT_LOAD);

#ifdef USE_FRIBIDI
   free(bidi_text);
#endif

   dbg_time_end(DBG_TIME_TEXT_FULL);
}

void roadmap_canvas_agg_configure (unsigned char *buf, int width, int height, int stride) {

   agg_rbuf.attach(buf, width, height, stride);
   
   agg_renb.attach(agg_pixf);
   agg_renb.reset_clipping(true);
   ras.clip_box(0, 0, agg_renb.width() - 1, agg_renb.height() - 1);

   agg::glyph_rendering gren = agg::glyph_ren_outline; 
   agg::glyph_rendering image_gren = agg::glyph_ren_agg_gray8; 

   roadmap_config_declare
       ("preferences", &RoadMapConfigFont, "font.ttf");

   if (!RoadMapCanvasFontLoaded) {
      const char *font_file;
      
      font_file = roadmap_config_get (&RoadMapConfigFont);

      if (! roadmap_path_is_full_path (font_file)) {
          const char *cursor;
          for (cursor = roadmap_scan ("user", font_file);
               cursor != NULL;
               cursor = roadmap_scan_next ("user", font_file, cursor)) {
             if (roadmap_file_exists (cursor, font_file))
                break;
          }

          if (cursor == NULL) {
             for (cursor = roadmap_scan ("config", font_file);
                  cursor != NULL;
                  cursor = roadmap_scan_next ("config", font_file, cursor)) {

                if (roadmap_file_exists (cursor, font_file))
                   break;
             }
          }
          if (cursor != NULL) {
             font_file = roadmap_path_join(cursor, font_file);
          }
      }

      if (PEN_DEBUG)
	  roadmap_log(ROADMAP_DEBUG, "loading AGG font from %s", font_file);


      if(m_feng.load_font(font_file, 0, gren) &&
            m_image_feng.load_font(font_file, 0, image_gren)) {

         m_feng.hinting(true);
         m_feng.height(DEFAULT_FONT_SIZE);
         m_feng.width(DEFAULT_FONT_SIZE);
         m_feng.flip_y(true);

         m_image_feng.hinting(true);
         m_image_feng.flip_y(true);

         RoadMapCanvasFontLoaded = 1;
      } else {
         RoadMapCanvasFontLoaded = -1;
         char message[300];
         snprintf(message, sizeof(message),
            "Can't load font: %s\n"
            "A suitable TrueType font must be accessible via the"
            " Labels.FontName preference setting." , font_file);  
         roadmap_messagebox("Error", message);
      }
   }
}


RoadMapImage roadmap_canvas_load_image (const char *path,
                                        const char* file_name) {

   return roadmap_canvas_agg_load_image (path, file_name);
}

void roadmap_canvas_draw_image (RoadMapImage image, RoadMapGuiPoint *pos,
                                int opacity, int mode) {

   if ((mode == IMAGE_SELECTED) || (opacity <= 0) || (opacity >= 255)) {
      opacity = 255;
   }

   agg_renb.blend_from(image->pixfmt, 0, pos->x, pos->y, opacity);

   if (mode == IMAGE_SELECTED) {

      RoadMapGuiPoint points[5] = {
         {pos->x, pos->y},
         {pos->x + image->rbuf.width(), pos->y},
         {pos->x + image->rbuf.width(), pos->y + image->rbuf.height()},
         {pos->x, pos->y + image->rbuf.height()},
         {pos->x, pos->y}};

      int num_points = 5;

      roadmap_canvas_draw_multiple_lines (1, &num_points, points, 0);
   }
}


void roadmap_canvas_copy_image (RoadMapImage dst_image,
                                const RoadMapGuiPoint *pos,
                                const RoadMapGuiRect  *rect,
                                RoadMapImage src_image, int mode) {

   agg::renderer_base<agg::pixfmt_rgba32> renb(dst_image->pixfmt);

   agg::rect_i agg_rect;
   agg::rect_i *agg_rect_p = NULL;

   if (rect) {
      agg_rect.x1 = rect->minx;
      agg_rect.y1 = rect->miny;
      agg_rect.x2 = rect->maxx;
      agg_rect.y2 = rect->maxy;

      agg_rect_p = &agg_rect;
   }

   if (mode == CANVAS_COPY_NORMAL) {
      renb.copy_from(src_image->rbuf, agg_rect_p, pos->x, pos->y);
   } else {
      renb.blend_from(src_image->pixfmt, agg_rect_p, pos->x, pos->y, 255);
   }
}


int  roadmap_canvas_image_width  (const RoadMapImage image) {

   if (!image) return 0;
   
   return image->rbuf.width();
}


int  roadmap_canvas_image_height (const RoadMapImage image) {
   
   if (!image) return 0;
   
   return image->rbuf.height();
}


void roadmap_canvas_draw_image_text (RoadMapImage image,
                                     const RoadMapGuiPoint *position,
                                     int size, const char *text) {
   
   if (RoadMapCanvasFontLoaded != 1) return;
   
   wchar_t wstr[255];
   int length = roadmap_canvas_agg_to_wchar (text, wstr, 255);
   if (length <=0) return;
   
#ifdef USE_FRIBIDI
   wchar_t *bidi_text = bidi_string(wstr);
   const wchar_t* p = bidi_text;
#else   
   const wchar_t* p = wstr;
#endif
   
   ren_solid.color(CurrentPen->font_color);
   
   double x  = position->x;
   double y  = position->y + size - 7;

   agg::renderer_base<agg::pixfmt_rgba32> renb(image->pixfmt);
   agg::renderer_scanline_aa_solid< agg::renderer_base<agg::pixfmt_rgba32> > ren_solid (renb);

   ren_solid.color(agg::rgba8(0, 0, 0));

   m_image_feng.height(size);
   m_image_feng.width(size);

   while(*p) {
      const agg::glyph_cache* glyph = m_image_fman.glyph(*p);

      if(glyph) {
         m_image_fman.init_embedded_adaptors(glyph, x, y);
         
         agg::render_scanlines(m_image_fman.gray8_adaptor(), 
               m_image_fman.gray8_scanline(), 
               ren_solid);      

         // increment pen position
         x += glyph->advance_x;
         y += glyph->advance_y;
      }
      ++p;
   }

#ifdef USE_FRIBIDI
   free(bidi_text);
#endif

}

