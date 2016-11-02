/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * o2dlm.h
 *
 * Defines the userspace locking api
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
 * Authors: Mark Fasheh <mark.fasheh@oracle.com>
 */

#ifndef _O2DLM_H_
#define _O2DLM_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>

#include <stdlib.h>

#include <et/com_err.h>

#include <o2dlm/o2dlm_err.h>

#define O2DLM_LOCK_ID_MAX_LEN       32
#define O2DLM_DOMAIN_MAX_LEN        255

/* + null pointer */
#define O2DLM_MAX_FULL_DOMAIN_PATH (PATH_MAX + 1)

/* valid lock flags */
#define O2DLM_TRYLOCK      0x01
#define O2DLM_VALID_FLAGS  (O2DLM_TRYLOCK)

/* Forward declarations */
struct o2dlm_ctxt;

/* valid lock levels */
enum o2dlm_lock_level
{
	O2DLM_LEVEL_PRMODE,
	O2DLM_LEVEL_EXMODE
};


/* Expects to be given a path to the root of a valid ocfs2_dlmfs file
 * system and a domain identifier of length <= 255 characters including
 * the '\0' */
errcode_t o2dlm_initialize(const char *dlmfs_path,
			   const char *domain_name,
			   struct o2dlm_ctxt **ctxt);

/*
 * lock_name, is a valid lock name -- 32 bytes long including the null
 * character
 *
 * Returns: 0 if we got the lock we wanted
 */
errcode_t o2dlm_lock(struct o2dlm_ctxt *ctxt,
		     const char *lockid,
		     int lockflags,
		     enum o2dlm_lock_level level);

/*
 * Like o2dlm_lock, but also registers a BAST function for this lock.  This
 * returns a file descriptor in poll_fd that can be fed to select(2) or
 * poll(2).  When there is POLLIN on the descriptor, call o2dlm_process_bast().
 */
errcode_t o2dlm_lock_with_bast(struct o2dlm_ctxt *ctxt,
			       const char *lockid,
			       int lockflags,
			       enum o2dlm_lock_level level,
			       void (*bast_func)(void *bast_arg),
			       void *bast_arg,
			       int *poll_fd);

/* returns 0 on success */
errcode_t o2dlm_unlock(struct o2dlm_ctxt *ctxt,
		       const char *lockid);

/* Remove an unlocked lock from the domain */
errcode_t o2dlm_drop_lock(struct o2dlm_ctxt *ctxt, const char *lockid);


/* Read the LVB out of a lock.
 * 'len' is the amount to read into 'lvb'
 *
 * We can only read LVB_MAX bytes out of the lock, even if you
 * specificy a len larger than that.  For classic o2dlm, LVB_MAX is
 * 64 bytes.  For fsdlm, it is 32 bytes.
 * 
 * If you want to know how much was read, then pass 'bytes_read'
 */
errcode_t o2dlm_read_lvb(struct o2dlm_ctxt *ctxt,
			 char *lockid,
			 char *lvb,
			 unsigned int len,
			 unsigned int *bytes_read);

errcode_t o2dlm_write_lvb(struct o2dlm_ctxt *ctxt,
			  char *lockid,
			  const char *lvb,
			  unsigned int len,
			  unsigned int *bytes_written);


/*
 * Call this when select(2) or poll(2) says there is data on poll_fd.  It
 * will fire off the BAST associated with poll_fd.
 */
void o2dlm_process_bast(struct o2dlm_ctxt *ctxt, int poll_fd);


/*
 * Unlocks all pending locks and frees the lock context.
 */
errcode_t o2dlm_destroy(struct o2dlm_ctxt *ctxt);

/*
 * Optional features that libo2dlm and dlmfs can support.
 */
errcode_t o2dlm_supports_bast(int *supports);
errcode_t o2dlm_supports_stackglue(int *supports);

#endif /* _O2DLM_H_ */
