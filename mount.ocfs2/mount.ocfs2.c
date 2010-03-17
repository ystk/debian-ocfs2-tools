/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * mount.ocfs2.c  Mounts ocfs2 volume
 *
 * Copyright (C) 2005 Oracle.  All rights reserved.
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

#include "mount.ocfs2.h"

#define OCFS2_CLUSTER_STACK_ARG		"cluster_stack="

int verbose = 0;
int mount_quiet = 0;
char *progname = NULL;

static int nomtab = 0;

struct mount_options {
	char *dev;
	char *dir;
	char *opts;
	int flags;
	char *xtra_opts;
	char *type;
};

static void handle_signal(int sig)
{
	switch (sig) {
	case SIGTERM:
	case SIGINT:
		printf("\nmount interrupted\n");
		exit(1);
	}
}

static void read_options(int argc, char **argv, struct mount_options *mo)
{
	int c;

	progname = basename(argv[0]);

	if (argc < 2)
		return;

	while(1) {
		c = getopt(argc, argv, "vno:t:");
		if (c == -1)
			break;

		switch (c) {
		case 'v':
			++verbose;
			break;

		case 'n':
			++nomtab;
			break;

		case 'o':
			if (optarg)
				mo->opts = xstrdup(optarg);
			break;

		case 't':
			if (optarg)
				mo->type = xstrdup(optarg);
			break;

		default:
			break;
		}
	}

	if (optind < argc && argv[optind])
		mo->dev = xstrdup(argv[optind]);

	++optind;

	if (optind < argc && argv[optind])
		mo->dir = xstrdup(argv[optind]);
}

/*
 * Code based on similar function in util-linux-2.12p/mount/mount.c
 *
 */
static void print_one(const struct my_mntent *me)
{
	if (mount_quiet)
		return ;

	printf ("%s on %s", me->mnt_fsname, me->mnt_dir);

	if (me->mnt_type != NULL && *(me->mnt_type) != '\0')
		printf (" type %s", me->mnt_type);

	if (me->mnt_opts)
		printf (" (%s)", me->mnt_opts);

	printf ("\n");
}


static void my_free(const void *s)
{
	if (s)
		free((void *) s);
}

/*
 * Code based on similar function in util-linux-2.12p/mount/mount.c
 *
 */
static void update_mtab_entry(char *spec, char *node, char *type, char *opts,
			      int flags, int freq, int pass)
{
	struct my_mntent mnt;

	mnt.mnt_fsname = canonicalize (spec);
	mnt.mnt_dir = canonicalize (node);
	mnt.mnt_type = type;
	mnt.mnt_opts = opts;
	mnt.mnt_freq = freq;
	mnt.mnt_passno = pass;
      
	/* We get chatty now rather than after the update to mtab since the
	   mount succeeded, even if the write to /etc/mtab should fail.  */
	if (verbose)
		print_one (&mnt);

	if (!nomtab && mtab_is_writable()) {
		if (flags & MS_REMOUNT)
			update_mtab (mnt.mnt_dir, &mnt);
		else {
			mntFILE *mfp;

			lock_mtab();

			mfp = my_setmntent(MOUNTED, "a+");
			if (mfp == NULL || mfp->mntent_fp == NULL) {
				com_err(progname, OCFS2_ET_IO, "%s, %s",
					MOUNTED, strerror(errno));
			} else {
				if ((my_addmntent (mfp, &mnt)) == 1) {
					com_err(progname, OCFS2_ET_IO, "%s, %s",
						MOUNTED, strerror(errno));
				}
			}
			my_endmntent(mfp);
			unlock_mtab();
		}
	}
	my_free(mnt.mnt_fsname);
	my_free(mnt.mnt_dir);
}

static int process_options(struct mount_options *mo)
{
	if (!mo->dev) {
		com_err(progname, OCFS2_ET_BAD_DEVICE_NAME, " ");
		return -1;
	}

	if (!mo->dir) {
		com_err(progname, OCFS2_ET_INVALID_ARGUMENT, "no mountpoint specified");
		return -1;
	}

	if (mo->type && strcmp(mo->type, OCFS2_FS_NAME)) {
		com_err(progname, OCFS2_ET_UNKNOWN_FILESYSTEM, mo->type);
		return -1;
	}

	if (mo->opts)
		parse_opts(mo->opts, &mo->flags, &mo->xtra_opts);

	return 0;
}

static int check_dev_readonly(const char *dev, int *dev_ro)
{
	int fd;
	int ret;

	fd = open64(dev, O_RDONLY);
	if (fd < 0)
		return errno;

	ret = ioctl(fd, BLKROGET, dev_ro);
	if (ret < 0)
		return errno;

	close(fd);

	return 0;
}

static int run_hb_ctl(const char *hb_ctl_path,
		      const char *device, const char *arg)
{
	int ret = 0;
	int child_status;
	char * argv[5];
	pid_t child;

	child = fork();
	if (child < 0) {
		ret = errno;
		goto bail;
	}

	if (!child) {
		argv[0] = (char *) hb_ctl_path;
		argv[1] = (char *) arg;
		argv[2] = "-d";
		argv[3] = (char *) device;
		argv[4] = NULL;

		ret = execv(argv[0], argv);

		ret = errno;
		exit(ret);
	} else {
		ret = waitpid(child, &child_status, 0);
		if (ret < 0) {
			ret = errno;
			goto bail;
		}

		ret = WEXITSTATUS(child_status);
	}

bail:
	return ret;
}


int main(int argc, char **argv)
{
	errcode_t ret = 0;
	struct mount_options mo;
	char hb_ctl_path[PATH_MAX];
	char *extra = NULL;
	int dev_ro = 0;
	char *hbstr = NULL;
	char stackstr[strlen(OCFS2_CLUSTER_STACK_ARG) + OCFS2_STACK_LABEL_LEN + 1] = "";
	ocfs2_filesys *fs = NULL;
	struct o2cb_cluster_desc cluster;
	struct o2cb_region_desc desc;
	int clustered = 1;
	int hb_started = 0;
	struct stat statbuf;

	initialize_ocfs_error_table();
	initialize_o2dl_error_table();
	initialize_o2cb_error_table();

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	if (signal(SIGTERM, handle_signal) == SIG_ERR) {
		fprintf(stderr, "Could not set SIGTERM\n");
		exit(1);
	}

	if (signal(SIGINT, handle_signal) == SIG_ERR) {
		fprintf(stderr, "Could not set SIGINT\n");
		exit(1);
	}

	memset(&mo, 0, sizeof(mo));
	read_options (argc, argv, &mo);

	ret = process_options(&mo);
	if (ret)
		goto bail;

	ret = ocfs2_open(mo.dev, OCFS2_FLAG_RO, 0, 0, &fs); //O_EXCL?
	if (ret) {
		com_err(progname, ret, "while opening device %s", mo.dev);
		goto bail;
	}

	clustered = (0 == ocfs2_mount_local(fs));

	if (verbose)
		printf("device=%s\n", mo.dev);

	if (clustered) {
		ret = o2cb_init();
		if (ret) {
			com_err(progname, ret, "while trying initialize cluster");
			goto bail;
		}

		ret = ocfs2_fill_cluster_desc(fs, &cluster);
		if (ret) {
			com_err(progname, ret,
				"while trying to determine cluster information");
			goto bail;
		}
		if (cluster.c_stack)
			snprintf(stackstr, sizeof(stackstr), "%s%s",
				 OCFS2_CLUSTER_STACK_ARG, cluster.c_stack);

		ret = ocfs2_fill_heartbeat_desc(fs, &desc);
		if (ret) {
			com_err(progname, ret,
				"while trying to determine heartbeat information");
			goto bail;
		}
		desc.r_persist = 1;
		desc.r_service = OCFS2_FS_NAME;

		ret = o2cb_get_hb_ctl_path(hb_ctl_path, sizeof(hb_ctl_path));
		if (ret) {
			com_err(progname, ret,
				"probably because o2cb service not started");
			goto bail;
		}
	}

	if (mo.flags & MS_RDONLY) {
		ret = check_dev_readonly(mo.dev, &dev_ro);
		if (ret) {
			com_err(progname, ret, "device not accessible");
			goto bail;
		}
	}

	block_signals (SIG_BLOCK);

	if (!(mo.flags & MS_REMOUNT) && !dev_ro && clustered) {
		ret = o2cb_begin_group_join(&cluster, &desc);
		if (ret) {
			block_signals (SIG_UNBLOCK);
			com_err(progname, ret,
				"while trying to join the group");
			goto bail;
		}
		hb_started = 1;
	}

	if (dev_ro || !clustered)
		hbstr = OCFS2_HB_NONE;
	else if (strlen(stackstr))
		hbstr = stackstr;
	else
		hbstr = OCFS2_HB_LOCAL;

	if (mo.xtra_opts && *mo.xtra_opts) {
		extra = xstrndup(mo.xtra_opts,
				 strlen(mo.xtra_opts) + strlen(hbstr) + 1);
		extra = xstrconcat3(extra, ",", hbstr);
	} else
		extra = xstrndup(hbstr, strlen(hbstr));

	ret = mount(mo.dev, mo.dir, OCFS2_FS_NAME, mo.flags & ~MS_NOSYS, extra);
	if (ret) {
		ret = errno;
		if (hb_started) {
			/* We ignore the return code because the mount
			 * failure is the important error.
			 * complete_group_join() will handle cleaning up */
			o2cb_complete_group_join(&cluster, &desc, errno);
		}
		block_signals (SIG_UNBLOCK);

		/* complain mount failure */
		if (lstat(mo.dir, &statbuf))
			com_err(progname, 0, "mount point %s does not "
				"exist", mo.dir);
		else if (stat(mo.dir, &statbuf))
			com_err(progname, 0, "mount point %s is a "
				"broken symbolic link", mo.dir);
		else if (!S_ISDIR(statbuf.st_mode))
			com_err(progname, 0, "mount point %s is not "
				"a directory", mo.dir);
		else
			com_err(progname, ret, "while mounting %s on %s. "
				"Check 'dmesg' for more information on this "
				"error.", mo.dev, mo.dir);
		goto bail;
	}
	if (hb_started) {
		ret = o2cb_complete_group_join(&cluster, &desc, 0);
		if (ret) {
			com_err(progname, ret,
				"while completing group join (WARNING)");
			/*
			 * XXX: GFS2 allows the mount to continue, so we
			 * will do the same.  I don't know how clean that
			 * is, but I don't have a better solution.
			 */
			ret = 0;
		}
	}

	run_hb_ctl (hb_ctl_path, mo.dev, "-P");
	update_mtab_entry(mo.dev, mo.dir, OCFS2_FS_NAME,
			  fix_opts_string(((mo.flags & ~MS_NOMTAB) |
					   (clustered ? MS_NETDEV : 0)),
					  extra, NULL),
			  mo.flags, 0, 0);

	block_signals (SIG_UNBLOCK);

bail:
	if (fs)
		ocfs2_close(fs);
	if (extra)
		free(extra);
	if (mo.dev)
		free(mo.dev);
	if (mo.dir)
		free(mo.dir);
	if (mo.opts)
		free(mo.opts);
	if (mo.xtra_opts)
		free(mo.xtra_opts);
	if (mo.type)
		free(mo.type);

	return ret ? 1 : 0;
}
