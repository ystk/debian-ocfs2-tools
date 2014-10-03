/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
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
 * --
 *
 * o2fsck_check_extents(), the only function this file exports, is called
 * by pass 0 to verify the extent trees that hang off of inodes.
 *
 * XXX
 * 	test reasonable/dup block references
 * 	fix up the i_ fields that depend on the extent trees
 *
 *	this could be much more clever when it finds an extent block that is
 *	self-consistent but in a crazy part of the file system that would lead
 *	us to not trust it.  it should first try and follow the suballoc bits
 *	back to a possibly orphaned desc.  failing that it should record the
 *	reference/block and continue on.  once it has parsed the rest of eb and
 *	fleshed out the bits of the extent block allocators it could go back
 *	and allocate a new block and copy the orphan into it.
 */
#include <string.h>
#include <inttypes.h>

#include "ocfs2/ocfs2.h"

#include "extent.h"
#include "fsck.h"
#include "problem.h"
#include "util.h"
#include "refcount.h"

static const char *whoami = "extent.c";

static errcode_t check_eb(o2fsck_state *ost, struct extent_info *ei,
			  uint64_t owner, uint64_t blkno,
			  int *is_valid)
{
	int changed = 0;
	char *buf = NULL;
	struct ocfs2_extent_block *eb;
	errcode_t ret;

	/* XXX test that the block isn't already used */

	/* we only consider an extent block invalid if we were able to read 
	 * it and it didn't have a extent block signature */ 
	*is_valid = 1;

	ret = ocfs2_malloc_block(ost->ost_fs->fs_io, &buf);
	if (ret) {
		com_err(whoami, ret, "while allocating a block-sized buffer "
			"for an extent block");
		goto out;
	}

	ret = ocfs2_read_extent_block_nocheck(ost->ost_fs, blkno, buf);
	if (ret) {
		com_err(whoami, ret, "reading extent block at %"PRIu64" in "
			"owner %"PRIu64" for verification", blkno, owner);
		if (ret == OCFS2_ET_BAD_EXTENT_BLOCK_MAGIC)
			*is_valid = 0;
		goto out;
	}

	eb = (struct ocfs2_extent_block *)buf;

	if (eb->h_blkno != blkno &&
	    prompt(ost, PY, PR_EB_BLKNO,
		   "An extent block at %"PRIu64" in owner %"PRIu64" "
		   "claims to be located at block %"PRIu64".  Update the "
		   "extent block's location?", blkno, owner,
		   (uint64_t)eb->h_blkno)) {
		eb->h_blkno = blkno;
		changed = 1;
	}

	if (eb->h_fs_generation != ost->ost_fs_generation) {
		if (prompt(ost, PY, PR_EB_GEN,
			   "An extent block at %"PRIu64" in owner "
			   "%"PRIu64" has a generation of %x which doesn't "
			   "match the volume's generation of %x.  Consider "
			   "this extent block invalid?", blkno, owner,
			   eb->h_fs_generation,
			   ost->ost_fs_generation)) {

			*is_valid = 0;
			goto out;
		}
		if (prompt(ost, PY, PR_EB_GEN_FIX,
			   "Update the extent block's generation to match the "
			   "volume?")) {

			eb->h_fs_generation = ost->ost_fs_generation;
			changed = 1;
		}
	}

	/* XXX worry about suballoc node/bit */
	/* XXX worry about next_leaf_blk */

	check_el(ost, ei, owner, &eb->h_list,
		 ocfs2_extent_recs_per_eb(ost->ost_fs->fs_blocksize), 
		 &changed);

	if (changed) {
		ret = ocfs2_write_extent_block(ost->ost_fs, blkno, buf);
		if (ret) {
			com_err(whoami, ret, "while writing an updated extent "
				"block at %"PRIu64" for owner %"PRIu64,
				blkno, owner);
			goto out;
		}
	}

out:
	if (buf)
		ocfs2_free(&buf);
	return 0;
}

/* the caller will check if er->e_blkno is out of range to determine if it
 * should try removing the record */
static errcode_t check_er(o2fsck_state *ost, struct extent_info *ei,
			  uint64_t owner,
			  struct ocfs2_extent_list *el,
			  struct ocfs2_extent_rec *er, int *changed)
{
	errcode_t ret = 0;
	uint32_t clusters;

	clusters = ocfs2_rec_clusters(el->l_tree_depth, er);
	verbosef("cpos %u clusters %u blkno %"PRIu64"\n", er->e_cpos,
		 clusters, (uint64_t)er->e_blkno);

	if (ocfs2_block_out_of_range(ost->ost_fs, er->e_blkno))
		goto out;

	if (el->l_tree_depth) {
		int is_valid = 0;
		/* we only expect a given depth when we descend to extent blocks
		 * from a previous depth.  these start at 0 when the inode
		 * is checked */
		ei->ei_expect_depth = 1;
		ei->ei_expected_depth = el->l_tree_depth - 1;
		check_eb(ost, ei, owner, er->e_blkno, &is_valid);
		if (!is_valid && 
		    prompt(ost, PY, PR_EXTENT_EB_INVALID,
			   "The extent record for cluster offset "
			   "%"PRIu32" in owner %"PRIu64" refers to an invalid "
			   "extent block at %"PRIu64".  Clear the reference "
			   "to this invalid block?", er->e_cpos, owner,
			   (uint64_t)er->e_blkno)) {

			er->e_blkno = 0;
			*changed = 1;
		}
		ret = 0;
		goto out;
	}

	if (ei->chk_rec_func)
		ret = ei->chk_rec_func(ost, owner, el, er, changed, ei->para);
	/* XXX offer to remove leaf records with er_clusters set to 0? */

	/* XXX check that the blocks that are referenced aren't already 
	 * used */

out:
	return ret;
}

errcode_t check_el(o2fsck_state *ost, struct extent_info *ei,
		   uint64_t owner,
		   struct ocfs2_extent_list *el,
		   uint16_t max_recs, int *changed)
{
	int trust_next_free = 1;
	struct ocfs2_extent_rec *er;
	uint64_t max_size;
	uint16_t i;
	uint32_t clusters;
	size_t cpy;

	verbosef("depth %u count %u next_free %u\n", el->l_tree_depth,
		 el->l_count, el->l_next_free_rec);

	if (ei->ei_expect_depth && 
	    el->l_tree_depth != ei->ei_expected_depth &&
	    prompt(ost, PY, PR_EXTENT_LIST_DEPTH,
		   "Extent list in owner %"PRIu64" is recorded as "
		   "being at depth %u but we expect it to be at depth %u. "
		   "update the list?", owner, el->l_tree_depth,
		   ei->ei_expected_depth)) {

		el->l_tree_depth = ei->ei_expected_depth;
		*changed = 1;
	}

	if (el->l_count > max_recs &&
	    prompt(ost, PY, PR_EXTENT_LIST_COUNT,
		   "Extent list in owner %"PRIu64" claims to have %u "
		   "records, but the maximum is %u. Fix the list's count?",
		   owner, el->l_count, max_recs)) {

		el->l_count = max_recs;
		*changed = 1;
	}

	if (max_recs > el->l_count)
		max_recs = el->l_count;

	if (el->l_next_free_rec > max_recs) {
		if (prompt(ost, PY, PR_EXTENT_LIST_FREE,
			   "Extent list in owner %"PRIu64" claims %u "
			   "as the next free chain record, but fsck believes "
			   "the largest valid value is %u.  Clamp the next "
			   "record value?", owner,
			   el->l_next_free_rec, max_recs)) {

			el->l_next_free_rec = el->l_count;
			*changed = 1;
		} else {
			trust_next_free = 0;
		}
	}

	if (trust_next_free)
		max_recs = el->l_next_free_rec;

	for (i = 0; i < max_recs; i++) {
		er = &el->l_recs[i];
		clusters = ocfs2_rec_clusters(el->l_tree_depth, er);

		/*
		 * For a sparse file, we may find an empty record
		 * in the left most record. Just skip it.
		 */
		if ((OCFS2_RAW_SB(ost->ost_fs->fs_super)->s_feature_incompat
		     & OCFS2_FEATURE_INCOMPAT_SPARSE_ALLOC) &&
		    el->l_tree_depth && !i && !clusters)
			continue;

		/* returns immediately if blkno is out of range.
		 * descends into eb.  checks that data er doesn't
		 * reference past the volume or anything crazy. */
		check_er(ost, ei, owner, el, er, changed);

		/* offer to remove records that point to nowhere */
		if (ocfs2_block_out_of_range(ost->ost_fs, er->e_blkno) && 
		    prompt(ost, PY, PR_EXTENT_BLKNO_RANGE,
			   "Extent record %u in owner %"PRIu64" "
			   "refers to a block that is out of range.  Remove "
			   "this record from the extent list?", i, owner)) {

			if (!trust_next_free) {
				printf("Can't remove the record becuase "
				       "next_free_rec hasn't been fixed\n");
				continue;
			}
			cpy = (max_recs - i - 1) * sizeof(*er);
			/* shift the remaining recs into this ones place */
			if (cpy != 0) {
				memcpy(er, er + 1, cpy);
				memset(&el->l_recs[max_recs - 1], 0, 
				       sizeof(*er));
				i--;
			}
			el->l_next_free_rec--;
			max_recs--;
			*changed = 1;
			continue;
		}


		/* we've already accounted for the extent block as part of
		 * the extent block chain groups */
		if (el->l_tree_depth)
			continue;

		/* mark the data clusters as used */
		if (ei->mark_rec_alloc_func)
			ei->mark_rec_alloc_func(ost, er, clusters, ei->para);

		ei->ei_clusters += clusters;

		max_size = (er->e_cpos + clusters) <<
			   OCFS2_RAW_SB(ost->ost_fs->fs_super)->s_clustersize_bits;
		if (max_size > ei->ei_max_size)
			ei->ei_max_size = max_size;
	}

	return 0;
}

errcode_t o2fsck_check_extent_rec(o2fsck_state *ost,
				  uint64_t owner,
				  struct ocfs2_extent_list *el,
				  struct ocfs2_extent_rec *er,
				  int *changed,
				  void *para)
{
	uint32_t clusters, last_cluster;
	uint64_t first_block;
	struct ocfs2_super_block *sb = OCFS2_RAW_SB(ost->ost_fs->fs_super);
	struct ocfs2_dinode *di = para;

	clusters = ocfs2_rec_clusters(el->l_tree_depth, er);
	first_block = ocfs2_blocks_to_clusters(ost->ost_fs, er->e_blkno);
	first_block = ocfs2_clusters_to_blocks(ost->ost_fs, first_block);

	if (first_block != er->e_blkno &&
	    prompt(ost, PY, PR_EXTENT_BLKNO_UNALIGNED,
		   "The extent record for cluster offset %"PRIu32" "
		   "in owner %"PRIu64" refers to block %"PRIu64" which isn't "
		   "aligned with the start of a cluster.  Point the extent "
		   "record at block %"PRIu64" which starts this cluster?",
		   er->e_cpos, owner,
		   (uint64_t)er->e_blkno, first_block)) {

		er->e_blkno = first_block;
		*changed = 1;
	}

	/* imagine blkno 0, 1 er_clusters.  last_cluster is 1 and
	 * fs_clusters is 1, which is ok.. */
	last_cluster = ocfs2_blocks_to_clusters(ost->ost_fs, er->e_blkno) +
		       clusters;

	if (last_cluster > ost->ost_fs->fs_clusters &&
	    prompt(ost, PY, PR_EXTENT_CLUSTERS_OVERRUN,
		   "The extent record for cluster offset %"PRIu32" "
		   "in inode %"PRIu64" refers to an extent that goes beyond "
		   "the end of the volume.  Truncate the extent by %"PRIu32" "
		   "clusters to fit it in the volume?", er->e_cpos, owner,
		   last_cluster - ost->ost_fs->fs_clusters)) {

		clusters -= last_cluster - ost->ost_fs->fs_clusters;
		ocfs2_set_rec_clusters(el->l_tree_depth, er, clusters);
		*changed = 1;
	}

	if (!ocfs2_writes_unwritten_extents(sb) &&
	    (er->e_flags & OCFS2_EXT_UNWRITTEN) &&
	    prompt(ost, PY, PR_EXTENT_MARKED_UNWRITTEN,
		   "The extent record for cluster offset %"PRIu32" "
		   "in owner %"PRIu64" has the UNWRITTEN flag set, but "
		   "this filesystem does not support unwritten extents.  "
		   "Clear the UNWRITTEN flag?", er->e_cpos,
		   (uint64_t)di->i_blkno)) {
		er->e_flags &= ~OCFS2_EXT_UNWRITTEN;
		*changed = 1;
	}

	if (((!ocfs2_refcount_tree(sb)) ||
	     !(di->i_dyn_features & OCFS2_HAS_REFCOUNT_FL)) &&
	    (er->e_flags & OCFS2_EXT_REFCOUNTED) &&
	    prompt(ost, PY, PR_EXTENT_MARKED_REFCOUNTED,
		   "The extent record for cluster offset %"PRIu32" at block "
		   "%"PRIu64" in inode %"PRIu64" has the REFCOUNTED flag set, "
		   "while it shouldn't have that flag. "
		   "Clear the REFCOUNTED flag?", er->e_cpos, er->e_blkno,
		   (uint64_t)di->i_blkno)) {
		er->e_flags &= ~OCFS2_EXT_REFCOUNTED;
		*changed = 1;
	}

	return 0;
}

errcode_t o2fsck_mark_tree_clusters_allocated(o2fsck_state *ost,
					      struct ocfs2_extent_rec *rec,
					      uint32_t clusters,
					      void *para)
{
	struct ocfs2_dinode *di = para;
	errcode_t ret = 0;

	if (rec->e_flags & OCFS2_EXT_REFCOUNTED)
		ret = o2fsck_mark_clusters_refcounted(ost,
						      di->i_refcount_loc,
						      di->i_blkno,
			ocfs2_blocks_to_clusters(ost->ost_fs, rec->e_blkno),
			clusters, rec->e_cpos);
	else
		o2fsck_mark_clusters_allocated(ost,
			ocfs2_blocks_to_clusters(ost->ost_fs, rec->e_blkno),
			clusters);

	return ret;
}

errcode_t o2fsck_check_extents(o2fsck_state *ost,
			       struct ocfs2_dinode *di)
{
	errcode_t ret;
	struct extent_info ei = {0, };
	int changed = 0;

	ei.chk_rec_func = o2fsck_check_extent_rec;
	ei.mark_rec_alloc_func = o2fsck_mark_tree_clusters_allocated;
	ei.para = di;
	ret = check_el(ost, &ei, di->i_blkno, &di->id2.i_list,
	         ocfs2_extent_recs_per_inode(ost->ost_fs->fs_blocksize),
		 &changed);

	if (changed)
		o2fsck_write_inode(ost, di->i_blkno, di);

	return ret;
}
