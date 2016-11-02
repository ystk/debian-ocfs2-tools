/*
 * chain.c
 *
 * chain group corruptions
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

static void mess_up_sys_file(ocfs2_filesys *fs, uint64_t blkno,
				enum fsck_type type)
{
	errcode_t ret;
	char *buf = NULL, *bufgroup = NULL;
	struct ocfs2_dinode *di;
	struct ocfs2_chain_list *cl;
	struct ocfs2_chain_rec *cr;
	uint64_t oldblkno;
	struct ocfs2_group_desc *bg = NULL;

	ret = ocfs2_malloc_block(fs->fs_io, &buf);
	if (ret)
		FSWRK_COM_FATAL(progname, ret);

	ret = ocfs2_read_inode(fs, blkno, buf);
	if (ret)
		FSWRK_COM_FATAL(progname, ret);

	di = (struct ocfs2_dinode *)buf;

	if (!(di->i_flags & OCFS2_BITMAP_FL))
		FSWRK_COM_FATAL(progname, ret);

	if (!(di->i_flags & OCFS2_CHAIN_FL))
		FSWRK_COM_FATAL(progname, ret);

	cl = &(di->id2.i_chain);

	/* for CHAIN_EMPTY, CHAIN_HEAD_LINK_RANGE, CHAIN_LINK_RANGE,
	 * CHAIN_BITS, CHAIN_LINK_GEN, CHAIN_LINK_MAGIC,
	 * we need to corrupt some chain rec, so check it first.
	 */
	if (type == CHAIN_EMPTY || type == CHAIN_HEAD_LINK_RANGE ||
		type == CHAIN_LINK_RANGE || type == CHAIN_BITS 	||
		type == CHAIN_LINK_GEN	 || type == CHAIN_LINK_MAGIC)
		if (!cl->cl_next_free_rec) {
			FSWRK_WARN("No chain record found at block#%"PRIu64
				",so can't corrupt it for type[%d].\n",
				 blkno, type);
			goto bail;
		}

	switch (type) {
	case CHAIN_COUNT:
		fprintf(stdout, "Corrupt CHAIN_COUNT: "
			"Modified cl_count "
			"in block#%"PRIu64" from %u to %u\n",  blkno,
			cl->cl_count, (cl->cl_count + 100));
		cl->cl_count += 100;
		break;
	case CHAIN_NEXT_FREE:
		fprintf(stdout, "Corrupt CHAIN_NEXT_FREE:"
			" Modified cl_next_free_rec "
			"in block#%"PRIu64" from %u to %u\n", blkno,
			cl->cl_next_free_rec, (cl->cl_count + 10));
		cl->cl_next_free_rec = cl->cl_count + 10;
		break;
	case CHAIN_EMPTY:
		cr = cl->cl_recs;
		fprintf(stdout, "Corrupt CHAIN_EMPTY:"
			" Modified e_blkno "
			"in block#%"PRIu64" from %"PRIu64" to 0\n",
			 blkno,	cr->c_blkno);
		cr->c_blkno = 0;
		break;
	case CHAIN_I_CLUSTERS:
		fprintf(stdout, "Corrupt CHAIN_I_CLUSTERS:"
			"change i_clusters in block#%"PRIu64" from %u to %u\n",
			 blkno, di->i_clusters, (di->i_clusters + 10));
		di->i_clusters += 10;
		break;
	case CHAIN_I_SIZE:
		fprintf(stdout, "Corrupt CHAIN_I_SIZE:"
			"change i_size "
			"in block#%"PRIu64" from %"PRIu64" to %"PRIu64"\n",
			 blkno, di->i_size, (di->i_size + 10));
		di->i_size += 10;
		break;
	case CHAIN_GROUP_BITS:
		fprintf(stdout, "Corrupt CHAIN_GROUP_BITS:"
			"change i_used of bitmap "
			"in block#%"PRIu64" from %u to %u\n", blkno, 
			di->id1.bitmap1.i_used, (di->id1.bitmap1.i_used + 10));
		di->id1.bitmap1.i_used += 10;
		break;
	case CHAIN_HEAD_LINK_RANGE:
		cr = cl->cl_recs;
		oldblkno = cr->c_blkno;
		cr->c_blkno = 
			ocfs2_clusters_to_blocks(fs, fs->fs_clusters) + 10;
		fprintf(stdout, "Corrupt CHAIN_HEAD_LINK_RANGE:"
			"change  "
			"in block#%"PRIu64" from %"PRIu64" to %"PRIu64"\n",
			 blkno, oldblkno, cr->c_blkno);
		break;
	case CHAIN_LINK_GEN:
	case CHAIN_LINK_MAGIC:
	case CHAIN_LINK_RANGE:
		ret = ocfs2_malloc_block(fs->fs_io, &bufgroup);
		if (ret)
			FSWRK_COM_FATAL(progname, ret);
		
		bg = (struct ocfs2_group_desc *)bufgroup;
		cr = cl->cl_recs;

		ret = ocfs2_read_group_desc(fs, cr->c_blkno, (char *)bg);
		if (ret)
			FSWRK_COM_FATAL(progname, ret);

		if (type == CHAIN_LINK_GEN) {
			fprintf(stdout, "Corrupt CHAIN_LINK_GEN: "
				"change generation num from %u to 0x1234\n",
				bg->bg_generation);
			bg->bg_generation = 0x1234;
		} else if (type == CHAIN_LINK_MAGIC) {
			fprintf(stdout, "Corrupt CHAIN_LINK_MAGIC: "
				"change signature to '1234'\n");
			sprintf((char *)bg->bg_signature,"1234");
		} else {
			oldblkno = bg->bg_next_group;
			bg->bg_next_group = 
			    ocfs2_clusters_to_blocks(fs, fs->fs_clusters) + 10;
			fprintf(stdout, "Corrupt CHAIN_LINK_RANGE: "
				"change next group from %"PRIu64" to %"PRIu64
				" \n", oldblkno, bg->bg_next_group);
		}
		
		ret = ocfs2_write_group_desc(fs, cr->c_blkno, (char *)bg);
		if (ret)
			FSWRK_COM_FATAL(progname, ret);
		break;	
	case CHAIN_BITS:
		cr = cl->cl_recs;
		fprintf(stdout, "Corrupt CHAIN_BITS:"
			"change inode#%"PRIu64" c_total from %u to %u\n",
			 blkno, cr->c_total, (cr->c_total + 10));
		cr->c_total += 10; 
		break;
	case CHAIN_CPG:
		fprintf(stdout, "Corrupt CHAIN_CPG: "
			"change cl_cpg of global_bitmap from %u to %u.\n",
			cl->cl_cpg, (cl->cl_cpg + 16));
		cl->cl_cpg += 16;
		cl->cl_next_free_rec = 1;
		break;
	default:
		FSWRK_FATAL("Unknown fsck_type[%d]\n", type);
	}

	ret = ocfs2_write_inode(fs, blkno, buf);
	if (ret)
		FSWRK_COM_FATAL(progname, ret);

bail:
	if (bufgroup)
		ocfs2_free(&bufgroup);
	if (buf)
		ocfs2_free(&buf);

	return ;
}

static void mess_up_sys_chains(ocfs2_filesys *fs, uint16_t slotnum,
			       enum fsck_type type)
{
	errcode_t ret;
	char sysfile[OCFS2_MAX_FILENAME_LEN];
	uint64_t blkno;
	struct ocfs2_super_block *sb = OCFS2_RAW_SB(fs->fs_super);
	
	if (slotnum == UINT16_MAX)
		snprintf(sysfile, sizeof(sysfile), "%s",
		ocfs2_system_inodes[GLOBAL_BITMAP_SYSTEM_INODE].si_name);
	else
		snprintf(sysfile, sizeof(sysfile),
			ocfs2_system_inodes[INODE_ALLOC_SYSTEM_INODE].si_name,
			slotnum);

	ret = ocfs2_lookup(fs, sb->s_system_dir_blkno, sysfile,
				   strlen(sysfile), NULL, &blkno);
	if (ret)
		FSWRK_COM_FATAL(progname, ret);
	
	mess_up_sys_file(fs, blkno, type);

	return ;
}

void mess_up_chains_list(ocfs2_filesys *fs, enum fsck_type type,
			 uint16_t slotnum)
{
	mess_up_sys_chains(fs, slotnum, type);
}

void mess_up_chains_rec(ocfs2_filesys *fs, enum fsck_type type,
			uint16_t slotnum)
{
	
	mess_up_sys_chains(fs, slotnum, type);
}

void mess_up_chains_inode(ocfs2_filesys *fs, enum fsck_type type,
			  uint16_t slotnum)
{
	mess_up_sys_chains(fs, slotnum, type);
}

void mess_up_chains_group(ocfs2_filesys *fs, enum fsck_type type,
			  uint16_t slotnum)
{
	mess_up_sys_chains(fs, slotnum, type);
}

void mess_up_chains_group_magic(ocfs2_filesys *fs, enum fsck_type type,
				uint16_t slotnum)
{
	mess_up_sys_chains(fs, slotnum, type);
}

void mess_up_chains_cpg(ocfs2_filesys *fs, enum fsck_type type,
			uint16_t slotnum)
{
	errcode_t ret;
	char sysfile[OCFS2_MAX_FILENAME_LEN];
	uint64_t blkno;
	struct ocfs2_super_block *sb = OCFS2_RAW_SB(fs->fs_super);

	snprintf(sysfile, sizeof(sysfile), "%s",
		 ocfs2_system_inodes[GLOBAL_BITMAP_SYSTEM_INODE].si_name);

	ret = ocfs2_lookup(fs, sb->s_system_dir_blkno, sysfile,
				strlen(sysfile), NULL, &blkno);
	if (ret)
		FSWRK_COM_FATAL(progname, ret);

	mess_up_sys_file(fs, blkno, CHAIN_CPG);

	return;
}

static void mess_up_superblock_clusters(ocfs2_filesys *fs, int excess)
{
	errcode_t ret;
	char *buf = NULL;
	struct ocfs2_dinode *di = fs->fs_super;
	uint32_t new_clusters, cpg, wrong;

	/* corrupt superblock, just copy the
	 * superblock, change it and
	 * write it back to disk.
	 */
	ret = ocfs2_malloc_block(fs->fs_io, &buf);
	if (ret)
		FSWRK_COM_FATAL(progname, ret);

	memcpy(buf, (char *)di, fs->fs_blocksize);

	di = (struct ocfs2_dinode *)buf;

	cpg = 8 * ocfs2_group_bitmap_size(fs->fs_blocksize, 0,
				OCFS2_RAW_SB(fs->fs_super)->s_feature_incompat);

	/* make the wrong value to 2.5 times of cluster_per_group. */
	wrong = cpg * 2 + cpg / 2;
	if (excess)
		new_clusters = di->i_clusters + wrong;
	else
		new_clusters = di->i_clusters - wrong;

	fprintf(stdout, "Corrupt SUPERBLOCK_CLUSTERS: "
		"change superblock i_clusters from %u to %u.\n",
		di->i_clusters, new_clusters);
	di->i_clusters = new_clusters;

	ret = io_write_block(fs->fs_io, di->i_blkno, 1, buf);
	if (ret)
		FSWRK_COM_FATAL(progname, ret);

	if(buf)
		ocfs2_free(&buf);
	return;
}

void mess_up_superblock_clusters_excess(ocfs2_filesys *fs, enum fsck_type type,
					uint16_t slotnum)
{
	mess_up_superblock_clusters(fs, 1);
}

void mess_up_superblock_clusters_lack(ocfs2_filesys *fs, enum fsck_type type,
				      uint16_t slotnum)
{
	mess_up_superblock_clusters(fs, 0);
}
