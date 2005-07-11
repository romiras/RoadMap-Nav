/* roadmap_driver.c - a driver manager for the RoadMap application.
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
 *   See roadmap_driver.h
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "roadmap.h"
#include "roadmap_types.h"
#include "roadmap_math.h"
#include "roadmap_config.h"

#include "roadmap_file.h"
#include "roadmap_path.h"
#include "roadmap_spawn.h"

#include "roadmap_input.h"
#include "roadmap_nmea.h"
#include "roadmap_gps.h"
#include "roadmap_object.h"

#include "roadmap_driver.h"


static RoadMapConfigDescriptor RoadMapDriverTemplate =
                         ROADMAP_CONFIG_ITEM("Drivers", "xxx");

/* What the driver may subscribe to (a bit mask): */
#define ROADMAP_DRIVER_RMC    1  /* Subscribed to GPRMC information. */

typedef struct roadmap_driver_descriptor {

   RoadMapDynamicString name;

   char *command;
   char *arguments;

   RoadMapFeedback process;
   RoadMapPipe     pipes[2];

   RoadMapConfigDescriptor enable;

   RoadMapInputContext nmea;

   int             subscription;

   struct roadmap_driver_descriptor *next;

} RoadMapDriver;

static RoadMapDriver *RoadMapDriverList = NULL;

static int RoadMapDriverSubscription = 0; /* OR of all drivers subscriptions. */


static void roadmap_driver_no_link_control (RoadMapIO *io) {}

static roadmap_gps_link_control RoadMapDriverLinkAdd =
                                    &roadmap_driver_no_link_control;

static roadmap_gps_link_control RoadMapDriverLinkRemove =
                                    &roadmap_driver_no_link_control;

static RoadMapNmeaListener RoadMapDriverNextPxrmadd = NULL;
static RoadMapNmeaListener RoadMapDriverNextPxrmmov = NULL;
static RoadMapNmeaListener RoadMapDriverNextPxrmdel = NULL;
static RoadMapNmeaListener RoadMapDriverNextPxrmsub = NULL;
static RoadMapNmeaListener RoadMapDriverNextPxrmcfg = NULL;



static char *roadmap_driver_strdup (const char *text) {

   if (text == NULL) return NULL;

   return strdup(text);
}


static int roadmap_driver_exists (const char *name) {

   RoadMapDriver *driver;

   for (driver = RoadMapDriverList; driver != NULL; driver = driver->next) {
      if (roadmap_string_match(driver->name, name)) return 1;
   }
   return 0;
}


static void roadmap_driver_pxrmadd (void *context,
                                    const RoadMapNmeaFields *fields) {

   RoadMapDriver *driver = (RoadMapDriver *)context;

   roadmap_object_add (driver->name,
                       fields->pxrmadd.id,
                       fields->pxrmadd.name,
                       fields->pxrmadd.sprite);

   (*RoadMapDriverNextPxrmadd) (context, fields);
}


static void roadmap_driver_pxrmmov (void *context,
                                    const RoadMapNmeaFields *fields) {

   RoadMapGpsPosition position;

   position.latitude  = fields->pxrmmov.latitude;
   position.longitude = fields->pxrmmov.longitude;

   position.speed    = fields->pxrmmov.speed;
   position.steering = fields->pxrmmov.steering;
   position.altitude = 0;

   roadmap_object_move (fields->pxrmadd.id, &position);

   (*RoadMapDriverNextPxrmmov) (context, fields);
}


static void roadmap_driver_pxrmdel (void *context,
                                    const RoadMapNmeaFields *fields) {

   roadmap_object_remove (fields->pxrmdel.id);

   (*RoadMapDriverNextPxrmdel) (context, fields);
}


static void roadmap_driver_pxrmsub (void *context,
                                    const RoadMapNmeaFields *fields) {

   int i;
   RoadMapDriver *driver = (RoadMapDriver *)context;

   if (driver != NULL) {
      for (i = 0; i < fields->pxrmsub.count; ++i) {

         if (roadmap_string_match(fields->pxrmsub.subscribed[i].item, "RMC")) {
            driver->subscription |= ROADMAP_DRIVER_RMC;
            RoadMapDriverSubscription |= ROADMAP_DRIVER_RMC;

         } else {

            roadmap_log (ROADMAP_ERROR,
                         "invalid subscription %s by driver %s",
                         fields->pxrmsub.subscribed[i].item,
                         driver->name);
         }
      }
   }

   (*RoadMapDriverNextPxrmdel) (context, fields);
}


static void roadmap_driver_pxrmcfg (void *context,
                                    const RoadMapNmeaFields *fields) {

   RoadMapDriver *driver = (RoadMapDriver *)context;

   const char *value;
   RoadMapConfigDescriptor descriptor;
   char  buffer[1024];

   roadmap_string_lock (fields->pxrmcfg.category);
   roadmap_string_lock (fields->pxrmcfg.name);
   roadmap_string_lock (fields->pxrmcfg.value);

   descriptor.category  = roadmap_string_get(fields->pxrmcfg.category);
   descriptor.name      = roadmap_string_get(fields->pxrmcfg.name);
   descriptor.reference = NULL;

   roadmap_config_declare ("preferences",
                           &descriptor,
                           roadmap_string_get(fields->pxrmcfg.value));

   value = roadmap_config_get (&descriptor);

   snprintf (buffer, sizeof(buffer), "$PXRMCFG,%s,%s,%s\n",
             descriptor.category,
             descriptor.name,
             value);
   roadmap_spawn_write_pipe (driver->pipes[1], buffer, strlen(buffer));

   /* We do not release the category, name and default value strings,
    * because these are still referenced in the configuration data.
    */

   (*RoadMapDriverNextPxrmcfg) (context, fields);
}


static void roadmap_driver_onexit (void *context) {

   RoadMapDriver *driver = (RoadMapDriver *) context;

   if (ROADMAP_SPAWN_IS_VALID_PIPE(driver->pipes[0])) {

      RoadMapIO io;

      roadmap_object_cleanup (driver->name);

      io.subsystem = ROADMAP_IO_PIPE;
      io.os.pipe = driver->pipes[0];
      (*RoadMapDriverLinkRemove) (&io);

      roadmap_spawn_close_pipe (driver->pipes[0]);
      roadmap_spawn_close_pipe (driver->pipes[1]);

      driver->pipes[0] = ROADMAP_SPAWN_INVALID_PIPE;
      driver->pipes[1] = ROADMAP_SPAWN_INVALID_PIPE;
   }
}


static int roadmap_driver_receive (void *context, char *data, int size) {

   RoadMapDriver *driver = (RoadMapDriver *)context;

   return roadmap_spawn_read_pipe (driver->pipes[0], data, size);
}


static void roadmap_driver_configure (const char *path) {

   char *p;
   char *name;
   char *command;
   char *arguments;
   FILE *file;
   char  buffer[1024];

   RoadMapDriver *driver;

   file = roadmap_file_fopen (path, "drivers", "sr");

   if (file != NULL) {

      while (! feof(file)) {

         fgets (buffer, sizeof(buffer), file);

         if (feof(file) || ferror(file)) break;

         buffer[sizeof(buffer)-1] = 0;

         /* remove the end-of-line character. */
         p = strchr (buffer, '\n');
         if (p != NULL) *p = 0;


         /* Retrieve the name part of the driver definition line:
          * skip any leading space.
          */
         for (name = buffer; isspace(*name); ++name) ;

         if ((*name == 0) || (*name == '#')) continue; /* Empty line. */


         /* retrieve the command (i.e. value) part of the driver
          * definition line: search for the ':' separator and skip
          * all leading space characters.
          */
         command = strchr(name, ':');
         if (command == NULL) {
            roadmap_log (ROADMAP_ERROR, "%s: invalid syntax", buffer);
            continue;
         }

         /* Eliminate trailing blanks (all spaces before the separator). */
         while (isspace(*(command-1))) --command;

         *command = 0; /* Separate the name from the value. */

         /* Eliminate all spaces after the separator. */
         for (++command; isspace(*command); ++command) ;

         /* retrieve the arguments part of the driver command. */
         arguments = strchr(command, ' ');
         if (arguments == NULL) {
            arguments = strchr(command, '\t');
         }

         if (arguments != NULL) {
            *arguments = 0; /* Separate the name from the arguments. */
            for (++arguments; isspace(*arguments); ++arguments) ;
         }

         /* Ignore this new entry if that driver was already configured. */
         if (roadmap_driver_exists(name)) continue;

         driver = calloc (1, sizeof(RoadMapDriver));
         if (driver == NULL) break;

         driver->name      = roadmap_string_new(name);
         driver->command   = roadmap_driver_strdup(command);
         driver->arguments = roadmap_driver_strdup(arguments);

         driver->pipes[0] = ROADMAP_SPAWN_INVALID_PIPE;
         driver->pipes[1] = ROADMAP_SPAWN_INVALID_PIPE;
         driver->process.handler = roadmap_driver_onexit;
         driver->process.data    = (void *)driver;

         driver->subscription = 0;

         /* Prepare the NMEA context. */
         driver->nmea.title        = roadmap_string_get(driver->name);
         driver->nmea.user_context = driver;
         driver->nmea.cursor       = 0;
         driver->nmea.logger       = NULL;
         driver->nmea.receiver     = roadmap_driver_receive;
         driver->nmea.decoder      = roadmap_nmea_decode;

         /* Configuration item (enable/disable). */
         driver->enable = RoadMapDriverTemplate;
         driver->enable.name = driver->nmea.title;

         roadmap_config_declare_enumeration
            ("preferences", &driver->enable, "Disabled", "Enabled", NULL);

         driver->next = RoadMapDriverList;
         RoadMapDriverList = driver;
      }

      fclose(file);
   }
}


static void roadmap_driver_to_nmea (int value, int *ddmm, int *mmmm) {

   int degrees;
   int minutes;

   value = abs(value);
   degrees = value / 1000000;
   value = 60 * (value % 1000000);
   minutes = value / 1000000;

   *ddmm = degrees * 100 + minutes;
   *mmmm = (value % 1000000) / 100;
}

void roadmap_driver_publish (const RoadMapGpsPosition *position) {

   /* Format the GPRMC sentence that is used to publish our position. */

   int latitude_ddmm;
   int latitude_mmmm;
   int longitude_ddmm;
   int longitude_mmmm;

   time_t now;
   struct tm *gmt;

   int length;
   char buffer[128];  /* large enough and more. */

   RoadMapDriver *driver;


   if (! (RoadMapDriverSubscription & ROADMAP_DRIVER_RMC)) return;

   roadmap_driver_to_nmea (position->latitude,
                           &latitude_ddmm, &latitude_mmmm);

   roadmap_driver_to_nmea (position->longitude,
                           &longitude_ddmm, &longitude_mmmm);

   time(&now);
   gmt = gmtime (&now);
   if (gmt == NULL) return;

   snprintf (buffer, sizeof(buffer),
             "$GPRMC,"
                     "%02d%02d%02d.000,A," /* Time (hhmmss) and status. */
                     "%d.%04d,%c,"         /* Latitude. */
                     "%d.%04d,%c,"         /* Longitude. */
                     "%d,"                 /* Speed (knots). */
                     "%d,"                 /* Course over ground. */
                     "%02d%02d%02d,"       /* UTC date (DDMMYY). */
                     "0,E\n",              /* Magnetic variation (none). */
            gmt->tm_hour, gmt->tm_min, gmt->tm_sec,
            latitude_ddmm, latitude_mmmm, position->latitude > 0 ? 'N' : 'S',
            longitude_ddmm, longitude_mmmm, position->longitude > 0 ? 'E' : 'W',
            position->speed,
            position->steering,
            gmt->tm_mday, gmt->tm_mon+1, gmt->tm_year%100);

   /* Send that data to all drivers. */

   length = strlen(buffer);

   for (driver = RoadMapDriverList; driver != NULL; driver = driver->next) {
      if (ROADMAP_SPAWN_IS_VALID_PIPE(driver->pipes[1]) &&
          (driver->subscription & ROADMAP_DRIVER_RMC)) {
         roadmap_spawn_write_pipe (driver->pipes[1], buffer, length);
      }
   }
}


void roadmap_driver_input (RoadMapIO *io) {

   RoadMapDriver *driver;

   if (io->subsystem != ROADMAP_IO_PIPE) return;

   for (driver = RoadMapDriverList; driver != NULL; driver = driver->next) {

      if (driver->pipes[0] == io->os.pipe) {
         if (roadmap_input (&driver->nmea) < 0) {
            roadmap_driver_onexit (driver);
         }
         break;
      }
   }
}


void roadmap_driver_initialize (void) {

   const char *path;


   roadmap_driver_configure (roadmap_path_user());

   for (path = roadmap_path_first("config");
        path != NULL;
        path = roadmap_path_next("config", path)) {

      roadmap_driver_configure (path);
   }

   RoadMapDriverNextPxrmadd =
      roadmap_nmea_subscribe ("XRM", "ADD", roadmap_driver_pxrmadd);

   RoadMapDriverNextPxrmmov =
      roadmap_nmea_subscribe ("XRM", "MOV", roadmap_driver_pxrmmov);

   RoadMapDriverNextPxrmdel =
      roadmap_nmea_subscribe ("XRM", "DEL", roadmap_driver_pxrmdel);

   RoadMapDriverNextPxrmsub =
      roadmap_nmea_subscribe ("XRM", "SUB", roadmap_driver_pxrmsub);

   RoadMapDriverNextPxrmcfg =
      roadmap_nmea_subscribe ("XRM", "CFG", roadmap_driver_pxrmcfg);
}


void roadmap_driver_activate (void) {

   RoadMapDriver *driver;

   for (driver = RoadMapDriverList; driver != NULL; driver = driver->next) {

      if (strcasecmp(roadmap_config_get (&driver->enable), "Enabled")) continue;

      if (roadmap_spawn_with_pipe
            (driver->command,
             driver->arguments,
             driver->pipes,
             &driver->process) > 0) {

         /* Declare this I/O to the GUI so that we can wake up
          * when the driver talks to us.
          */
         RoadMapIO io;

         io.subsystem = ROADMAP_IO_PIPE;
         io.os.pipe = driver->pipes[0];

         (*RoadMapDriverLinkAdd) (&io);

         // TBD: any initialization message needed??
      }
   }
}


void roadmap_driver_register_link_control
        (roadmap_gps_link_control add, roadmap_gps_link_control remove) {

   RoadMapDriverLinkAdd    = add;
   RoadMapDriverLinkRemove = remove;
}


void roadmap_driver_shutdown (void) {

   RoadMapDriver *driver;

   for (driver = RoadMapDriverList; driver != NULL; driver = driver->next) {
      roadmap_driver_onexit (driver);
   }
}

