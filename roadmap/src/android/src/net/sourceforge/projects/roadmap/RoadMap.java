package net.sourceforge.projects.roadmap;

import android.os.Bundle;
import android.app.Activity;
import android.widget.TextView;
import android.widget.Toast;
import android.content.Context;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;

import android.os.Handler;
import android.os.Message;

import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.location.GpsStatus;
import android.location.GpsStatus.Listener;
import android.location.GpsSatellite;

import android.content.res.AssetManager;
import android.content.pm.PackageManager;
import android.content.pm.ApplicationInfo;

public class RoadMap extends Activity
{
	private LocationManager mgr = null;
	public RoadMap thiz = this;
	LinearLayout	ll, buttons = null;
	Panel	p;
	Menu	myMenu = null;

	AssetManager	asm;

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
	public native void RoadMapStart();
	public native void RoadMapStartWindow();
	public native int MenuCallback(int item);
	public native void CallPeriodic(int index);
	public native void ToolbarCallback(int cb);

	@Override
	public void onCreate(Bundle state)
	{
		super.onCreate(state);

		Log.e("RoadMap", "onCreate(" + state + ")");

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
		System.loadLibrary("expat-1");
		System.loadLibrary("jni");
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

			int	gpstime = (int) location.getTime(),
				lat = (int) (location.getLatitude() * 1000000),
				lon = (int) (location.getLongitude() * 1000000),
				alt = (int) (location.getAltitude() * 1000000),
				speed = (int) (location.getSpeed() * 1000),
				steering = (int) location.getBearing();

			// The code in roadmap_gps.c requires 'A' as status.
			// See roadmap_gps_update_reception().
			HereAmI('A', gpstime, lat, lon, alt, speed, steering);
		}

	};

	GpsStatus.Listener onGpsChange = new GpsStatus.Listener() {
		@Override
		public void onGpsStatusChanged(int event) {
			GpsStatus	status;

//		Cannot resolve this symbol !! FIX ME
//			if (event != GPS_EVENT_SATELLITE_STATUS) return;

			status = mgr.getGpsStatus(null);
			// int maxsat = status.getMaxSatellites();
			Iterable<GpsSatellite> iter = status.getSatellites();

			int	active = 0, nsats = 0;
			// int	id = 0;
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

			try {
				menuCache[i].menu = parent.addSubMenu(menuCache[i].label);
			} catch (Exception e) {
				Log.e("RoadMap", "AddSubMenu {" + i
						+ "} (" + parent + ","
						+ menuCache[i].label + ") : exception " + e);
				return false;
			}
		}

		// add menu items
		for (i=1; i<nMenuItemCache; i++) {
			if (menuItemCache[i].parent == 0)
				parent = myMenu;
			else {
				int iparent = menuItemCache[i].parent;
				parent = menuCache[iparent].menu;
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
		return nMenuCache++;
	}

	/*
	public void MenuParent(int m, String s)
	{
		try {
		} catch (Exception e) {
			Log.e("RoadMap", "LabelMenu(" + m + "," + s + ") : " + e);
		}
	}
	*/

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

	OnClickListener mToolbarClick = new OnClickListener() {
		public void onClick(View v)
		{
			int	i;

			for (i=0; i<nToolbarCache; i++)
				if (v == toolBar[i].view) {
					ToolbarCallback(i);
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
}
