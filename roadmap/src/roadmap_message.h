/* roadmap_message.h - Format RoadMap messages.
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

#ifndef INCLUDE__ROADMAP_MESSAGE__H
#define INCLUDE__ROADMAP_MESSAGE__H

void roadmap_message_set    (int parameter, const char *format, ...);
char *roadmap_message_get (int parameter);
void roadmap_message_unset  (int parameter);

int  roadmap_message_format (char *text, int length, const char *format);

int roadmap_message_time_in_use(void);
int roadmap_message_changed(void);

#endif // INCLUDE__ROADMAP_MESSAGE__H
