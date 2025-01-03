// SPDX-License-Identifier: GPL-2.0
/*
 * drivers/staging/android/mtk_ion/ion_dummy_driver.c
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/memblock.h>
#include <linux/sizes.h>
#include <linux/io.h>
#include "ion.h"
#include "ion_priv.h"

static struct ion_device *idev;
static struct ion_heap **heaps;

static void *carveout_ptr;
static void *chunk_ptr;

static struct ion_platform_heap dummy_heaps[] = {
		{
			.id	= ION_HEAP_TYPE_SYSTEM,
			.type	= ION_HEAP_TYPE_SYSTEM,
			.name	= "system",
		},
		{
			.id	= ION_HEAP_TYPE_CARVEOUT,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= "carveout",
			.size	= SZ_4M,
		},
		{
			.id	= ION_HEAP_TYPE_CHUNK,
			.type	= ION_HEAP_TYPE_CHUNK,
			.name	= "chunk",
			.size	= SZ_4M,
			.align	= SZ_16K,
			.priv	= (void *)(SZ_16K),
		},
};

static struct ion_platform_data dummy_ion_pdata = {
	.nr = ARRAY_SIZE(dummy_heaps),
	.heaps = dummy_heaps,
};

static int __init ion_dummy_init(void)
{
	int i, err;

	idev = ion_device_create(NULL);
	if (IS_ERR(idev))
		return PTR_ERR(idev);
	heaps = kcalloc(dummy_ion_pdata.nr, sizeof(struct ion_heap *),
			GFP_KERNEL);
	if (!heaps)
		return -ENOMEM;


	for (i = 0; i < dummy_ion_pdata.nr; i++) {
		struct ion_platform_heap *heap_data = &dummy_ion_pdata.heaps[i];

		if (heap_data->type == ION_HEAP_TYPE_CARVEOUT) {
			/* Allocate a dummy carveout heap */
			carveout_ptr = alloc_pages_exact(heap_data->size,
							 GFP_KERNEL);
			if (!carveout_ptr) {
				pr_err("ion_dummy: Could not allocate carveout\n");
				continue;
			}
			heap_data->base = virt_to_phys(carveout_ptr);
		}

		if (heap_data->type == ION_HEAP_TYPE_CHUNK) {
			/* Allocate a dummy chunk heap */
			chunk_ptr = alloc_pages_exact(heap_data->size,
							GFP_KERNEL);
			if (!chunk_ptr) {
				pr_err("ion_dummy: Could not allocate chunk\n");
				continue;
			}
			heap_data->base = virt_to_phys(chunk_ptr);
		}

		heaps[i] = ion_heap_create(heap_data);
		if (IS_ERR_OR_NULL(heaps[i])) {
			err = PTR_ERR(heaps[i]);
			goto err;
		}
		ion_device_add_heap(idev, heaps[i]);
	}
	return 0;
err:
	for (i = 0; i < dummy_ion_pdata.nr; ++i) {
		struct ion_platform_heap *heap_data = &dummy_ion_pdata.heaps[i];

		if (!IS_ERR_OR_NULL(heaps[i]))
			ion_heap_destroy(heaps[i]);

		if (heap_data->type == ION_HEAP_TYPE_CARVEOUT) {
			if (carveout_ptr) {
				free_pages_exact(carveout_ptr,
						 heap_data->size);
			}
			carveout_ptr = NULL;
		}
		if (heap_data->type == ION_HEAP_TYPE_CHUNK) {
			if (chunk_ptr) {
				free_pages_exact(chunk_ptr, heap_data->size);
			}
			chunk_ptr = NULL;
		}
	}
	kfree(heaps);

	return err;
}
device_initcall(ion_dummy_init);

static void __exit ion_dummy_exit(void)
{
	int i;

	ion_device_destroy(idev);

	for (i = 0; i < dummy_ion_pdata.nr; ++i) {
		struct ion_platform_heap *heap_data = &dummy_ion_pdata.heaps[i];

		if (!IS_ERR_OR_NULL(heaps[i]))
			ion_heap_destroy(heaps[i]);

		if (heap_data->type == ION_HEAP_TYPE_CARVEOUT) {
			if (carveout_ptr) {
				free_pages_exact(carveout_ptr,
						 heap_data->size);
			}
			carveout_ptr = NULL;
		}
		if (heap_data->type == ION_HEAP_TYPE_CHUNK) {
			if (chunk_ptr) {
				free_pages_exact(chunk_ptr, heap_data->size);
			}
			chunk_ptr = NULL;
		}
	}
	kfree(heaps);
}
__exitcall(ion_dummy_exit);
