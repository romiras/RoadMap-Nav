/* roadmap_kismet.c - RoadMap driver for kismet.
 *
 * LICENSE:
 *
 *   Copyright 2003 tz1
 *   Copyright 2005 Pascal F. Martin
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
 *
 * DESCRIPTION:
 *
 *   This program is a kismet client that receive updates from kismet
 *   and forwards them to RoadMap as "moving objects".
 *
 *   This program is normally launched by RoadMap.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>

// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>

#include "roadmap_net.h"


#define MAX_WIRELESS_HOT_SPOTS   9999

struct knetlist {
    int btop, bbot;
};

static struct knetlist knl[MAX_WIRELESS_HOT_SPOTS];
static int knlmax = 0;

static char kismet_data[256];


static int connect_to_kismet (const char *host) {

   int socket = -1;
   
   while (socket < 0) {

      socket = roadmap_net_connect (host, 2501);
      sleep (1);
   }

   /* Consume out any introductory data from kismet. */
   read(socket, kismet_data, sizeof(kismet_data));
   sleep(1);

   {
      static char xx[] =
         "!2 ENABLE NETWORK bssid,channel,bestsignal,bestlat,bestlon,wep\n";
      write(socket, xx, strlen(xx));
   }

   /* Get rid of the answer? */
   read(socket, kismet_data, sizeof(kismet_data));
   sleep(1);

   return socket;
}


/* Convert the kismet "decimal degree" format into the NMEA ddmm.mmmmm
 * format.
 */
static void convert_to_nmea (double kismet, int *nmea_ddmm, int *nmea_mmmm) {

   double converting;
   int nmea_dd;
   int nmea_mm;

   converting = fabs(kismet) + 0.0000005;
   nmea_dd = (int) converting;
   converting -= nmea_dd;

   converting *= 60;
   nmea_mm = (int) converting;
   converting -= nmea_mm;

   if (converting > 1.0) {
      fprintf (stderr,
               "invalid value %f (dd -> %d, mm -> %d, remainder %f)\n"
                  "kismet data was: %s\n",
               kismet,
               nmea_dd,
               nmea_mm,
               converting,
               kismet_data);
      exit(1);
   }
   *nmea_mmmm = (int) (kismet * 10000.0);
   *nmea_ddmm = (nmea_dd * 100) + nmea_mm;
}


int main(int argc, char *argv[]) {

   int gps_mode = 0;

   const char *kismet_host;

   FILE *sfp;
   int knet = -1;
   int i, kfd;

   unsigned int chan, sig, wep, maxsig = 0, minsig = 255;
   unsigned char bssid[6];
   unsigned int bsstop, bssbot;

   double la, lo;
   int la_ddmm, lo_ddmm; /* The "ddmm" part. */
   int la_mmmm, lo_mmmm; /* The fractional ".mmmmm" part. */
   char la_hemi, lo_hemi;


   if (argc > 1 && strcmp(argv[1], "--help") == 0) {
      printf ("usage: %s [--help] [--gps] [hostname]\n"
              "  --gps:    simulate GPS position information.\n"
              "  hostname: computer where kismet is running on (default is"
              " the local computer).\n");
      exit(0);
   }

   if (argc > 1 && strcmp(argv[1], "--gps") == 0) {
      gps_mode = 1;
      argc -= 1;
      argv += 1;
   }

   if (argc > 1) {
      kismet_host = argv[1];
   } else {
      kismet_host = "127.0.0.1";
   }
   knet = connect_to_kismet (kismet_host);

   sfp = fdopen(knet, "r");
   wep = 0;

   for(;;) {

      kismet_data[0] = 0;
      fgets(kismet_data, sizeof(kismet_data)-1, sfp);

      if (ferror(sfp)) {

         /* The kismet connection went down. Try to re-establish it. */
         fclose (sfp);
         knet = connect_to_kismet (kismet_host);
         sfp = fdopen(knet, "r");

         continue;
      }

      if(strncmp(kismet_data, "*NETWORK: ", 10))
         continue;

      sscanf(&kismet_data[28],
             "%d %d %lf %lf %d", &chan, &sig, &la, &lo, &wep);

      if(sig > 1 && sig < minsig - 1)
         minsig = sig - 1;
      if(sig > maxsig)
         maxsig = sig;

      for(i = 0; i < 6; i++)
         bssid[i] = strtoul(&kismet_data[10 + 3 * i], NULL, 16);
      bsstop = (bssid[0] << 16) | (bssid[1] << 8) | bssid[2];
      bssbot = (bssid[3] << 16) | (bssid[4] << 8) | bssid[5];

      /* We must now convert the latitude & longitude into the standard
       * NMEA format (since RoadMap uses the NMEA syntax).
       */
      la_hemi = (la < 0) ? 'S' : 'N';
      convert_to_nmea (la, &la_ddmm, &la_mmmm);

      lo_hemi = (lo < 0) ? 'W' : 'E';
      convert_to_nmea (lo, &lo_ddmm, &lo_mmmm);

      if (la_ddmm == 0 && la_mmmm == 0 && lo_ddmm == 0 && lo_mmmm == 0)
         continue;

      for (i = knlmax - 1; i >= 0; --i) {
         if(bsstop == knl[i].btop && bssbot == knl[i].bbot)
            break;
      }
      if(i == -1) {
         if (knlmax >= MAX_WIRELESS_HOT_SPOTS) continue;
         i = knlmax++;
         knl[i].btop = bsstop;
         knl[i].bbot = bssbot;
         printf ("$PXRMADD,ksm%d,"    /* ID for this node. */
                     "%02x:%02x:%02x:%02x:%02x:%02x,kismet," /* MAC address */
                     "%d.%04d,%c,"    /* Latitude. */
                     "%d.%04d,%c\n",  /* Longitude. */
                 i,
                 bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
                 la_ddmm, la_mmmm, la_hemi,
                 lo_ddmm, lo_mmmm, lo_hemi);
      } else {
         printf ("$PXRMMOV,ksm%d,%d.%04d,%c,%d.%04d,%c\n",
                 i,
                 la_ddmm, la_mmmm, la_hemi,
                 lo_ddmm, lo_mmmm, lo_hemi);
      }

      if (gps_mode) {
         /* In this mode, there is no GPS, so we use the kismet data
          * to regenerate some GPS information.
          */
         printf ("$GPGLL,%d.%04d,%c,%d.%04d,%c,,A\n",
                 la_ddmm, la_mmmm, la_hemi,
                 lo_ddmm, lo_mmmm, lo_hemi);
      }
      fflush (stdout);

      if (ferror(stdout))
         exit(0); /* RoadMap closed the pipe. */
   }
   return 0; /* Some compilers might not detect the forever loop. */
}

