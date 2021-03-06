/*
 * LICENSE:
 *
 *   Copyright (c) 2011, Danny Backx
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
 * @brief Code to support running on "Eclair", Android 1.6 .
 *
 * This class is only used to avoid linking functions that are unsupported in
 * older versions of Android.
 * See the MainSetInput function in the RoadMap class.
 *
 * @ingroup android
 */
package net.sourceforge.projects.roadmap;

import android.location.LocationManager;
import android.location.GpsStatus.NmeaListener;

class EclairHelper {
   static int	InputHandler;

   static native void NMEALogger(int id, String nmea);

   static NmeaListener onNmea = new NmeaListener() {
      public void onNmeaReceived(long ts, String nmea) {
         NMEALogger(InputHandler, nmea);
      }
   };

   static void MainSetInputAndroid(LocationManager mgr, int id) {
      InputHandler = id;
      mgr.addNmeaListener(onNmea);
   };
}
