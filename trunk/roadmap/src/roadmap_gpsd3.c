/*
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
 *   Copyright (c) 2010, 2011, Danny Backx.
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
#include "roadmap_gps.h"
#include "roadmap_nmea.h"

#if defined(ROADMAP_USES_LIBGPS)
/* This include won't work on WinCE but libgps won't run on WinCE either. */
#include "errno.h"
#endif

#ifdef ROADMAP_USES_LIBGPS
#include "gps.h"
#endif

RoadMapSocket gpsd3_socket;
#ifdef ROADMAP_USES_LIBGPS
#if GPSD_API_MAJOR_VERSION == 5
struct gps_data_t	gpsd_e, *gpsdp = &gpsd_e;
#else
struct gps_data_t	*gpsdp = NULL;
#endif
#endif

RoadMapSocket roadmap_gpsd3_connect (const char *name) {
#if defined(ROADMAP_USES_LIBGPS) && defined(GPSD_API_MAJOR_VERSION)
#if GPSD_API_MAJOR_VERSION == 5
   if (gps_open(name, "2947", gpsdp) < 0)
      return ROADMAP_INVALID_SOCKET;
   gps_stream(gpsdp, WATCH_JSON, NULL);
#else
   gpsdp = gps_open(name, "2947");
   if (gpsdp == NULL)
      return ROADMAP_INVALID_SOCKET;
   gps_stream(gpsdp, WATCH_NMEA, NULL);
#endif
   return (RoadMapSocket)gpsdp->gps_fd;
#else /* ! (defined(ROADMAP_USES_LIBGPS) && defined(GPSD_API_MAJOR_VERSION)) */
   gpsd3_socket = roadmap_net_connect ("tcp", name, 2947);

   if (ROADMAP_NET_IS_VALID(gpsd3_socket)) {

      /* Start watching what happens. */

      static const char request[] = "?WATCH={\"enable\":true,\"nmea\":true}\n";

      if (roadmap_net_send
	    (gpsd3_socket, request, sizeof(request)-1) != sizeof(request)-1) {

	 roadmap_log (ROADMAP_WARNING, "Lost gpsd server session");
	 roadmap_net_close (gpsd3_socket);

	 return ROADMAP_INVALID_SOCKET;
      }
   }

   return gpsd3_socket;
#endif /* defined(ROADMAP_USES_LIBGPS) && defined(GPSD_API_MAJOR_VERSION) */
}

void roadmap_gpsd3_subscriptions(void)
{
#ifndef ROADMAP_USES_LIBGPS
	 roadmap_gps_nmea();
#endif
}

void *roadmap_gpsd3_decoder_context(void)
{
#ifdef ROADMAP_USES_LIBGPS
	 return NULL;
#else
	 extern RoadMapNmeaAccount RoadMapGpsNmeaAccount;
	 return (void *)RoadMapGpsNmeaAccount;
#endif
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

   int i, s;
#define	MAX_POSSIBLE_SATS	(MAXCHANNELS - 2)

   if (gps_unpack(sentence, gpsdp) < 0) {
      roadmap_log(ROADMAP_ERROR, "gpsd error %d", errno);
      // only roadmap_input can really detect socket errors.  not clear
      // what this return might be.
      return 0;
   }

#ifdef THIS_DONT_WORK
   roadmap_log (ROADMAP_DEBUG, "gpsd set 0x%x online_set %d",
	(unsigned int)gpsdp->set, gpsdp->set & ONLINE_SET);

   roadmap_log (ROADMAP_DEBUG, "gpsd online %d status %d dev_act %f",
   	(int)gpsdp->online, gpsdp->status, gpsdp->dev.activated);

    // we get no reports at all while the gps is unplugged, ONLINE_SET
    // never happens, and "online" is always 0.
   if (!(gpsdp->set & ONLINE_SET))
   	return 0;
   if (!gpsdp->online) {
      roadmap_gps_satellites (-1, 0, 0, 0, 0, 0);
      roadmap_gps_navigation ('V', 0, 0, 0, 0, 0, 0);
      roadmap_log (ROADMAP_WARNING, "gpsd: not online");
      return 0;	// No data
   }
#endif

   if (gpsdp->set & SATELLITE_SET) {
       if (gpsdp->satellites_visible == 0) {
	  roadmap_gps_satellites (0, 0, 0, 0, 0, 0);
	  roadmap_gps_navigation ('V', 0, 0, 0, 0, 0, 0);
	  roadmap_log (ROADMAP_DEBUG, "gpsd: no satellites");
	  gpsdp->set &= ~SATELLITE_SET;
	  return 0;	// No data
       }
#if GPSD_API_MAJOR_VERSION == 5
       static bool     usedflags[MAXCHANNELS];
       int j;
       /* See demo app cgps.c in gpsd source distribution */
       /* Must build bit vector of which satellites are used */
       for (i = 0; i < MAXCHANNELS; i++) {
	  usedflags[i] = false;
	  for (j = 0; j < gpsdp->satellites_used; j++)
	     if (gpsdp->used[j] == gpsdp->PRN[i])
	       usedflags[i] = true;
       }
     
     
       for (i=0, s=1; i<MAX_POSSIBLE_SATS; i++) {
	  if (i < gpsdp->satellites_visible) {
	     roadmap_gps_satellites
		(s,                                // sequence
		gpsdp->PRN[i],             // id
		gpsdp->elevation[i],       // elevation
		gpsdp->azimuth[i],         // azimuth
		(int)gpsdp->ss[i],         // strength
		usedflags[i]);           // active
	    s++;
	  }
       }
#elif GPSD_API_MAJOR_VERSION == 6
       // for version 6, untested
       for (i=0, s=1; i<MAX_POSSIBLE_SATS; i++) {
	  if (i < gpsdp->satellites_visible) {
	     roadmap_gps_satellites
		(s,                                // sequence
		gpsdp->skyview[i]PRN,             // id
		gpsdp->skyview[i]elevation,       // elevation
		gpsdp->skyview[i]azimuth,         // azimuth
		(int)gpsdp->skyview[i]ss,         // strength
		gpsdp->skyview[i].used);           // active
	    s++;
	  }
       }
#endif
 
       roadmap_gps_satellites (0, 0, 0, 0, 0, 0);

       gpsdp->set &= ~SATELLITE_SET;
   }

   if (gpsdp->set & (LATLON_SET|SPEED_SET|ALTITUDE_SET|TIME_SET|TRACK_SET)) {
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
	  if (isnan(gpsdp->fix.track) == 0) {
	     steering = gpsdp->fix.track;
	  }
       }
       roadmap_gps_navigation
           (status, gps_time, latitude, longitude, altitude, speed, steering);

       /* Provide dilution */
       if (gpsdp->fix.mode >= MODE_NO_FIX) {
	  /* No conversion required */
	  roadmap_gps_dilution(gpsdp->fix.mode, gpsdp->fix.epx, gpsdp->fix.epy, gpsdp->fix.epv);
       }

       gpsdp->set &= ~(LATLON_SET|SPEED_SET|ALTITUDE_SET|TIME_SET|TRACK_SET);
   }


   return 1;
#else
   // fprintf(stderr, "%s\n", sentence);
   if (!strncmp(sentence, "{\"class\":",9)) /* } */ {
      // try to detect: {"class":"DEVICE","path":"/dev/ttyUSB1","activated":0}
      roadmap_log (ROADMAP_DEBUG, "gpsd: found 'class'");
      if (strstr(sentence, "\"activated\":0}")) {
	  roadmap_log (ROADMAP_DEBUG, "gpsd: not online");
	  roadmap_gps_satellites (-1, 0, 0, 0, 0, 0);
      }
      return 0;
   }
   return roadmap_nmea_decode (user_context, decoder_context, sentence);
#endif
}
