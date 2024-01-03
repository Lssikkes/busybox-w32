/* vi: set sw=4 ts=4: */
/*
 * Trick to assign a const ptr with barrier for clang
 *
 * Copyright (C) 2021 by YU Jincheng <shana@zju.edu.cn>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"

#if defined(__clang_major__) && __clang_major__ >= 9
void FAST_FUNC XZALLOC_CONST_PTR(const void *pptr, size_t size)
{
	ASSIGN_CONST_PTR(pptr, xzalloc(size));
}

# if ENABLE_PLATFORM_MINGW32
void FAST_FUNC ASSIGN_CONST_PTR(const void *pptr, const void *v)
{
	do {
		*(void**)not_const_pp(pptr) = (void*)(v);
		barrier();
	} while (0);
}
# endif
#endif
