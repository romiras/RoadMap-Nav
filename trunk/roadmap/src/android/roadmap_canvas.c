/*
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
 *   Copyright 2010 Danny Backx
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

/**
 * @file
 * @brief manage the canvas that is used to draw the map.
 * @ingroup android
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "roadmap.h"
#include "roadmap_types.h"
#include "roadmap_gui.h"

#include "roadmap_canvas.h"

#include <android/log.h>
#include "roadmap_jni.h"

#define ROADMAP_CURSOR_SIZE           10
#define ROADMAP_CANVAS_POINT_BLOCK  1024

#define	MYCLS	"net/sourceforge/projects/roadmap/Panel"

/**
 * @brief cache for the reference to the Panel class.
 *
 * Make sure to clean this up when terminating the Activity,
 * but cache during one run.
 */
static jclass	myPanelClass;

static jclass TheClass()
{
	if (myPanelClass == 0) {
		myPanelClass = (*RoadMapJniEnv)->FindClass(RoadMapJniEnv, MYCLS);
		myPanelClass = (*RoadMapJniEnv)->NewGlobalRef(RoadMapJniEnv, myPanelClass);
	}
	if (myPanelClass == 0) {
		__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "Class not found");
		// throw
		(*RoadMapJniEnv)->ThrowNew(RoadMapJniEnv,
			(*RoadMapJniEnv)->FindClass(RoadMapJniEnv, "java/io/IOException"),
			"A JNI Exception occurred");
	}

	return myPanelClass;
}

static jmethodID TheMethod(const jclass cls, const char *name, const char *signature)
{
	jmethodID	mid;

	mid = (*RoadMapJniEnv)->GetMethodID(RoadMapJniEnv, cls, name, signature);
	if (mid == 0) {
		(*RoadMapJniEnv)->ThrowNew(RoadMapJniEnv,
			(*RoadMapJniEnv)->FindClass(RoadMapJniEnv, "java/io/IOException"),
			"A JNI Exception occurred");
	}
	return mid;
}

/**
 * @brief
 */
struct roadmap_canvas_pen {
   struct roadmap_canvas_pen *next;
   char  *name;
};
static RoadMapPen	CurrentPen = NULL;

static struct roadmap_canvas_pen *RoadMapPenList = NULL;
static char *roadmap_canvas_lookup_colourname(const char *name);


/* The canvas callbacks: all callbacks are initialized to do-nothing
 * functions, so that we don't care checking if one has been setup.
 */
static void roadmap_canvas_ignore_mouse (int button, RoadMapGuiPoint *point) {}
static void roadmap_canvas_ignore_configure (void) {}

static RoadMapCanvasMouseHandler
	RoadMapCanvasMouseButtonPressed = roadmap_canvas_ignore_mouse,
	RoadMapCanvasMouseButtonReleased = roadmap_canvas_ignore_mouse,
	RoadMapCanvasMouseMoved = roadmap_canvas_ignore_mouse,
	RoadMapCanvasMouseScroll = roadmap_canvas_ignore_mouse;

static RoadMapCanvasConfigureHandler
	RoadMapCanvasConfigure = roadmap_canvas_ignore_configure;

/**
 * @brief convert a list of points
 */
static void roadmap_canvas_convert_points (jfloatArray apoints,
		RoadMapGuiPoint *points, int count)
{
    int		i, j;
    float	*tmp = (float *)malloc(2 * sizeof(float) * count);

    for (i=0, j=0; i<count; i++, points++) {
        tmp[j++] = points->x;
        tmp[j++] = points->y;
    }

    (*RoadMapJniEnv)->SetFloatArrayRegion(RoadMapJniEnv, apoints, 0, 2*count, tmp);
    free(tmp);
}

#if 0
/*
 * @brief convert a list of points, but make sure to duplicate all but the first and last points
 * because RoadMap and Android have different semantics here.
 */
static void roadmap_canvas_convert_lines (jfloatArray apoints,
		RoadMapGuiPoint *points, int count)
{
    int		i, j;
    float	*tmp = (float *)malloc(4 * sizeof(float) * count);

    /* first entry : only once */
    tmp[0] = points->x;
    tmp[1] = points->y;
    points++;

    for (i=1, j=2; i<count-1; i++, points++) {
        tmp[j++] = points->x;
        tmp[j++] = points->y;
        tmp[j++] = points->x;
        tmp[j++] = points->y;
    }

    /* last entry : only once */
    tmp[j++] = points->x;
    tmp[j++] = points->y;

    (*RoadMapJniEnv)->SetFloatArrayRegion(RoadMapJniEnv, apoints, 0, 4 * count - 4, tmp);
    free(tmp);
}

static void roadmap_canvas_convert_ints (jintArray acenters,
		int *centers, int count)
{
	(*RoadMapJniEnv)->SetIntArrayRegion(RoadMapJniEnv, acenters, 0, count, centers);
}
#endif

/**
 * @brief
 * @param text
 * @param size
 * @param width
 * @param ascent
 * @param descent
 * @param can_tilt
 */
void roadmap_canvas_get_text_extents (const char *text, int size, int *width,
            int *ascent, int *descent, int *can_tilt)
{
	jclass		cls = TheClass();
	jmethodID	mid = TheMethod(cls, "MeasureWidth", "(Ljava/lang/String;I)I");
	jstring		js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, text);
	int		r;

	r = (*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, PanelThiz, mid, js);
	if (width)
		*width = r;

	mid = TheMethod(cls, "MeasureAscent", "()I");
	if (ascent)
		*ascent = (*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, PanelThiz, mid);

	mid = TheMethod(cls, "MeasureDescent", "()I");
	if (descent)
		*descent = (*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, PanelThiz, mid);

	if (can_tilt)
		*can_tilt = 0;	// copied from Win32 implementation

	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "text(%s) wid %d asc %d desc %d",
			text, *width, *ascent, *descent);
	return;
}

/**
 * @brief Cache for the Pen names in C so we only pass small numbers between Java and C.
 * This is much more efficient than converting strings all the time.
 */
typedef struct PenStruct {
	char	*name;
} PenStruct;
static PenStruct	*PenName = 0;
static int		nPens = 0, maxPen = 0;

RoadMapPen roadmap_canvas_select_pen (RoadMapPen pen)
{
	jclass		cls = TheClass();
	jmethodID	mid = TheMethod(cls, "SelectPen", "(I)V");

	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "select_pen(%d,%s)", (int)pen, PenName[(int)pen].name);

	CurrentPen = pen;	// for debugging only

	(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, PanelThiz, mid, pen);
	return pen;
}

/**
 * @brief
 * @param name
 * @return a Pen (which is a numeric index in this implementation)
 */
RoadMapPen roadmap_canvas_create_pen (const char *name)
{
	jclass		cls = TheClass();
	jmethodID	mid = TheMethod(cls, "CreatePen", "(I)V");
	RoadMapPen	pen;
	int		i;

	for (i=0; i<nPens; i++) {
		if (strcmp(name, PenName[i].name) == 0)
			return (RoadMapPen)i;
	}

	if (nPens == maxPen) {
		maxPen += 100;
		PenName = (PenStruct *)realloc((void *)PenName, maxPen * sizeof(PenStruct));
	}

	PenName[nPens].name = strdup(name);
	pen = (RoadMapPen)nPens;
	nPens++;

	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "Create Pen(%s) -> %d", name, (int)pen);

	(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, PanelThiz, mid, pen);

	roadmap_canvas_select_pen (pen);
	return pen;
}

/**
 * @brief set the foreground colour
 * @param color the colour to be used
 */
void roadmap_canvas_set_foreground (const char *color)
{
	jstring		js;
	jclass		cls = TheClass();
	jmethodID	mid = TheMethod(cls, "SetForeground", "(Ljava/lang/String;)I");
	int		r;

	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "set_foreground(%d,%s) - %s", (int)CurrentPen, PenName[(int)CurrentPen].name, color);

	js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, color);

	r = (*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, PanelThiz, mid, js);
	if (r == -1) {
		/* Colour name unknown, try to map it ourselves */
		char *s = roadmap_canvas_lookup_colourname(color);
		if (s) {
			js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, s);
			r = (*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, PanelThiz, mid, js);
		}
	}
}

/**
 * @brief set the line style
 * @param style
 */
void roadmap_canvas_set_linestyle (const char *style)
{
	int		dashed = 0;
	jclass		cls = TheClass();
	jmethodID	mid = TheMethod(cls, "SetLinestyle", "(I)V");

	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "set_linestyle(%d,%s) - %s", (int)CurrentPen, PenName[(int)CurrentPen].name, style);

	if (strcasecmp(style, "dashed") == 0)
		dashed = 1;

	(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, PanelThiz, mid, dashed);
}

/**
 * @brief set line thickness
 * @param thickness
 */
void roadmap_canvas_set_thickness (int thickness)
{
	jclass		cls = TheClass();
	jmethodID	mid = TheMethod(cls, "SetThickness", "(I)V");

	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "set_thickness(%d,%s) - %d", (int)CurrentPen, PenName[(int)CurrentPen].name, thickness);

	if (thickness < 2)
		thickness = 1;

	(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, PanelThiz, mid, thickness);
}

void roadmap_canvas_erase (void)
{
	jclass		cls = TheClass();
	jmethodID	mid = TheMethod(cls, "Erase", "()V");

	(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, PanelThiz, mid);
}


void roadmap_canvas_draw_string (RoadMapGuiPoint *position, int corner, int size, const char *text)
{
	jstring		js;
	jclass		cls = TheClass();
	jmethodID	mid = TheMethod(cls, "DrawString", "(IIIILjava/lang/String;)V");

	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "draw_string(%s)", text);

	js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, text);
	(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, PanelThiz, mid,
		position->x, position->y, corner, size, js);
}

void roadmap_canvas_draw_string_angle (RoadMapGuiPoint *position,
                                       int size,
                                       int angle, const char *text)
{
	jstring		js;
	jclass		cls = TheClass();
	jmethodID	mid = TheMethod(cls, "DrawStringAngle", "(IIIILjava/lang/String;)V");

	js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, text);
	(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, PanelThiz, mid,
		position->x, position->y, size, angle, js);
}


void roadmap_canvas_draw_multiple_points (int count, RoadMapGuiPoint *points)
{
	jclass		cls = TheClass();
	jmethodID	mid = TheMethod(cls, "DrawPoints", "(I[F)V");
	jfloatArray	apoints;

	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "draw_multiple_points(%d)", count);

	while (count > 1024) {
		apoints = (*RoadMapJniEnv)->NewFloatArray(RoadMapJniEnv, 2048);
		roadmap_canvas_convert_points (apoints, points, 1024);
		(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, PanelThiz, mid,
				1024, apoints);

		points += 1024;
		count -= 1024;
	}
	apoints = (*RoadMapJniEnv)->NewFloatArray(RoadMapJniEnv, 2048);
	roadmap_canvas_convert_points (apoints, points, count);
	(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, PanelThiz, mid,
			count, apoints);
}

/**
 * @brief cut the buffer down to a number acceptable for the Android VM
 * Note this is not the number of lines but the size of the point buffer.
 * So actually number of lines (per Java call) is one fourth of this.
 *
 * Not doing this buffering gets the JNI into trouble.
 */
#define	RM_MAXLINES	40

/**
 * @brief
 * @param count
 * @param lines
 * @param points
 * @param fast_draw
 */
void roadmap_canvas_draw_multiple_lines 
         (int count, int *lines, RoadMapGuiPoint *points, int fast_draw)
{
	jclass		cls = TheClass();
	jmethodID	mid = TheMethod(cls, "DrawLines", "(I[F)V");
	jfloatArray	apoints;
	int		i, count_of_points;
	int		j, ix_lines, ix_tmp, ix_points, nlines, npoints;
	float		*tmp;
	
	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "draw_multiple_lines(%d)", count);

	/*
	 * Loop over the input, cut it up in chunks so we only use buffers of size
	 * RM_MAXLINES.
	 * In the process, convert it into pairs of points - RoadMap passes lines
	 * that pass more than one point, like the example below.
	 *	E/RoadMap (11057): draw_line - line 1317 has 3 points
	 *	E/RoadMap (11057):  - line 1317 point 0 - 125 232
	 *	E/RoadMap (11057):  - line 1317 point 1 - 128 235
	 *	E/RoadMap (11057):  - line 1317 point 2 - 128 235
	 * Convert these into pairs of points so the Java layer just calls simple
	 * line draws.
	 */
	tmp = (float *)malloc(sizeof(float) * RM_MAXLINES);
	apoints = (*RoadMapJniEnv)->NewFloatArray(RoadMapJniEnv, RM_MAXLINES);

	for (i=0, ix_lines=0, ix_tmp=0, ix_points=0, nlines=0, npoints=0; i<count; i++) {
		count_of_points = lines[i];
		// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "draw_multiple_lines{%d -> %d}", i, count_of_points);

		/*
		 * Add current line to the buffer. Initial point : only once.
		 */
		tmp[ix_tmp++] = points[ix_points].x;
		tmp[ix_tmp++] = points[ix_points].y;
		npoints+=2;

		for (j=1; j<count_of_points-1; j++) {
			/* Intermediate points : added twice */
			tmp[ix_tmp++] = points[ix_points+j].x;
			tmp[ix_tmp++] = points[ix_points+j].y;
			npoints+=2;

			/*
			 * Odd as it may seem, this is one of the places where we have
			 * to check for buffer overflow : between two (low level) lines.
			 * (A low level line is what I called a pair of points above.)
			 */
			if (ix_tmp >= RM_MAXLINES - 6) {
				// int k; for (k=0; k<npoints; k+=4) __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "Line %d (%3.0f,%3.0f)-(%3.0f,%3.0f)", k/4, tmp[k], tmp[k+1], tmp[k+2], tmp[k+3]);
				(*RoadMapJniEnv)->SetFloatArrayRegion(RoadMapJniEnv, apoints,
						0, npoints, tmp);
				(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, PanelThiz, mid,
						npoints, apoints);

				nlines=0;
				npoints=0;
				ix_tmp = 0;

				free(tmp);
				tmp = (float *)malloc(sizeof(float) * RM_MAXLINES);
				apoints = (*RoadMapJniEnv)->NewFloatArray(RoadMapJniEnv,
						RM_MAXLINES);
			}

			tmp[ix_tmp++] = points[ix_points+j].x;
			tmp[ix_tmp++] = points[ix_points+j].y;
			npoints+=2;
		}

		/* End point : once. */
		tmp[ix_tmp++] = points[ix_points+count_of_points-1].x;
		tmp[ix_tmp++] = points[ix_points+count_of_points-1].y;
		npoints+=2;
		ix_points += count_of_points;

		/*
		 * Once more : check for buffer overflow, this time between two
		 * high level lines.
		 */
		if (ix_tmp >= RM_MAXLINES - 6) {
			// int k; for (k=0; k<npoints; k+=4) __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "Line %d (%3.0f,%3.0f)-(%3.0f,%3.0f)", k/4, tmp[k], tmp[k+1], tmp[k+2], tmp[k+3]);
			(*RoadMapJniEnv)->SetFloatArrayRegion(RoadMapJniEnv, apoints,
					0, npoints, tmp);
			(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, PanelThiz, mid,
					npoints, apoints);

			nlines=0;
			npoints=0;
			ix_tmp = 0;

			free(tmp);
			tmp = (float *)malloc(sizeof(float) * RM_MAXLINES);
			apoints = (*RoadMapJniEnv)->NewFloatArray(RoadMapJniEnv,
					RM_MAXLINES);
		}
	}

	/*
	 * Finish up, shove out remaining data.
	 */
	// int k; for (k=0; k<npoints; k+=4) __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "Line %d (%3.0f,%3.0f)-(%3.0f,%3.0f)", k/4, tmp[k], tmp[k+1], tmp[k+2], tmp[k+3]);
	(*RoadMapJniEnv)->SetFloatArrayRegion(RoadMapJniEnv, apoints, 0, npoints, tmp);
	(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, PanelThiz, mid,
			npoints, apoints);
	free(tmp);
}


void roadmap_canvas_draw_multiple_polygons
         (int count, int *polygons, RoadMapGuiPoint *points, int filled,
            int fast_draw)
{
	jclass		cls = TheClass();
	jmethodID	mid = TheMethod(cls, "DrawPolygon", "(II[F)V");
	jfloatArray	apoints = 0;
	int i, count_of_points;

	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "draw_multiple_polygons(%d)", count);
	for (i=0; i<count; i++) {
		count_of_points = *polygons;
		// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "draw_multiple_polygons{%d -> %d}", i, count_of_points);
		apoints = (*RoadMapJniEnv)->NewFloatArray(RoadMapJniEnv, count_of_points * 2);

		if (count_of_points > 1024) {
			__android_log_print(ANDROID_LOG_ERROR, "RoadMap",
				"draw_polygon(%d) Cannot draw %d points ",
				i, count_of_points);
		} else {

			roadmap_canvas_convert_points (apoints, points, count_of_points);
			(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, PanelThiz, mid,
				filled, count_of_points, apoints);
		}

		polygons++;
		points += count_of_points;
	}
}

void roadmap_canvas_draw_multiple_circles
        (int count, RoadMapGuiPoint *centers, int *radius, int filled, int fast_draw)
{
	jclass		cls = TheClass();
	jmethodID	mid = TheMethod(cls, "DrawCircle", "(IIIII)V");
	int		i;
	
	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "draw_multiple_circles(%d)", count);

	for (i=0; i<count; i++) {
		(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, PanelThiz, mid,
				centers[i].x, centers[i].y, radius[i], filled, fast_draw);
	}
}

/**
 * @brief register a function to be called when the canvas can be accessed by the app
 * @param handler function to be called
 */
void roadmap_canvas_register_configure_handler (RoadMapCanvasConfigureHandler handler)
{
	RoadMapCanvasConfigure = handler;
}

/**
 * @brief called when appropriate (when the graphics apis can be used)
 * to configure the application based on screen characteristics
 */
void roadmap_canvas_configure(void)
{
	(*RoadMapCanvasConfigure) ();
}

/**
 * @brief Register a button press handler
 */
void roadmap_canvas_register_button_pressed_handler (RoadMapCanvasMouseHandler handler)
{
	RoadMapCanvasMouseButtonPressed = handler;
}

/**
 * @brief Register a button release handler
 */
void roadmap_canvas_register_button_released_handler (RoadMapCanvasMouseHandler handler)
{
	RoadMapCanvasMouseButtonReleased = handler;
}

/**
 * @brief Register a mouse movement handler
 */
void roadmap_canvas_register_mouse_move_handler (RoadMapCanvasMouseHandler handler)
{
	RoadMapCanvasMouseMoved = handler;
}

/**
 * @brief
 */
void
Java_net_sourceforge_projects_roadmap_Panel_HandleTouchEvent(JNIEnv* env, jobject thiz,
		int x, int y, int ac)
{
	RoadMapGuiPoint point;
	point.x = x;
	point.y = y;

//	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "Touch(%d,%d,%d)", ac, x, y);
	switch (ac) {
	case 2 /* ACTION_MOVE */:
		(*RoadMapCanvasMouseMoved) (0, &point);
		break;
	case 1 /* ACTION_UP */:
	case 6 /* ACTION_POINTER_UP */:
		(*RoadMapCanvasMouseButtonReleased) (1, &point);
		break;
	case 0 /* ACTION_DOWN */:
	case 5 /* ACTION_POINTER_DOWN */:
		(*RoadMapCanvasMouseButtonPressed) (1, &point);
		break;
	}
}

/**
 * @brief Register a mouse scroll handler
 * Not used on Android.
 */
void roadmap_canvas_register_mouse_scroll_handler (RoadMapCanvasMouseHandler handler)
{
	RoadMapCanvasMouseScroll = handler;
}

/**
 * @brief query the canvas dimension
 * @return the canvas width, in pixels
 */
int roadmap_canvas_width (void)
{
	int		wid = 0;
	jclass		cls = TheClass();
	jmethodID	mid = TheMethod(cls, "GetWidth", "()I");

	wid = (*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, PanelThiz, mid);

	return wid ? wid : 320;
}

/**
 * @brief query the canvas height
 * @return the canvas height, in pixels
 */
int roadmap_canvas_height (void)
{
	int		ht = 0;
	jclass		cls = TheClass();
	jmethodID	mid = TheMethod(cls, "GetHeight", "()I");;

	ht = (*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, PanelThiz, mid);

	return ht ? ht : 480;
}

/**
 * @brief force a redraw
 */
void roadmap_canvas_refresh (void)
{
	jclass		cls = TheClass();
	jmethodID	mid = TheMethod(cls, "Refresh", "()V");

	(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, PanelThiz, mid);
}

/**
 * @brief take a screenshot, stuff into this file
 */
void roadmap_canvas_save_screenshot (const char* filename)
{
	jstring		js;
	jclass		cls = TheClass();
	jmethodID	mid = TheMethod(cls, "Screenshot", "(Ljava/lang/String;)I");
	int		r;

	js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, filename);
	r = (*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, PanelThiz, mid, js);
	/* FIX ME what to do with return value */
}

#if defined(ROADMAP_ADVANCED_STYLE)
/* these are stubs */
void roadmap_canvas_set_opacity (int opacity) {}

void roadmap_canvas_set_linejoin(const char *join) {}
void roadmap_canvas_set_linecap(const char *cap) {}

void roadmap_canvas_set_brush_color(const char *color) {}
void roadmap_canvas_set_brush_style(const char *style) {}
void roadmap_canvas_set_brush_isbackground(int isbackground) {}

void roadmap_canvas_set_label_font_name(const char *name) {}
void roadmap_canvas_set_label_font_color(const char *color) {}
void roadmap_canvas_set_label_font_size(int size) {}
void roadmap_canvas_set_label_font_spacing(int spacing) {}
void roadmap_canvas_set_label_font_weight(const char *weight) {}
void roadmap_canvas_set_label_font_style(int style) {}

void roadmap_canvas_set_label_buffer_color(const char *color) {}
void roadmap_canvas_set_label_buffer_size(int size) {}
#endif /* ROADMAP_ADVANCED_STYLE */

/**
 * @brief initialize the roadmap_canvas private variables
 */
void roadmap_canvas_initialize(void)
{
	myPanelClass = 0;
	RoadMapPenList = NULL;
}

/**
 * @brief cleanup private variables
 */
void roadmap_canvas_shutdown(void)
{
	int	i;

	for (i=0; i<nPens; i++) {
		free(PenName[i].name);
	}

	free(PenName);
	PenName = 0;
	nPens = 0;
	maxPen = 0;

	myPanelClass = 0;
	RoadMapPenList = NULL;

	RoadMapCanvasMouseButtonPressed = roadmap_canvas_ignore_mouse;
	RoadMapCanvasMouseButtonReleased = roadmap_canvas_ignore_mouse;
	RoadMapCanvasMouseMoved = roadmap_canvas_ignore_mouse;
	RoadMapCanvasMouseScroll = roadmap_canvas_ignore_mouse;
	RoadMapCanvasConfigure = roadmap_canvas_ignore_configure;
}

/**
 * @brief helper function to look up colour names
 * @param name
 * @return -1 on failure, r * 2^16+ g * 2^8 + b otherwise
 */
#include "../win32/colors.h"

static char *roadmap_canvas_lookup_colourname(const char *name)
{
	int	i;
	static char	code[8];

	for (i=0; i<roadmap_ncolors; i++)
		if (strcasecmp(name, color_table[i].name) == 0) {
			sprintf(code, "#%02x%02x%02x",
				color_table[i].r,
				color_table[i].g,
				color_table[i].b);
			return &code[0];
		}
	return NULL;
}
