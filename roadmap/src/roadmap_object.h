/* roadmap_object.h - manage the roadmap moving objects.
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
 * DESCRIPTION:
 *
 *   This module manages a dynamic list of objects to be displayed on the map.
 *   The objects are usually imported from external applications for which
 *   a RoadMap driver has been installed (see roadmap_driver.c).
 *
 *   These objects are dynamic and not persistent (i.e. these are not points
 *   of interest).
 *
 *   It is possible for a module to register a listener function for
 *   a specific object, i.e. to get a listener function to be called
 *   each time the object has moved. The registration will fail (return
 *   NULL) if the object could not be found. A valid listener address is
 *   always returned if the registration is successful.
 *
 *   It is also possible to monitor the creation and deletion of objects.
 *   Combined with the listener registration, this makes it possible for
 *   a module to listen to a specific object when it is created, or register
 *   again when it is deleted and then re-created.
 *
 *   Listeners and monitors must be linked to each other in a daisy-chain
 *   fashion, i.e. each listener (monitor) must call the listener (monitor)
 *   that was declared before itself.
 *
 *   This module is self-initializing and can be used at any time during
 *   the initialization of RoadMap.
 */

#ifndef INCLUDE__ROADMAP_OBJECT__H
#define INCLUDE__ROADMAP_OBJECT__H

#include "roadmap_canvas.h"
#include "roadmap_string.h"
#include "roadmap_gps.h"


void roadmap_object_add_sprite (RoadMapDynamicString origin,
                                RoadMapDynamicString id,
                                RoadMapDynamicString name,
                                RoadMapDynamicString sprite,
                                RoadMapDynamicString color);

void roadmap_object_add_polygon (RoadMapDynamicString  origin,
                                 RoadMapDynamicString  id,
                                 RoadMapDynamicString  name,
                                 RoadMapDynamicString  color,
				 int                   count,
				 const RoadMapPosition edge[]);

void roadmap_object_add_circle (RoadMapDynamicString   origin,
                                RoadMapDynamicString   id,
                                RoadMapDynamicString   name,
                                RoadMapDynamicString   color,
				const RoadMapPosition *center,
				int                    radius);

void roadmap_object_move (RoadMapDynamicString id,
                          const RoadMapGpsPosition *position);

void roadmap_object_color (RoadMapDynamicString id, RoadMapDynamicString color);

void roadmap_object_remove (RoadMapDynamicString id);

void roadmap_object_cleanup (RoadMapDynamicString origin);


typedef void (*RoadMapSpriteAction) (const char               *name,
                                     const char               *sprite,
                                     RoadMapPen                pen,
                                     const RoadMapGpsPosition *gps_position);

void roadmap_object_iterate_sprite (RoadMapSpriteAction action);


typedef void (*RoadMapPolygonAction) (const char            *name,
                                      RoadMapPen             pen,
				      int                    count,
				      const RoadMapPosition *edges,
                                      const RoadMapArea     *area);

void roadmap_object_iterate_polygon (RoadMapPolygonAction action);


typedef void (*RoadMapCircleAction) (const char            *name,
                                     RoadMapPen             pen,
				     int                    radius,
                                     const RoadMapPosition *center);

void roadmap_object_iterate_circle (RoadMapCircleAction action);


typedef void (*RoadMapObjectListener) (RoadMapDynamicString id,
                                       const RoadMapGpsPosition *position);

RoadMapObjectListener roadmap_object_register_listener
                           (RoadMapDynamicString id,
                            RoadMapObjectListener listener);


typedef void (*RoadMapObjectMonitor) (RoadMapDynamicString id);

RoadMapObjectMonitor roadmap_object_register_monitor
                           (RoadMapObjectMonitor monitor);

#endif // INCLUDE__ROADMAP_OBJECT__H

