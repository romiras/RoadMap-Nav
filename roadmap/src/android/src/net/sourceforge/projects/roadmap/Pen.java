package net.sourceforge.projects.roadmap;

import android.util.Log;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PathEffect;
import android.graphics.DashPathEffect;

import java.lang.Exception;

class Pen {
	private int current = -1;
	private Paint	pens[];
	private int maxPens = 0;
	private float[]	dash = new float[] {5, 5};
	private DashPathEffect	dashEffect = null;

	public Pen() {
		pens = new Paint[200];
		maxPens = 200;
	}

	public void Create(int index)
	{
		pens[index] = new Paint();
	}

	public void SelectPen(int index)
	{
		if (index > maxPens)
			return;
		current = index;
	}

	public int SetForeground(String color)
	{
		int colorvalue;
		try {
			colorvalue = Color.parseColor(color);
		} catch (Exception e) {
			return -1;
		};

		try {
			pens[current].setColor(colorvalue);
		} catch (Exception e) {
			Log.e("RoadMap", "SetForeground(" + color + ") failed : " + e);
			return -2;
		};
		return 0;
	}

	public void SetLineStyle(int dashed)
	{
		try {
		if (dashed != 0) {
			if (dashEffect == null)
				dashEffect = new DashPathEffect(dash, 0);
			pens[current].setPathEffect(dashEffect);
		} else {
			pens[current].setPathEffect(null);
		}
		} catch (Exception e) {
			Log.e("RoadMap", "SetLineStyle failed : " + e);
		};
	}

	public void SetThickness(int thickness)
	{
		try {
			pens[current].setStrokeWidth(thickness);
		} catch (Exception e) {
			Log.e("RoadMap", "SetThickness failed : " + e);
		};
	}

	public Paint GetPaint()
	{
		try {
			// Log.e("RoadMap", "GetPaint(" + current + ")");
			return pens[current];
		} catch (Exception e) {
			Log.e("RoadMap", "GetPaint: failed (pen " + current + ")");
			return pens[0];
		}
	}
}
