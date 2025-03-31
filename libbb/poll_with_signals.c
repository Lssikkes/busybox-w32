/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2025 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//kbuild:lib-$(CONFIG_PLATFORM_POSIX) += poll_with_signals.o

#include "libbb.h"

/* Shells, for example, need their line input and "read" builtin
 * to be interruptible, and the naive handling of it a-la:
 *	if (bb_got_signal) {
 *		errno = EINTR;
 *		return -1;
 *	}
 *	poll(pfd, 1, -1); // signal here would set EINTR
 * is racy.
 * This is a bit heavy-handed, but safe wrt races:
 */
int FAST_FUNC check_got_signal_and_poll(struct pollfd pfd[1], int timeout)
{
	int n;
	struct timespec tv;
	sigset_t orig_mask;

	if (bb_got_signal) /* optimization */
		goto eintr;

	if (timeout >= 0) {
		tv.tv_sec = timeout / 1000;
		tv.tv_nsec = (timeout % 1000) * 1000000;
	}
	/* test bb_got_signal, then poll(), atomically wrt signals */
	sigfillset(&orig_mask);
	sigprocmask2(SIG_BLOCK, &orig_mask);
	if (bb_got_signal) {
		sigprocmask2(SIG_SETMASK, &orig_mask);
 eintr:
		errno = EINTR; /* inform the caller that we got a signal */
		return -1;
	}
#if defined(__APPLE__)
	/* macOS lacks ppoll; pselect provides identical atomic
	 * signal-mask-swap semantics. Convert pollfd -> fd_sets, call
	 * pselect, then translate the result back to revents. */
	{
		fd_set rfds, wfds, efds;
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_ZERO(&efds);
		if (pfd[0].events & POLLIN)  FD_SET(pfd[0].fd, &rfds);
		if (pfd[0].events & POLLOUT) FD_SET(pfd[0].fd, &wfds);
		FD_SET(pfd[0].fd, &efds);
		n = pselect(pfd[0].fd + 1, &rfds, &wfds, &efds,
		            timeout >= 0 ? &tv : NULL, &orig_mask);
		if (n > 0) {
			pfd[0].revents = 0;
			if (FD_ISSET(pfd[0].fd, &rfds)) pfd[0].revents |= POLLIN;
			if (FD_ISSET(pfd[0].fd, &wfds)) pfd[0].revents |= POLLOUT;
			if (FD_ISSET(pfd[0].fd, &efds)) pfd[0].revents |= POLLERR;
		}
	}
#else
	n = ppoll(pfd, 1, timeout >= 0 ? &tv : NULL, &orig_mask);
#endif
	sigprocmask2(SIG_SETMASK, &orig_mask);
	return n;
}
