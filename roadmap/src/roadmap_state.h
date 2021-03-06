/* roadmap_state.h - manage object states.
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

#ifndef INCLUDE__ROADMAP_STATE__H
#define INCLUDE__ROADMAP_STATE__H

typedef int (*RoadMapStateFn) (void);

void roadmap_state_add (const char *name, RoadMapStateFn state_fn);
RoadMapStateFn roadmap_state_find (const char *name);

/* 10 bits of the int returned from a RoadMapStateFn are used for
 * actual state (e.g.  to determine which, if any, sprite or icon
 * to display) and the upper bits are used to give an angle
 * relative to north.
 */
#define ROADMAP_STATE_BITS 10
#define ROADMAP_STATE_MASK ((1 << ROADMAP_STATE_BITS) - 1)
#define ROADMAP_STATE_ANGLE_MASK (~ROADMAP_STATE_MASK)

#define ROADMAP_STATE_ENCODE_STATE(angle, state) \
    (((angle) << ROADMAP_STATE_BITS) | (state))

#define ROADMAP_STATE_DECODE_ANGLE(state) \
    (((state) & ROADMAP_STATE_ANGLE_MASK ) >> ROADMAP_STATE_BITS)

#define ROADMAP_STATE_DECODE_STATE(state) \
    ((state) & ROADMAP_STATE_MASK)

void roadmap_state_monitor (RoadMapCallback monitor);
void roadmap_state_refresh (void);

#endif // INCLUDE__ROADMAP_STATE__H

