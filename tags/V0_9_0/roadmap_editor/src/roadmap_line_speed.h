/* roadmap_line_speed.h - Manage the line speed data.
 *
 * LICENSE:
 *
 *   Copyright 2006 Ehud Shabtai
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

#ifndef _ROADMAP_LINE_SPEED__H_
#define _ROADMAP_LINE_SPEED__H_

#include <time.h>
#include "roadmap_types.h"
#include "roadmap_dbread.h"
#include "roadmap_db_line_speed.h"

extern roadmap_db_handler RoadMapLineSpeedHandler;
int roadmap_line_speed_get_avg (int speed_ref);
int roadmap_line_speed_get (int speed_ref, int time_slot);

#endif // _ROADMAP_LINE_SPEED__H_
