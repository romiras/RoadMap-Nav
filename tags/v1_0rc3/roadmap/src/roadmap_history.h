/* roadmap_history.h - manage the roadmap address history.
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

#ifndef INCLUDE__ROADMAP_HISTORY__H
#define INCLUDE__ROADMAP_HISTORY__H

void roadmap_history_initialize (void);
void roadmap_history_load (void);

void roadmap_history_add
        (char *number, char *street, char *city, char *state);

void *roadmap_history_get_latest
         (char **number, char **street, char **city, char **state);

void *roadmap_history_get_before
         (void *reference,
          char **number, char **street, char **city, char **state);

void *roadmap_history_get_after
         (void *reference,
          char **number, char **street, char **city, char **state);

void roadmap_history_purge (int count);

void roadmap_history_save (void);

#endif // INCLUDE__ROADMAP_HISTORY__H
