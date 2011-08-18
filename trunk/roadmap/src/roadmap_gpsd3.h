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
 *
 *
 * DESCRIPTION:
 *
 *   This module hides the gpsd library API (version 2).
 */
/**
 * @file
 * @brief roadmap_gpsd3.h - a module to interact with gpsd using its library.
 */

#ifndef INCLUDE__ROADMAP_GPSD3__H
#define INCLUDE__ROADMAP_GPSD3__H

#include "roadmap_gpsd2.h"

#include "roadmap_net.h"
#include "roadmap_input.h"

void roadmap_gpsd3_subscriptions(void);
void *roadmap_gpsd3_decoder_context(void);


RoadMapSocket roadmap_gpsd3_connect (const char *name);

int roadmap_gpsd3_decode (void *user_context,
                          void *decoder_context, char *sentence);

#endif // INCLUDE__ROADMAP_GPSD3__H

