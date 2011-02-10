/*
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
 *   Copyright 2010, 2011, Danny Backx.
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
 * @brief roadmap_gpsd3.c - a module to interact with gpsd using its library.
 */

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "roadmap.h"
#include "roadmap_gpsd3.h"

#if defined(ROADMAP_USES_LIBGPS)
/* This include won't work on WinCE but libgps won't run on WinCE either. */
#include "errno.h"
#endif

#ifdef ROADMAP_USES_LIBGPS
#include "gps.h"
#endif

static RoadMapGpsdNavigation RoadmapGpsd2NavigationListener = NULL;
static RoadMapGpsdSatellite  RoadmapGpsd2SatelliteListener = NULL;
static RoadMapGpsdDilution   RoadmapGpsd2DilutionListener = NULL;

RoadMapSocket gpsd3_socket;
struct gps_data_t	*gpsdp;

RoadMapSocket roadmap_gpsd3_connect (const char *name) {
#ifdef ROADMAP_USES_LIBGPS
   gpsdp = gps_open(name, "2947");
   if (gpsdp == NULL)
      return ROADMAP_INVALID_SOCKET;
   gps_stream(gpsdp, WATCH_NMEA, NULL);
   return (RoadMapSocket)gpsdp->gps_fd;
#else
   return 0;
#endif
}

void roadmap_gpsd3_subscribe_to_navigation (RoadMapGpsdNavigation navigation) {

   RoadmapGpsd2NavigationListener = navigation;
}


void roadmap_gpsd3_subscribe_to_satellites (RoadMapGpsdSatellite satellite) {

   RoadmapGpsd2SatelliteListener = satellite;
}


void roadmap_gpsd3_subscribe_to_dilution (RoadMapGpsdDilution dilution) {

   RoadmapGpsd2DilutionListener = dilution;
}


int roadmap_gpsd3_decode (void *user_context,
                          void *decoder_context, char *sentence) {
#ifdef ROADMAP_USES_LIBGPS
   int status = 0;
   int latitude = 0;
   int longitude = 0;
   int altitude = 0;
   int speed = 0;
   int steering = 0;

   int  gps_time = 0;

   int i, j, s;
   static bool	used[MAXCHANNELS];
#define	MAX_POSSIBLE_SATS	(MAXCHANNELS - 2)

   if (gps_read(gpsdp) < 0) {
      roadmap_log(ROADMAP_ERROR, "gpsd error %d", errno);
   }

   if (gpsdp->satellites_visible == 0)
      return 0;	// No data

   status = (gpsdp->fix.mode >= MODE_2D) ? 'A' : 'V';

   if (gpsdp->fix.mode >= MODE_2D) {
      if (isnan(gpsdp->fix.longitude) == 0) {
         longitude = 1000000 * gpsdp->fix.longitude;
      }
      if (isnan(gpsdp->fix.latitude) == 0) {
         latitude = 1000000 * gpsdp->fix.latitude;
      }
      if (isnan(gpsdp->fix.speed) == 0) {
         speed = gpsdp->fix.speed;
      }
      if (isnan(gpsdp->fix.altitude) == 0) {
         altitude = gpsdp->fix.altitude;
      }
      if (isnan(gpsdp->fix.time) == 0) {
         gps_time = gpsdp->fix.time;
      }
   }

   RoadmapGpsd2NavigationListener
         (status, gps_time, latitude, longitude, altitude, speed, steering);

   /* See demo app cgps.c in gpsd source distribution */
   /* Must build bit vector of which satellites are used */
   for (i = 0; i < MAXCHANNELS; i++) {
      used[i] = false;
      for (j = 0; j < gpsdp->satellites_used; j++)
         if (gpsdp->used[j] == gpsdp->PRN[i])
	    used[i] = true;
   }

   for (i=0, s=1; i<MAX_POSSIBLE_SATS; i++) {
      if (gpsdp->used[i]) {
         (*RoadmapGpsd2SatelliteListener)
            (s,				// sequence
	     gpsdp->PRN[i],		// id
	     gpsdp->elevation[i],	// elevation
	     gpsdp->azimuth[i],		// azimuth
	     (int)gpsdp->ss[i],		// strength
	     gpsdp->used[i]);		// active
	 s++;
      }
   }
   (*RoadmapGpsd2SatelliteListener) (0, 0, 0, 0, 0, 0);
   return 1;
#else
   return 0;
#endif
}
