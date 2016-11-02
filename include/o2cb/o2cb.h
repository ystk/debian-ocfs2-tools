/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * o2cb.h
 *
 * Routines for accessing the o2cb configuration.
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
 */

#ifndef _O2CB_H
#define _O2CB_H

#ifndef _XOPEN_SOURCE
# define _XOPEN_SOURCE 600
#endif
#ifndef _LARGEFILE64_SOURCE
# define _LARGEFILE64_SOURCE
#endif

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

#include <linux/types.h>

#include <et/com_err.h>

#include <ocfs2-kernel/sparse_endian_types.h>

#include <o2cb/o2cb_err.h>
#include <o2cb/ocfs2_nodemanager.h>
#include <o2cb/ocfs2_heartbeat.h>
#include <ocfs2-kernel/ocfs2_fs.h>

#define OCFS2_FS_NAME			"ocfs2"

/* Classic (historically speaking) cluster stack */
#define OCFS2_CLASSIC_CLUSTER_STACK	"o2cb"

#define OCFS2_PCMK_CLUSTER_STACK	"pcmk"
#define OCFS2_CMAN_CLUSTER_STACK	"cman"

static inline int o2cb_valid_stack_name(char *name)
{
	return !strcmp(name, OCFS2_CLASSIC_CLUSTER_STACK) ||
		!strcmp(name, OCFS2_PCMK_CLUSTER_STACK) ||
		!strcmp(name, OCFS2_CMAN_CLUSTER_STACK);
}

static inline int o2cb_valid_cluster_name(char *name)
{
	unsigned len;

	if (!name)
		return 0;

	len = strlen(name);
	if (len == 0 || len > OCFS2_CLUSTER_NAME_LEN)
		return 0;

	return 1;
}

static inline int o2cb_valid_o2cb_cluster_name(char *name)
{
	int len;

	if (!name)
		return 0;

	len = strlen(name);
	if (!len)
		return 0;

	while(isalnum(*name++) && len--);

	if (len)
		return 0;

	return 1;
}

#define O2CB_GLOBAL_HEARTBEAT_TAG	"global"
#define O2CB_LOCAL_HEARTBEAT_TAG	"local"

static inline int o2cb_valid_heartbeat_mode(char *mode)
{
	return !strcmp(mode, O2CB_GLOBAL_HEARTBEAT_TAG) ||
		!strcmp(mode, O2CB_LOCAL_HEARTBEAT_TAG);
}

errcode_t o2cb_init(void);

errcode_t o2cb_get_stack_name(const char **name);
errcode_t o2cb_create_cluster(const char *cluster_name);
errcode_t o2cb_remove_cluster(const char *cluster_name);

errcode_t o2cb_add_node(const char *cluster_name,
			const char *node_name, const char *node_num,
			const char *ip_address, const char *ip_port,
			const char *local);
errcode_t o2cb_del_node(const char *cluster_name, const char *node_name);

errcode_t o2cb_list_clusters(char ***clusters);
void o2cb_free_cluster_list(char **clusters);

errcode_t o2cb_list_nodes(char *cluster_name, char ***nodes);
void o2cb_free_nodes_list(char **nodes);

errcode_t o2cb_list_hb_regions(char *cluster_name, char ***regions);
void o2cb_free_hb_regions_list(char **regions);

errcode_t o2cb_global_heartbeat_mode(char *cluster_name, int *global);
errcode_t o2cb_set_heartbeat_mode(char *cluster_name, char *mode);

errcode_t o2cb_control_daemon_debug(char **debug);

struct o2cb_cluster_desc {
	char *c_stack;		/* The cluster stack, NULL for classic */
	char *c_cluster;	/* The name of the cluster, NULL for the
				   default cluster, which is only valid in
				   the classic stack.  */
	uint8_t c_flags;
};

struct o2cb_region_desc {
	char		*r_name;	/* The uuid of the region */
	char		*r_device_name; /* The device the region is on */
	char		*r_service;	/* A program or mountpoint */
	int		r_block_bytes;
	uint64_t	r_start_block;
	uint64_t	r_blocks;
	int		r_persist;	/* Persist past process exit */
};

/* Expected use case for the region descriptor is to allocate it on
 * the stack and completely fill it before calling
 * begin_group_join().  Regular programs (not mount.ocfs2) should provide
 * a mountpoint that does not begin with a '/'.  Eg, fsck could use "fsck"
 */
errcode_t o2cb_start_heartbeat(struct o2cb_cluster_desc *cluster,
			       struct o2cb_region_desc *region);

errcode_t o2cb_stop_heartbeat(struct o2cb_cluster_desc *cluster,
			      struct o2cb_region_desc *region);

errcode_t o2cb_begin_group_join(struct o2cb_cluster_desc *cluster,
				struct o2cb_region_desc *region);
errcode_t o2cb_complete_group_join(struct o2cb_cluster_desc *cluster,
				   struct o2cb_region_desc *region,
				   int result);
errcode_t o2cb_group_leave(struct o2cb_cluster_desc *cluster,
			   struct o2cb_region_desc *region);

errcode_t o2cb_get_hb_thread_pid (const char *cluster_name, 
				  const char *region_name, 
				  pid_t *pid);

errcode_t o2cb_get_region_ref(const char *region_name,
			      int undo);
errcode_t o2cb_put_region_ref(const char *region_name,
			      int undo);
errcode_t o2cb_num_region_refs(const char *region_name,
			       int *num_refs);

errcode_t o2cb_get_node_num(const char *cluster_name,
			    const char *node_name,
			    uint16_t *node_num);
errcode_t o2cb_get_node_port(const char *cluster_name,
			     const char *node_name,
			     uint32_t *ip_port);
errcode_t o2cb_get_node_ip_string(const char *cluster_name,
				  const char *node_name,
				  char *ip_address, int count);
errcode_t o2cb_get_node_local(const char *cluster_name,
			      const char *node_name,
			      uint32_t *local);

void o2cb_free_cluster_desc(struct o2cb_cluster_desc *cluster);
errcode_t o2cb_running_cluster_desc(struct o2cb_cluster_desc *cluster);

struct ocfs2_protocol_version {
	uint8_t	pv_major;
	uint8_t pv_minor;
};
errcode_t o2cb_get_max_locking_protocol(struct ocfs2_protocol_version *proto);
errcode_t o2cb_control_open(unsigned int this_node,
			    struct ocfs2_protocol_version *proto);
void o2cb_control_close(void);
errcode_t o2cb_control_node_down(const char *uuid, unsigned int nodeid);

errcode_t o2cb_get_hb_ctl_path(char *buf, int count);
errcode_t o2cb_setup_stack(char *stack_name);

#endif  /* _O2CB_H */
