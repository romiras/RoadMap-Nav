/* roadmap_gps.c - GPS interface for the RoadMap application.
 *
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
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
 * SYNOPSYS:
 *
 *   See roadmap_gps.h
 */

#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "roadmap.h"
#include "roadmap_types.h"
#include "roadmap_math.h"
#include "roadmap_string.h"
#include "roadmap_object.h"
#include "roadmap_config.h"

#include "roadmap_net.h"
#include "roadmap_file.h"
#include "roadmap_serial.h"
#include "roadmap_state.h"
#include "roadmap_nmea.h"
#include "roadmap_gpsd2.h"
#include "roadmap_message.h"

#include "roadmap_dialog.h"
#include "roadmap_main.h"
#include "roadmap_messagebox.h"

#include "roadmap_gps.h"

#ifdef J2ME
#include "roadmap_gpsj2me.h"
#include <javax/microedition/midlet.h>
#include <gps_manager.h>
#endif

static RoadMapConfigDescriptor RoadMapConfigGPSAccuracy =
                        ROADMAP_CONFIG_ITEM("Accuracy", "GPS Position");

static RoadMapConfigDescriptor RoadMapConfigGPSSpeedAccuracy =
                        ROADMAP_CONFIG_ITEM("Accuracy", "GPS Speed");

static RoadMapConfigDescriptor RoadMapConfigGPSSource =
                        ROADMAP_CONFIG_ITEM("GPS", "Source");

#ifdef _WIN32
static RoadMapConfigDescriptor RoadMapConfigGPSVirtual =
                        ROADMAP_CONFIG_ITEM("GPS", "Virtual");

static RoadMapConfigDescriptor RoadMapConfigGPSBaudRate =
                        ROADMAP_CONFIG_ITEM("GPS", "Baud Rate");

static void roadmap_gps_detect_periodic(void);
#endif

static RoadMapConfigDescriptor RoadMapConfigGPSTimeout =
                        ROADMAP_CONFIG_ITEM("GPS", "Timeout");


static char RoadMapGpsTitle[] = "GPS receiver";

static RoadMapIO RoadMapGpsLink;

static time_t RoadMapGpsConnectedSince = -1;

#define ROADMAP_GPS_CLIENTS 16
static roadmap_gps_listener RoadMapGpsListeners[ROADMAP_GPS_CLIENTS] = {NULL};
static roadmap_gps_monitor  RoadMapGpsMonitors[ROADMAP_GPS_CLIENTS] = {NULL};
static roadmap_gps_logger   RoadMapGpsLoggers[ROADMAP_GPS_CLIENTS] = {NULL};

#define ROADMAP_GPS_NONE     0
#define ROADMAP_GPS_NMEA     1
#define ROADMAP_GPS_GPSD2    2
#define ROADMAP_GPS_OBJECT   3
#define ROADMAP_GPS_J2ME     4
static int RoadMapGpsProtocol = ROADMAP_GPS_NONE;


/* Listeners information (navigation data) ----------------------------- */

static char   RoadMapLastKnownStatus = 'A';
static time_t RoadMapGpsLatestData = 0;
static int    RoadMapGpsEstimatedError = 0;
static int    RoadMapGpsRetryPending = 0;
static time_t RoadMapGpsReceivedTime = 0;
static int    RoadMapGpsReception = 0;

static RoadMapGpsPosition RoadMapGpsReceivedPosition;


/* Monitors information (GPS system status) ---------------------------- */

static int RoadMapGpsActiveSatelliteHash;
static int RoadMapGpsSatelliteCount;
static int RoadMapGpsActiveSatelliteCount;

static char RoadMapGpsActiveSatellite[ROADMAP_NMEA_MAX_SATELLITE];
static RoadMapGpsSatellite RoadMapGpsDetected[ROADMAP_NMEA_MAX_SATELLITE];

static RoadMapGpsPrecision RoadMapGpsQuality;


static void roadmap_gps_no_link_control (RoadMapIO *io) {}
static void roadmap_gps_no_periodic_control (RoadMapCallback callback) {}


static roadmap_gps_periodic_control RoadMapGpsPeriodicAdd =
                                    &roadmap_gps_no_periodic_control;

static roadmap_gps_periodic_control RoadMapGpsPeriodicRemove =
                                    &roadmap_gps_no_periodic_control;

static roadmap_gps_link_control RoadMapGpsLinkAdd =
                                    &roadmap_gps_no_link_control;

static roadmap_gps_link_control RoadMapGpsLinkRemove =
                                    &roadmap_gps_no_link_control;


/* Basic support functions -------------------------------------------- */

static int roadmap_gps_reception_state (void) {

   return RoadMapGpsReception;
}


static void roadmap_gps_update_reception (void) {

   int new_state;

   if (!roadmap_gps_active ()) {
      new_state = GPS_RECEPTION_NA;

   } else if (RoadMapLastKnownStatus != 'A') {
      new_state = GPS_RECEPTION_NONE;

   } else if ((RoadMapGpsActiveSatelliteCount <= 3) ||
         (RoadMapGpsQuality.dilution_horizontal > 2.3)) {

      new_state = GPS_RECEPTION_POOR;
   } else {
      new_state = GPS_RECEPTION_GOOD;
   }

   if (RoadMapGpsReception != new_state) {

      int old_state = RoadMapGpsReception;
      RoadMapGpsReception = new_state;

      if ((old_state <= GPS_RECEPTION_NONE) ||
            (new_state <= GPS_RECEPTION_NONE)) {

         roadmap_state_refresh ();
      }
   }
}


static void roadmap_gps_update_status (char status) {

   if (status != RoadMapLastKnownStatus) {
       if (RoadMapLastKnownStatus == 'A') {
          roadmap_log (ROADMAP_ERROR,
                       "GPS receiver lost satellite fix (status: %c)", status);
       }
       RoadMapLastKnownStatus = status;
   }
}


static void roadmap_gps_process_position (void) {

   int i;

   for (i = 0; i < ROADMAP_GPS_CLIENTS; ++i) {

      if (RoadMapGpsListeners[i] == NULL) break;

      (RoadMapGpsListeners[i])
           (RoadMapGpsReceivedTime,
            &RoadMapGpsQuality,
            &RoadMapGpsReceivedPosition);
   }

   roadmap_gps_update_reception ();
}


static void roadmap_gps_call_monitors (void) {

   int i;

   roadmap_message_set ('c', "%d", RoadMapGpsActiveSatelliteCount);

   for (i = 0; i < ROADMAP_GPS_CLIENTS; ++i) {

      if (RoadMapGpsMonitors[i] == NULL) break;

      (RoadMapGpsMonitors[i])
         (&RoadMapGpsQuality, RoadMapGpsDetected, RoadMapGpsSatelliteCount);
   }

   roadmap_gps_update_reception ();
}


static void roadmap_gps_call_loggers (const char *data) {

   int i;

   for (i = 0; i < ROADMAP_GPS_CLIENTS; ++i) {
      if (RoadMapGpsLoggers[i] == NULL) break;
      (RoadMapGpsLoggers[i]) (data);
   }
}


static void roadmap_gps_keep_alive (void) {

   if (RoadMapGpsLink.subsystem == ROADMAP_IO_INVALID) return;

   if (roadmap_gps_active ()) return;

   roadmap_log (ROADMAP_ERROR, "GPS timeout detected.");

   roadmap_gps_shutdown ();

   /* Try to establish a new IO channel: */
   roadmap_gps_open();
}


/* NMEA protocol support ----------------------------------------------- */

static RoadMapNmeaAccount RoadMapGpsNmeaAccount;


static void roadmap_gps_pgrmm (void *context, const RoadMapNmeaFields *fields) {

    if ((strcasecmp (fields->pgrmm.datum, "NAD83") != 0) &&
        (strcasecmp (fields->pgrmm.datum, "WGS 84") != 0)) {
        roadmap_log (ROADMAP_FATAL,
                     "bad datum '%s': 'NAD83' or 'WGS 84' is required",
                     fields->pgrmm.datum);
    }
}


static void roadmap_gps_pgrme (void *context, const RoadMapNmeaFields *fields) {

    RoadMapGpsEstimatedError =
        roadmap_math_to_current_unit (fields->pgrme.horizontal,
                                      fields->pgrme.horizontal_unit);
}


static void roadmap_gps_gga (void *context, const RoadMapNmeaFields *fields) {

   RoadMapGpsQuality.dilution_horizontal = fields->gga.dilution/100.0;
   roadmap_message_set ('h', "%.2f", RoadMapGpsQuality.dilution_horizontal);

   RoadMapGpsActiveSatelliteCount = fields->gga.count;
   roadmap_message_set ('c', "%d", fields->gga.count);

   if (fields->gga.quality == ROADMAP_NMEA_QUALITY_INVALID) {

      roadmap_gps_update_status ('V');

   } else {

      roadmap_gps_update_status ('A');

      RoadMapGpsReceivedTime = fields->gga.fixtime;

      RoadMapGpsReceivedPosition.latitude  = fields->gga.latitude;
      RoadMapGpsReceivedPosition.longitude = fields->gga.longitude;
      /* speed not available: keep previous value. */
      RoadMapGpsReceivedPosition.altitude  =
      roadmap_math_to_current_unit (fields->gga.altitude,
                                       fields->gga.altitude_unit);

      roadmap_gps_process_position();
   }
}


static void roadmap_gps_gll (void *context, const RoadMapNmeaFields *fields) {

   roadmap_gps_update_status (fields->gll.status);

   if (fields->gll.status == 'A') {

      RoadMapGpsReceivedPosition.latitude  = fields->gll.latitude;
      RoadMapGpsReceivedPosition.longitude = fields->gll.longitude;

      /* speed not available: keep previous value. */
      /* altitude not available: keep previous value. */
      /* steering not available: keep previous value. */

      roadmap_gps_process_position();
   }
}


static void roadmap_gps_vtg (void *context, const RoadMapNmeaFields *fields) {
   RoadMapGpsReceivedPosition.speed = fields->vtg.speed;
   if (fields->vtg.speed > roadmap_gps_speed_accuracy()) {
      RoadMapGpsReceivedPosition.steering = fields->vtg.steering;
   }
}


static void roadmap_gps_rmc (void *context, const RoadMapNmeaFields *fields) {

   roadmap_gps_update_status (fields->rmc.status);

   if (fields->rmc.status == 'A') {

      RoadMapGpsReceivedTime = fields->rmc.fixtime;

      RoadMapGpsReceivedPosition.latitude  = fields->rmc.latitude;
      RoadMapGpsReceivedPosition.longitude = fields->rmc.longitude;
      RoadMapGpsReceivedPosition.speed     = fields->rmc.speed;
      /* altitude not available: keep previous value. */

      if (fields->rmc.speed > roadmap_gps_speed_accuracy()) {

         /* Update the steering only if the speed is significant:
          * when the speed is too low, the steering indicated by
          * the GPS device is not reliable; in that case the best
          * guess is that we did not turn.
          */
         RoadMapGpsReceivedPosition.steering  = fields->rmc.steering;
      }

      roadmap_gps_process_position();
   }
}


static void roadmap_gps_gsa
               (void *context, const RoadMapNmeaFields *fields) {

   int i;

   RoadMapGpsActiveSatelliteHash = 0;

   for (i = 0; i < ROADMAP_NMEA_MAX_SATELLITE; i += 1) {

      RoadMapGpsActiveSatellite[i] = fields->gsa.satellite[i];
      RoadMapGpsActiveSatelliteHash |=
         (1 << (RoadMapGpsActiveSatellite[i] & 0x1f));
   }

   RoadMapGpsQuality.dimension = fields->gsa.dimension;
   RoadMapGpsQuality.dilution_position   = fields->gsa.dilution_position;
   RoadMapGpsQuality.dilution_horizontal = fields->gsa.dilution_horizontal;
   RoadMapGpsQuality.dilution_vertical   = fields->gsa.dilution_vertical;

   roadmap_message_set ('p', "%.2f", RoadMapGpsQuality.dilution_position);
   roadmap_message_set ('h', "%.2f", RoadMapGpsQuality.dilution_horizontal);
   roadmap_message_set ('v', "%.2f", RoadMapGpsQuality.dilution_vertical);
   
   roadmap_gps_update_reception ();
}


static void roadmap_gps_gsv
               (void *context, const RoadMapNmeaFields *fields) {

   int i;
   int id;
   int index;

   for (i = 0, index = (fields->gsv.index - 1) * 4;
        i < 4 && index < fields->gsv.count;
        i += 1, index += 1) {

      RoadMapGpsDetected[index].id        = fields->gsv.satellite[i];
      RoadMapGpsDetected[index].elevation = fields->gsv.elevation[i];
      RoadMapGpsDetected[index].azimuth   = fields->gsv.azimuth[i];
      RoadMapGpsDetected[index].strength  = fields->gsv.strength[i];

      RoadMapGpsDetected[index].status  = 'F';
   }

   if (fields->gsv.index == fields->gsv.total) {
      int active_count = 0;

      RoadMapGpsSatelliteCount = fields->gsv.count;

      if (RoadMapGpsSatelliteCount > ROADMAP_NMEA_MAX_SATELLITE) {
         RoadMapGpsSatelliteCount = ROADMAP_NMEA_MAX_SATELLITE;
      }

      for (index = 0; index < RoadMapGpsSatelliteCount; index += 1) {

         id = RoadMapGpsDetected[index].id;

         if (RoadMapGpsActiveSatelliteHash & (1 << id)) {

            for (i = 0; i < ROADMAP_NMEA_MAX_SATELLITE; i += 1) {

               if (RoadMapGpsActiveSatellite[i] == id) {
                  RoadMapGpsDetected[index].status = 'A';
                  active_count++;
                  break;
               }
            }
         }
      }

      RoadMapGpsActiveSatelliteCount = active_count;
      roadmap_gps_call_monitors ();
   }
}


static void roadmap_gps_nmea (void) {

   if (RoadMapGpsNmeaAccount == NULL) {

      RoadMapGpsNmeaAccount = roadmap_nmea_create (RoadMapGpsTitle);

      roadmap_nmea_subscribe
         (NULL, "RMC", roadmap_gps_rmc, RoadMapGpsNmeaAccount);

      roadmap_nmea_subscribe
         (NULL, "GGA", roadmap_gps_gga, RoadMapGpsNmeaAccount);

      roadmap_nmea_subscribe
         (NULL, "GLL", roadmap_gps_gll, RoadMapGpsNmeaAccount);

      roadmap_nmea_subscribe
         (NULL, "GSA", roadmap_gps_gsa, RoadMapGpsNmeaAccount);

      roadmap_nmea_subscribe
         (NULL, "VTG", roadmap_gps_vtg, RoadMapGpsNmeaAccount);

      roadmap_nmea_subscribe
         ("GRM", "E", roadmap_gps_pgrme, RoadMapGpsNmeaAccount);

      roadmap_nmea_subscribe
         ("GRM", "M", roadmap_gps_pgrmm, RoadMapGpsNmeaAccount);

      roadmap_nmea_subscribe
         (NULL, "GSV", roadmap_gps_gsv, RoadMapGpsNmeaAccount);
   }
}

/* End of NMEA protocol support ---------------------------------------- */


/* GPSD (or other) protocol support ------------------------------------ */

static void roadmap_gps_navigation (char status,
                                    int gmt_time,
                                    int latitude,
                                    int longitude,
                                    int altitude,
                                    int speed,
                                    int steering) {

   roadmap_gps_update_status (status);

   if (status == 'A') {

      RoadMapGpsReceivedTime = gmt_time;

      if (latitude != ROADMAP_NO_VALID_DATA) {
         RoadMapGpsReceivedPosition.latitude  = latitude;
      }

      if (longitude != ROADMAP_NO_VALID_DATA) {
         RoadMapGpsReceivedPosition.longitude  = longitude;
      }

      if (altitude != ROADMAP_NO_VALID_DATA) {
         RoadMapGpsReceivedPosition.altitude  = altitude;
      }

      if (speed != ROADMAP_NO_VALID_DATA) {
         RoadMapGpsReceivedPosition.speed  = speed;
      }

      if (steering != ROADMAP_NO_VALID_DATA) {
         RoadMapGpsReceivedPosition.steering  = steering;
      }

      roadmap_gps_process_position();
   }
}


static void roadmap_gps_satellites  (int sequence,
                                     int id,
                                     int elevation,
                                     int azimuth,
                                     int strength,
                                     int active) {

   static int active_count;
   if (sequence == 0) {

      /* End of list: propagate the information. */

      RoadMapGpsActiveSatelliteCount = active_count;
      roadmap_gps_call_monitors ();

   } else {

      int index = sequence - 1;

      if (index == 0) {
         active_count = 0;
      }

      RoadMapGpsDetected[index].id        = (char)id;
      RoadMapGpsDetected[index].elevation = (char)elevation;
      RoadMapGpsDetected[index].azimuth   = (short)azimuth;
      RoadMapGpsDetected[index].strength  = (short)strength;

      if (active) {

         if (RoadMapGpsQuality.dimension < 2) {
            RoadMapGpsQuality.dimension = 2;
         }
         RoadMapGpsDetected[index].status  = 'A';
         active_count++;

      } else {
         RoadMapGpsDetected[index].status  = 'F';
      }
   }

   RoadMapGpsSatelliteCount = sequence;
}


static void roadmap_gps_dilution (int dimension,
                                  double position,
                                  double horizontal,
                                  double vertical) {

   RoadMapGpsQuality.dimension = dimension;
   RoadMapGpsQuality.dilution_position   = position;
   RoadMapGpsQuality.dilution_horizontal = horizontal;
   RoadMapGpsQuality.dilution_vertical   = vertical;

   roadmap_message_set ('p', "%.2f", RoadMapGpsQuality.dilution_position);
   roadmap_message_set ('h', "%.2f", RoadMapGpsQuality.dilution_horizontal);
   roadmap_message_set ('v', "%.2f", RoadMapGpsQuality.dilution_vertical);
}

/* End of GPSD protocol support ---------------------------------------- */


/* OBJECTS pseudo protocol support ------------------------------------- */

static RoadMapObjectListener RoadMapGpsNextObjectListener;
static RoadMapDynamicString  RoadmapGpsObjectId;

static void roadmap_gps_object_listener (RoadMapDynamicString id,
                                         const RoadMapGpsPosition *position) {

   RoadMapGpsReceivedPosition = *position;

   roadmap_gps_update_status ('A');
   roadmap_gps_process_position();

   (*RoadMapGpsNextObjectListener) (id, position);
}


static void roadmap_gps_object_monitor (RoadMapDynamicString id) {

   if (id == RoadmapGpsObjectId) {
      roadmap_gps_open();
   }
}

/* End of OBJECT protocol support -------------------------------------- */

void roadmap_gps_initialize (void) {

   static int RoadMapGpsInitialized = 0;
#ifdef _WIN32
   const int *serial_ports;
   static const char **speeds;
   RoadMapConfigItem *source_item = NULL;
   RoadMapConfigItem *virtual_item = NULL;
   RoadMapConfigItem *speed_item = NULL;
   int i;
#endif

   if (! RoadMapGpsInitialized) {

      roadmap_config_declare
         ("preferences", &RoadMapConfigGPSSpeedAccuracy, "4", NULL);
      roadmap_config_declare
         ("preferences", &RoadMapConfigGPSAccuracy, "30", NULL);
#ifndef _WIN32
      roadmap_config_declare
         ("preferences", &RoadMapConfigGPSSource, "gpsd://localhost", NULL);
#else

      virtual_item = roadmap_config_declare_enumeration
               ("preferences", &RoadMapConfigGPSVirtual, NULL, "", NULL);

      serial_ports = roadmap_serial_enumerate ();
      for (i=0; i<MAX_SERIAL_ENUMS; ++i) {

         char name[10];
         sprintf (name, "COM%d:", i);

         if (!serial_ports[i]) {
            roadmap_config_add_enumeration_value (virtual_item, name);
            continue;
         }

/*         
         if (!source_item) {
            source_item = roadmap_config_declare_enumeration
                     ("preferences", &RoadMapConfigGPSSource, name, NULL);
         } else {
            roadmap_config_add_enumeration_value (source_item, name);
         }
*/         
      }

      if (!source_item) {
         roadmap_config_declare
            ("preferences", &RoadMapConfigGPSSource, "COM1:", NULL);
      }


      speed_item = roadmap_config_declare_enumeration
               ("preferences", &RoadMapConfigGPSBaudRate, NULL, "", NULL);
      speeds = roadmap_serial_get_speeds ();
      i = 0;
      while (speeds[i] != NULL) {
         roadmap_config_add_enumeration_value (speed_item, speeds[i]);
         i++;
      }

#endif
      roadmap_config_declare
         ("preferences", &RoadMapConfigGPSTimeout, "3", NULL);

      RoadMapGpsInitialized = 1;

      roadmap_state_add ("GPS_reception", &roadmap_gps_reception_state);
   }
}


void roadmap_gps_shutdown (void) {

   if (RoadMapGpsLink.subsystem == ROADMAP_IO_INVALID) return;

   (*RoadMapGpsPeriodicRemove) (roadmap_gps_keep_alive);

   (*RoadMapGpsLinkRemove) (&RoadMapGpsLink);

   roadmap_io_close (&RoadMapGpsLink);
}


void roadmap_gps_register_listener (roadmap_gps_listener listener) {

   int i;

   for (i = 0; i < ROADMAP_GPS_CLIENTS; ++i) {
      if (RoadMapGpsListeners[i] == NULL) {
         RoadMapGpsListeners[i] = listener;
         break;
      }
   }
}


void roadmap_gps_register_monitor (roadmap_gps_monitor monitor) {

   int i;

   for (i = 0; i < ROADMAP_GPS_CLIENTS; ++i) {
      if (RoadMapGpsMonitors[i] == NULL) {
         RoadMapGpsMonitors[i] = monitor;
         break;
      }
   }
}


void roadmap_gps_open (void) {

   const char *url;


   /* Check if we have a gps interface defined: */

   roadmap_gps_update_reception ();

   url = roadmap_gps_source ();

   if (url == NULL) {
#ifdef _WIN32
      url = roadmap_main_get_virtual_serial ();
      if (!url) {
         url = roadmap_config_get (&RoadMapConfigGPSSource);
      }
#else
      url = roadmap_config_get (&RoadMapConfigGPSSource);
#endif

#ifndef J2ME
      if (url == NULL) {
         return;
      }
      if (*url == 0) {
         return;
      }
#endif
   }

   /* We do have a gps interface: */

   RoadMapGpsLink.subsystem = ROADMAP_IO_INVALID;
   RoadMapGpsProtocol = ROADMAP_GPS_NMEA; /* This is the default. */

#ifndef J2ME
   if (strncasecmp (url, "gpsd://", 7) == 0) {

      RoadMapGpsLink.os.socket = roadmap_net_connect ("tcp", url+7, 2947);

      if (ROADMAP_NET_IS_VALID(RoadMapGpsLink.os.socket)) {

         if (roadmap_net_send (RoadMapGpsLink.os.socket, "r\n", 2, 1) == 2) {

            RoadMapGpsLink.subsystem = ROADMAP_IO_NET;

         } else {

            roadmap_log (ROADMAP_WARNING, "cannot subscribe to gpsd");
            roadmap_net_close(RoadMapGpsLink.os.socket);
         }
      }

   } else if (strncasecmp (url, "gpsd2://", 8) == 0) {

      RoadMapGpsLink.os.socket = roadmap_gpsd2_connect (url+8);

      if (ROADMAP_NET_IS_VALID(RoadMapGpsLink.os.socket)) {

            RoadMapGpsLink.subsystem = ROADMAP_IO_NET;
            RoadMapGpsProtocol = ROADMAP_GPS_GPSD2;
      }

#ifndef _WIN32
   } else if (strncasecmp (url, "tty://", 6) == 0) {

      /* The syntax of the url is: tty://dev/ttyXXX[:speed] */

      char *device = strdup (url + 5); /* url is a const (config data). */
      char *speed  = strchr (device, ':');

      if (speed == NULL) {
         speed = "4800"; /* Hardcoded default matches NMEA standard. */
      } else {
         *(speed++) = 0;
      }

#else
   } else if ((strncasecmp (url, "com", 3) == 0) && (url[4] == ':')) {

      char *device = strdup(url); /* I do know this is not smart.. */
      const char *speed = roadmap_config_get (&RoadMapConfigGPSBaudRate);

#endif

      RoadMapGpsLink.os.serial =
         roadmap_serial_open (device, "r", atoi(speed));

      if (ROADMAP_SERIAL_IS_VALID(RoadMapGpsLink.os.serial)) {
         RoadMapGpsLink.subsystem = ROADMAP_IO_SERIAL;
      }

      free(device);

   } else if (strncasecmp (url, "file://", 7) == 0) {

      RoadMapGpsLink.os.file = roadmap_file_open (url+7, "r");

      if (ROADMAP_FILE_IS_VALID(RoadMapGpsLink.os.file)) {
         RoadMapGpsLink.subsystem = ROADMAP_IO_FILE;
      }

   } else if (strncasecmp (url, "object:", 7) == 0) {

      if (strcmp (url+7, "GPS") == 0) {

         roadmap_log (ROADMAP_ERROR, "cannot resolve self-reference to GPS");

      } else {
         RoadMapGpsLink.subsystem = ROADMAP_IO_NULL;
         RoadMapGpsProtocol = ROADMAP_GPS_OBJECT;

         RoadmapGpsObjectId = roadmap_string_new(url+7);

         RoadMapGpsNextObjectListener =
            roadmap_object_register_listener (RoadmapGpsObjectId,
                                              roadmap_gps_object_listener);

         if (RoadMapGpsNextObjectListener == NULL) {
            RoadMapGpsLink.subsystem = ROADMAP_IO_INVALID;
            roadmap_object_register_monitor (roadmap_gps_object_monitor);
         }
      }

   } else if (url[0] == '/') {

      RoadMapGpsLink.os.file = roadmap_file_open (url, "r");

      if (ROADMAP_FILE_IS_VALID(RoadMapGpsLink.os.file)) {
         RoadMapGpsLink.subsystem = ROADMAP_IO_FILE;
      }

   } else {
      roadmap_log (ROADMAP_ERROR, "invalid protocol in url %s", url);
      return;
   }
#else /* J2ME */
   if (1) {
      char mgr_url[255];
      NOPH_GpsManager_t gps_mgr = NOPH_GpsManager_getInstance();

      if (NOPH_GpsManager_getURL (gps_mgr, mgr_url, sizeof(mgr_url)) != -1) {
         if (strcmp (url, mgr_url)) {
            roadmap_config_set (&RoadMapConfigGPSSource, mgr_url);
            url = mgr_url;
         }
      }

      if (!strchr(url, ':')) {
         /* Location API protocol */
         RoadMapGpsProtocol = ROADMAP_GPS_J2ME;
      } else {
         RoadMapGpsProtocol = ROADMAP_GPS_NMEA;
      }
      RoadMapGpsLink.os.serial = roadmap_serial_open (url, "r", 0);

      if (ROADMAP_SERIAL_IS_VALID(RoadMapGpsLink.os.serial)) {
         RoadMapGpsLink.subsystem = ROADMAP_IO_SERIAL;
      }
   }

#endif

   if (RoadMapGpsLink.subsystem == ROADMAP_IO_INVALID) {
      if (! RoadMapGpsRetryPending) {
         roadmap_log (ROADMAP_WARNING, "cannot access GPS source %s", url);
         (*RoadMapGpsPeriodicAdd) (roadmap_gps_open);
         RoadMapGpsRetryPending = 1;
      }
      return;
   }

   if (RoadMapGpsRetryPending) {
      (*RoadMapGpsPeriodicRemove) (roadmap_gps_open);
      RoadMapGpsRetryPending = 0;
   }

   RoadMapGpsConnectedSince = time(NULL);
   RoadMapGpsLatestData = time(NULL);

   (*RoadMapGpsPeriodicAdd) (roadmap_gps_keep_alive);

   /* Declare this IO to the GUI toolkit so that we wake up on GPS data. */

   if (RoadMapGpsLink.subsystem != ROADMAP_IO_NULL) {
      (*RoadMapGpsLinkAdd) (&RoadMapGpsLink);
   }

   switch (RoadMapGpsProtocol) {

      case ROADMAP_GPS_NMEA:

         roadmap_gps_nmea();
         break;

#ifndef J2ME
      case ROADMAP_GPS_GPSD2:

         roadmap_gpsd2_subscribe_to_navigation (roadmap_gps_navigation);
         roadmap_gpsd2_subscribe_to_satellites (roadmap_gps_satellites);
         roadmap_gpsd2_subscribe_to_dilution   (roadmap_gps_dilution);
         break;
#else
      case ROADMAP_GPS_J2ME:

         roadmap_gpsj2me_subscribe_to_navigation (roadmap_gps_navigation);
         //roadmap_gpsj2me_subscribe_to_satellites (roadmap_gps_satellites);
         //roadmap_gpsj2me_subscribe_to_dilution   (roadmap_gps_dilution);
         break;
#endif	 

      case ROADMAP_GPS_OBJECT:
         break;

      default:

         roadmap_log (ROADMAP_FATAL, "internal error (unsupported protocol)");
   }
}


void roadmap_gps_register_logger (roadmap_gps_logger logger) {

   int i;

   for (i = 0; i < ROADMAP_GPS_CLIENTS; ++i) {

      if (RoadMapGpsLoggers[i] == logger) {
         break;
      }
      if (RoadMapGpsLoggers[i] == NULL) {
         RoadMapGpsLoggers[i] = logger;
         break;
      }
   }
}


void roadmap_gps_register_link_control
        (roadmap_gps_link_control add, roadmap_gps_link_control remove) {

   RoadMapGpsLinkAdd    = add;
   RoadMapGpsLinkRemove = remove;
}


void roadmap_gps_register_periodic_control
                 (roadmap_gps_periodic_control add,
                  roadmap_gps_periodic_control remove) {

   RoadMapGpsPeriodicAdd      = add;
   RoadMapGpsPeriodicRemove   = remove;
}


void roadmap_gps_input (RoadMapIO *io) {

   static RoadMapInputContext decode;
   int res;


   if (decode.title == NULL) {

      decode.title    = RoadMapGpsTitle;
      decode.logger   = roadmap_gps_call_loggers;
   }

   decode.io = io;

   switch (RoadMapGpsProtocol) {

      case ROADMAP_GPS_NMEA:

         decode.decoder = roadmap_nmea_decode;
         decode.decoder_context = (void *)RoadMapGpsNmeaAccount;
         decode.is_binary = 0;

         break;

#ifndef J2ME
      case ROADMAP_GPS_GPSD2:

         decode.decoder = roadmap_gpsd2_decode;
         decode.decoder_context = NULL;
         decode.is_binary = 0;
         break;
#else
      case ROADMAP_GPS_J2ME:

         decode.decoder = roadmap_gpsj2me_decode;
         decode.decoder_context = NULL;
         decode.is_binary = 1;
         break;
#endif	 

      case ROADMAP_GPS_OBJECT:

         return;

      default:

         roadmap_log (ROADMAP_FATAL, "internal error (unsupported protocol)");
   }


   res = roadmap_input (&decode);

   if (res < 0) {

      (*RoadMapGpsLinkRemove) (io);

      roadmap_io_close (io);

      /* Try to establish a new IO channel: */

      (*RoadMapGpsPeriodicRemove) (roadmap_gps_keep_alive);
      roadmap_gps_open();
   }

   RoadMapGpsLatestData = time (NULL);
}


int roadmap_gps_active (void) {

   time_t timeout;

   if (RoadMapGpsLink.subsystem == ROADMAP_IO_INVALID) {
      return 0;
   }

   timeout = (time_t) roadmap_config_get_integer (&RoadMapConfigGPSTimeout);

   if (time(NULL) - RoadMapGpsLatestData >= timeout) {
      return 0;
   }

   return 1;
}


int roadmap_gps_estimated_error (void) {

    if (RoadMapGpsEstimatedError == 0) {
        return roadmap_config_get_integer (&RoadMapConfigGPSAccuracy);
    }

    return RoadMapGpsEstimatedError;
}


int  roadmap_gps_speed_accuracy (void) {
    return roadmap_config_get_integer (&RoadMapConfigGPSSpeedAccuracy);
}


int  roadmap_gps_is_nmea (void) {

   switch (RoadMapGpsProtocol) {

      case ROADMAP_GPS_NMEA:              return 1;
      case ROADMAP_GPS_GPSD2:             return 0;
      case ROADMAP_GPS_OBJECT:            return 0;
      case ROADMAP_GPS_J2ME:              return 0;
   }

   return 0; /* safe bet in case of something wrong. */
}


/* GPS auto detection - win32 only */
#ifdef _WIN32
static void roadmap_gps_detect_finalize(void){
   roadmap_main_remove_periodic (roadmap_gps_detect_periodic);
   roadmap_dialog_set_data ("GPS Receiver Auto Detect", "Port", "");
   roadmap_dialog_set_data ("GPS Receiver Auto Detect", "Speed", "");
   roadmap_dialog_hide ("Detect GPS receiver");
}


static void roadmap_gps_detect_periodic(void) {

   static const int *serial_ports;
   static const char **speeds;
   static time_t OpenTime;

   static int SpeedIndex = -1;
   static int CurrentPort = -1;
   static int VirtualPort;

   static char *SavedSpeed;
   static char *SavedPort;
   static int SavedRetryPending = 0;

   static char Prompt[100];

   if (CurrentPort == -1) { /* new run */
      const char *virtual_port = roadmap_config_get (&RoadMapConfigGPSVirtual);
      serial_ports = roadmap_serial_enumerate ();
      speeds = roadmap_serial_get_speeds ();

      /* check for virtual port configuration */
      if ((strlen(virtual_port) >= 5) && !strncmp(virtual_port, "COM", 3)) {
         VirtualPort = atoi (virtual_port + 3);
      } else {
         VirtualPort = -1;
      }

      SavedSpeed = (char *)roadmap_config_get (&RoadMapConfigGPSBaudRate);
      SavedPort = (char *)roadmap_config_get (&RoadMapConfigGPSSource);
      SavedRetryPending = RoadMapGpsRetryPending;

      OpenTime = time(NULL) - (time_t)3; /* make sure first time will be processed */
      CurrentPort++;
      /* skip undefined ports */
      while ((CurrentPort < MAX_SERIAL_ENUMS) &&
            (!serial_ports[CurrentPort] ||
            (CurrentPort == VirtualPort))) {

         CurrentPort++;
      }
   }

   if (time(NULL) - OpenTime > (time_t)2) { /* passed 2 seconds since trying to open gps */
      SpeedIndex++;
      if (speeds[SpeedIndex] == NULL) {
         SpeedIndex = 0;
         CurrentPort++;

         /* skip undefined ports */
         while ((CurrentPort < MAX_SERIAL_ENUMS) &&
               (!serial_ports[CurrentPort] ||
               (CurrentPort == VirtualPort))) {

            CurrentPort++;
         }
      }

      if (CurrentPort == MAX_SERIAL_ENUMS) { /* not found */
         roadmap_gps_detect_finalize();
         roadmap_config_set (&RoadMapConfigGPSSource, SavedPort);
         roadmap_config_set (&RoadMapConfigGPSBaudRate, SavedSpeed);
         if ((SavedRetryPending) && (! RoadMapGpsRetryPending)) {
            (*RoadMapGpsPeriodicAdd) (roadmap_gps_open);
            RoadMapGpsRetryPending = 1;
         }
         SpeedIndex = -1;
         CurrentPort = -1;
         roadmap_messagebox(roadmap_lang_get ("Error"),
            roadmap_lang_get ("GPS Receiver not found. Make sure your receiver is connected and turned on."));
         return;
      }

      /* prepare to test the new configuration */
      if (RoadMapGpsRetryPending) {
         (*RoadMapGpsPeriodicRemove) (roadmap_gps_open);
         RoadMapGpsRetryPending = 0;
      }

      roadmap_gps_shutdown();

      sprintf (Prompt, "COM%d:", CurrentPort);

      roadmap_dialog_set_data ("GPS Receiver Auto Detect", "Speed",
                               speeds[SpeedIndex]);
      roadmap_dialog_set_data ("GPS Receiver Auto Detect", "Port", Prompt);

      roadmap_config_set (&RoadMapConfigGPSSource, Prompt);
      roadmap_config_set (&RoadMapConfigGPSBaudRate, speeds[SpeedIndex]);

      OpenTime = time(NULL);
      roadmap_gps_open();
      return;
   }

   if (RoadMapGpsReception != 0) { /* found */
      roadmap_gps_detect_finalize();
      snprintf (Prompt, sizeof(Prompt),
             "%s\nPort: COM%d:\nSpeed: %s",
             roadmap_lang_get ("Found GPS Receiver."),
             CurrentPort, speeds[SpeedIndex]);
      SpeedIndex = -1;
      CurrentPort = -1;
      roadmap_messagebox(roadmap_lang_get ("Info"), Prompt);
   }
}


void roadmap_gps_detect_receiver (void) {

   if (RoadMapGpsReception != 0) {
      roadmap_messagebox(roadmap_lang_get ("Info"),
                         roadmap_lang_get ("GPS already connected!"));
   } else {
      if (roadmap_dialog_activate ("Detect GPS receiver", NULL, 1)) {

         roadmap_dialog_new_label  ("GPS Receiver Auto Detect", "Port");
         roadmap_dialog_new_label  ("GPS Receiver Auto Detect", "Speed");
         roadmap_dialog_new_label  ("GPS Receiver Auto Detect", "Status");

         roadmap_dialog_complete (0);
      }

      roadmap_dialog_set_data ("GPS Receiver Auto Detect", "Status",
                               roadmap_lang_get ("Running, please wait..."));
      roadmap_main_set_periodic (200,roadmap_gps_detect_periodic);
   }
}

#else
#ifdef J2ME
void roadmap_gps_detect_receiver (void) {
   NOPH_GpsManager_t gps_mgr = NOPH_GpsManager_getInstance();
   NOPH_MIDlet_t m = NOPH_MIDlet_get();
   const char *wait_msg = roadmap_lang_get("Please wait...");
   const char *not_found_msg = roadmap_lang_get("GPS Receiver not found. Make sure your receiver is connected and turned on.");

   NOPH_GpsManager_searchGps(gps_mgr, m, wait_msg, not_found_msg);
}
#else
/* Unix */

void roadmap_gps_detect_receiver (void) {}
#endif
#endif

