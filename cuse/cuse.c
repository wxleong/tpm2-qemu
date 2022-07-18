/*
  CUSE example: Character device in Userspace
  Copyright (C) 2008-2009  SUSE Linux Products GmbH
  Copyright (C) 2008-2009  Tejun Heo <tj@kernel.org>
  Copyright (C) 2022       WenXin Leong <wenxin.leong@infineon.com>

  This program can be distributed under the terms of the GNU GPLv2.
  See the file COPYING.

*/

/** @file
 *
 * This example demonstrates how to implement a character device in
 * userspace ("CUSE"). This is only allowed for root. The character
 * device should appear in /dev under the specified name. It can be
 * tested with the cuse_client.c program.
 *
 * Mount the file system with:
 *
 *     cuse -f --name=mydevice
 *
 * You should now have a new /dev/mydevice character device. To "unmount" it,
 * kill the "cuse" process.
 *
 * To compile this example, run
 *
 *     gcc -Wall cuse.c `pkg-config fuse3 --cflags --libs` -o cuse
 *
 * ## Source code ##
 * \include cuse.c
 */

#define FUSE_USE_VERSION 31

#include <cuse_lowlevel.h>
#include <fuse_opt.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "ioctl.h"

#define HOST "localhost"
#define PORT 9883

static int sock;
static char buffer[4096];
static ssize_t resp_len;

static const char *usage =
"usage: cusexmp [options]\n"
"\n"
"options:\n"
"    --help|-h             print this help message\n"
"    --maj=MAJ|-M MAJ      device major number\n"
"    --min=MIN|-m MIN      device minor number\n"
"    --name=NAME|-n NAME   device name (mandatory)\n"
"    -d   -o debug         enable debug output (implies -f)\n"
"    -f                    foreground operation\n"
"    -s                    disable multi-threaded operation\n"
"\n";

static void cusexmp_open(fuse_req_t req, struct fuse_file_info *fi)
{
	struct sockaddr_in addr;
	struct hostent *host;

	if ((host = gethostbyname(HOST)) == NULL)
	{
		fuse_reply_err(req, EIO);
		return;
	}

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	addr.sin_addr.s_addr = *(long*)(host->h_addr);

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0
		|| connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		fuse_reply_err(req, EIO);
		return;
	}


	fuse_reply_open(req, fi);
}

static void cusexmp_release(fuse_req_t req, struct fuse_file_info *fi)
{
	(void)req;
	(void)fi;
	close(sock);
}

static void cusexmp_read(fuse_req_t req, size_t size, off_t off,
			 struct fuse_file_info *fi)
{
	(void)fi;

	if (off >= resp_len)
		off = resp_len;
	if (size > (size_t)(resp_len - off))
		size = resp_len - off;

	fuse_reply_buf(req, buffer + off, size);
}

static void cusexmp_write(fuse_req_t req, const char *buf, size_t size,
			  off_t off, struct fuse_file_info *fi)
{
	(void)fi;
	ssize_t num;

	if ((num = send(sock, buf + off, size, 0)) < 0)
	{
		fuse_reply_err(req, EIO);
		return;
	}

	if ((resp_len = recv(sock, buffer, sizeof(buffer), 0)) < 0)
	{
		fuse_reply_err(req, EIO);
		return;
	}

	fuse_reply_write(req, num);
}

static void cusexmp_ioctl(fuse_req_t req, int cmd, void *arg,
			  struct fuse_file_info *fi, unsigned flags,
			  const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
	(void)req;
	(void)cmd;
	(void)arg;
	(void)fi;
	(void)flags;
	(void)in_buf;
	(void)in_bufsz;
	(void)out_bufsz;
}

struct cusexmp_param {
	unsigned		major;
	unsigned		minor;
	char			*dev_name;
	int			is_help;
};

#define CUSEXMP_OPT(t, p) { t, offsetof(struct cusexmp_param, p), 1 }

static const struct fuse_opt cusexmp_opts[] = {
	CUSEXMP_OPT("-M %u",		major),
	CUSEXMP_OPT("--maj=%u",		major),
	CUSEXMP_OPT("-m %u",		minor),
	CUSEXMP_OPT("--min=%u",		minor),
	CUSEXMP_OPT("-n %s",		dev_name),
	CUSEXMP_OPT("--name=%s",	dev_name),
	FUSE_OPT_KEY("-h",		0),
	FUSE_OPT_KEY("--help",		0),
	FUSE_OPT_END
};

static int cusexmp_process_arg(void *data, const char *arg, int key,
			       struct fuse_args *outargs)
{
	struct cusexmp_param *param = data;

	(void)outargs;
	(void)arg;

	switch (key) {
	case 0:
		param->is_help = 1;
		fprintf(stderr, "%s", usage);
		return fuse_opt_add_arg(outargs, "-ho");
	default:
		return 1;
	}
}

static const struct cuse_lowlevel_ops cusexmp_clop = {
	.open		= cusexmp_open,
	.read		= cusexmp_read,
	.write		= cusexmp_write,
	.release	= cusexmp_release,
	.ioctl		= cusexmp_ioctl,
};

int main(int argc, char **argv)
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct cusexmp_param param = { 0, 0, NULL, 0 };
	char dev_name[128] = "DEVNAME=";
	const char *dev_info_argv[] = { dev_name };
	struct cuse_info ci;
	int ret = 1;

	if (fuse_opt_parse(&args, &param, cusexmp_opts, cusexmp_process_arg)) {
		printf("failed to parse option\n");
		free(param.dev_name);
		goto out;
	}

	if (!param.is_help) {
		if (!param.dev_name) {
			fprintf(stderr, "Error: device name missing\n");
			goto out;
		}
		strncat(dev_name, param.dev_name, sizeof(dev_name) - sizeof("DEVNAME="));
		free(param.dev_name);
	}

	memset(&ci, 0, sizeof(ci));
	ci.dev_major = param.major;
	ci.dev_minor = param.minor;
	ci.dev_info_argc = 1;
	ci.dev_info_argv = dev_info_argv;
	ci.flags = CUSE_UNRESTRICTED_IOCTL;

	ret = cuse_lowlevel_main(args.argc, args.argv, &ci, &cusexmp_clop, NULL);

out:
	fuse_opt_free_args(&args);
	return ret;
}
