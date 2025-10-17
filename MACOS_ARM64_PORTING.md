# Porting Busybox-w32 to ARM64 macOS

This document details all changes required to successfully build busybox-w32 on ARM64 macOS (Apple Silicon).

## Overview

The main challenges in porting busybox to macOS were:
1. **BSD vs GNU toolchain differences** (ar, strip, linker flags)
2. **Missing Linux-specific headers and system calls**
3. **Different API signatures for POSIX functions**
4. **Compiler flag incompatibilities between GCC and Clang**

---

## Build System Changes

### 1. scripts/Makefile.build
**Issue**: macOS's BSD `ar` command cannot create empty archives (requires at least one object file).

**Fix**: Modified `cmd_link_o_target` and `cmd_link_l_target` to create a temporary empty object file when the object list is empty.

```make
# Lines 261-263
cmd_link_o_target = $(if $(strip $(obj-y)),\
    rm -f $@; $(AR) rcs $@ $(filter $(obj-y), $^), \
    printf '' | $(CC) -x c -c -o $@.tmp.o - && $(AR) rcs $@ $@.tmp.o && rm -f $@.tmp.o)

# Lines 293-295
cmd_link_l_target = $(if $(strip $(lib-y)),\
    rm -f $@ && $(AR) $(EXTRA_ARFLAGS) rcs $@ $(lib-y), \
    printf '' | $(CC) -x c -c -o $@.tmp.o - && $(AR) $(EXTRA_ARFLAGS) rcs $@ $@.tmp.o && rm -f $@.tmp.o)
```

Also changed `echo -n` to `printf ''` for macOS compatibility.

---

### 2. Makefile.flags
**Issue**: GCC-specific optimization flags cause warnings/errors with Clang.

**Fix**: Wrapped incompatible flags with Darwin OS detection:

```make
# Lines 55-57
ifneq ($(shell uname -s),Darwin)
CFLAGS += $(call cc-option,-finline-limit=0,)
endif

# Lines 78-80
ifneq ($(shell uname -s),Darwin)
CFLAGS += $(call cc-option,-falign-jumps=1 -falign-labels=1 -falign-loops=1,)
endif
```

---

### 3. Makefile
**Issue**: macOS's `strip` command doesn't support `--remove-section` flag.

**Fix**: Added Darwin-specific strip command:

```make
# Lines 798-810
busybox$(EXEEXT): busybox_unstripped$(EXEEXT)
ifeq ($(SKIP_STRIP),y)
	$(Q)cp $< $@
else
ifeq ($(shell uname -s),Darwin)
	$(Q)$(STRIP) -S -x busybox_unstripped$(EXEEXT) -o $@
else
	$(Q)$(STRIP) -s --remove-section=.note --remove-section=.comment \
		busybox_unstripped$(EXEEXT) -o $@
endif
# strip is confused by PIE executable and does not set exec bits
	$(Q)chmod a+x $@
endif
```

---

### 4. scripts/trylink
**Issue**: macOS's linker doesn't support GNU ld-specific flags.

**Fix**: Added detection for unsupported linker options:

```bash
# Lines 102-108
START_GROUP="-Wl,--start-group"
END_GROUP="-Wl,--end-group"
if ! check_cc "-Wl,--start-group -Wl,--end-group"; then
    echo "Your linker does not support --start-group/--end-group"
    START_GROUP=""
    END_GROUP=""
fi

# Lines 110-119
MAP_OPTS=""
if check_cc "-Wl,-Map,mapfile.tmp"; then
    MAP_OPTS="-Wl,-Map,$EXE.map"
    rm -f mapfile.tmp
fi

VERBOSE_OPTS=""
if check_cc "-Wl,--verbose"; then
    VERBOSE_OPTS="-Wl,--verbose"
fi

# Lines 121-123
INFO_OPTS() {
	echo "$WARN_COMMON $MAP_OPTS $VERBOSE_OPTS"
}
```

---

## Platform Compatibility Changes

### 5. include/platform.h
**Issue**: Missing byte swap functions and unavailable libc functions on macOS.

**Fix**: Added macOS-specific byte swap definitions and disabled unavailable functions:

```c
// Lines 174-180 - Byte swap functions
#elif defined(__APPLE__)
# include <libkern/OSByteOrder.h>
# define bswap_64(x) OSSwapInt64(x)
# define bswap_32(x) OSSwapInt32(x)
# define bswap_16(x) OSSwapInt16(x)
#else

// Lines 548-551 - Disable unavailable functions
#if defined(__APPLE__)
# undef HAVE_MEMPCPY
# undef HAVE_MEMRCHR
#endif
```

---

### 6. coreutils/touch.c
**Issue**: macOS uses different structure member names for nanosecond timestamps in `struct stat`.

**Fix**: Added macOS-specific code:

```c
// Lines 137-143
#if defined(__APPLE__)
				ts[1].tv_nsec = stbuf.st_mtimespec.tv_nsec;
				ts[0].tv_nsec = stbuf.st_atimespec.tv_nsec;
#else
				ts[1].tv_nsec = stbuf.st_mtim.tv_nsec;
				ts[0].tv_nsec = stbuf.st_atim.tv_nsec;
#endif
```

---

### 7. init/init.c
**Issue**: `sigtimedwait()` is not available on macOS.

**Fix**: Provided a fallback implementation using `pselect()`:

```c
// Lines 140-185
#ifdef __APPLE__
static int sigtimedwait(const sigset_t *set, siginfo_t *info, const struct timespec *timeout)
{
	sigset_t pending;
	int sig;
	struct timespec zero_timeout = { 0, 0 };
	
	/* Check if any signals in set are already pending */
	if (sigpending(&pending) == -1)
		return -1;
	
	for (sig = 1; sig < NSIG; sig++) {
		if (sigismember(set, sig) && sigismember(&pending, sig)) {
			/* Signal is pending, retrieve it without blocking */
			sigset_t wait_set;
			sigemptyset(&wait_set);
			sigaddset(&wait_set, sig);
			if (sigwait(&wait_set, &sig) == 0) {
				if (info) {
					memset(info, 0, sizeof(*info));
					info->si_signo = sig;
				}
				return sig;
			}
			return -1;
		}
	}
	
	/* No signals pending, wait using pselect */
	if (pselect(0, NULL, NULL, NULL, timeout, set) == -1) {
		if (errno == EINTR) {
			/* Signal was caught, retrieve it */
			if (sigpending(&pending) == -1)
				return -1;
			
			for (sig = 1; sig < NSIG; sig++) {
				if (sigismember(set, sig) && sigismember(&pending, sig)) {
					sigset_t wait_set;
					sigemptyset(&wait_set);
					sigaddset(&wait_set, sig);
					if (sigwait(&wait_set, &sig) == 0) {
						if (info) {
							memset(info, 0, sizeof(*info));
							info->si_signo = sig;
						}
						return sig;
					}
					return -1;
				}
			}
		}
		return -1;
	}
	
	/* Timeout */
	errno = EAGAIN;
	return -1;
}
#endif
```

---

### 8. libbb/alloc_affinity.c
**Issue**: `sched_getaffinity()` is not available on macOS.

**Fix**: Return a mask with all CPUs available:

```c
// Lines 14-20
#ifdef __APPLE__
	/* macOS doesn't have sched_getaffinity, assume all CPUs are available */
	memset(mask, 0xff, sz);
	return mask;
#else
	if (sched_getaffinity(0, sz, (void*)mask) != 0)
		bb_simple_perror_msg_and_die("can't get CPU affinity");
	return mask;
#endif
```

---

### 9. libbb/capability.c
**Issue**: Linux capabilities API (`linux/capability.h`) is not available on macOS.

**Fix**: Wrapped all capability code with `#ifdef __linux__` and provided stubs:

```c
// Lines 9-20
#ifdef __linux__
# include <linux/capability.h>
#else
/* Stub implementations for non-Linux platforms */
typedef struct { uint32_t dummy; } cap_user_header_t;
typedef struct { uint32_t dummy; } cap_user_data_t;

static unsigned getcaps(char *caps) { return 0; }
static unsigned cap_name_to_number(const char *name) { return 0; }
static void printf_cap(const char *pfx, unsigned cap_no) { }
#endif

// Lines 22-27
#ifdef __linux__
extern int capset(cap_user_header_t header, const cap_user_data_t data);
extern int capget(cap_user_header_t header, cap_user_data_t data);
#endif

// Line 141
#endif /* __linux__ */
```

---

### 10. libbb/xconnect.c
**Issue**: Missing `inet_ntoa()` and `inet_pton()` declarations, and Linux-specific netlink headers.

**Fix**: Added include and wrapped Linux code:

```c
// Lines 9-14
#include <sys/types.h>
#include <sys/socket.h> /* netinet/in.h needs it */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/un.h>

// Lines 15-18
#if ENABLE_IFPLUGD || ENABLE_FEATURE_MDEV_DAEMON || ENABLE_UEVENT
# ifdef __linux__
#  include <linux/netlink.h>
# endif
#endif

// Lines 420-453
#ifdef __linux__
int FAST_FUNC create_and_bind_to_netlink(int proto, int grp, unsigned rcvbuf)
{
	// ... netlink implementation ...
}
#endif
```

---

### 11. libbb/inet_common.c
**Issue**: Missing `inet_aton()` declaration.

**Fix**: Added include:

```c
// Line 12
#include <arpa/inet.h>
```

---

### 12. libbb/find_mount_point.c
**Issue**: `mntent.h` is not available on macOS.

**Fix**: Wrapped mntent code and provided a NULL return for macOS:

```c
// Lines 13-15
#if !defined(__APPLE__)
# include <mntent.h>
#endif

// Lines 27-36
#if defined(__APPLE__)
	/* macOS doesn't have mntent.h, return NULL */
	struct mntent *mountEntry = NULL;
	return NULL;
#elif !ENABLE_PLATFORM_MINGW32
	struct stat stbuf;
	struct mntent *mountEntry;
	// ... rest of Linux implementation ...
#else
	// ... MinGW implementation ...
#endif
```

---

### 13. miscutils/getfattr.c
**Issue**: macOS has different function signatures for extended attribute APIs (no separate functions for symlinks).

**Fix**: Added wrapper functions:

```c
// Lines 24-39
#if defined(__APPLE__)
/* macOS xattr API uses flags instead of separate l* functions */
static ssize_t bb_getxattr(const char *path, const char *name, void *value, size_t size, int flags) {
	return getxattr(path, name, value, size, 0, flags);
}
static ssize_t bb_lgetxattr(const char *path, const char *name, void *value, size_t size, int flags) {
	return getxattr(path, name, value, size, 0, flags | XATTR_NOFOLLOW);
}
static ssize_t bb_listxattr(const char *path, char *list, size_t size, int flags) {
	return listxattr(path, list, size, flags);
}
static ssize_t bb_llistxattr(const char *path, char *list, size_t size, int flags) {
	return listxattr(path, list, size, flags | XATTR_NOFOLLOW);
}
#define getxattr(path, name, value, size) bb_getxattr(path, name, value, size, 0)
#define lgetxattr(path, name, value, size) bb_lgetxattr(path, name, value, size, 0)
#define listxattr(path, list, size) bb_listxattr(path, list, size, 0)
#define llistxattr(path, list, size) bb_llistxattr(path, list, size, 0)
#endif
```

---

### 14. miscutils/setfattr.c
**Issue**: Same as getfattr - different xattr API signatures.

**Fix**: Added wrapper functions:

```c
// Lines 23-38
#if defined(__APPLE__)
/* macOS xattr API uses flags instead of separate l* functions */
static int bb_setxattr(const char *path, const char *name, const void *value, size_t size, int flags) {
	return setxattr(path, name, value, size, 0, flags);
}
static int bb_lsetxattr(const char *path, const char *name, const void *value, size_t size, int flags) {
	return setxattr(path, name, value, size, 0, flags | XATTR_NOFOLLOW);
}
static int bb_removexattr(const char *path, const char *name, int flags) {
	return removexattr(path, name, flags);
}
static int bb_lremovexattr(const char *path, const char *name, int flags) {
	return removexattr(path, name, flags | XATTR_NOFOLLOW);
}
#define setxattr(path, name, value, size, flags) bb_setxattr(path, name, value, size, 0)
#define lsetxattr(path, name, value, size, flags) bb_lsetxattr(path, name, value, size, 0)
#define removexattr(path, name) bb_removexattr(path, name, 0)
#define lremovexattr(path, name) bb_lremovexattr(path, name, 0)
#endif
```

---

### 15. miscutils/i2c_tools.c
**Issue**: I2C tools require Linux-specific headers (`linux/i2c.h`).

**Fix**: Wrapped entire implementation with stub functions for non-Linux:

```c
// Line 68
#ifdef __linux__
// ... entire i2c_tools implementation ...

// Lines 1545-1577
#else /* !__linux__ */
/* Stub implementations for non-Linux platforms */
int i2cget_main(int argc UNUSED_PARAM, char **argv UNUSED_PARAM) {
	bb_simple_error_msg_and_die("i2c tools are not available on this platform");
}
int i2cset_main(int argc UNUSED_PARAM, char **argv UNUSED_PARAM) {
	bb_simple_error_msg_and_die("i2c tools are not available on this platform");
}
int i2cdump_main(int argc UNUSED_PARAM, char **argv UNUSED_PARAM) {
	bb_simple_error_msg_and_die("i2c tools are not available on this platform");
}
int i2cdetect_main(int argc UNUSED_PARAM, char **argv UNUSED_PARAM) {
	bb_simple_error_msg_and_die("i2c tools are not available on this platform");
}
int i2ctransfer_main(int argc UNUSED_PARAM, char **argv UNUSED_PARAM) {
	bb_simple_error_msg_and_die("i2c tools are not available on this platform");
}
#endif /* __linux__ */
```

---

### 16. networking/hostname.c
**Issue**: Missing `inet_ntoa()` declaration.

**Fix**: Added include:

```c
// Line 49
#include <arpa/inet.h>
```

---

### 17. networking/libiproute/ip_common.h
**Issue**: Linux-specific networking headers not available on macOS.

**Fix**: Added conditional includes and minimal type definitions:

```c
// Lines 6-19
#ifdef __linux__
# include <asm/types.h>
# include <linux/netlink.h>
# include <linux/rtnetlink.h>
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <net/if.h>
  typedef uint8_t __u8;
  typedef uint16_t __u16;
  typedef uint32_t __u32;
  typedef int32_t __s32;
#endif

// Lines 21-27
#ifdef __linux__
# include <linux/if_addr.h>
# include <linux/if_link.h>
#endif
```

---

### 18. networking/libiproute/libnetlink.h
**Issue**: Same as ip_common.h - Linux-specific headers.

**Fix**: Added conditional includes with stub structures:

```c
// Lines 5-20
#ifdef __linux__
# include <linux/types.h>
# include <linux/netlink.h>
# include <linux/rtnetlink.h>
#else
# include <sys/types.h>
# include <sys/socket.h>
  typedef uint8_t __u8;
  typedef uint16_t __u16;
  typedef uint32_t __u32;
  typedef int32_t __s32;
  
  /* Minimal stub for non-Linux platforms */
  struct sockaddr_nl {
      uint16_t nl_family;
      uint16_t nl_pad;
      uint32_t nl_pid;
      uint32_t nl_groups;
  };
#endif
```

---

## Configuration Changes (.config)

The following Linux-specific utilities were disabled as they require Linux kernel features not available on macOS:

### Disabled Utilities:

```
# CONFIG_BLKDISCARD is not set       # Requires linux/fs.h
# CONFIG_FALLOCATE is not set        # Requires posix_fallocate
# CONFIG_FSFREEZE is not set         # Requires linux/fs.h
# CONFIG_FSTRIM is not set            # Requires linux/fs.h
# CONFIG_IPNEIGH is not set           # Requires AF_PACKET, linux/neighbour.h
# CONFIG_MKE2FS is not set            # Requires linux/fs.h
# CONFIG_MKDOSFS is not set           # Requires linux/hdreg.h
# CONFIG_NSENTER is not set           # Requires CLONE_NEWNS, setns()
# CONFIG_PARTPROBE is not set         # Requires linux/fs.h
# CONFIG_RUN_INIT is not set          # Requires sys/vfs.h
# CONFIG_SEEDRNG is not set           # Requires linux/random.h
# CONFIG_SETPRIV is not set           # Requires linux/capability.h
# CONFIG_SWAPON is not set            # Requires mntent.h
# CONFIG_SWAPOFF is not set           # Requires mntent.h
# CONFIG_TC is not set                # Requires linux/pkt_sched.h
# CONFIG_UBIRENAME is not set         # Requires mtd/mtd-user.h
# CONFIG_UEVENT is not set            # Requires linux/netlink.h
# CONFIG_UNSHARE is not set           # Requires unshare() syscall
# CONFIG_LINUX32 is not set           # Requires sys/personality.h
# CONFIG_LINUX64 is not set           # Requires sys/personality.h
```

### Enabled Shell:

```
CONFIG_BASH_IS_ASH=y                # Enable 'bash' as alias to 'ash'
```

---

## Build Results

**Binary**: `busybox` (ARM64 Mach-O executable)
**Size**: 659KB (stripped), 752KB (unstripped)
**Platform**: macOS ARM64 (Apple Silicon)
**Available Applets**: ~300+ utilities

### Building on macOS:
```bash
# Use the macOS default configuration
make osx_defconfig

# Build
make -j$(sysctl -n hw.ncpu)

# Test
./busybox bash -c "echo 'Bash works!'"
```

### Test:
```bash
$ file busybox
busybox: Mach-O 64-bit executable arm64

$ ./busybox bash -c "echo 'Bash works!'"
Bash works!
```

### Configuration File:
The complete working configuration is saved in `configs/osx_defconfig`. This includes all the necessary settings with Linux-specific utilities disabled and macOS-compatible options enabled.

---

## Summary

This port required:
- **13 source file modifications** for platform compatibility
- **4 build system modifications** for toolchain differences
- **20 utilities disabled** due to Linux kernel dependencies
- **1 configuration change** to enable bash

All changes use conditional compilation (`#ifdef __APPLE__` or `#ifdef __linux__`) to maintain compatibility with existing Linux builds.

---

## Future Work

Potential improvements:
1. Implement macOS equivalents for some disabled utilities where feasible
2. Add more comprehensive testing suite for macOS
3. Consider contributing these changes upstream
4. Add CI/CD support for macOS builds

---

**Date**: October 17, 2025  
**Busybox Version**: 1.38.0.git  
**macOS Version**: macOS 15.0 (Darwin 25.0.0)  
**Architecture**: ARM64 (Apple Silicon)

