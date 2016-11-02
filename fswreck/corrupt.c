/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * corrupt.c
 *
 * corruption routines
 *
 * Copyright (C) 2006 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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
 */

#include "main.h"

extern char *progname;

void create_named_directory(ocfs2_filesys *fs, char *dirname,
				   uint64_t *blkno)
{
	errcode_t ret;
	struct ocfs2_super_block *sb = OCFS2_RAW_SB(fs->fs_super);

	ret = ocfs2_lookup(fs, sb->s_root_blkno, dirname, strlen(dirname), NULL,
			   blkno);
	if (!ret)
		return;
	else if (ret != OCFS2_ET_FILE_NOT_FOUND)
		FSWRK_COM_FATAL(progname, ret);

	ret  = ocfs2_new_inode(fs, blkno, S_IFDIR | 0755);
	if (ret)
		FSWRK_COM_FATAL(progname, ret);

	ret = ocfs2_init_dir(fs, *blkno, fs->fs_root_blkno);
	if (ret)
		FSWRK_COM_FATAL(progname, ret);

	ret = ocfs2_link(fs, fs->fs_root_blkno, dirname, *blkno, OCFS2_FT_DIR);
	if (ret)
		FSWRK_COM_FATAL(progname, ret);

	return;
}

void corrupt_file(ocfs2_filesys *fs, enum fsck_type type, uint16_t slotnum)
{
	void (*func)(ocfs2_filesys *fs, enum fsck_type type, uint64_t blkno) = NULL;
	uint64_t blkno;

	switch (type) {
	case EB_BLKNO:
		func = mess_up_extent_block;
		break;
	case EB_GEN:
		func = mess_up_extent_block;
		break;
	case EB_GEN_FIX:
		func = mess_up_extent_block;
		break;
	case EXTENT_EB_INVALID:
		func = mess_up_extent_block;
		break;
	case EXTENT_MARKED_UNWRITTEN:
	case EXTENT_MARKED_REFCOUNTED:
	case EXTENT_BLKNO_UNALIGNED:
		func = mess_up_extent_record;
		break;
	case EXTENT_CLUSTERS_OVERRUN:
		func = mess_up_extent_record;
		break;
	case EXTENT_BLKNO_RANGE:
		func = mess_up_extent_record;
		break;
	case EXTENT_OVERLAP:
		func = mess_up_extent_record;
		break;
	case EXTENT_HOLE:
		func = mess_up_extent_record;
		break;
	case EXTENT_LIST_DEPTH:
		func = mess_up_extent_list;
		break;
	case EXTENT_LIST_COUNT:
		func = mess_up_extent_list;
		break;
	case EXTENT_LIST_FREE:
		func = mess_up_extent_list;
		break;
	case INODE_SUBALLOC:
		func = mess_up_inode_field;
		break;
	case INODE_GEN:
		func = mess_up_inode_field;
		break;
	case INODE_GEN_FIX:
		func = mess_up_inode_field;
		break;
	case INODE_BLKNO:
		func = mess_up_inode_field;
		break;
	case INODE_NZ_DTIME:
		func = mess_up_inode_field;
		break;
	case INODE_SIZE:
		func = mess_up_inode_field;
		break;
	case INODE_SPARSE_SIZE:
		func = mess_up_inode_field;
		break;
	case INODE_CLUSTERS:
		func = mess_up_inode_field;
		break;
	case INODE_SPARSE_CLUSTERS:
		func = mess_up_inode_field;
		break;
	case INODE_COUNT:
		func = mess_up_inode_field;
		break;
	case INODE_NOT_CONNECTED:
		func = mess_up_inode_not_connected;
		break;
	case LINK_FAST_DATA:
		func = mess_up_symlink;
		break;
	case LINK_NULLTERM:
		func = mess_up_symlink;
		break;
	case LINK_SIZE:
		func = mess_up_symlink;
		break;
	case LINK_BLOCKS:
		func = mess_up_symlink;
		break;
	case ROOT_NOTDIR:
		func = mess_up_root;
		break;
	case ROOT_DIR_MISSING:
		func = mess_up_root;
		break;
	case LOSTFOUND_MISSING:
		func = mess_up_root;
		break;
	case DIR_ZERO:
		func = mess_up_dir_inode;
		break;
	case DIR_HOLE:
		func = mess_up_dir_inode;
		break;
	case DIRENT_DOTTY_DUP:
		func = mess_up_dir_ent;
		break;
	case DIRENT_NOT_DOTTY:
		func = mess_up_dir_ent;
		break;
	case DIRENT_DOT_INODE:
		func = mess_up_dir_ent;
		break;
	case DIRENT_DOT_EXCESS:
		func = mess_up_dir_ent;
		break;
	case DIR_DOTDOT:
		func = mess_up_dir_ent;
		break;
	case DIRENT_ZERO:
		func = mess_up_dir_ent;
		break;
	case DIRENT_NAME_CHARS:
		func = mess_up_dir_ent;
		break;
	case DIRENT_INODE_RANGE:
		func = mess_up_dir_ent;
		break;
	case DIRENT_INODE_FREE:
		func = mess_up_dir_ent;
		break;
	case DIRENT_TYPE:
		func = mess_up_dir_ent;
		break;
	case DIRENT_DUPLICATE:
		func = mess_up_dir_ent;
		break;
	case DIRENT_LENGTH:
		func = mess_up_dir_ent;
		break;
	case DIR_PARENT_DUP:
		func = mess_up_dir_parent_dup;
		break;
	case DIR_NOT_CONNECTED:
		func = mess_up_dir_not_connected;
		break;
	case INLINE_DATA_FLAG_INVALID:
		func = mess_up_inline_flag;
		break;
	case INLINE_DATA_COUNT_INVALID:
		func = mess_up_inline_inode;
		break;
	case INODE_INLINE_SIZE:
		func = mess_up_inline_inode;
		break;
	case INODE_INLINE_CLUSTERS:
		func = mess_up_inline_inode;
		break;
	case DUP_CLUSTERS_CLONE:
	case DUP_CLUSTERS_DELETE:
	case DUP_CLUSTERS_SYSFILE_CLONE:
		func = mess_up_dup_clusters;
		break;
	case REFCOUNT_FLAG_INVALID:
		func = mess_up_inode_field;
		break;
	case REFCOUNT_LOC_INVALID:
		func = mess_up_inode_field;
		break;
	default:
		FSWRK_FATAL("Invalid code=%d", type);
	}

	create_named_directory(fs, "tmp", &blkno);

	if (func)
		func(fs, type, blkno);

	return;
}

void corrupt_sys_file(ocfs2_filesys *fs, enum fsck_type type, uint16_t slotnum)
{
	void (*func)(ocfs2_filesys *fs, enum fsck_type type, uint16_t slotnum) = NULL;

	switch (type) {
	case CHAIN_COUNT:
		func = mess_up_chains_list;
		break;
	case CHAIN_NEXT_FREE:
		func = mess_up_chains_list;
		break;
	case CHAIN_EMPTY:
		func = mess_up_chains_rec;
		break;
	case CHAIN_HEAD_LINK_RANGE:
		func = mess_up_chains_rec;
		break;
	case CHAIN_BITS:
		func = mess_up_chains_rec;
		break;
	case CHAIN_I_CLUSTERS:
		func = mess_up_chains_inode;
		break;
	case CHAIN_I_SIZE:
		func = mess_up_chains_inode;
		break;
	case CHAIN_GROUP_BITS:
		func = mess_up_chains_inode;
		break;
	case CHAIN_LINK_GEN:
		func = mess_up_chains_group;
		break;
	case CHAIN_LINK_RANGE:
		func = mess_up_chains_group;
		break;
	case CHAIN_LINK_MAGIC:
		func = mess_up_chains_group_magic;
		break;
	case CHAIN_CPG:
		func = mess_up_chains_cpg;
		break;
	case SUPERBLOCK_CLUSTERS_EXCESS:
		func = mess_up_superblock_clusters_excess;
		break;
	case SUPERBLOCK_CLUSTERS_LACK:
		func = mess_up_superblock_clusters_lack;
		break;
	case INODE_ORPHANED:
		func = mess_up_inode_orphaned;
		break;
	case INODE_ALLOC_REPAIR:
		func = mess_up_inode_alloc;
		break;
	case JOURNAL_FILE_INVALID:
	case JOURNAL_UNKNOWN_FEATURE:
	case JOURNAL_MISSING_FEATURE:
	case JOURNAL_TOO_SMALL:
		func = mess_up_journal;
		break;
	case QMAGIC_INVALID:
	case QTREE_BLK_INVALID:
	case DQBLK_INVALID:
	case DUP_DQBLK_INVALID:
	case DUP_DQBLK_VALID:
		func = mess_up_quota;
		break;
	default:
		FSWRK_FATAL("Invalid code=%d", type);
	}

	if (func)
		func(fs, type, slotnum);

	return;
}

void corrupt_group_desc(ocfs2_filesys *fs, enum fsck_type type,
			uint16_t slotnum)
{
	void (*func)(ocfs2_filesys *fs, enum fsck_type type, uint16_t slotnum) = NULL;

	switch (type) {
	case GROUP_PARENT:
		func = mess_up_group_minor;
		break;
	case GROUP_BLKNO:
		func = mess_up_group_minor;
		break;
	case GROUP_CHAIN:
		func = mess_up_group_minor;
		break;
	case GROUP_FREE_BITS:
		func = mess_up_group_minor;
		break;
	case GROUP_CHAIN_LOOP:
		func = mess_up_group_minor;
		break;
	case GROUP_GEN:
		func = mess_up_group_gen;
		break;
	case GROUP_UNEXPECTED_DESC:
		func = mess_up_group_list;
		break;
	case GROUP_EXPECTED_DESC:
		func = mess_up_group_list;
		break;
	case CLUSTER_GROUP_DESC:
		func = mess_up_cluster_group_desc;
		break;
	case CLUSTER_ALLOC_BIT:
		func = mess_up_cluster_alloc_bits;
		break;
	default:
		FSWRK_FATAL("Invalid code=%d", type);
	}

	if (func)
		func(fs, type, slotnum);

	return;
}

void corrupt_local_alloc(ocfs2_filesys *fs, enum fsck_type type,
			 uint16_t slotnum)
{
	void (*func)(ocfs2_filesys *fs, enum fsck_type type, uint16_t slotnum) = NULL;

	switch (type) {
	case LALLOC_SIZE:
		func = mess_up_local_alloc_empty;
		break;
	case LALLOC_NZ_USED:
		func = mess_up_local_alloc_empty;
		break;
	case LALLOC_NZ_BM:
		func = mess_up_local_alloc_empty;
		break;
	case LALLOC_BM_OVERRUN:
		func = mess_up_local_alloc_bitmap;
		break;
	case LALLOC_BM_STRADDLE:
		func = mess_up_local_alloc_bitmap;
		break;
	case LALLOC_BM_SIZE:
		func = mess_up_local_alloc_bitmap;
		break;
	case LALLOC_USED_OVERRUN:
		func = mess_up_local_alloc_used;
		break;
	case LALLOC_CLEAR:
		func = mess_up_local_alloc_used;
		break;
	default:
		FSWRK_FATAL("Invalid code = %d", type);
	}

	if (func)
		func(fs, type, slotnum);

	return;
}

void corrupt_truncate_log(ocfs2_filesys *fs, enum fsck_type type,
			  uint16_t slotnum)
{
	void (*func)(ocfs2_filesys *fs, enum fsck_type type, uint16_t slotnum) = NULL;

	switch (type) {
	case DEALLOC_COUNT:
		func = mess_up_truncate_log_list;
		break;
	case DEALLOC_USED:
		func = mess_up_truncate_log_list;
		break;
	case TRUNCATE_REC_START_RANGE:
		func = mess_up_truncate_log_rec;
		break;
	case TRUNCATE_REC_WRAP:
		func = mess_up_truncate_log_rec;
		break;
	case TRUNCATE_REC_RANGE:
		func = mess_up_truncate_log_rec;
		break;
	default:
		FSWRK_FATAL("Invalid code = %d", type);
	}

	if (func)
		func(fs, type, slotnum);

	return;
}

void corrupt_refcount(ocfs2_filesys *fs, enum fsck_type type, uint16_t slotnum)
{
	uint64_t blkno;
	void (*func)(ocfs2_filesys *fs,
		     enum fsck_type type, uint64_t blkno) = NULL;

	switch (type) {
	case RB_BLKNO:
	case RB_GEN:
	case RB_GEN_FIX:
	case RB_PARENT:
	case REFCOUNT_BLOCK_INVALID:
	case REFCOUNT_ROOT_BLOCK_INVALID:
	case REFCOUNT_LIST_COUNT:
	case REFCOUNT_LIST_USED:
	case REFCOUNT_CLUSTER_RANGE:
	case REFCOUNT_CLUSTER_COLLISION:
	case REFCOUNT_LIST_EMPTY:
	case REFCOUNT_REC_REDUNDANT:
	case REFCOUNT_COUNT_INVALID:
	case DUP_CLUSTERS_ADD_REFCOUNT:
		func = mess_up_refcount_tree_block;
		break;
	case REFCOUNT_CLUSTERS:
	case REFCOUNT_COUNT:
		func = mess_up_refcount_tree;
		break;
	default:
		FSWRK_FATAL("Invalid code = %d", type);
	}

	create_named_directory(fs, "tmp", &blkno);

	if (func)
		func(fs, type, blkno);

	return;
}

void corrupt_discontig_bg(ocfs2_filesys *fs, enum fsck_type type,
			  uint16_t slotnum)
{
	mess_up_discontig_bg(fs, type, slotnum);
}
