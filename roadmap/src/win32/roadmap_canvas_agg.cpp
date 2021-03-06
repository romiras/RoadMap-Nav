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

#include <windows.h>

#define USE_FRIBIDI

#ifdef WIN32_PROFILE
#include <C:\Program Files\Windows CE Tools\Common\Platman\sdk\wce500\include\cecap.h>
#endif

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
#include "util/agg_color_conv_rgb8.h"
#include "platform/win32/agg_win32_bmp.h"

extern "C" {
#include "../roadmap.h"
#include "../roadmap_canvas.h"
#include "../roadmap_messagebox.h"
#include "../roadmap_math.h"
#include "../roadmap_config.h"
#include "../roadmap_path.h"
#include "../roadmap_screen.h"
#include "roadmap_wincecanvas.h"
#include "colors.h"
}

#include "../roadmap_canvas_agg.h"

static HWND			RoadMapDrawingArea;
static HDC			RoadMapDrawingBuffer;
static RECT			ClientRect;
static HBITMAP		OldBitmap = NULL;


int roadmap_canvas_agg_to_wchar (const char *text, wchar_t *output, int size) {

   LPWSTR text_unicode = ConvertToWideChar(text, CP_UTF8);
   wcsncpy(output, text_unicode, size);
   output[size - 1] = 0;

   return wcslen(output);
}


agg::rgba8 roadmap_canvas_agg_parse_color (const char *color) {
   int high, i, low;

   if (*color == '#') {
      int r, g, b, a;
      int count;

      count = sscanf(color, "#%2x%2x%2x%2x", &r, &g, &b, &a);

      if (count == 4) {
         return agg::rgba8(r, g, b, a);
      } else {
         return agg::rgba8(r, g, b);
      }

   } else {
      /* Do binary search on color table */
      for (low=(-1), high=sizeof(color_table)/sizeof(color_table[0]);
         high-low > 1;) {
         i = (high+low) / 2;
         if (strcmp(color, color_table[i].name) <= 0) {
            high = i;
         } else {
            low = i;
         }
      }

      if (!strcmp(color, color_table[high].name)) {
         return agg::rgba8(color_table[high].r, color_table[high].g,
                            color_table[high].b);
      } else {
         return agg::rgba8(0, 0, 0);
      }
   }
}


void roadmap_canvas_button_pressed(int button, POINT *data) {
   RoadMapGuiPoint point;

   point.x = (short)data->x;
   point.y = (short)data->y;

   (*RoadMapCanvasMouseButtonPressed) (button, &point);

}


void roadmap_canvas_button_released(int button, POINT *data) {
   RoadMapGuiPoint point;

   point.x = (short)data->x;
   point.y = (short)data->y;

   (*RoadMapCanvasMouseButtonReleased) (button, &point);

}


void roadmap_canvas_mouse_moved(POINT *data) {
   RoadMapGuiPoint point;

   point.x = (short)data->x;
   point.y = (short)data->y;

   (*RoadMapCanvasMouseMoved) (0, &point);

}

void roadmap_canvas_mouse_scroll(int direction, POINT *data)
{
    RoadMapGuiPoint point;

    point.x = (short)data->x;
    point.y = (short)data->y;

    direction = (direction > 0) ? 1 : ((direction < 0) ? -1 : 0);

    (*RoadMapCanvasMouseScroll) (direction, &point);

}



void roadmap_canvas_refresh (void) {
   HDC hdc;

   if (RoadMapDrawingArea == NULL) return;

   dbg_time_start(DBG_TIME_FLIP);

   hdc = GetDC(RoadMapDrawingArea);
   BitBlt(hdc, ClientRect.left, ClientRect.top,
      ClientRect.right - ClientRect.left + 1,
      ClientRect.bottom - ClientRect.top + 1,
      RoadMapDrawingBuffer, 0, 0, SRCCOPY);

   DeleteDC(hdc);
   dbg_time_end(DBG_TIME_FLIP);
}


HWND roadmap_canvas_new (HWND hWnd, HWND tool_bar) {
   HDC hdc;
   static BITMAPINFO* bmp_info = (BITMAPINFO*) malloc(sizeof(BITMAPINFO) +
                                       (sizeof(RGBQUAD)*3));
   HBITMAP bmp;
   void* buf = 0;

   memset(bmp_info, 0, sizeof(BITMAPINFO) + sizeof(RGBQUAD)*3);

   if ((RoadMapDrawingBuffer != NULL) && IsWindowVisible(tool_bar)) {

      DeleteObject(SelectObject(RoadMapDrawingBuffer, OldBitmap));
      DeleteDC(RoadMapDrawingBuffer);
   }

   hdc = GetDC(RoadMapDrawingArea);

   RoadMapDrawingArea = hWnd;
   GetClientRect(hWnd, &ClientRect);
   if (tool_bar != NULL) {
      RECT tb_rect;
      GetClientRect(tool_bar, &tb_rect);
      ClientRect.top += tb_rect.bottom;
   }


   RoadMapDrawingBuffer = CreateCompatibleDC(hdc);

   bmp_info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
   bmp_info->bmiHeader.biWidth = ClientRect.right - ClientRect.left + 1;
   bmp_info->bmiHeader.biHeight = ClientRect.bottom - ClientRect.top + 1;
   bmp_info->bmiHeader.biPlanes = 1;
   bmp_info->bmiHeader.biBitCount = 16;
   bmp_info->bmiHeader.biCompression = BI_BITFIELDS;
   bmp_info->bmiHeader.biSizeImage = 0;
   bmp_info->bmiHeader.biXPelsPerMeter = 0;
   bmp_info->bmiHeader.biYPelsPerMeter = 0;
   bmp_info->bmiHeader.biClrUsed = 0;
   bmp_info->bmiHeader.biClrImportant = 0;
   ((DWORD*)bmp_info->bmiColors)[0] = 0xF800;
   ((DWORD*)bmp_info->bmiColors)[1] = 0x07E0;
   ((DWORD*)bmp_info->bmiColors)[2] = 0x001F;

   bmp = CreateDIBSection(
      RoadMapDrawingBuffer,
      bmp_info,
      DIB_RGB_COLORS,
      &buf,
      0,
      0
      );

   int stride = (((ClientRect.right - ClientRect.left + 1) * 2 + 3) >> 2) << 2;
   roadmap_canvas_agg_configure((unsigned char*)buf,
      ClientRect.right - ClientRect.left + 1,
      ClientRect.bottom - ClientRect.top + 1,
      -stride);

   OldBitmap = (HBITMAP)SelectObject(RoadMapDrawingBuffer, bmp);

   DeleteDC(hdc);
   (*RoadMapCanvasConfigure) ();

   return RoadMapDrawingArea;
}

RoadMapImage roadmap_canvas_agg_load_image (const char *path, const char *file_name) {

   char *full_name = roadmap_path_join (path, file_name);

   agg::pixel_map pmap_tmp;
   if(!pmap_tmp.load_from_bmp(full_name)) {
      free(full_name);
      return NULL;
   }

   free(full_name);

   if (pmap_tmp.bpp() != 24) {
      return NULL;
   }

   int width = pmap_tmp.width();
   int height = pmap_tmp.height();
   int stride = pmap_tmp.stride();

   unsigned char *buf = (unsigned char *)malloc (width * height * 4);

   agg::rendering_buffer tmp_rbuf (pmap_tmp.buf(),
                                   width, height,
                                   -pmap_tmp.stride());

   RoadMapImage image =  new roadmap_canvas_image();

   image->rbuf.attach (buf,
                       width, height,
                       width * 4);

   agg::color_conv(&image->rbuf, &tmp_rbuf, agg::color_conv_bgr24_to_rgba32());
   return image;
}
