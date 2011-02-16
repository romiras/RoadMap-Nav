/*
 * LICENSE:
 *
 *   Copyright (c) 2010, 2011 Danny Backx
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
 * @brief Stuff to support the application on Android
 * @ingroup android
 */
package net.sourceforge.projects.roadmap;

import android.os.Bundle;
import android.app.Activity;
import android.content.Context;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;

import android.os.Handler;
import android.os.Message;

import android.widget.Button;
import android.widget.ImageButton;
import android.widget.TextView;
import android.widget.EditText;
import android.widget.Toast;
import android.widget.ScrollView;
import android.widget.ListView;
import android.widget.ArrayAdapter;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;

import android.widget.LinearLayout;
import android.widget.RelativeLayout;

import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.location.LocationManager;
import android.location.GpsStatus;
import android.location.GpsStatus.Listener;
import android.location.GpsStatus.NmeaListener;
import android.location.GpsSatellite;

import android.content.res.AssetManager;
import android.content.pm.PackageManager;
import android.content.pm.ApplicationInfo;

import android.os.PowerManager;

import android.app.Dialog;
import android.app.AlertDialog;
import android.app.AlertDialog.Builder;
import android.content.DialogInterface;
import android.view.ViewGroup;
import android.util.AttributeSet;
import android.view.Gravity;

import android.media.SoundPool;
import android.media.AudioManager;
import java.util.HashMap;

import android.media.MediaPlayer;

import android.app.ProgressDialog;

public class RoadMap extends Activity
{
	private LocationManager mgr = null;
	public RoadMap thiz = this;
	LinearLayout	ll, buttons = null;
	Panel	p;
	Menu	myMenu = null;
	int	InputHandler;

	AssetManager		asm;
	PowerManager		power;
	PowerManager.WakeLock	wl;

	/*
	 * References to native functions
	 */
	public native void DoubleJniTest(String s);
	public native void JniStart();
	public native void CallIdleFunction();
	public native void KickRedraw();
	public native void HereAmI(int status, int gpstime, int lat, int lon,
			int alt, int speed, int steering);
	public native void AndroidgpsSatellites(int sequence,
			int id, int elevation, int azimuth,
			int strength, int active);
	public native int MenuCallback(int item);
	public native void CallPeriodic(int index);
	public native void ToolbarCallback(int cb);
	public native void HardExit(int rc);
	public native void DialogSpecialCallback(int dlg, int btn);
	public native void ReturnStringDataHack(String s);
	public native void RoadMapDialogChosen(int position, long id);
	public native void NMEALogger(int id, String nmea);

	@Override
	public void onCreate(Bundle state)
	{
		super.onCreate(state);

//		Log.e("RoadMap", "onCreate(" + state + ")");

		ll = new LinearLayout(this);
		ll.setOrientation(LinearLayout.VERTICAL);

		buttons = new LinearLayout(this);
		buttons.setOrientation(LinearLayout.HORIZONTAL);
		ll.addView(buttons);

		p = new Panel(this);
		ll.addView(p);

		setContentView(ll);

		mgr = (LocationManager)getSystemService(LOCATION_SERVICE);

		/*
		 * Keep on the screen, possibly dimmed.
		 */
		power = (PowerManager)getSystemService(POWER_SERVICE);
		wl = power.newWakeLock(PowerManager.SCREEN_DIM_WAKE_LOCK, "RoadMap");
		wl.acquire();

		/*
		 * Some experimental code - not really used yet - to try and
		 * figure out stuff about Android packaging, bitmaps, ..
		 *
		try {
			asm = getAssets();
			displayFiles(asm, "/");
		} catch (Exception e) {
			Log.e("RoadMap Assets", "Exception " + e);
		};

		try {
			PackageManager packMgr = this.getPackageManager();
			ApplicationInfo appInfo = packMgr.getApplicationInfo("net.sourceforge.projects.roadmap", 0);
			String apkFilePath = appInfo.sourceDir;
			Log.e("RoadMap Assets 2", "Apk {" + apkFilePath + "}");

		} catch (Exception e2) {
			Log.e("RoadMap Assets 2", "Exception " + e2);
		};
		/* End experimental code */

		/*
		 * Start C code for RoadMap
		 */
		JniStart();

		// Don't do this here - moved to Panel
		// so we're sure that the View is created first.
		//
		// RoadMapStart();
	}

	@Override
	public void onPause()
	{
		super.onPause();
		wl.release();
	}

	@Override
	public void onRestart()
	{
		super.onRestart();
		wl.acquire();
	}

	private void displayFiles (AssetManager mgr, String path) {
		try {
			String list[] = mgr.list(path);
			if (list != null)
				for (int i=0; i<list.length; ++i) {
					Log.v("Assets", "{" + path +"/"+ list[i] + "}");
					displayFiles(mgr, path + "/" + list[i]);
				}
		} catch (Exception e) {
			Log.v("List error:", "can't list" + path);
		}
	}

	@Override
	public void onSaveInstanceState(Bundle state)
	{
		super.onSaveInstanceState(state);

		Log.e("RoadMap", "onSaveInstanceState(" + state + ")");

		// ...
	}

	@Override
	public void onRestoreInstanceState(Bundle state)
	{
		super.onRestoreInstanceState(state);

		Log.e("RoadMap", "onRestoreInstanceState(" + state + ")");

		// ...
	}

	static {
		//
		// Currently this happens only once : not unloaded / cleaned up after
		// the activity is terminated by the user.
		//
		try {
			System.loadLibrary("expat-1");
			System.loadLibrary("rmnative");
		} catch (Exception e) {
			Log.e("RoadMap", "Shared library installation problem");
		}
	}

	public void StartGPS()
	{
		mgr.requestLocationUpdates(LocationManager.GPS_PROVIDER,
			0, 0,	// updates as fast as possible
			onLocationChange);
		mgr.addGpsStatusListener(onGpsChange);
	}

	public void StopGPS()
	{
		mgr.removeUpdates(onLocationChange);
		mgr.removeGpsStatusListener(onGpsChange);
	}

	public void MainSetInput(int id) {
		InputHandler = id;

		// Log.e("RoadMap", "MainSetInput("+id+")");
		try {
			mgr.addNmeaListener(onNmea);
		} catch (Exception e) {
			Log.e("RoadMap", "MainSetInput exception" + e);
		}
	}

	NmeaListener onNmea = new NmeaListener() {
		public void onNmeaReceived(long ts, String nmea) {
			// Log.e("RoadMap", "onNmeaReceived("+ts+","+nmea+")");
			NMEALogger(InputHandler, nmea);
		}
	};

	LocationListener onLocationChange = new LocationListener() {
		int mystatus;

		public void onProviderDisabled(String provider) {
			// required for interface, not used
		}

		public void onProviderEnabled(String provider) {
			// required for interface, not used
		}
						            
		public void onStatusChanged(String provider, int status, Bundle extras) {
			mystatus = status;
		}

		@Override
		public void onLocationChanged(Location location) {
			/*
			Log.e("RoadMap", "GPS {" + location.getProvider()
				+ "}, lat " + location.getLatitude()
				+ ", lon " + location.getLongitude());
			/* */

				/* getTime() returns UTC of this fix in ms since 1/1/1970 */
			long	gpstime = location.getTime();
			int	lat = (int) (location.getLatitude() * 1000000),
				lon = (int) (location.getLongitude() * 1000000),
				/* getAltitude() returns m */
				alt = (int) (location.getAltitude() * 1000000),
				/* getSpeed() returns m/s, this calculation
				 * turns it into knots, see roadmap_gpsd2.c .
				 * Still need to divide by 1000, see below.
				 * */
				speed = (int) (1944 * location.getSpeed()),
				/* getBearing returns degrees East of true North */
				steering = (int) location.getBearing();
			int	sgpstime;

			/* Convert time units between the Android GPS interface and Unix.
			 * Unix counts in seconds since the Epoch, this particular Android
			 * provides ms since the Epoch. */
			sgpstime = (int)(gpstime / 1000);

			/*
			 * Hopefully cutting the calculation in two
			 * is enough to avoid precision loss.
			 */
			speed /= 1000;

			// The code in roadmap_gps.c requires 'A' as status.
			// See roadmap_gps_update_reception().
			HereAmI('A', sgpstime, lat, lon, alt, speed, steering);
		}

	};

	GpsStatus.Listener onGpsChange = new GpsStatus.Listener() {
		@Override
		public void onGpsStatusChanged(int event) {
			GpsStatus	status;

			if (event != GpsStatus.GPS_EVENT_SATELLITE_STATUS)
				return;

			status = mgr.getGpsStatus(null);

			Iterable<GpsSatellite> iter = status.getSatellites();

			int	active = 0, nsats = 0;

			for (GpsSatellite sat : iter) {
				nsats++;
				if (sat.usedInFix())
					active++;
				// note the function called relies on the first parameter
				// really being a sequence that starts with 1,
				// and ending with sequence == 0.
				AndroidgpsSatellites(nsats,
						sat.getPrn(),
						(int)sat.getElevation(),
						(int)sat.getAzimuth(),
						(int)sat.getSnr(),
						sat.usedInFix() ? 1 : 0);
			}
			AndroidgpsSatellites(0, 0, 0, 0, 0, 0);
			// Log.e("RoadMap", "onGpsStatusChanged(" + event + "), " + active + "/" + nsats + " satellites");


		}
	};

	public void MessageBox(String title, String message)
	{
		Toast.makeText(thiz, message, 5000).show();
	}

	public void MessageBoxWait(String title, String message)
	{
		new AlertDialog.Builder(thiz)
			.setTitle("Fatal error")
			.setMessage(message)
			.setNeutralButton("Dismiss", new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dlg, int ok) {
					HardExit(1);
				}
			})
			.show();
	}

	public void Finish()
	{
		thiz.finish();
	}

	/*
	 * The code below implements a bunch of handlers so the UI actions happen
	 * in the main thread.
	 */
	Handler periodicHandler = new Handler() {
		@Override
		public void handleMessage(Message msg) {
			/* schedule ourselves again */
			Message m = periodicHandler.obtainMessage();

			m.what = msg.what;
			m.arg1 = msg.arg1;	/* This is the delay */
			m.arg2 = msg.arg2;

			sendMessageDelayed(m, msg.arg1);

			/* do something useful */
			CallPeriodic(msg.what);
		}
	};

	/*
	 * @brief call the periodicHandler from C
	 */
	public void SetPeriodic(int index, int interval)
	{
		Message m = periodicHandler.obtainMessage();
		m.what = index;
		m.arg1 = interval;
		m.arg2 = 0;	/* Not used */

		periodicHandler.sendMessageDelayed(m, interval);
	}

	public void RemovePeriodic(int index)
	{
		periodicHandler.removeMessages(index);
	}

	/**
	 * @brief Idle function handler : call this function once when idle
	 */
	Handler idleHandler = new Handler() {
		@Override
		public void handleMessage(Message msg) {
			CallIdleFunction();
		}
	};

	/**
	 * @brief Trigger the idle function
	 */
	public void TriggerIdleFunction()
	{
		idleHandler.sendMessage(idleHandler.obtainMessage());
	}

	/*
	 * @brief Menu code - to be reviewed because no nesting possible on Android
	 *
	 * Note onCreateOptionsMenu needs to scan structures created from C
	 * and build the menu from it.
	 * Can't do this easily in one pass because we'd have to postpone much
	 * configuration file reading until the user hits the menu button.
	 */
	@Override
	public boolean  onCreateOptionsMenu(Menu menu)
	{
		boolean	r;
		int	i;
		Menu	parent;

		myMenu = menu;
		r = super.onCreateOptionsMenu(menu);

		// add (sub)menus
		for (i=1; i<nMenuCache; i++) {
			if (menuCache[i].parent == 0)
				parent = myMenu;
			else
				parent = menuCache[i].menu;

			// Log.e("RoadMap", "onCreateOptionsMenu -> " + i + " [" + menuCache[i].label + "] in bar : " + menuCache[i].inbar);
			if (menuCache[i].inbar != 0) {
			    try {
				menuCache[i].menu = parent.addSubMenu(menuCache[i].label);
			    } catch (Exception e) {
				// Log.e("RoadMap", "AddSubMenu {" + i + "} (" + parent + "," + menuCache[i].label + ") : exception " + e);
				return false;
			    }
			}
		}

		// add menu items
		for (i=1; i<nMenuItemCache; i++) {
			if (menuItemCache[i].parent == 0)
				parent = myMenu;
			else {
				int iparent = menuItemCache[i].parent;
				parent = menuCache[iparent].menu;

				if (parent == null) {
					/*
					 * this probably means menuCache[iparent].inbar == 0
					 * so this menu is not mentioned in the user specified
					 * roadmap.menus file.
					 */
					continue;
				}
			}

			try {
				menuItemCache[i].item = parent.add(Menu.NONE,
					i, Menu.NONE, menuItemCache[i].label);
			} catch (Exception e) {
				Log.e("RoadMap", "AddMenuItem {" + i
						+ "} (" + parent + ","
						+ menuItemCache[i].label + ") : exception " + e);
				return false;
			}
		}

		return r;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item)
	{
		return(applyMenuChoice(item) || super.onOptionsItemSelected(item));
	}

	/*
	 * @brief
	 * needs to return true if a menu was activated
	 */
	private boolean applyMenuChoice(MenuItem item)
	{
		if (MenuCallback(item.getItemId()) != 0) {
			return true;
		}
		return false;
	}

	class MenuCacheEntry {
		int	parent;
		Menu	menu;
		String	label;
		int	inbar;
		// more FIX ME
	};

	// FIX ME make dynamic
	final int maxMenuCacheEntry = 100;
	MenuCacheEntry	menuCache[] = new MenuCacheEntry[maxMenuCacheEntry];
	int		nMenuCache = 1;

	public int CreateMenu(String s)
	{
		if (nMenuCache == maxMenuCacheEntry) {
			Log.e("RoadMap", "CreateMenu(" + s + ") : cache full");
			return -1;
		}
		menuCache[nMenuCache] = new MenuCacheEntry();
		menuCache[nMenuCache].parent = 0 /* m */;
		menuCache[nMenuCache].label = s;
		menuCache[nMenuCache].inbar = 0;
		return nMenuCache++;
	}

	/**
	 * @brief indicate that this menu should be in the menu bar.
	 * @param ix the index returned by CreateMenu()
	 */
	public void AttachMenuToBar(int ix)
	{
		menuCache[ix].inbar = 1;
	}

	class MenuItemCacheEntry {
		int		parent;
		MenuItem	item;
		String		label;
		// more FIX ME
	};

	// FIX ME make dynamic
	final int maxMenuItemCache = 200;
	MenuItemCacheEntry menuItemCache[] = new MenuItemCacheEntry[maxMenuItemCache];
	int		nMenuItemCache = 1;

	public int AddMenuItem(int m, String s)
	{
		if (nMenuItemCache == maxMenuItemCache) {
			Log.e("RoadMap", "AddMenuItem(" + m + "," + s + ") : cache full");
			return -1;
		}
		try {
			menuItemCache[nMenuItemCache] = new MenuItemCacheEntry();
			menuItemCache[nMenuItemCache].parent = m;
			menuItemCache[nMenuItemCache].label = s;
		} catch (Exception e) {
			Log.e("RoadMap", "AddMenuItem(" + m + "," + s + ") : " + e);
			return -1;
		};
		return nMenuItemCache++;
	}

	/*
	 * @brief AddToolbar() - this is already performed in the constructor
	 * Either leave it in the constructor, or find a way to alter the geometry
	 * afterwards (if it needs to come on top).
	 * FIX ME
	 */
	private void AddToolbar(String orientation)
	{
	}

	// Is there no better way to pass an id ?
	class ToolbarCacheEntry {
		View	view;
		// int	ix;
	};

	// FIX ME make dynamic
	final int		maxToolbarCache = 30;
	ToolbarCacheEntry	toolBar[] = new ToolbarCacheEntry[maxToolbarCache];
	int			nToolbarCache = 0;

	void JToolbarCallback(int i)
	{
		try {
			ToolbarCallback(i);
		} catch (Exception e) {
			e.printStackTrace();
			Log.e("RoadMap", "Stack trace " + e.toString());
		};
	}

	OnClickListener mToolbarClick = new OnClickListener() {
		public void onClick(View v)
		{
			int	i;

			for (i=0; i<nToolbarCache; i++)
				if (v == toolBar[i].view) {
					// ToolbarCallback(i);
					JToolbarCallback(i);
					return;
				}
			// FIX ME No button found, should we throw an exception ?
			Log.e("RoadMap", "mToolbarClick(" + v + ") : unregistered button");
		}
	};

	public void AddTool(String label, String icon, String tip, int cb)
	{
		ImageButton	ib;
		Button		tb;
		Bitmap		bm;

		if (nToolbarCache == maxToolbarCache) {
			Log.e("RoadMap", "AddTool(" + label + "," + icon + ") : cache full");
			return;
		}
		toolBar[nToolbarCache] = new ToolbarCacheEntry();

		ib = new ImageButton(thiz);
		ib.setOnClickListener(mToolbarClick);

		// Need to make this device independent.
		// bm = BitmapFactory.decodeFile("/sdcard/roadmap/icons/" + icon + ".png");
		//
		// Assume that the C layer has already located an icon, or passed NULL.
		// Note: it does that through roadmap_path_search_icon().
		bm = BitmapFactory.decodeFile(icon);

		if (bm != null) {
			ib.setImageBitmap(bm);
			// ib.setMaxWidth(bm.getWidth());
			// ib.setMaxHeight(bm.getHeight());
			buttons.addView(ib);
			toolBar[nToolbarCache].view = ib;

//			Log.e("RoadMap", "AddTool(" + label + "," + icon + ") : image button {" + ib + "}");
		} else {
			tb = new Button(thiz);
			tb.setText(label);
			buttons.addView(tb);
			toolBar[nToolbarCache].view = tb;

//			Log.e("RoadMap", "AddTool(" + label + "," + icon + ") : text button {" + tb + "}");
		}

		nToolbarCache++;
	}

	private int	dialog_id = 1;
	final int	maxdialogs = 50;

	class Dialogs {
		String			name;
		LinearLayout		ll, row;
		AlertDialog		ad;
		AlertDialog.Builder	db;
		String			button0, button1, button2;
		int			rowid;
//		int			nrows, maxrows;
//		LinearLayout		rows[];
	};
	Dialogs dialogs[] = new Dialogs[maxdialogs];

	protected Dialog onCreateDialog(int dlg_id)
	{
//		Log.e("RoadMap", "onCreateDialog(" + dlg_id + ")");

		if (dlg_id <= 0 || dlg_id >= dialog_id) {
			return null;
		}

		if (dialogs[dlg_id] != null && dialogs[dlg_id].ad != null) {
			return dialogs[dlg_id].ad;
		}

		if (dialogs[dlg_id] != null && dialogs[dlg_id].ll != null) {
			AlertDialog.Builder     db = new AlertDialog.Builder(this);
			ScrollView		sv;

			/* We need the "final" clause to pass the right value */
			final int id = dlg_id;

			db.setMessage(dialogs[dlg_id].name)
				.setCancelable(true)
				.setPositiveButton(dialogs[dlg_id].button0,
					new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface dialog, int btn)
						{
							DialogSpecialCallback(id, 0);
						}
					}
				);
			db.setMessage(dialogs[dlg_id].name)
				.setCancelable(true)
				.setNeutralButton(dialogs[dlg_id].button1,
					new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface dialog, int btn)
						{
							DialogSpecialCallback(id, 1);
						}
					}
				);
			db.setMessage(dialogs[dlg_id].name)
				.setCancelable(true)
				.setNegativeButton(dialogs[dlg_id].button2,
					new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface dialog, int btn)
						{
							DialogSpecialCallback(id, 2);
						}
					}
				);

			AlertDialog     d = db.create();

			LinearLayout ll = dialogs[dlg_id].ll;
			sv = new ScrollView(this);
			d.setView(sv);
			sv.addView(ll);

			dialogs[dlg_id].ad = d;
			dialogs[dlg_id].db = db;

			return d;
		}
		return null;
	}

	/*
	 * 2nd parameter is the reference to the parent dialog
	 */
	public int CreateDialog(String name, int parent)
	{
		// Log.e("RoadMap", "CreateDialog(" + name + ") -> " + dialog_id);

		dialogs[dialog_id] = new Dialogs();
		dialogs[dialog_id].name = name;
		dialogs[dialog_id].ll = new LinearLayout(this);
		dialogs[dialog_id].rowid = 0;
		dialogs[dialog_id].ll.setOrientation(LinearLayout.VERTICAL);

		if (dialogs[parent] != null) {
			// Log.e("RoadMap", "Add button {" + name + "} in dlg " + parent + " (" + dialogs[parent].name + ")");

			/* Put a button in the parent dialog ! */
			Button b = new Button(thiz);
			b.setText(name);
			dialogs[parent].ll.addView(b);
			b.setOnClickListener(
				new View.OnClickListener() {
					final int dlg = dialog_id;
					public void onClick(View v)
					{
						// Log.e("RoadMap", "ShowDialog(" + dlg + ")");
						showDialog(dlg);
					}
			});
		} else {
//			Log.e("RoadMap", "CreateDialog(" + name + ") - no parent ??");
		}

		return dialog_id++;
	}

	/*
	 * 2nd parameter is the reference to the parent dialog
	 */
	public int CreateHeading(String name, int id)
	{
//		Log.e("RoadMap", "CreateHeading(" + name + "," + id + ")");

		try {
			/*
			LinearLayout	row = new LinearLayout(this);
			row.setOrientation(LinearLayout.HORIZONTAL);
			dialogs[id].ll.addView(row);
			dialogs[id].row = row;
			*/

			TextView b = new TextView(thiz);
			b.setText(name);
			dialogs[id].ll.addView(b);
			// dialogs[id].row.addView(b);
			b.setGravity(Gravity.LEFT);
		} catch (Exception e) {
			Log.e("RoadMap", "CreateHeading(" + id + "," + name + ") exception " + e);
			return 0;
		}
		return 1;
	}

	/*
	 * Add an object that shows a label
	 * Precede this by creating a "row" object - currently moved to CreateHeading.
	 */
	public int DialogAddLabel(int id, String name)
	{
//		Log.e("RoadMap", "DialogAddLabel(" + id + "," + name + ")");
		try {
			LinearLayout	row = new LinearLayout(this);
			row.setOrientation(LinearLayout.HORIZONTAL);
			dialogs[id].ll.addView(row);
			dialogs[id].row = row;

			// Button b = new Button(thiz);
			TextView b = new TextView(thiz);
			b.setText(name);
			dialogs[id].row.addView(b);

			// b.setGravity(Gravity.RIGHT);
			// LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
			// 	ViewGroup.LayoutParams.FILL_PARENT,
			// 	ViewGroup.LayoutParams.WRAP_CONTENT);
			// lp.weight = 1.0f;
			// b.setLayoutParams(lp);
		} catch (Exception e) {
			Log.e("RoadMap", "DialogAddLabel(" + id + "," + name + ") exception " + e);
		}
		return 1;
	}

	/*
	 * @return the index of the text field in the container widget
	 */
	public int DialogAddTextEntry(int id, String name)
	{
		try {
			EditText tv = new EditText(thiz);

			tv.setGravity(Gravity.FILL_HORIZONTAL);
			tv.setLayoutParams(new ViewGroup.LayoutParams(
				ViewGroup.LayoutParams.FILL_PARENT,
				ViewGroup.LayoutParams.WRAP_CONTENT));

			LinearLayout	row = dialogs[id].row;

			row.addView(tv);
			int r = dialogs[id].ll.indexOfChild(row);

			// dialogs[id].ll.addView(tv);
			// int r = dialogs[id].ll.indexOfChild(tv);

			/* Change text size */
			tv.setTextSize(10);
//			Log.e("RoadMap", "DialogAddTextEntry(" + id + "," + name + ") EditText " + tv + " row " + row + " added -> ix " + r);
			return r;
		} catch (Exception e) {
			Log.e("RoadMap", "DialogAddTextEntry(" + id + "," + name + ") exception " + e);
			return -1;
		}
	}

	public void ShowDialog(int id)
	{
		showDialog(id);
	}

	/*
	 * Special buttons - the ones that belong to the dialog, and appear at the bottom.
	 *
	 * Note this happens before onCreateDialog so note the button names but do no more.
	 * Also : depends on order of the calls in RoadMap.
	 */
	public int DialogAddSpecialButton(int id, String name, int btn)
	{
		try {
			switch (btn) {
			case 0:	dialogs[id].button0 = name;
				break;
			case 1:	dialogs[id].button1 = name;
				break;
			case 2:	dialogs[id].button2 = name;
				break;
			}
			return 0;
		} catch (Exception e) {
			Log.e("RoadMap", "DialogAddSpecialButton(" + id + "," + name + ") exception " + e);
			return 0;
		}
	}

	/*
	 * Manipulate field contents
	 */
	public void DialogSetData(int id, int rownr, String value)
	{
		try {
			LinearLayout	ll = null, row;
			EditText	tv = null;

			/* Assume EditText is a direct child of LinearLayout
			ll = dialogs[id].ll;
			tv = (EditText)ll.getChildAt(rownr);
			tv.setText(value);
			*/

			/* Assume EditText is in a row, so grandchild of LinearLayout */
			ll = dialogs[id].ll;
			/*
			 * The textView is always child #1, child 0 is the label in the row.
			 */
			row = (LinearLayout)ll.getChildAt(rownr);
			tv = (EditText)row.getChildAt(1);
			tv.setText(value);

		} catch (Exception e) {
			Log.e("RoadMap", "DialogSetData(" + id + "," + rownr + "," + value + ") " + e);
		}
	}

	public void DialogGetData(int id, int rownr)
	{
		try {
			LinearLayout	ll = dialogs[id].ll;

			/*
			 * See above, TextView is #1.
			 */
			LinearLayout	row = (LinearLayout)ll.getChildAt(rownr);
			EditText	tv = (EditText)row.getChildAt(1);

			/*
			EditText	tv = (EditText)ll.getChildAt(rownr);
			/*
			 * Instead of returning data in a parameter, we're calling C instead.
			 * So after returning from this function, the data we want to pass is
			 * in a global variable that we write into with this call.
			 */
			ReturnStringDataHack(tv.getText().toString());
		} catch (Exception e) {
			ReturnStringDataHack("");
		}
	}

	/*
	 * Create ListView
	 */
	public void DialogCreateList(int id)
	{
		try {
			LinearLayout	row = new LinearLayout(this);
			row.setOrientation(LinearLayout.HORIZONTAL);

			dialogs[id].ll.addView(row);
			dialogs[id].row = row;
//			Log.e("RoadMap", "DialogCreateList(" + id + ")");
			ListView	lv = new ListView(thiz);
//			Log.e("RoadMap", "DialogCreateList(" + id + "), row " + dialogs[id].row);
			dialogs[id].row.addView(lv);

			// lv.setOnItemClickListener(new AdapterView.OnItemClickListener() { });
			lv.setOnItemClickListener(listener);
		} catch (Exception e) {
			Log.e("RoadMap", "DialogCreateList(" + id + ") : exception " + e);
		};
	}

	OnItemClickListener listener = new OnItemClickListener() {
		public void onItemClick(AdapterView <?> parent, View view, int position, long id) {
			Log.e("RoadMap", "OnItemClickListener(" + position + "," + id + ")");
			RoadMapDialogChosen(position, id);
		}
	};

	DialogInterface.OnClickListener listener2 = new DialogInterface.OnClickListener() {
		public void onClick(DialogInterface dialog, int which) {
			Log.e("RoadMap", "OnClickListener(" + which + ")");
			// RoadMapDialogChosen(position, id);
		}
	};

	/*
	 * Set a list contents
	 */
	public void DialogSetListContents(int id, String[] list)
	{
/* Not sure how this should work
		AlertDialog.Builder     db = dialogs[id].db;

		ArrayAdapter<String>	adapter = new ArrayAdapter<String>(thiz,
			R.layout.list_item,
			list);
		db.setAdapter(adapter, listener2);
		dialogs[id].ad.getListView().invalidateViews();
/* */

/* This does work but looks like overkill.
 *
 * Requires DialogCreateList() to create an additional widget...
 * which may not be necessary
 */
		try {
			ListView lv = (ListView)dialogs[id].row.getChildAt(0);

			lv.setAdapter(new ArrayAdapter<String>(thiz,
//				android.R.layout.simple_list_item_1,
				R.layout.list_item,	// smaller than the big default
				list));
		} catch (Exception e) {
			Log.e("RoadMap", "DialogSetListContents -> exception " + e);
		}
/* */
	}

	/*
	 * Sound
	 */
	public int PlaySound(String fn)
	{
		MediaPlayer	mp;

		try {
			mp = new MediaPlayer();
			mp.setDataSource(fn);
			mp.prepare();
			mp.start();
		} catch (Exception e) {
			Log.e("RoadMap", "PlaySound exception " + e);
			return -1;
		};
		return 0;
	}

	/*
	 * Progress bar
	 */
	Thread	progressThread;
	Dialog	progressDialog;

	public void StartProgressBar()
	{
	}

	public void EndProgressBar()
	{
	}

	public void SetProgress(int percent)
	{
	}
}
