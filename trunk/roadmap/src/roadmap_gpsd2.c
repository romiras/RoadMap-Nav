/*
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
 *   Copyright 2010, Danny Backx.
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
 * @brief roadmap_gpsd2.c - a module to interact with gpsd using its library.
 *   This module hides the gpsd library API (version 2).
 *   If ROADMAP_USES_LIBGPS is defined, use the (pre) version 3 libgps.
 */

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "roadmap.h"
#include "roadmap_gpsd2.h"
#include "roadmap_math.h"

#ifdef ROADMAP_USES_LIBGPS
#include "errno.h"
#include "gps.h"
#endif


static RoadMapGpsdNavigation RoadmapGpsd2NavigationListener = NULL;
static RoadMapGpsdSatellite  RoadmapGpsd2SatelliteListener = NULL;
static RoadMapGpsdDilution   RoadmapGpsd2DilutionListener = NULL;


static int roadmap_gpsd2_decode_numeric (const char *input) {

   if (input[0] == '?') return ROADMAP_NO_VALID_DATA;

   while (*input == '0') ++input;
   if (*input == 0) return 0;

   return atoi(input);
}


static double roadmap_gpsd2_decode_float (const char *input) {

   if (input[0] == '?') return 0.0;

   return atof(input);
}


static int roadmap_gpsd2_decode_coordinate (const char *input) {

   char *point = strchr (input, '.');

   if (point != NULL) {

      /* This is a floating point value: patch the input to multiply
       * it by 1000000 and then make it an integer (TIGER format).
       */
      const char *from;

      int   i;
      char *to;
      char  modified[16];

      to = modified;

      /* Copy the integer part. */
      for (from = input; from < point; ++from) {
         *(to++) = *from;
      }

      /* Now copy the decimal part. */
      for (from = point + 1, i = 0; *from > 0 && i < 6; ++from, ++i) {
         *(to++) = *from;
      }
      while (i++ < 6) *(to++) = '0';
      *to = 0;

      return atoi(modified);
   }

   return roadmap_gpsd2_decode_numeric (input);
}

RoadMapSocket gpsd2_socket;
#ifdef ROADMAP_USES_LIBGPS
struct gps_data_t	*gpsdp;
#endif

RoadMapSocket roadmap_gpsd2_connect (const char *name) {
#ifdef ROADMAP_USES_LIBGPS
   gpsdp = gps_open(name, "2947");
   if (gpsdp == NULL)
      return ROADMAP_INVALID_SOCKET;
   gps_stream(gpsdp, WATCH_NMEA, NULL);
   return (RoadMapSocket)gpsdp->gps_fd;
#else
   gpsd2_socket = roadmap_net_connect ("tcp", name, 2947);

   if (ROADMAP_NET_IS_VALID(gpsd2_socket)) {

      /* Start watching what happens. */

      static const char request[] = "w+\n";

      if (roadmap_net_send
            (gpsd2_socket, request, sizeof(request)-1) != sizeof(request)-1) {

         roadmap_log (ROADMAP_WARNING, "Lost gpsd server session");
         roadmap_net_close (gpsd2_socket);

         return ROADMAP_INVALID_SOCKET;
      }
   }

   return gpsd2_socket;
#endif
}

/**
 * @brief this is probably a keepalive FIX ME
 */
void roadmap_gpsd2_periodic (void) {
#ifndef ROADMAP_USES_LIBGPS
   roadmap_net_send (gpsd2_socket, "q\n", 2);
#endif
}

void roadmap_gpsd2_subscribe_to_navigation (RoadMapGpsdNavigation navigation) {

   RoadmapGpsd2NavigationListener = navigation;
}


void roadmap_gpsd2_subscribe_to_satellites (RoadMapGpsdSatellite satellite) {

   RoadmapGpsd2SatelliteListener = satellite;
}


void roadmap_gpsd2_subscribe_to_dilution (RoadMapGpsdDilution dilution) {

   RoadmapGpsd2DilutionListener = dilution;
}


int roadmap_gpsd2_decode (void *user_context,
                          void *decoder_context, char *sentence) {
   int status;
   int latitude;
   int longitude;
   int altitude;
   int speed;
   int steering;

   int  gps_time = 0;

#ifdef ROADMAP_USES_LIBGPS
   int i, j, s;
   static bool	used[MAXCHANNELS];
#define	MAX_POSSIBLE_SATS	(MAXCHANNELS - 2)

   if (gps_read(gpsdp) < 0) {
      roadmap_log(ROADMAP_ERROR, "gpsd error %d", errno);
   }

   if (gpsdp->satellites_visible == 0)
      return 0;	// No data

   roadmap_log(ROADMAP_WARNING, "gpsd sats %d", gpsdp->satellites_visible);

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
   int got_navigation_data;

   int got_o = 0;
   int got_q = 0;

   char *reply[256];
   int   reply_count;
   int   i, f;

   int   s;
   int   count;
   int   satellite_count;

   int    dimension_of_fix = -1;
   double pdop = 0.0;
   double hdop = 0.0;
   double vdop = 0.0;

   reply_count = roadmap_input_split (sentence, ',', reply, 256);

   if ((reply_count <= 1) || strcmp (reply[0], "GPSD")) {
      return 0;
   }

   /* default value (invalid value): */
   status = 'V';
   latitude  = ROADMAP_NO_VALID_DATA;
   longitude = ROADMAP_NO_VALID_DATA;
   altitude  = ROADMAP_NO_VALID_DATA;
   speed     = ROADMAP_NO_VALID_DATA;
   steering  = ROADMAP_NO_VALID_DATA;


   got_navigation_data = 0;

   for(i = 1; i < reply_count; ++i) {

      char *item = reply[i];
      char *value = item + 2;
#define N_ARGUM 40  // currently 32 max
      char *argument[N_ARGUM];
#define N_TUPLE 10  // currently 5 max
      char *tuple[N_TUPLE];


      if (item[1] != '=' || item[2] == '?') {
         continue;
      }

      switch (item[0]) {

         case 'A':

            if (got_o) continue; /* Consider the 'O' answer only. */

            altitude = roadmap_gpsd2_decode_numeric (value);
            break;

         case 'M':

            dimension_of_fix = roadmap_gpsd2_decode_numeric(value);

            if (dimension_of_fix > 1) {
               status = 'A';
            } else {
               status = 'V';
               got_navigation_data = 1;
               goto end_of_decoding;
            }

            if (got_q) {

               RoadmapGpsd2DilutionListener
                  (dimension_of_fix, pdop, hdop, vdop);

               got_q = 0; /* We already "used" it. */
            }
            break;

         case 'O':

            f = roadmap_input_split (value, ' ', argument, N_ARGUM);
            if (f < 10) {
               continue;
            }

            got_o = 1;
            got_navigation_data = 1;

            gps_time  = roadmap_gpsd2_decode_numeric    (argument[1]);
            latitude  = roadmap_gpsd2_decode_coordinate (argument[3]);
            longitude = roadmap_gpsd2_decode_coordinate (argument[4]);

            // meters
            altitude  = roadmap_gpsd2_decode_numeric    (argument[5]);

            steering  = roadmap_gpsd2_decode_numeric    (argument[8]);

            // meters/sec
            speed     = roadmap_gpsd2_decode_numeric    (argument[9]);

            if (f >= 15) { // protocol level 3 and higher 
                dimension_of_fix =
                        roadmap_gpsd2_decode_numeric    (argument[14]);
                if (dimension_of_fix > 1) {
                   status = 'A';
                } else {
                   status = 'V';
                }
                got_q = 1; /* not really, but we want to relay the fix
                            * value, and at worst we'll give
                            * stale dilution values.  */
            }

            break;

         case 'P':

            if (got_o) continue; /* Consider the 'O' answer only. */

            if (roadmap_input_split (value, ' ', argument, N_ARGUM) < 2) {
               continue;
            }

            latitude = roadmap_gpsd2_decode_coordinate (argument[0]);
            longitude = roadmap_gpsd2_decode_coordinate (argument[1]);

            if ((longitude != ROADMAP_NO_VALID_DATA) &&
                  (latitude != ROADMAP_NO_VALID_DATA)) {
               got_navigation_data = 1;
               status = 'A';
            }
            break;

         case 'Q':

            if (RoadmapGpsd2DilutionListener == NULL) continue;

            if (roadmap_input_split (value, ' ', argument, N_ARGUM) < 4) {
               continue;
            }

            pdop = roadmap_gpsd2_decode_float (argument[1]);
            hdop = roadmap_gpsd2_decode_float (argument[2]);
            vdop = roadmap_gpsd2_decode_float (argument[3]);

            if (dimension_of_fix > 0) {

               RoadmapGpsd2DilutionListener
                  (dimension_of_fix, pdop, hdop, vdop);

            } else if ((dimension_of_fix < 0) &&
                (roadmap_gpsd2_decode_numeric(argument[0]) > 0)) {

               dimension_of_fix = 2;
               got_q = 1;
            }
            break;

         case 'T':

            if (got_o) continue; /* Consider the 'O' answer only. */

            steering = roadmap_gpsd2_decode_numeric (value);
            break;

         case 'V':

            if (got_o) continue; /* Consider the 'O' answer only. */

            speed = roadmap_gpsd2_decode_numeric (value);
            break;

         case 'Y':

            if (RoadmapGpsd2SatelliteListener == NULL) continue;

            count = roadmap_input_split (value, ':', argument, N_ARGUM);
            if (count <= 0) continue;

            switch (roadmap_input_split (argument[0], ' ', tuple, N_TUPLE)) {

               case 1:
                  
                  // protocol level 1
                  satellite_count = roadmap_gpsd2_decode_numeric(tuple[0]);
                  break;

               case 3:

                  // protocol level 2 and higher
                  gps_time = roadmap_gpsd2_decode_numeric(tuple[1]);
                  satellite_count = roadmap_gpsd2_decode_numeric(tuple[2]);
                  break;

               default: continue; /* Invalid. */
            }

            if (satellite_count != count - 2) {
               roadmap_log (ROADMAP_WARNING,
                     "invalid gpsd 'Y' answer: count = %d, but %d satellites",
                     count, satellite_count);
               continue;
            }

            for (s = 1; s <= satellite_count; ++s) {

               if (roadmap_input_split (argument[s], ' ', tuple, N_TUPLE) < 5) {
                  continue;
               }
               (*RoadmapGpsd2SatelliteListener)
                    (s,
                     roadmap_gpsd2_decode_numeric(tuple[0]),
                     roadmap_gpsd2_decode_numeric(tuple[1]),
                     roadmap_gpsd2_decode_numeric(tuple[2]),
                     roadmap_gpsd2_decode_numeric(tuple[3]),
                     roadmap_gpsd2_decode_numeric(tuple[4]));
            }
            (*RoadmapGpsd2SatelliteListener) (0, 0, 0, 0, 0, 0);

            break;
      }
   }

end_of_decoding:

   if (dimension_of_fix >= 0 && got_q) {
      RoadmapGpsd2DilutionListener (dimension_of_fix, pdop, hdop, vdop);
      got_q = 0;
   }

   if (got_navigation_data) {

      // meters to "preferred" units
      if (altitude != ROADMAP_NO_VALID_DATA)
          altitude = roadmap_math_to_current_unit (100 * altitude, "cm");

      // was m/s, now knots
      if (speed != ROADMAP_NO_VALID_DATA)
          speed = 1944 * speed / 1000;

      RoadmapGpsd2NavigationListener
         (status, gps_time, latitude, longitude, altitude, speed, steering);

      return 1;
   }

   return 0;
#endif
}
