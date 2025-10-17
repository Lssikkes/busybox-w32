/*
 * setfattr - set extended attributes of filesystem objects.
 *
 * Copyright (C) 2017 by Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config SETFATTR
//config:	bool "setfattr (3.9 kb)"
//config:	default y
//config:	help
//config:	Set/delete extended attributes on files

//applet:IF_SETFATTR(APPLET_NOEXEC(setfattr, setfattr, BB_DIR_USR_BIN, BB_SUID_DROP, setfattr))

//kbuild:lib-$(CONFIG_SETFATTR) += setfattr.o

#include <sys/xattr.h>
#include "libbb.h"

/* macOS has different xattr API - provide wrappers */
#ifdef __APPLE__
static int bb_setxattr(const char *path, const char *name, const void *value, size_t size, int flags) {
	return setxattr(path, name, value, size, 0, flags);
}
static int bb_lsetxattr(const char *path, const char *name, const void *value, size_t size, int flags) {
	return setxattr(path, name, value, size, 0, XATTR_NOFOLLOW | flags);
}
static int bb_removexattr(const char *path, const char *name) {
	return removexattr(path, name, 0);
}
static int bb_lremovexattr(const char *path, const char *name) {
	return removexattr(path, name, XATTR_NOFOLLOW);
}
# define setxattr bb_setxattr
# define lsetxattr bb_lsetxattr
# define removexattr bb_removexattr
# define lremovexattr bb_lremovexattr
#endif

//usage:#define setfattr_trivial_usage
//usage:       "[-h] -n|-x ATTR [-v VALUE] FILE..."
//usage:#define setfattr_full_usage "\n\n"
//usage:       "Set extended attributes"
//usage:     "\n"
//usage:     "\n	-h		Do not follow symlinks"
//usage:     "\n	-x ATTR		Remove attribute ATTR"
//usage:     "\n	-n ATTR		Set attribute ATTR to VALUE"
//usage:     "\n	-v VALUE	(default: empty)"
int setfattr_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int setfattr_main(int argc UNUSED_PARAM, char **argv)
{
	const char *name;
	const char *value = "";
	exitcode_t status;
	int opt;
	enum {
		OPT_h = (1 << 0),
		OPT_x = (1 << 1),
	};

	opt = getopt32(argv, "^"
		"hx:n:v:"
		/* Min one arg, either -x or -n is a must, -x does not allow -v */
		"\0" "-1:x:n:n--x:x--nv:v--x"
		, &name, &name, &value
	);
	argv += optind;

	status = EXIT_SUCCESS;
	do {
		int r;
		if (opt & OPT_x)
			r = ((opt & OPT_h) ? lremovexattr : removexattr)(*argv, name);
		else {
			r = ((opt & OPT_h) ? lsetxattr : setxattr)(
					*argv, name,
					value, strlen(value), /*flags:*/ 0
			);
		}
		if (r) {
			bb_simple_perror_msg(*argv);
			status = EXIT_FAILURE;
		}
	} while (*++argv);

	return status;
}
