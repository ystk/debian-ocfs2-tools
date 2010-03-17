/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * extent_map.h
 *
 * Internal extent map structures for the OCFS2 userspace library.
 *
 * Copyright (C) 2004 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License, version 2,  as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 *
 * Authors: Joel Becker
 */

#ifndef _EXTENT_MAP_H
#define _EXTENT_MAP_H

#include "ocfs2/kernel-rbtree.h"

typedef struct _ocfs2_extent_map_entry ocfs2_extent_map_entry;

struct _ocfs2_extent_map {
	struct rb_root em_extents;
	uint32_t em_clusters;
};

struct _ocfs2_extent_map_entry {
	struct rb_node e_node;
	int e_tree_depth;
	struct ocfs2_extent_rec e_rec;
};

#endif  /* _EXTENT_MAP_H */
