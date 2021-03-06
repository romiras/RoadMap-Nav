/* roadmap_download.c - Download RoadMap maps.
 *
 * LICENSE:
 *
 *   Copyright 2003 Pascal Martin.
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
 *   See roadmap_download.h
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "roadmap.h"
#include "roadmap_types.h"
#include "roadmap_hash.h"
#include "roadmap_path.h"
#include "roadmap_file.h"
#include "roadmap_config.h"
#include "roadmap_gui.h"
#include "roadmap_county.h"
#include "roadmap_screen.h"
#include "roadmap_state.h"
#include "roadmap_locator.h"
#include "roadmap_messagebox.h"
#include "roadmap_dialog.h"
#include "roadmap_start.h"
#include "roadmap_main.h"
#include "roadmap_preferences.h"
#include "roadmap_spawn.h"
#include "roadmap_math.h"

#include "roadmap_download.h"


#define ROADMAP_FILE_NAME_FORMAT "usc%05d.rdm"


static RoadMapConfigDescriptor RoadMapConfigSource =
                                  ROADMAP_CONFIG_ITEM("Download", "Source");

static RoadMapConfigDescriptor RoadMapConfigDestination =
                                  ROADMAP_CONFIG_ITEM("Download", "Destination");

struct roadmap_download_tool {
   char *suffix;
   char *name;
};

static struct roadmap_download_tool RoadMapDownloadCompressTools[] = {
   {".gz",  "gunzip -f"},
   {".bz2", "bunzip2 -f"},
   {".lzo", "lzop -d -Uf"},
   {NULL, NULL}
};

static RoadMapHash *RoadMapDownloadBlock = NULL;
static int *RoadMapDownloadBlockList = NULL;
static int  RoadMapDownloadBlockCount = 0;

static int *RoadMapDownloadQueue = NULL;
static int  RoadMapDownloadQueueConsumer = 0;
static int  RoadMapDownloadQueueProducer = 0;
static int  RoadMapDownloadQueueSize = 0;

static int  RoadMapDownloadRefresh = 0;

static int  RoadMapDownloadCurrentFileSize = 0;
static int  RoadMapDownloadDownloaded = 0;


struct roadmap_download_protocol {

   struct roadmap_download_protocol *next;

   const char *prefix;
   RoadMapDownloadProtocol handler;
};

static struct roadmap_download_protocol *RoadMapDownloadProtocolMap = NULL;


static void roadmap_download_next_county (void);

static void roadmap_download_no_handler (void) {}

static RoadMapDownloadEvent RoadMapDownloadWhenDone =
                               roadmap_download_no_handler;



static int roadmap_download_request (int size) {

   /* TBD: for the time being, answer everything is fine. */

   RoadMapDownloadCurrentFileSize = size;
   RoadMapDownloadDownloaded = 0;
   return 1;
}


static void roadmap_download_format_size (char *image, int value) {

   if (value > (10 * 1024 * 1024)) {
      sprintf (image, "%dMB", value / (1024 * 1024));
   } else {
      sprintf (image, "%dKB", value / 1024);
   }
}


static void roadmap_download_error (const char *format, ...) {

   va_list ap;
   char message[2048];

   va_start(ap, format);
   vsnprintf (message, sizeof(message), format, ap);
   va_end(ap);

   roadmap_messagebox ("Download Error", message);
}


static int roadmap_download_increment (int cursor) {

   if (++cursor >= RoadMapDownloadQueueSize) {
      cursor = 0;
   }
   return cursor;
}


static void roadmap_download_end (void) {


   RoadMapDownloadQueueConsumer =
      roadmap_download_increment (RoadMapDownloadQueueConsumer);

   if (RoadMapDownloadQueueConsumer != RoadMapDownloadQueueProducer) {

      /* The queue is not yet empty: start the next download. */
      roadmap_download_next_county ();

   } else if (RoadMapDownloadRefresh) {

      /* The queue is empty: tell the final consumer, but only
       * if there was at least one successful download.
       */
      RoadMapDownloadRefresh = 0;
      RoadMapDownloadWhenDone ();
   }
}


static void roadmap_download_progress (int loaded) {

   int  fips;
   char image[32];

   int progress = (100 * loaded) / RoadMapDownloadCurrentFileSize;


   /* Avoid updating the dialod too often: this may slowdown the download. */

   if (progress == RoadMapDownloadDownloaded) return;

   RoadMapDownloadDownloaded = progress;


   fips = RoadMapDownloadQueue[RoadMapDownloadQueueConsumer];

   if (roadmap_dialog_activate ("Downloading", NULL)) {

      roadmap_dialog_new_label  (".file", "County");
      roadmap_dialog_new_label  (".file", "State");
      roadmap_dialog_new_label  (".file", "Size");
      roadmap_dialog_new_label  (".file", "Download");

      roadmap_dialog_complete (0);
   }
   roadmap_dialog_set_data (".file", "County", roadmap_county_get_name (fips));
   roadmap_dialog_set_data (".file", "State", roadmap_county_get_state (fips));

   roadmap_download_format_size (image, RoadMapDownloadCurrentFileSize);
   roadmap_dialog_set_data (".file", "Size", image);

   if (loaded == RoadMapDownloadCurrentFileSize) {
      roadmap_dialog_hide (".file");
      roadmap_download_end ();
   } else {
      roadmap_download_format_size (image, loaded);
      roadmap_dialog_set_data (".file", "Download", image);
   }

   roadmap_main_flush ();
}


static RoadMapDownloadCallbacks RoadMapDownloadCallbackFunctions = {
   roadmap_download_request,
   roadmap_download_progress,
   roadmap_download_error
};


static void roadmap_download_allocate (void) {

   if (RoadMapDownloadQueue == NULL) {

      RoadMapDownloadQueueSize = roadmap_county_count();

      RoadMapDownloadQueue =
         calloc (RoadMapDownloadQueueSize, sizeof(int));
      roadmap_check_allocated(RoadMapDownloadQueue);

      RoadMapDownloadBlockList = calloc (RoadMapDownloadQueueSize, sizeof(int));
      roadmap_check_allocated(RoadMapDownloadBlockList);

      RoadMapDownloadBlock =
         roadmap_hash_new ("download", RoadMapDownloadQueueSize);
   }
}


void roadmap_download_block (int fips) {

   int i;
   int candidate = -1;

   roadmap_download_allocate ();

   /* Check if the county was not already locked. While doing this,
    * detect any unused slot in that hash list that we might reuse.
    */
   for (i = roadmap_hash_get_first (RoadMapDownloadBlock, fips);
        i >= 0;
        i = roadmap_hash_get_next (RoadMapDownloadBlock, i)) {

      if (RoadMapDownloadBlockList[i] == fips) {
         return; /* Already done. */
      }
      if (RoadMapDownloadBlockList[i] == 0) {
         candidate = i;
      }
   }

   if (candidate >= 0) {
      /* This county is not yet blocked, and there is a free slot
       * for us to reuse in the right hash list.
       */
      RoadMapDownloadBlockList[candidate] = fips;
      return;
   }

   /* This county was not locked and there was no free slot: we must create
    * a new one.
    */
   if (RoadMapDownloadBlockCount < RoadMapDownloadQueueSize) {

      roadmap_hash_add (RoadMapDownloadBlock,
                        fips,
                        RoadMapDownloadBlockCount);

      RoadMapDownloadBlockList[RoadMapDownloadBlockCount] = fips;
      RoadMapDownloadBlockCount += 1;
   }
}


void roadmap_download_unblock (int fips) {

   int i;

   if (RoadMapDownloadQueue == NULL)  return;

   for (i = roadmap_hash_get_first (RoadMapDownloadBlock, fips);
        i >= 0;
        i = roadmap_hash_get_next (RoadMapDownloadBlock, i)) {

      if (RoadMapDownloadBlockList[i] == fips) {
         RoadMapDownloadBlockList[i] = 0;
      }
   }
}


void roadmap_download_unblock_all (void) {

   int i;

   if (RoadMapDownloadQueue == NULL)  return;

   for (i = RoadMapDownloadQueueSize - 1; i >= 0; --i) {
      RoadMapDownloadBlockList[i] = 0;
   }
}


int roadmap_download_blocked (int fips) {

   int i;

   roadmap_download_allocate ();

   for (i = roadmap_hash_get_first (RoadMapDownloadBlock, fips);
        i >= 0;
        i = roadmap_hash_get_next (RoadMapDownloadBlock, i)) {

      if (RoadMapDownloadBlockList[i] == fips) {
         return 1;
      }
   }
   return 0;
}


static void roadmap_download_uncompress (const char *destination) {

   char *p;
   char  command[2048];
   struct roadmap_download_tool *tool;

   for (tool = RoadMapDownloadCompressTools; tool->suffix != NULL; ++tool) {

      p = strstr (destination, tool->suffix);

      if ((p != NULL) && (strcmp (p, tool->suffix) == 0)) {

         snprintf (command, sizeof(command), "%s %s", tool->name, destination);
         roadmap_spawn_command (command);
         break;
      }
   }
}


static void roadmap_download_ok (const char *name, void *context) {

   int  fips = RoadMapDownloadQueue[RoadMapDownloadQueueConsumer];
   struct roadmap_download_protocol *protocol;

   char source[256];
   char destination[256];

   const char *format;
   const char *directory;


   format = roadmap_dialog_get_data (".file", "From");
   snprintf (source, sizeof(source), format, fips);

   format = roadmap_dialog_get_data (".file", "To");
   snprintf (destination, sizeof(destination), format, fips);

   roadmap_dialog_hide (name);

   directory = roadmap_path_parent (NULL, destination);
   roadmap_path_create (directory);
   roadmap_path_free (directory);


   /* FIXME: at this point, we should set a temporary destination
    * file name. When done with the transfer, we should rename the file
    * to its final name. That would replace the "freeze" in a more
    * elegant manner.
    */

   /* Search for the correct protocol handler to call this time. */

   for (protocol = RoadMapDownloadProtocolMap;
        protocol != NULL;
        protocol = protocol->next) {

      if (strncmp (source, protocol->prefix, strlen(protocol->prefix)) == 0) {

         roadmap_start_freeze ();

         if (protocol->handler (&RoadMapDownloadCallbackFunctions,
                                source, destination)) {

            roadmap_download_uncompress (destination);
            RoadMapDownloadRefresh = 1;
         }
         roadmap_download_unblock (fips);
         roadmap_start_unfreeze ();
         break;
      }
   }

   if (protocol == NULL) {

      roadmap_messagebox ("Download Error", "invalid download protocol");
      roadmap_log (ROADMAP_WARNING, "invalid download source %s", source);
   }

   if (RoadMapDownloadCurrentFileSize > 0) {
      roadmap_download_progress (RoadMapDownloadCurrentFileSize);
   }
}


static void roadmap_download_cancel (const char *name, void *context) {

   roadmap_dialog_hide (name);

   roadmap_download_block (RoadMapDownloadQueue[RoadMapDownloadQueueConsumer]);

   roadmap_download_end ();
}


static void roadmap_download_next_county (void) {

   int fips = RoadMapDownloadQueue[RoadMapDownloadQueueConsumer];

   const char *source;
   const char *basename;

   char buffer[2048];


   source = roadmap_config_get (&RoadMapConfigSource);
   basename = strrchr (source, '/');
   if (basename == NULL) {
      roadmap_messagebox ("Download Error", "Bad source file name (no path)");
      return;
   }

   if (roadmap_dialog_activate ("Download a Map", NULL)) {

      roadmap_dialog_new_label  (".file", "County");
      roadmap_dialog_new_label  (".file", "State");
      roadmap_dialog_new_entry  (".file", "From");
      roadmap_dialog_new_entry  (".file", "To");

      roadmap_dialog_add_button ("OK", roadmap_download_ok);
      roadmap_dialog_add_button ("Cancel", roadmap_download_cancel);

      roadmap_dialog_complete (roadmap_preferences_use_keyboard());
   }
   roadmap_dialog_set_data (".file", "County", roadmap_county_get_name (fips));
   roadmap_dialog_set_data (".file", "State", roadmap_county_get_state (fips));

   roadmap_dialog_set_data (".file", "From", source);

   snprintf (buffer, sizeof(buffer), "%s%s", 
             roadmap_config_get (&RoadMapConfigDestination), basename);

   roadmap_dialog_set_data (".file", "To", buffer);
}


int roadmap_download_get_county (int fips) {

   int next;

   if (RoadMapDownloadWhenDone == roadmap_download_no_handler) return 0;


   /* Check that we did not refuse to download that county already.
    * If not, set a block, which we will release when done. This is to
    * avoid requesting a download while we are downloading this county.
    */
   if (roadmap_download_blocked (fips)) return 0;

   roadmap_download_block (fips);


   /* Add this county to the download request queue. */

   next = roadmap_download_increment(RoadMapDownloadQueueProducer);

   if (next == RoadMapDownloadQueueConsumer) {
      /* The queue is full: stop downloading more. */
      return 0;
   }

   RoadMapDownloadQueue[RoadMapDownloadQueueProducer] = fips;

   if (RoadMapDownloadQueueProducer == RoadMapDownloadQueueConsumer) {

      /* The queue was empty: start downloading now. */

      RoadMapDownloadQueueProducer = next;
      roadmap_download_next_county();
      return 0;
   }

   RoadMapDownloadQueueProducer = next;
   return 0;
}


/* -------------------------------------------------------------------------
 * Show map statistics: number of files, disk space occupied.
 */

static void roadmap_download_compute_space (int *count, int *size) {

   char **files;
   char **cursor;
   const char *directory = roadmap_config_get (&RoadMapConfigDestination);

   files = roadmap_path_list (directory, ".rdm");

   for (cursor = files, *size = 0, *count = 0; *cursor != NULL; ++cursor) {
      if (strcmp (*cursor, "usdir.rdm")) {
         *size  += roadmap_file_length (directory, *cursor);
         *count += 1;
      }
   }

   roadmap_path_list_free (files);
}


static void roadmap_download_show_space_ok (const char *name, void *context) {
   roadmap_dialog_hide (name);
}


void roadmap_download_show_space (void) {

   int  size;
   int  count;
   char image[32];


   if (roadmap_dialog_activate ("Disk Usage", NULL)) {

      roadmap_dialog_new_label  (".file", "Files");
      roadmap_dialog_new_label  (".file", "Space");

      roadmap_dialog_add_button ("OK", roadmap_download_show_space_ok);

      roadmap_dialog_complete (0);
   }

   roadmap_dialog_set_data   (".file", ".title", "Map statistics:");

   roadmap_download_compute_space (&count, &size);

   sprintf (image, "%d", count);
   roadmap_dialog_set_data (".file", "Files", image);

   roadmap_download_format_size (image, size);
   roadmap_dialog_set_data (".file", "Space", image);
}


/* -------------------------------------------------------------------------
 * Select & delete map files.
 */
static int   *RoadMapDownloadDeleteFips = NULL;

static char **RoadMapDownloadDeleteNames = NULL;
static int    RoadMapDownloadDeleteCount = 0;
static int    RoadMapDownloadDeleteSelected = 0;


static void roadmap_download_delete_free (void) {

   int i;

   if (RoadMapDownloadDeleteNames != NULL) {

      for (i = RoadMapDownloadDeleteCount-1; i >= 0; --i) {
         free (RoadMapDownloadDeleteNames[i]);
      }
      free (RoadMapDownloadDeleteNames);
   }

   RoadMapDownloadDeleteNames = NULL;
   RoadMapDownloadDeleteCount = 0;
   RoadMapDownloadDeleteSelected = -1;
}


static void roadmap_download_delete_done (const char *name, void *context) {

   roadmap_dialog_hide (name);
   roadmap_download_delete_free ();
}


static void roadmap_download_delete_selected (const char *name, void *data) {

   int i;

   char *map_name = (char *) roadmap_dialog_get_data (".delete", ".maps");

   for (i = RoadMapDownloadDeleteCount-1; i >= 0; --i) {
      if (! strcmp (map_name, RoadMapDownloadDeleteNames[i])) {
         RoadMapDownloadDeleteSelected = RoadMapDownloadDeleteFips[i];
      }
   }
}


static void roadmap_download_delete_populate (void) {

   int i;
   int size;
   int count;
   char name[1024];


   roadmap_download_delete_free ();

   roadmap_download_compute_space (&count, &size);

   sprintf (name, "%d", count);
   roadmap_dialog_set_data (".delete", "Files", name);

   roadmap_download_format_size (name, size);
   roadmap_dialog_set_data (".delete", "Space", name);

   roadmap_main_flush();


   
   RoadMapDownloadDeleteCount =
      roadmap_locator_by_position
            (roadmap_math_get_center (), &RoadMapDownloadDeleteFips);

    RoadMapDownloadDeleteNames =
       calloc (RoadMapDownloadDeleteCount, sizeof(char *));
    roadmap_check_allocated(RoadMapDownloadDeleteNames);

    /* - List each candidate county: */

    for (i = 0; i < RoadMapDownloadDeleteCount; ++i) {

       snprintf (name, sizeof(name), "%s/" ROADMAP_FILE_NAME_FORMAT,
                 roadmap_config_get (&RoadMapConfigDestination),
                 RoadMapDownloadDeleteFips[i]);

       if (! roadmap_file_exists (NULL, name)) {

          /* The map for this county is not available: remove
           * the county from the list.
           */
          RoadMapDownloadDeleteFips[i] =
             RoadMapDownloadDeleteFips[--RoadMapDownloadDeleteCount];

          if (RoadMapDownloadDeleteCount <= 0) break; /* Empty list. */

          --i;
          continue;
       }

       snprintf (name, sizeof(name), "%s, %s",
                 roadmap_county_get_name (RoadMapDownloadDeleteFips[i]),
                 roadmap_county_get_state (RoadMapDownloadDeleteFips[i]));

       RoadMapDownloadDeleteNames[i] = strdup (name);
       roadmap_check_allocated(RoadMapDownloadDeleteNames[i]);
    }

    roadmap_dialog_show_list
        (".delete", ".maps",
         RoadMapDownloadDeleteCount,
         RoadMapDownloadDeleteNames, (void **)RoadMapDownloadDeleteNames,
         roadmap_download_delete_selected);
}


static void roadmap_download_delete_doit (const char *name, void *context) {

   char path[1024];

   if (RoadMapDownloadDeleteSelected > 0) {

      roadmap_download_block (RoadMapDownloadDeleteSelected);

      snprintf (path, sizeof(path), "%s/" ROADMAP_FILE_NAME_FORMAT,
                roadmap_config_get (&RoadMapConfigDestination),
                RoadMapDownloadDeleteSelected);

      roadmap_locator_close (RoadMapDownloadDeleteSelected);
      roadmap_file_remove (NULL, path);

      roadmap_start_request_repaint_map (REPAINT_NOW);

      roadmap_download_delete_populate ();
   }
}


void roadmap_download_delete (void) {

   if (roadmap_dialog_activate ("Delete Maps", NULL)) {

      roadmap_dialog_new_label  (".delete", "Files");
      roadmap_dialog_new_label  (".delete", "Space");

      roadmap_dialog_new_list   (".delete", ".maps");

      roadmap_dialog_add_button ("Delete", roadmap_download_delete_doit);
      roadmap_dialog_add_button ("Done", roadmap_download_delete_done);

      roadmap_dialog_complete (0); /* No need for a keyboard. */
   }

   roadmap_download_delete_populate ();
}


/* -------------------------------------------------------------------------
 * Configure this download module.
 */

void roadmap_download_subscribe_protocol  (const char *prefix,
                                           RoadMapDownloadProtocol handler) {

   struct roadmap_download_protocol *protocol;

   protocol = malloc (sizeof(*protocol));
   roadmap_check_allocated(protocol);

   protocol->prefix = strdup(prefix);
   protocol->handler = handler;

   protocol->next = RoadMapDownloadProtocolMap;
   RoadMapDownloadProtocolMap = protocol;
}


void roadmap_download_subscribe_when_done (RoadMapDownloadEvent handler) {

   if (handler == NULL) {
      RoadMapDownloadWhenDone = roadmap_download_no_handler;
   } else {
      RoadMapDownloadWhenDone = handler;
   }
}


int  roadmap_download_enabled (void) {
   return (RoadMapDownloadWhenDone != roadmap_download_no_handler);
}

static int roadmap_download_state (void) {

   if (roadmap_download_enabled()) {
      return DOWNLOAD_ENABLED;
   } else {
      return DOWNLOAD_DISABLED;
   }
}


void roadmap_download_initialize (void) {

   char default_destination[1024];

   roadmap_config_declare
      ("preferences",
      &RoadMapConfigSource,
      "/usr/local/share/roadmap/" ROADMAP_FILE_NAME_FORMAT);

   snprintf (default_destination, sizeof(default_destination),
             "%s/maps", roadmap_path_user());

   roadmap_config_declare
      ("preferences",
      &RoadMapConfigDestination, strdup(default_destination));

    roadmap_state_add ("get_download_enabled", &roadmap_download_state);
}

