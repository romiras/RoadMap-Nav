package net.sourceforge.projects.roadmap;

import android.content.Context;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;

import android.graphics.Bitmap;
import android.graphics.Bitmap.CompressFormat;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;

import java.lang.Exception;

import java.io.File;
import java.io.FileOutputStream;

class Panel
	extends View
	implements View.OnClickListener, View.OnLongClickListener
{

	public native void RoadMapStart();
	public native void PanelJniStart();
	public native void HandleTouchEvent(int x, int y, int ac);
	private Pen	pen;

	private Bitmap	cacheBitmap = null;
	private Canvas	cacheCanvas = null;

	private Paint	paint;
	private Paint	drawPaint;

	private void doThisFirst()
	{
		if (cacheBitmap != null)
			return;

		Log.e("RoadMap Panel", "Size wid " + getWidth() + ", ht " + getHeight());
		cacheBitmap = Bitmap.createBitmap(getWidth(), getHeight(), Bitmap.Config.ARGB_8888);
		cacheCanvas = new Canvas(cacheBitmap);

		RoadMapStart();
	}

	public Panel(Context context) {
		super(context);

		PanelJniStart();
		setFocusable(true);
		pen = new Pen();
		drawPaint = new Paint();
	}

	static float xp = 0, yp = 0;

	@Override
	public boolean onTouchEvent(MotionEvent event)
	{
		Float	x = event.getX(),
			y = event.getY();
		int	ac = event.getAction();

		switch (ac) {
		case 2 /* ACTION_MOVE */ :
		case 0 /* ACTION_DOWN */ :
		case 1 /* ACTION_UP */ :
		case 6 /* ACTION_POINTER_UP */ :
		case 5 /* ACTION_POINTER_DOWN */ :
			HandleTouchEvent(x.intValue(), y.intValue(), ac);
			return true;
		}
		return false;
	}

	public void onClick(View view)
	{
	}

	/*
	 * for .. extends View.OnTouchListener
	 *
	public boolean onTouch(View view, MotionEvent e)
	{
		return false;
	}
	/* */

	public boolean onLongClick(View view)
	{
		return false;
	}

	public int GetHeight()
	{
		int	r;

		try {
			return getHeight();
		} catch (Exception e) {
			Log.e("RoadMap", "Panel GetHeight " + e.getMessage());
			return 0;
		}
	}

	public int GetWidth()
	{
		int	r;

		try {
			return getWidth();
		} catch (Exception e) {
			Log.e("RoadMap", "Panel GetWidth " + e.getMessage());
			return 0;
		}
	}
	
	public void Refresh()
	{
		invalidate();
	}

	public void Erase()
	{
		try {
			cacheCanvas.drawColor(Color.WHITE);
			// cacheCanvas.drawColor(Color.RED);
		} catch (Exception e) {
			Log.e("RoadMap", "Panel Erase " + e.getMessage());
		}
	}

	@Override
	public void onDraw(Canvas canvas)
	{
		doThisFirst();
		super.onDraw(canvas);
		canvas.drawBitmap(cacheBitmap, 0, 0, drawPaint);
	}

	public void SetLinestyle(int dashed)
	{
		pen.SetLineStyle(dashed);
	}

	public void SetThickness(int thickness)
	{
		pen.SetThickness(thickness);
	}

	public int SetForeground(String color)
	{
		return pen.SetForeground(color);
	}

	// Copy from roadmap_canvas.h
	private final int ROADMAP_CANVAS_RIGHT = 1;
	private final int ROADMAP_CANVAS_BOTTOM = 2;
	private final int ROADMAP_CANVAS_CENTER_X = 4;
	private final int ROADMAP_CANVAS_CENTER_Y = 8;

	public void DrawString(int x, int y, int corner, int size, String text)
	{
		try {
			paint = pen.GetPaint();

			// Copy from gtk2/roadmap_canvas.c:roadmap_canvas_draw_string()
			int width = (int)paint.measureText(text);
			int descent = (int)paint.descent();
			// Ascent is negative on Android
			int ascent = (int)-paint.ascent();

			int height = ascent + descent;

			if ((corner & ROADMAP_CANVAS_RIGHT) != 0)
				x -= width;
			else if ((corner & ROADMAP_CANVAS_CENTER_X) != 0)
				x -= width / 2;

			if ((corner & ROADMAP_CANVAS_BOTTOM) != 0)
				y -= descent;
			else if ((corner & ROADMAP_CANVAS_CENTER_Y) != 0)
				y -= descent + height / 2;
			else /* TOP */
				y += ascent;

			cacheCanvas.drawText(text, x, y, paint);
		} catch (Exception e) {
			Log.e("RoadMap", "Exception " + e + " in DrawString");
		}
	}

	public int Screenshot(String filename)
	{
		try {
			File			file;
			FileOutputStream	os;

			file = new File(filename);
			file.getParentFile().mkdirs();
			os = new FileOutputStream(file);
			cacheBitmap.compress(CompressFormat.PNG, 80, os);
		} catch (Exception e1) {
			Log.e("RoadMap", "Screenshot failed");
			return -1;
		}

		Log.e("RoadMap", "Screenshot saved in " + filename);
		return 0;
	}

	/*
	 * @return pointer to the pen
	 *
	 * Note we're relying on sizeof(int) >= sizeof(pointer) here.
	 * Note we're not passing the pen name from C to Java, we're not using it anyway.
	 */
	public void CreatePen(int penNumber)
	{
		/* Should set defaults :
		 *       gdk_gc_set_fill (gc, GDK_SOLID);
		 *             pen->style = GDK_LINE_SOLID;
		 */
		pen.Create(penNumber);
	}

	public void SelectPen(int index)
	{
		pen.SelectPen(index);
	}

	public void DrawCircle(int x, int y, int radius, int filled, int fastdraw)
	{
		try {
			paint = pen.GetPaint();

			if (filled == 1)
				paint.setStyle(Paint.Style.FILL_AND_STROKE);
			else
				paint.setStyle(Paint.Style.STROKE);

			cacheCanvas.drawCircle(x, y, radius, paint);
		} catch (Exception e) {
			Log.e("RoadMap", "Exception " + e + " in DrawCircle");
		}
	}

	public void DrawPolygon(int filled, int count, float[] points)
	{
		// Log.e("Java", "DrawPolygon(count " + count + ", fill " + filled + ")");
		try {
			int i;
			paint = pen.GetPaint();

			if (filled == 1)
				paint.setStyle(Paint.Style.FILL_AND_STROKE);
			else
				paint.setStyle(Paint.Style.STROKE);

			Path path = new Path();

			path.moveTo(points[0], points[1]);
			for (i=2; i<2*count; i+=2) {
				path.lineTo(points[i], points[i+1]);
			}

			path.close();
			cacheCanvas.drawPath(path, paint);
		} catch (Exception e) {
			Log.e("RoadMap", "Exception " + e + " in DrawPolygon");
		}
	}

	public void DrawLines(int count, float[] points)
	{
		try {
			paint = pen.GetPaint();
			cacheCanvas.drawLines(points, paint);
		} catch (Exception e) {
			Log.e("RoadMap", "Exception " + e + " in DrawLines");
		}
	}

	// RoadMap cannot cope with the angle, so just call DrawString.
	public void DrawStringAngle(int x, int y, int size, int angle, String text)
	{
		DrawString(x, y, ROADMAP_CANVAS_CENTER_X | ROADMAP_CANVAS_BOTTOM, 0, text);
	}

	public void DrawPoints(int count, float[] points)
	{
		try {
			paint = pen.GetPaint();
			cacheCanvas.drawPoints(points, paint);
		} catch (Exception e) {
			Log.e("RoadMap", "Exception " + e + " in DrawPoints");
		}
	}

	public int MeasureWidth(String text, int s)
	{
		try {
			paint = pen.GetPaint();
			// Log.e("RoadMap", "Measure(" + text + ") -> " + paint.measureText(text));
			return (int) paint.measureText(text);
		} catch (Exception e) {
			Log.e("RoadMap", "MeasureWidth(" + text + ") : exception " + e);
			return 10;
		}
	}

	public int MeasureAscent()
	{
		try {
			paint = pen.GetPaint();
			// Log.e("RoadMap", "MeasureAscent() -> " + paint.ascent());
			// Ascent is negative on Android
			return (int)-paint.ascent();
		} catch (Exception e) {
			Log.e("RoadMap", "MeasureAscent() : exception " + e);
			return 10;
		}
	}

	public int MeasureDescent()
	{
		try {
			paint = pen.GetPaint();
			// Log.e("RoadMap", "MeasureDescent() -> " + paint.descent());
			return (int) paint.descent();
		} catch (Exception e) {
			Log.e("RoadMap", "MeasureDescent() : exception " + e);
			return 10;
		}
	}
}
