/*
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
 */

/**
 * @file
 * @brief roadmap_gps.h - GPS interface for the RoadMap application.
 */

#ifndef INCLUDED__ROADMAP_GPS__H
#define INCLUDED__ROADMAP_GPS__H

#include "roadmap_io.h"

enum {GPS_RECEPTION_NA = 0,
      GPS_RECEPTION_NO_COMM,
      GPS_RECEPTION_NONE,
      GPS_RECEPTION_POOR,
      GPS_RECEPTION_GOOD
};


void roadmap_gps_initialize (void);


/* The listener is a function to be called each time a valid GPS coordinate
 * has been received. There can be more than one listener at a given time.
 */
typedef struct {

   int longitude;  /* TIGER format (decimal degrees multiplied by 1000000) */
   int latitude;
   int altitude;   /* Using the selected unit system (metric or imperial) */
   int speed;      /* knots */
   int steering;   /* Degrees */

} RoadMapGpsPosition;

#define ROADMAP_GPS_NULL_POSITION {0, 0, 0, 0, 0}

typedef struct {

   int    dimension;        /* <2: none, 2: 2D fix, 3: 3D fix. */
   double dilution_position;
   double dilution_horizontal;
   double dilution_vertical;

} RoadMapGpsPrecision;

typedef void (*roadmap_gps_listener) (int reception,
                                      int gps_time,
                                      const RoadMapGpsPrecision *dilution,
                                      const RoadMapGpsPosition  *position);

void roadmap_gps_register_listener (roadmap_gps_listener listener);


/* The monitor is a function to be called each time a valid GPS satellite
 * status has been received. There can be more than one monitor at a given
 * time.
 */
typedef struct {

   unsigned char  id;
   unsigned char  status;     /* 0: not detected, 'F': fixing, 'A': active. */
   unsigned char  elevation;
   unsigned char  reserved;
   short azimuth;
   short strength;

} RoadMapGpsSatellite;

typedef void (*roadmap_gps_monitor) (int reception,
                                     const RoadMapGpsPrecision *precision,
                                     const RoadMapGpsSatellite *satellites,
                                     int activecount,
                                     int count);

void roadmap_gps_register_monitor (roadmap_gps_monitor monitor);


/* The link and periodic control functions are hooks designed to let the GPS
 * link to be managed from within an application's GUI main loop.
 *
 * When data is detected from the GPS link, roadmap_gps_input() should be
 * called. If the GPS link is down, roadmap_gps_open() should be called
 * periodically.
 *
 * The functions below provide this module with a way for managing these
 * callbacks in behalf of the application.
 */
typedef void (*roadmap_gps_link_control) (RoadMapIO *io);
typedef void (*roadmap_gps_periodic_control) (RoadMapCallback handler);

void roadmap_gps_register_link_control
                 (roadmap_gps_link_control add,
                  roadmap_gps_link_control remove);

void roadmap_gps_register_periodic_control
                 (roadmap_gps_periodic_control add,
                  roadmap_gps_periodic_control remove);


/* The logger is a function to be called each time data has been
 * received from the GPS link (good or bad). Its should record the data
 * for later analysis or replay.
 */
typedef void (*roadmap_gps_logger)   (const char *sentence);

void roadmap_gps_register_logger (roadmap_gps_logger logger);

void roadmap_gps_nmea (void);
void roadmap_gps_open   (void);
void roadmap_gps_input  (RoadMapIO *io);
int  roadmap_gps_active (void);

void roadmap_gps_device_inactive(void);

int  roadmap_gps_estimated_error (void);
int  roadmap_gps_speed_accuracy  (void);

int  roadmap_gps_is_nmea (void);

void roadmap_gps_shutdown (void);

void roadmap_gps_detect_receiver (void);

/* These are no longer static because win32/roadmap_gps_detect needs them */
#include "roadmap_config.h"

extern RoadMapConfigDescriptor RoadMapConfigGPSSource;
extern int    RoadMapGpsRetryPending;
extern int    RoadMapGpsReception;
extern roadmap_gps_periodic_control RoadMapGpsPeriodicAdd;
extern roadmap_gps_periodic_control RoadMapGpsPeriodicRemove;

extern void roadmap_gps_navigation (char status,
                                    int gmt_time,
                                    int latitude,
                                    int longitude,
                                    int altitude,   // "preferred" units
                                    int speed,      // knots
                                    int steering);
void roadmap_gps_satellites  (int sequence,
                                     int id,
                                     int elevation,
                                     int azimuth,
                                     int strength,
                                     int active);
void roadmap_gps_dilution (int dimension,
                                  double position,
                                  double horizontal,
                                  double vertical);
#endif // INCLUDED__ROADMAP_GPS__H

