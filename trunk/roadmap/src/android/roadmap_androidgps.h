/*
 * LICENSE:
 *
 *   Copyright 2010 Danny Backx
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

/*
 * @file
 * @brief define the API for AndroidGps
 */

#ifndef INCLUDE__ROADMAP_ANDROIDGPS__H
#define INCLUDE__ROADMAP_ANDROIDGPS__H

#include "roadmap_net.h"
#include "roadmap_input.h"


void roadmap_androidgps_subscribe_to_navigation (RoadMapGpsdNavigation navigation);

void roadmap_androidgps_subscribe_to_satellites (RoadMapGpsdSatellite satellite);

void roadmap_androidgps_subscribe_to_dilution   (RoadMapGpsdDilution dilution);


RoadMapSocket roadmap_androidgps_connect (const char *name);

int roadmap_androidgps_decode (void *user_context, void *decoder_context, char *sentence);

void roadmap_androidgps_periodic (void);
void roadmap_androidgps_close (void);

#endif // INCLUDE__ROADMAP_ANDROIDGPS__H

