/* navigate_cost.h - generic navigate cost functions
 *
 * LICENSE:
 *
 *   Copyright 2007 Ehud Shabtai
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
 */

#ifndef _NAVIGATE_COST_H_
#define _NAVIGATE_COST_H_

#define COST_FASTEST 1
#define COST_SHORTEST 2

typedef int (*NavigateCostFn) (int line_id, int is_revesred, int cur_cost,
                               int prev_line_id, int is_prev_reversed,
                               int node_id);

void navigate_cost_reset (void);
NavigateCostFn navigate_cost_get (void);

int navigate_cost_time (int line_id, int is_revesred, int cur_cost,
                        int prev_line_id, int is_prev_reversed);

void navigate_cost_initialize (void);

int navigate_cost_type (void);

#endif /* _NAVIGATE_COST_H_ */

