/*
 * LICENSE:
 *
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
 * @brief Pen abstraction - defines a pen with some features
 * @ingroup android
 */
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
	private Paint fallback = null;
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
			if (pens[current] == null) {
				Log.e("RoadMap.Pen", "GetPaint(" + current + ") replaced by dummy paint");
				if (fallback == null)
					fallback = new Paint();
				return fallback;
			}
			return pens[current];
		} catch (Exception e) {
			Log.e("RoadMap", "GetPaint: failed (pen " + current + ")");
			return pens[0];
		}
	}
}
