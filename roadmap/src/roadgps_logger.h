/* roadgps_logging.h - The interface for the RoadGps logging module.
 *
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

#ifndef INCLUDE__ROADGPS_LOGGER__H
#define INCLUDE__ROADGPS_LOGGER__H

void roadgps_logger_initialize (void);
void roadgps_logger_shutdown (void);

void roadgps_logger_start (void);
void roadgps_logger_stop (void);

#endif /* INCLUDE__ROADGPS_LOGGER__H */

