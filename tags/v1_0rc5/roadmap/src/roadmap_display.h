/* roadmap_display.h - Manage screen signs.
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
 */

#ifndef INCLUDE__ROADMAP_DISPLAY__H
#define INCLUDE__ROADMAP_DISPLAY__H

#include "roadmap_canvas.h"

void roadmap_display_initialize (void);

int roadmap_display_activate
        (const char *title, int line, const RoadMapPosition *position);

void roadmap_display_hide (const char *title);

void roadmap_display_console    (void);
void roadmap_display_signs      (void);

const char *roadmap_display_get_id (const char *title);

#endif // INCLUDE__ROADMAP_DISPLAY__H
