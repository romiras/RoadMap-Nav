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
#include "agg_pixfmt_rgb_packed.h"
#include "agg_path_storage.h"
#include "../agg/font_freetype/agg_font_freetype.h"

#ifdef USE_FRIBIDI
#include <fribidi.h>
#define MAX_STR_LEN 65000
#endif

extern "C" {
#include "../roadmap.h"
#include "../roadmap_types.h"
#include "../roadmap_gui.h"
#include "../roadmap_screen.h"
#include "../roadmap_canvas.h"
#include "../roadmap_messagebox.h"
#include "../roadmap_math.h"
#include "../roadmap_config.h"
#include "../roadmap_path.h"
}
#include "../roadmap_canvas_agg.h"

typedef agg::pixfmt_rgb565 pixfmt;
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
   agg::rgba8 color;
   int thickness;
};

static struct roadmap_canvas_pen *RoadMapPenList = NULL;
static RoadMapPen CurrentPen;
static int        RoadMapCanvasFontLoaded = 0;

/* The canvas callbacks: all callbacks are initialized to do-nothing
 * functions, so that we don't care checking if one has been setup.
 */
static void roadmap_canvas_ignore_mouse (RoadMapGuiPoint *point) {}

RoadMapCanvasMouseHandler RoadMapCanvasMouseButtonPressed =
                                     roadmap_canvas_ignore_mouse;

RoadMapCanvasMouseHandler RoadMapCanvasMouseButtonReleased =
                                     roadmap_canvas_ignore_mouse;

RoadMapCanvasMouseHandler RoadMapCanvasMouseMoved =
                                     roadmap_canvas_ignore_mouse;


static void roadmap_canvas_ignore_configure (void) {}

RoadMapCanvasConfigureHandler RoadMapCanvasConfigure =
                                     roadmap_canvas_ignore_configure;


void roadmap_canvas_get_text_extents 
        (const char *text, int size, int *width, int *ascent, int *descent) {

   *ascent = 0;
   *descent = 0;

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
      fman = &m_fman;
   } else {

      m_image_feng.height(size);
      m_image_feng.width(size);
      fman = &m_image_fman;
   }

   while(*p) {
      const agg::glyph_cache* glyph = fman->glyph(*p);

      if(glyph) {
         x += glyph->advance_x;
         y += glyph->advance_y;
         if (-glyph->bounds.y1 > *descent) *descent=-glyph->bounds.y1 - 1;
      }
      ++p;
   }

   *width = (int)x;
}


RoadMapPen roadmap_canvas_select_pen (RoadMapPen pen)
{
   RoadMapPen old_pen = CurrentPen;
   dbg_time_start(DBG_TIME_SELECT_PEN);
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
      pen->color = agg::rgba8(0, 0, 0);
      pen->thickness = 1;
      pen->next = RoadMapPenList;
      
      RoadMapPenList = pen;
   }
   
   roadmap_canvas_select_pen (pen);
   
   return pen;
}


void roadmap_canvas_set_foreground (const char *color) {

   if (!CurrentPen) return;

   CurrentPen->color = roadmap_canvas_agg_parse_color(color);
   roadmap_canvas_select_pen(CurrentPen);
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


void roadmap_canvas_erase (void) {

   agg_renb.clear(CurrentPen->color);
}


void roadmap_canvas_draw_string (RoadMapGuiPoint *position,
                                 int corner,
                                 const char *text) {
   int x;
   int y;
   int text_width;
   int text_ascent;
   int text_descent;
   int text_height;
   
   roadmap_canvas_get_text_extents 
         (text, -1, &text_width, &text_ascent, &text_descent);
   
   text_height = text_ascent + text_descent;
   
   switch (corner) {
      
   case ROADMAP_CANVAS_TOPLEFT:
      y = position->y;
      x = position->x;
      break;
      
   case ROADMAP_CANVAS_TOPRIGHT:
      y = position->y;
      x = position->x - text_width;
      break;
      
   case ROADMAP_CANVAS_BOTTOMRIGHT:
      y = position->y - text_height;
      x = position->x - text_width;
      break;
      
   case ROADMAP_CANVAS_BOTTOMLEFT:
      y = position->y - text_height;
      x = position->x;
      break;
      
   case ROADMAP_CANVAS_CENTER:
      y = position->y - (text_height / 2);
      x = position->x - (text_width / 2);
      break;
      
   default:
      return;
   }

   RoadMapGuiPoint start = {x, y+text_height};
   roadmap_canvas_draw_string_angle (&start, position, 0, text);
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
   if (!fast_draw) {
      raso.line_join(agg::outline_miter_accurate_join);
   }
//   raso.accurate_join(true);

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
      
      if (fast_draw) {
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
         
      } else if (fast_draw) {
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
         
      } else if (fast_draw) {
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


void roadmap_canvas_draw_string_angle (RoadMapGuiPoint *position,
                                       RoadMapGuiPoint *center,
                                       int angle, const char *text)
{
   
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
   
   ren_solid.color(CurrentPen->color);
   dbg_time_end(DBG_TIME_TEXT_CNV);
   
   dbg_time_start(DBG_TIME_TEXT_LOAD);
   
   double x  = 0;
   double y  = 0;
   
   dbg_time_end(DBG_TIME_TEXT_LOAD);
   while(*p) {
      dbg_time_start(DBG_TIME_TEXT_ONE_LETTER);
      dbg_time_start(DBG_TIME_TEXT_GET_GLYPH);
      const agg::glyph_cache* glyph = m_fman.glyph(*p);
      dbg_time_end(DBG_TIME_TEXT_GET_GLYPH);

      if(glyph) {
         m_fman.init_embedded_adaptors(glyph, x, y);
         
         //agg::conv_curve<font_manager_type::path_adaptor_type> stroke(m_fman.path_adaptor());
         
         agg::trans_affine mtx;
         if (abs(angle) > 5) {
            mtx *= agg::trans_affine_rotation(agg::deg2rad(angle));
         }
         mtx *= agg::trans_affine_translation(position->x, position->y);
         
         agg::conv_transform<font_manager_type::path_adaptor_type> tr(m_fman.path_adaptor(), mtx);

         agg::conv_curve<agg::conv_transform<font_manager_type::path_adaptor_type> > fill(tr);

         //agg::conv_stroke<
            //agg::conv_curve<agg::conv_transform<font_manager_type::path_adaptor_type> > > 
         //stroke(fill);

         //agg::conv_contour<agg::conv_transform<font_manager_type::path_adaptor_type> >contour(tr);
         //contour.width(2);

         //agg::conv_stroke< agg::conv_contour<agg::conv_transform<font_manager_type::path_adaptor_type> > > stroke(contour);
         //agg::conv_stroke< agg::conv_transform<font_manager_type::path_adaptor_type> > stroke(tr);

         dbg_time_start(DBG_TIME_TEXT_ONE_RAS);
         
#ifdef WIN32_PROFILE
         ResumeCAPAll();
#endif
         ras.reset();
         ras.add_path(tr);
         agg::render_scanlines(ras, sl, ren_solid);
         //ras.add_path(fill);
         //ras.add_path(stroke);
         //ren_solid.color(agg::rgba8(255, 255, 255));
         //agg::render_scanlines(ras, sl, ren_solid);
         //ras.add_path(tr);
         //ren_solid.color(agg::rgba8(0, 0, 0));
         //agg::render_scanlines(ras, sl, ren_solid);

#ifdef WIN32_PROFILE
         SuspendCAPAll();
#endif
         
         dbg_time_end(DBG_TIME_TEXT_ONE_RAS);
         
         // increment pen position
         x += glyph->advance_x;
         y += glyph->advance_y;
         dbg_time_end(DBG_TIME_TEXT_ONE_LETTER);
      }
      ++p;
   }

#ifdef USE_FRIBIDI
   free(bidi_text);
#endif

   dbg_time_end(DBG_TIME_TEXT_FULL);
}

void roadmap_canvas_agg_configure (unsigned char *buf, int width, int height, int stride) {

   agg_rbuf.attach(buf, width, height, stride);
   
   agg_renb.attach(agg_pixf);
   agg_renb.reset_clipping(true);
   ras.clip_box(0, 0, agg_renb.width(), agg_renb.height());

   agg::glyph_rendering gren = agg::glyph_ren_outline; 
   agg::glyph_rendering image_gren = agg::glyph_ren_agg_gray8; 

   roadmap_config_declare
       ("preferences", &RoadMapConfigFont, "font.ttf");

   char font_file[255];
   
   snprintf(font_file, sizeof(font_file), "%s/%s", roadmap_path_user(),
      roadmap_config_get (&RoadMapConfigFont));

   if (!RoadMapCanvasFontLoaded) {

      if(m_feng.load_font(font_file, 0, gren) &&
            m_image_feng.load_font(font_file, 0, image_gren)) {

         m_feng.hinting(true);
         m_feng.height(15);
         m_feng.width(15);
         m_feng.flip_y(true);

         m_image_feng.hinting(true);
         m_image_feng.flip_y(true);

         RoadMapCanvasFontLoaded = 1;
      } else {
         RoadMapCanvasFontLoaded = -1;
         char message[300];
         snprintf(message, sizeof(message), "Can't load font: %s\n", font_file);
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
   
   ren_solid.color(CurrentPen->color);
   
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

