/* SPDX-License-Identifier: GPL-2.0 */

#ifndef MMAP_LOCK_H
#define MMAP_LOCK_H

#include <linux/version.h>

#ifndef RHEL_RELEASE_CODE
#define RHEL_RELEASE_CODE 0
#endif

#ifndef RHEL_RELEASE_VERSION
#define RHEL_RELEASE_VERSION(a,b) 1
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,8,0) && RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(8,8)
static inline void mmap_write_lock(struct mm_struct *mm)
{
	down_write(&mm->mmap_sem);
}

static inline void mmap_write_unlock(struct mm_struct *mm)
{
	up_write(&mm->mmap_sem);
}
#else
#include <linux/mmap_lock.h>
#endif

#endif
