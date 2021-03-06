/*
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
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
 * @brief roadgps_logger.c - Log NMEA messages.
 */

#include <stdio.h>

#include "roadmap.h"
#include "roadmap_config.h"
#include "roadmap_gps.h"
#include "roadmap_path.h"
#include "roadmap_main.h"
#include "roadmap_fileselection.h"

#include "roadgps_logger.h"


static FILE *RoadGpsOutput = NULL;

static RoadMapConfigDescriptor RoadMapConfigLogPath =
                       ROADMAP_CONFIG_ITEM("Log", "Path");

/**
 * @brief send this string to the log file
 * @param sentence string to print
 */
static void roadgps_logger (const char *sentence) {

   if (RoadGpsOutput != NULL) {
      fprintf (RoadGpsOutput, "%s\n", sentence);
   }

   roadmap_main_set_status (sentence);
}

/**
 * @brief stop logging, close the file
 */
void roadgps_logger_stop (void) {

   if (RoadGpsOutput != NULL) {
      fclose (RoadGpsOutput);
      RoadGpsOutput = NULL;
   }
}

/**
 * @brief function called when the user confirms in the file selection dialog
 * @param filename full path name of the file selected by the user
 * @param mode "r" or "w" to indicate type of access -- always "w" in this case
 */
static void roadgps_logger_file_dialog_ok
                           (const char *filename, const char *mode) {

   const char *path;

   roadgps_logger_stop ();

   RoadGpsOutput = fopen (filename, "w");

   if (RoadGpsOutput == NULL) {
      roadmap_log (ROADMAP_ERROR, "cannot open log file %s", filename);
   }

   path = roadmap_path_parent(NULL, filename);
   roadmap_config_set (&RoadMapConfigLogPath, path);
   roadmap_path_free(path);
}

/**
 * @brief start logging, open the file
 */
void roadgps_logger_start (void) {
                                
   roadmap_fileselection_new ("Select file for GPS logging",
                              NULL, /* no filter. */
                              roadmap_config_get(&RoadMapConfigLogPath),
                              "w",
                              roadgps_logger_file_dialog_ok);
}

/**
 * @brief initialize this module
 */
void roadgps_logger_initialize (void) {

   roadmap_config_declare
      ("session", &RoadMapConfigLogPath, roadmap_path_temporary());

   roadmap_gps_register_logger (roadgps_logger);
}

/**
 * @brief close down this module
 */
void roadgps_logger_shutdown (void) {

   roadgps_logger_stop ();
}
