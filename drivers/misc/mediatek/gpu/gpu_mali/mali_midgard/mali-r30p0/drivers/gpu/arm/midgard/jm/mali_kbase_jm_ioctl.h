/*
 *
 * (C) COPYRIGHT 2020 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#ifndef _KBASE_JM_IOCTL_H_
#define _KBASE_JM_IOCTL_H_

#include <asm-generic/ioctl.h>
#include <linux/types.h>
/*
 * 11.1:
 * - Add BASE_MEM_TILER_ALIGN_TOP under base_mem_alloc_flags
 * 11.2:
 * - KBASE_MEM_QUERY_FLAGS can return KBASE_REG_PF_GROW and KBASE_REG_SECURE,
 *   which some user-side clients prior to 11.2 might fault if they received
 *   them
 * 11.3:
 * - New ioctls KBASE_IOCTL_STICKY_RESOURCE_MAP and
 *   KBASE_IOCTL_STICKY_RESOURCE_UNMAP
 * 11.4:
 * - New ioctl KBASE_IOCTL_MEM_FIND_GPU_START_AND_OFFSET
 * 11.5:
 * - New ioctl: KBASE_IOCTL_MEM_JIT_INIT (old ioctl renamed to _OLD)
 * 11.6:
 * - Added flags field to base_jit_alloc_info structure, which can be used to
 *   specify pseudo chunked tiler alignment for JIT allocations.
 * 11.7:
 * - Removed UMP support
 * 11.8:
 * - Added BASE_MEM_UNCACHED_GPU under base_mem_alloc_flags
 * 11.9:
 * - Added BASE_MEM_PERMANENT_KERNEL_MAPPING and BASE_MEM_FLAGS_KERNEL_ONLY
 *   under base_mem_alloc_flags
 * 11.10:
 * - Enabled the use of nr_extres field of base_jd_atom_v2 structure for
 *   JIT_ALLOC and JIT_FREE type softjobs to enable multiple JIT allocations
 *   with one softjob.
 * 11.11:
 * - Added BASE_MEM_GPU_VA_SAME_4GB_PAGE under base_mem_alloc_flags
 * 11.12:
 * - Removed ioctl: KBASE_IOCTL_GET_PROFILING_CONTROLS
 * 11.13:
 * - New ioctl: KBASE_IOCTL_MEM_EXEC_INIT
 */
#define BASE_UK_VERSION_MAJOR 11
#define BASE_UK_VERSION_MINOR 13

/**
 * struct kbase_ioctl_job_submit - Submit jobs/atoms to the kernel
 *
 * @addr: Memory address of an array of struct base_jd_atom_v2
 * @nr_atoms: Number of entries in the array
 * @stride: sizeof(struct base_jd_atom_v2)
 */
struct kbase_ioctl_job_submit {
	__u64 addr;
	__u32 nr_atoms;
	__u32 stride;
};

#define KBASE_IOCTL_JOB_SUBMIT \
	_IOW(KBASE_IOCTL_TYPE, 2, struct kbase_ioctl_job_submit)

#define KBASE_IOCTL_POST_TERM \
	_IO(KBASE_IOCTL_TYPE, 4)

/**
 * struct kbase_ioctl_soft_event_update - Update the status of a soft-event
 * @event: GPU address of the event which has been updated
 * @new_status: The new status to set
 * @flags: Flags for future expansion
 */
struct kbase_ioctl_soft_event_update {
	__u64 event;
	__u32 new_status;
	__u32 flags;
};

#define KBASE_IOCTL_SOFT_EVENT_UPDATE \
	_IOW(KBASE_IOCTL_TYPE, 28, struct kbase_ioctl_soft_event_update)

#endif /* _KBASE_JM_IOCTL_H_ */
