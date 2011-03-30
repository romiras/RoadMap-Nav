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
 * @brief roadmap_androidgps.c - a module to interact with Android's GPS data
 */

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "roadmap.h"
#include "roadmap_math.h"

#include <android/log.h>
#include <jni.h>

#include "roadmap_gpsd2.h"

/*
 * These functions need to be called when Android has info from the GPS.
 */
static RoadMapGpsdNavigation navigationListener = NULL;
static RoadMapGpsdSatellite  satelliteListener = NULL;
static RoadMapGpsdDilution   dilutionListener = NULL;

/**
 * @brief periodic call - unused on Android
 */
void roadmap_androidgps_periodic (void)
{
}

/**
 * @brief indicate that this function is to be called when the GPS triggers us
 * @param navigation
 */
void roadmap_androidgps_subscribe_to_navigation (RoadMapGpsdNavigation navigation)
{
	navigationListener = navigation;
}

/**
 * @brief indicate that this function is to be called when the GPS triggers us
 * @param satellite
 */
void roadmap_androidgps_subscribe_to_satellites (RoadMapGpsdSatellite satellite)
{
	satelliteListener = satellite;
}

/**
 * @brief indicate that this function is to be called when the GPS triggers us
 * @param dilution
 */
void roadmap_androidgps_subscribe_to_dilution (RoadMapGpsdDilution dilution)
{
	dilutionListener = dilution;
}

#if 0
/**
 * @brief On Android, we don't need this function : no need to decode ASCII streams
 *	from the GPS device in RoadMap. Android already does that for us.
 * @param user_context
 * @param decoder_context
 * @param sentence
 */
int roadmap_androidgps_decode (void *user_context, void *decoder_context, char *sentence)
{
	__android_log_print (ANDROID_LOG_ERROR, "RoadMap", "roadmap_androidgps_decode()");
}
#endif

/**
 * @brief function called every time the Android GPS passes a new gps fix
 * @param env the JNI environment
 * @param thiz the Java object referred to via JNI
 * @param status a status byte, RoadMap related
 * @param gpstime the time
 * @param lat the latitude
 * @param lon the longitude
 * @param alt the altitude
 * @param speed the speed
 * @param steering the steering
 */
void
Java_net_sourceforge_projects_roadmap_RoadMap_HereAmI(JNIEnv* env, jobject thiz,
		int status, int gpstime, int lat, int lon, int alt, int speed, int steering)
{
	if (! navigationListener) {
		// FIX ME should be an exception ?
		__android_log_print (ANDROID_LOG_ERROR, "RoadMap", "No listener");
		return;
	}

	// roadmap_log (ROADMAP_DEBUG, "HereAmI(st %d, gps %d %d, spd %d, tm %d)", status, lat, lon, speed, gpstime);

	navigationListener(status, gpstime, lat, lon, alt, speed, steering);

}

extern jclass		RoadMapJniClass;
extern JNIEnv		*RoadMapJniEnv;
extern jobject		RoadMapThiz;

/**
 * @brief get the Java layer to request Android location info
 * @param name not used on Android
 * @return 0 on success, -1 on failure
 */
RoadMapSocket roadmap_androidgps_connect (const char *name)
{
	jmethodID	mid;

	mid = (*RoadMapJniEnv)->GetMethodID(RoadMapJniEnv, RoadMapJniClass, "StartGPS", "()V");
	if (mid == 0) {
		__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "Couldn't find StartGPS()");
		return -1;
	}
	(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, RoadMapThiz, mid);
	return 0;
}

/**
 * @brief stop getting location info
 */
void roadmap_androidgps_close (void)
{
	jmethodID	mid;

	mid = (*RoadMapJniEnv)->GetMethodID(RoadMapJniEnv, RoadMapJniClass, "StopGPS", "()V");
	if (mid == 0) {
		__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "Couldn't find StopGPS()");
		return;
	}
	(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, RoadMapThiz, mid);
}

void roadmap_androidgps_satellites (int sequence, int id, int elevation, int azimuth,
                                    int strength, int active)
{
//	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "Satellite(%d,%d,%d,%d,%d,%d", sequence, id, elevation, azimuth, strength, active);

	satelliteListener(sequence, id, elevation, azimuth, strength, active);
}

void
Java_net_sourceforge_projects_roadmap_RoadMap_AndroidgpsSatellites(JNIEnv* env, jobject thiz,
	int sequence, int id, int elevation, int azimuth, int strength, int active)
{
	roadmap_androidgps_satellites (sequence, id, elevation, azimuth, strength, active);
}
