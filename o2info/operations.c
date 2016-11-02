/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * operations.c
 *
 * Implementations for all o2info's operation.
 *
 * Copyright (C) 2010 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#define _XOPEN_SOURCE 600
#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE /* Because libc really doesn't want us using O_DIRECT? */

#include <errno.h>
#include <sys/raw.h>
#include <inttypes.h>

#include "ocfs2/ocfs2.h"
#include "ocfs2/bitops.h"
#include "ocfs2-kernel/ocfs2_ioctl.h"
#include "ocfs2-kernel/kernel-list.h"
#include "tools-internal/verbose.h"

#include "utils.h"
#include "libo2info.h"

extern void print_usage(int rc);
extern int cluster_coherent;

static inline void o2info_fill_request(struct ocfs2_info_request *req,
				       size_t size,
				       enum ocfs2_info_type code,
				       int flags)
{
	memset(req, 0, size);

	req->ir_magic = OCFS2_INFO_MAGIC;
	req->ir_size = size;
	req->ir_code = code,
	req->ir_flags = flags;
}

static void o2i_info(struct o2info_operation *op, const char *fmt, ...)
{
	va_list ap;

	fprintf(stdout, "%s Info: ", op->to_name);
	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);

	return;
}

static void o2i_error(struct o2info_operation *op, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s Error: ", op->to_name);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	return;
}

/*
 * Helper to scan all requests:
 *
 * - Print all errors and unknown requests.
 * - Return number of unknown requests.
 * - Return number of errors.
 * - Return number of handled requesets.
 * - Return first and last error code.
 */
static void o2i_scan_requests(struct o2info_operation *op,
			      struct ocfs2_info info, uint32_t *unknowns,
			      uint32_t *errors, uint32_t *fills)
{
	uint32_t i, num_unknown = 0, num_error = 0, num_filled = 0;
	uint64_t *reqs;
	struct ocfs2_info_request *req;

	for (i = 0; i < info.oi_count; i++) {

		reqs = (uint64_t *)info.oi_requests;
		req = (struct ocfs2_info_request *)reqs[i];
		if (req->ir_flags & OCFS2_INFO_FL_ERROR) {
			o2i_error(op, "o2info request(%d) failed.\n",
				  req->ir_code);
			num_error++;
			continue;
		}

		if (!(req->ir_flags & OCFS2_INFO_FL_FILLED)) {
			o2i_info(op, "o2info request(%d) is unsupported.\n",
				 req->ir_code);
			num_unknown++;
			continue;
		}

		num_filled++;
	}

	*unknowns = num_unknown;
	*errors = num_error;
	*fills = num_filled;
}

static int get_fs_features_ioctl(struct o2info_operation *op,
				 int fd,
				 struct o2info_fs_features *ofs)
{
	int rc = 0, flags = 0;
	uint32_t unknowns = 0, errors = 0, fills = 0;
	uint64_t reqs[1];
	struct ocfs2_info_fs_features oif;
	struct ocfs2_info info;

	memset(ofs, 0, sizeof(*ofs));

	if (!cluster_coherent)
		flags |= OCFS2_INFO_FL_NON_COHERENT;

	o2info_fill_request((struct ocfs2_info_request *)&oif, sizeof(oif),
			    OCFS2_INFO_FS_FEATURES, flags);

	reqs[0] = (unsigned long)&oif;

	info.oi_requests = (uint64_t)reqs;
	info.oi_count = 1;

	rc = ioctl(fd, OCFS2_IOC_INFO, &info);
	if (rc) {
		rc = errno;
		o2i_error(op, "ioctl failed: %s\n", strerror(rc));
		o2i_scan_requests(op, info, &unknowns, &errors, &fills);
		goto out;
	}

	if (oif.if_req.ir_flags & OCFS2_INFO_FL_FILLED) {
		ofs->compat = oif.if_compat_features;
		ofs->incompat = oif.if_incompat_features;
		ofs->rocompat = oif.if_ro_compat_features;
	}

out:
	return rc;
}

static void o2info_print_line(char const *qualifier, char *content,
			      char splitter)
{
	char *ptr = NULL, *token = NULL, *tmp = NULL;
	uint32_t max_len = 80, len = 0;

	tmp = malloc(max_len);
	ptr = content;

	snprintf(tmp, max_len, "%s", qualifier);
	fprintf(stdout, "%s", tmp);
	len += strlen(tmp);

	while (ptr) {

		token = ptr;
		ptr = strchr(ptr, splitter);

		if (ptr)
			*ptr = 0;

		if (strcmp(token, "") != 0) {
			snprintf(tmp, max_len, "%s ", token);
			len += strlen(tmp);
			if (len > max_len) {
				fprintf(stdout, "\n");
				len = 0;
				snprintf(tmp, max_len, "%s", qualifier);
				fprintf(stdout, "%s", tmp);
				len += strlen(tmp);
				snprintf(tmp, max_len, "%s ", token);
				fprintf(stdout, "%s", tmp);
				len += strlen(tmp);
			} else
				fprintf(stdout, "%s", tmp);
		}

		if (!ptr)
			break;

		ptr++;
	}

	fprintf(stdout, "\n");

	if (tmp)
		ocfs2_free(&tmp);
}

static int fs_features_run(struct o2info_operation *op,
			   struct o2info_method *om,
			   void *arg)
{
	int rc = 0;
	static struct o2info_fs_features ofs;

	char *compat = NULL;
	char *incompat = NULL;
	char *rocompat = NULL;
	char *features = NULL;

	if (om->om_method == O2INFO_USE_IOCTL)
		rc = get_fs_features_ioctl(op, om->om_fd, &ofs);
	else
		rc = o2info_get_fs_features(om->om_fs, &ofs);
	if (rc)
		goto out;

	rc = o2info_get_compat_flag(ofs.compat, &compat);
	if (rc)
		goto out;

	rc = o2info_get_incompat_flag(ofs.incompat, &incompat);
	if (rc)
		goto out;

	rc = o2info_get_rocompat_flag(ofs.rocompat, &rocompat);
	if (rc)
		goto out;

	features = malloc(strlen(compat) + strlen(incompat) +
			  strlen(rocompat) + 3);

	sprintf(features, "%s %s %s", compat, incompat, rocompat);

	o2info_print_line("", features, ' ');

out:
	if (compat)
		ocfs2_free(&compat);

	if (incompat)
		ocfs2_free(&incompat);

	if (rocompat)
		ocfs2_free(&rocompat);

	if (features)
		ocfs2_free(&features);

	return rc;
}

DEFINE_O2INFO_OP(fs_features,
		 fs_features_run,
		 NULL);

static int get_volinfo_ioctl(struct o2info_operation *op,
			     int fd,
			     struct o2info_volinfo *vf)
{
	int rc = 0, flags = 0;
	uint32_t unknowns = 0, errors = 0, fills = 0;
	struct ocfs2_info_blocksize oib;
	struct ocfs2_info_clustersize oic;
	struct ocfs2_info_maxslots oim;
	struct ocfs2_info_label oil;
	struct ocfs2_info_uuid oiu;
	uint64_t reqs[5];
	struct ocfs2_info info;

	memset(vf, 0, sizeof(*vf));

	if (!cluster_coherent)
		flags |= OCFS2_INFO_FL_NON_COHERENT;

	o2info_fill_request((struct ocfs2_info_request *)&oib, sizeof(oib),
			    OCFS2_INFO_BLOCKSIZE, flags);
	o2info_fill_request((struct ocfs2_info_request *)&oic, sizeof(oic),
			    OCFS2_INFO_CLUSTERSIZE, flags);
	o2info_fill_request((struct ocfs2_info_request *)&oim, sizeof(oim),
			    OCFS2_INFO_MAXSLOTS, flags);
	o2info_fill_request((struct ocfs2_info_request *)&oil, sizeof(oil),
			    OCFS2_INFO_LABEL, flags);
	o2info_fill_request((struct ocfs2_info_request *)&oiu, sizeof(oiu),
			    OCFS2_INFO_UUID, flags);

	reqs[0] = (unsigned long)&oib;
	reqs[1] = (unsigned long)&oic;
	reqs[2] = (unsigned long)&oim;
	reqs[3] = (unsigned long)&oil;
	reqs[4] = (unsigned long)&oiu;

	info.oi_requests = (uint64_t)reqs;
	info.oi_count = 5;

	rc = ioctl(fd, OCFS2_IOC_INFO, &info);
	if (rc) {
		rc = errno;
		o2i_error(op, "ioctl failed: %s\n", strerror(rc));
		o2i_scan_requests(op, info, &unknowns, &errors, &fills);
		goto out;
	}

	if (oib.ib_req.ir_flags & OCFS2_INFO_FL_FILLED)
		vf->blocksize = oib.ib_blocksize;

	if (oic.ic_req.ir_flags & OCFS2_INFO_FL_FILLED)
		vf->clustersize = oic.ic_clustersize;

	if (oim.im_req.ir_flags & OCFS2_INFO_FL_FILLED)
		vf->maxslots = oim.im_max_slots;

	if (oil.il_req.ir_flags & OCFS2_INFO_FL_FILLED)
		memcpy(vf->label, oil.il_label, OCFS2_MAX_VOL_LABEL_LEN);

	if (oiu.iu_req.ir_flags & OCFS2_INFO_FL_FILLED)
		memcpy(vf->uuid_str, oiu.iu_uuid_str, OCFS2_TEXT_UUID_LEN + 1);

	rc = get_fs_features_ioctl(op, fd, &(vf->ofs));

out:
	return rc;
}

static int volinfo_run(struct o2info_operation *op,
		       struct o2info_method *om,
		       void *arg)
{
	int rc = 0;
	static struct o2info_volinfo vf;

	char *compat = NULL;
	char *incompat = NULL;
	char *rocompat = NULL;
	char *features = NULL;

#define VOLINFO "       Label: %s\n" \
		"        UUID: %s\n" \
		"  Block Size: %u\n" \
		"Cluster Size: %u\n" \
		"  Node Slots: %u\n"

	if (om->om_method == O2INFO_USE_IOCTL)
		rc = get_volinfo_ioctl(op, om->om_fd, &vf);
	else
		rc = o2info_get_volinfo(om->om_fs, &vf);
	if (rc)
		goto out;

	rc = o2info_get_compat_flag(vf.ofs.compat, &compat);
	if (rc)
		goto out;

	rc = o2info_get_incompat_flag(vf.ofs.incompat, &incompat);
	if (rc)
		goto out;

	rc = o2info_get_rocompat_flag(vf.ofs.rocompat, &rocompat);
	if (rc)
		goto out;

	features = malloc(strlen(compat) + strlen(incompat) +
			  strlen(rocompat) + 3);

	sprintf(features, "%s %s %s", compat, incompat, rocompat);

	fprintf(stdout, VOLINFO, vf.label, vf.uuid_str, vf.blocksize,
		vf.clustersize, vf.maxslots);

	o2info_print_line("    Features: ", features, ' ');

out:
	if (compat)
		ocfs2_free(&compat);

	if (incompat)
		ocfs2_free(&incompat);

	if (rocompat)
		ocfs2_free(&rocompat);

	if (features)
		ocfs2_free(&features);

	return rc;
}

DEFINE_O2INFO_OP(volinfo,
		 volinfo_run,
		 NULL);

static int get_mkfs_ioctl(struct o2info_operation *op, int fd,
			  struct o2info_mkfs *oms)
{
	int rc = 0, flags = 0;
	uint32_t unknowns = 0, errors = 0, fills = 0;
	struct ocfs2_info_journal_size oij;
	uint64_t reqs[1];
	struct ocfs2_info info;

	memset(oms, 0, sizeof(*oms));

	if (!cluster_coherent)
		flags |= OCFS2_INFO_FL_NON_COHERENT;

	o2info_fill_request((struct ocfs2_info_request *)&oij, sizeof(oij),
			    OCFS2_INFO_JOURNAL_SIZE, flags);

	reqs[0] = (unsigned long)&oij;

	info.oi_requests = (uint64_t)reqs;
	info.oi_count = 1;

	rc = ioctl(fd, OCFS2_IOC_INFO, &info);
	if (rc) {
		rc = errno;
		o2i_error(op, "ioctl failed: %s\n", strerror(rc));
		o2i_scan_requests(op, info, &unknowns, &errors, &fills);
		goto out;
	}

	if (oij.ij_req.ir_flags & OCFS2_INFO_FL_FILLED)
		oms->journal_size = oij.ij_journal_size;

	rc = get_volinfo_ioctl(op, fd, &(oms->ovf));

out:
	return rc;
}

static int o2info_gen_mkfs_string(struct o2info_mkfs oms, char **mkfs)
{
	int rc = 0;
	char *compat = NULL;
	char *incompat = NULL;
	char *rocompat = NULL;
	char *features = NULL;
	char *ptr = NULL;
	char op_fs_features[PATH_MAX];
	char op_label[PATH_MAX];
	char buf[4096];

#define MKFS "-N %u "		\
	     "-J size=%llu "	\
	     "-b %u "		\
	     "-C %u "		\
	     "%s "		\
	     "%s "

	rc = o2info_get_compat_flag(oms.ovf.ofs.compat, &compat);
	if (rc)
		goto out;

	rc = o2info_get_incompat_flag(oms.ovf.ofs.incompat, &incompat);
	if (rc)
		goto out;

	rc = o2info_get_rocompat_flag(oms.ovf.ofs.rocompat, &rocompat);
	if (rc)
		goto out;

	features = malloc(strlen(compat) + strlen(incompat) +
			  strlen(rocompat) + 3);

	sprintf(features, "%s %s %s", compat, incompat, rocompat);

	ptr = features;

	while ((ptr = strchr(ptr, ' ')))
		*ptr = ',';

	if (strcmp("", features))
		snprintf(op_fs_features, PATH_MAX, "--fs-features %s",
			 features);
	else
		strcpy(op_fs_features, "");

	if (strcmp("", (char *)oms.ovf.label))
		snprintf(op_label, PATH_MAX, "-L %s", (char *)(oms.ovf.label));
	else
		strcpy(op_label, "");

	snprintf(buf, 4096, MKFS, oms.ovf.maxslots, oms.journal_size,
		 oms.ovf.blocksize, oms.ovf.clustersize, op_fs_features,
		 op_label);

	*mkfs = strdup(buf);
out:
	if (compat)
		ocfs2_free(&compat);

	if (incompat)
		ocfs2_free(&incompat);

	if (rocompat)
		ocfs2_free(&rocompat);

	if (features)
		ocfs2_free(&features);

	return rc;
}

static int mkfs_run(struct o2info_operation *op, struct o2info_method *om,
		    void *arg)
{
	int rc = 0;
	static struct o2info_mkfs oms;
	char *mkfs = NULL;

	if (om->om_method == O2INFO_USE_IOCTL)
		rc = get_mkfs_ioctl(op, om->om_fd, &oms);
	else
		rc = o2info_get_mkfs(om->om_fs, &oms);
	if (rc)
		goto out;

	o2info_gen_mkfs_string(oms, &mkfs);

	fprintf(stdout, "%s\n", mkfs);
out:
	if (mkfs)
		ocfs2_free(&mkfs);

	return rc;
}

DEFINE_O2INFO_OP(mkfs,
		 mkfs_run,
		 NULL);

static int get_freeinode_ioctl(struct o2info_operation *op,
			       int fd,
			       struct o2info_freeinode *ofi)
{
	uint64_t reqs[1];
	int ret = 0, flags = 0;
	struct ocfs2_info info;
	struct ocfs2_info_freeinode oifi;
	uint32_t unknowns = 0, errors = 0, fills = 0;

	memset(ofi, 0, sizeof(*ofi));

	if (!cluster_coherent)
		flags |= OCFS2_INFO_FL_NON_COHERENT;

	o2info_fill_request((struct ocfs2_info_request *)&oifi, sizeof(oifi),
			    OCFS2_INFO_FREEINODE, flags);

	reqs[0] = (unsigned long)&oifi;

	info.oi_requests = (uint64_t)reqs;
	info.oi_count = 1;

	ret = ioctl(fd, OCFS2_IOC_INFO, &info);
	if (ret) {
		ret = errno;
		o2i_error(op, "ioctl failed: %s\n", strerror(ret));
		o2i_scan_requests(op, info, &unknowns, &errors, &fills);
		goto out;
	}

	if (oifi.ifi_req.ir_flags & OCFS2_INFO_FL_FILLED) {
		ofi->slotnum = oifi.ifi_slotnum;
		memcpy(ofi->fi, oifi.ifi_stat,
		       sizeof(struct o2info_local_freeinode) * OCFS2_MAX_SLOTS);
	}

out:
	return ret;
}

static int freeinode_run(struct o2info_operation *op,
			struct o2info_method *om,
			void *arg)
{
	int ret = 0, i;

	struct o2info_freeinode ofi;
	unsigned long total = 0, free = 0;

	if (om->om_method == O2INFO_USE_IOCTL)
		ret = get_freeinode_ioctl(op, om->om_fd, &ofi);
	else
		ret = o2info_get_freeinode(om->om_fs, &ofi);

	if (ret)
		return ret;

	fprintf(stdout, "Slot\t\tSpace\t\tFree\n");
	for (i = 0; i < ofi.slotnum ; i++) {
		fprintf(stdout, "%3d\t%13lu\t%12lu\n", i, ofi.fi[i].total,
			ofi.fi[i].free);
		total += ofi.fi[i].total;
		free += ofi.fi[i].free;
	}

	fprintf(stdout, "Total\t%13lu\t%12lu\n", total, free);

	return ret;
}

DEFINE_O2INFO_OP(freeinode,
		 freeinode_run,
		 NULL);

static int ul_log2(unsigned long arg)
{
	unsigned int i = 0;

	arg >>= 1;
	while (arg) {
		i++;
		arg >>= 1;
	}

	return i;
}

static int o2info_init_freefrag(struct o2info_freefrag *off,
				struct o2info_volinfo *ovf)
{
	int ret = 0, i;

	off->clustersize_bits = ul_log2((unsigned long)ovf->clustersize);
	off->blksize_bits = ul_log2((unsigned long)ovf->blocksize);

	if (off->chunkbytes) {
		off->chunkbits = ul_log2(off->chunkbytes);
		off->clusters_in_chunk = off->chunkbytes >>
						off->clustersize_bits;
	} else {
		off->chunkbits = ul_log2(DEFAULT_CHUNKSIZE);
		off->clusters_in_chunk = DEFAULT_CHUNKSIZE >>
						off->clustersize_bits;
	}

	off->min = ~0U;
	off->max = off->avg = 0;
	off->free_chunks_real = 0;
	off->free_chunks = 0;

	for (i = 0; i < OCFS2_INFO_MAX_HIST; i++) {
		off->histogram.fc_chunks[i] = 0;
		off->histogram.fc_clusters[i] = 0;
	}

	return ret;
}

static int get_freefrag_ioctl(struct o2info_operation *op, int fd,
			      struct o2info_freefrag *off)
{
	uint64_t reqs[1];
	int ret = 0, flags = 0;
	struct ocfs2_info info;
	struct ocfs2_info_freefrag oiff;
	uint32_t unknowns = 0, errors = 0, fills = 0;

	if (!cluster_coherent)
		flags |= OCFS2_INFO_FL_NON_COHERENT;

	o2info_fill_request((struct ocfs2_info_request *)&oiff, sizeof(oiff),
			    OCFS2_INFO_FREEFRAG, flags);
	oiff.iff_chunksize = off->clusters_in_chunk;

	reqs[0] = (unsigned long)&oiff;

	info.oi_requests = (uint64_t)reqs;
	info.oi_count = 1;

	ret = ioctl(fd, OCFS2_IOC_INFO, &info);
	if (ret) {
		ret = errno;
		o2i_error(op, "ioctl failed: %s\n", strerror(ret));
		o2i_scan_requests(op, info, &unknowns, &errors, &fills);
		goto out;
	}

	if (oiff.iff_req.ir_flags & OCFS2_INFO_FL_FILLED) {
		off->clusters = oiff.iff_ffs.ffs_clusters;
		off->free_clusters = oiff.iff_ffs.ffs_free_clusters;
		off->free_chunks = oiff.iff_ffs.ffs_free_chunks;
		off->free_chunks_real = oiff.iff_ffs.ffs_free_chunks_real;
		if (off->free_chunks_real) {
			off->min = oiff.iff_ffs.ffs_min <<
					(off->clustersize_bits - 10);
			off->max = oiff.iff_ffs.ffs_max <<
					(off->clustersize_bits - 10);
			off->avg = oiff.iff_ffs.ffs_avg <<
					(off->clustersize_bits - 10);
		} else
			off->min = 0;

		memcpy(&(off->histogram), &(oiff.iff_ffs.ffs_fc_hist),
		       sizeof(struct free_chunk_histogram));
	}

	off->total_chunks = (off->clusters + off->clusters_in_chunk) >>
				(off->chunkbits - off->clustersize_bits);
out:
	return ret;
}

static void o2info_report_freefrag(struct o2info_freefrag *off)
{
	char *unitp = "KMGTPEZY";
	char end_str[32];

	int i, unit = 10; /* Begin from KB in terms of 10 bits */
	unsigned int start = 0, end;

	fprintf(stdout, "Blocksize: %u bytes\n", 1 << off->blksize_bits);
	fprintf(stdout, "Clustersize: %u bytes\n", 1 << off->clustersize_bits);
	fprintf(stdout, "Total clusters: %llu\nFree clusters: %u (%0.1f%%)\n",
		off->clusters, off->free_clusters,
		(double)off->free_clusters * 100 / off->clusters);

	fprintf(stdout, "\nMin. free extent: %u KB \nMax. free extent: %u KB\n"
		"Avg. free extent: %u KB\n", off->min, off->max, off->avg);

	if (off->chunkbytes) {
		fprintf(stdout, "\nChunksize: %lu bytes (%u clusters)\n",
			off->chunkbytes, off->clusters_in_chunk);

		fprintf(stdout, "Total chunks: %u\nFree chunks: %u (%0.1f%%)\n",
			off->total_chunks, off->free_chunks,
			(double)off->free_chunks * 100 / off->total_chunks);
	}

	fprintf(stdout, "\nHISTOGRAM OF FREE EXTENT SIZES:\n");
	fprintf(stdout, "%s :  %12s  %12s  %7s\n", "Extent Size Range",
		"Free extents", "Free Clusters", "Percent");

	/*
	 * We probably need to start with 'M' when clustersize = 1M.
	 */
	start = 1 << (off->clustersize_bits - unit);
	if (start == (1 << 10)) {
		unit += 10;
		unitp++;
	}

	for (i = 0; i < OCFS2_INFO_MAX_HIST; i++) {

		start = 1 << (i + off->clustersize_bits - unit);
		end = start << 1;

		if (off->histogram.fc_chunks[i] != 0) {
			snprintf(end_str, 32,  "%5lu%c-", end, *unitp);
			if (i == (OCFS2_INFO_MAX_HIST - 1))
				strcpy(end_str, "max ");
			fprintf(stdout, "%5u%c...%7s  :  "
				"%12u  %12u  %6.2f%%\n",
				start, *unitp, end_str,
				off->histogram.fc_chunks[i],
				off->histogram.fc_clusters[i],
				(double)off->histogram.fc_clusters[i] * 100 /
				off->free_clusters);
		}

		start = end;
		if (start == (1 << 10)) {
			unit += 10;
			unitp++;
			if (!(*unitp))
				break;
		}
	}
}

static int freefrag_run(struct o2info_operation *op,
			struct o2info_method *om,
			void *arg)
{
	int ret = 0;
	static struct o2info_freefrag off;
	static struct o2info_volinfo ovf;
	char *end;

	off.chunkbytes = strtoull((char *)arg, &end, 0);
	if (*end != '\0') {
		o2i_error(op, "bad chunk size '%s'\n", (char *)arg);
		ret = -1;
		print_usage(ret);
	}

	if (off.chunkbytes & (off.chunkbytes - 1)) {
		o2i_error(op, "chunksize needs to be power of 2\n");
		ret = -1;
		print_usage(ret);
	}

	off.chunkbytes *= 1024;

	if (om->om_method == O2INFO_USE_IOCTL)
		ret = get_volinfo_ioctl(op, om->om_fd, &ovf);
	else
		ret = o2info_get_volinfo(om->om_fs, &ovf);
	if (ret)
		return -1;

	if (off.chunkbytes &&
	    (off.chunkbytes < ovf.clustersize)) {
		o2i_error(op, "chunksize should be greater than or equal to "
			  "filesystem cluster size\n");
		ret = -1;
		print_usage(ret);
	}

	ret = o2info_init_freefrag(&off, &ovf);
	if (ret)
		return -1;

	if (om->om_method == O2INFO_USE_IOCTL)
		ret = get_freefrag_ioctl(op, om->om_fd, &off);
	else
		ret = o2info_get_freefrag(om->om_fs, &off);

	if (ret)
		return ret;

	o2info_report_freefrag(&off);

	return ret;
}

DEFINE_O2INFO_OP(freefrag,
		 freefrag_run,
		 NULL);

static int space_usage_run(struct o2info_operation *op,
			   struct o2info_method *om,
			   void *arg)
{
	int ret = 0, flags = 0;
	struct stat st;
	struct o2info_volinfo ovf;
	struct o2info_fiemap ofp;

	if (om->om_method == O2INFO_USE_LIBOCFS2) {
		o2i_error(op, "specify a none-device file to stat\n");
		ret = -1;
		goto out;
	}

	ret = lstat(om->om_path, &st);
	if (ret < 0) {
		ret = errno;
		o2i_error(op, "lstat error: %s\n", strerror(ret));
		ret = -1;
		goto out;
	}

	memset(&ofp, 0, sizeof(ofp));

	ret = get_volinfo_ioctl(op, om->om_fd, &ovf);
	if (ret)
		return -1;

	ofp.blocksize = ovf.blocksize;
	ofp.clustersize = ovf.clustersize;

	ret = o2info_get_fiemap(om->om_fd, flags, &ofp);
	if (ret)
		goto out;

	fprintf(stdout, "Blocks: %-10u Shared: %-10u\tUnwritten: %-7u "
		"Holes: %-6u\n", st.st_blocks, ofp.shared, ofp.unwrittens,
		ofp.holes);

out:
	return ret;
}

DEFINE_O2INFO_OP(space_usage,
		 space_usage_run,
		 NULL);

static int o2info_report_filestat(struct o2info_method *om,
				  struct stat *st,
				  struct o2info_fiemap *ofp)
{
	int ret = 0;
	uint16_t perm;

	char *path = NULL;
	char *filetype = NULL, *h_perm = NULL;
	char *uname = NULL, *gname = NULL;
	char *ah_time = NULL, *ch_time = NULL, *mh_time = NULL;

	ret = o2info_get_human_path(st->st_mode, om->om_path, &path);
	if (ret)
		goto out;

	ret = o2info_get_filetype(*st, &filetype);
	if (ret)
		goto out;

	ret = o2info_uid2name(st->st_uid, &uname);
	if (ret)
		goto out;

	ret = o2info_gid2name(st->st_gid, &gname);
	if (ret)
		goto out;

	ret = o2info_get_human_permission(st->st_mode, &perm, &h_perm);
	if (ret)
		goto out;

	if (!ofp->blocksize)
		ofp->blocksize = st->st_blksize;

	fprintf(stdout, "  File: %s\n", path);
	fprintf(stdout, "  Size: %-10lu\tBlocks: %-10u IO Block: %-6u %s\n",
		st->st_size, st->st_blocks, ofp->blocksize, filetype);
	 if (S_ISBLK(st->st_mode) || S_ISCHR(st->st_mode))
		fprintf(stdout, "Device: %xh/%dd\tInode: %-10i  Links: %-5u"
			" Device type: %u,%u\n", st->st_dev, st->st_dev,
			st->st_ino, st->st_nlink,
			st->st_dev >> 16UL, st->st_dev & 0x0000FFFF);
	else
		fprintf(stdout, "Device: %xh/%dd\tInode: %-10i  Links: %u\n",
			st->st_dev, st->st_dev, st->st_ino, st->st_nlink);
	fprintf(stdout, " Frag%: %-10.2f\tClusters: %-8u Extents: "
		"%-6lu Score: %.0f\n", ofp->frag, ofp->clusters,
		ofp->num_extents, ofp->score);
	fprintf(stdout, "Shared: %-10u\tUnwritten: %-7u Holes: %-8u "
		"Xattr: %u\n", ofp->shared, ofp->unwrittens,
		ofp->holes, ofp->xattr);
	fprintf(stdout, "Access: (%04o/%10s)  Uid: (%5u/%8s)   "
		"Gid: (%5u/%8s)\n", perm, h_perm, st->st_uid,
		uname, st->st_gid, gname);

	ret = o2info_get_human_time(&ah_time, o2info_get_stat_atime(st));
	if (ret)
		goto out;
	ret = o2info_get_human_time(&mh_time, o2info_get_stat_mtime(st));
	if (ret)
		goto out;
	ret = o2info_get_human_time(&ch_time, o2info_get_stat_ctime(st));
	if (ret)
		goto out;

	fprintf(stdout, "Access: %s\n", ah_time);
	fprintf(stdout, "Modify: %s\n", mh_time);
	fprintf(stdout, "Change: %s\n", ch_time);

out:
	if (path)
		ocfs2_free(&path);
	if (filetype)
		ocfs2_free(&filetype);
	if (uname)
		ocfs2_free(&uname);
	if (gname)
		ocfs2_free(&gname);
	if (h_perm)
		ocfs2_free(&h_perm);
	if (ah_time)
		ocfs2_free(&ah_time);
	if (mh_time)
		ocfs2_free(&mh_time);
	if (ch_time)
		ocfs2_free(&ch_time);

	return ret;
}

static int filestat_run(struct o2info_operation *op,
			struct o2info_method *om,
			void *arg)
{
	int ret = 0, flags = 0;
	struct stat st;
	struct o2info_volinfo ovf;
	struct o2info_fiemap ofp;

	if (om->om_method == O2INFO_USE_LIBOCFS2) {
		o2i_error(op, "specify a none-device file to stat\n");
		ret = -1;
		goto out;
	}

	ret = lstat(om->om_path, &st);
	if (ret < 0) {
		ret = errno;
		o2i_error(op, "lstat error: %s\n", strerror(ret));
		ret = -1;
		goto out;
	}

	memset(&ofp, 0, sizeof(ofp));

	ret = get_volinfo_ioctl(op, om->om_fd, &ovf);
	if (ret)
		return -1;

	ofp.blocksize = ovf.blocksize;
	ofp.clustersize = ovf.clustersize;

	ret = o2info_get_fiemap(om->om_fd, flags, &ofp);
	if (ret)
		goto out;

	ret = o2info_report_filestat(om, &st, &ofp);
out:
	return ret;
}

DEFINE_O2INFO_OP(filestat,
		 filestat_run,
		 NULL);
