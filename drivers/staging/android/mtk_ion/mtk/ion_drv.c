/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/export.h>
#ifndef CONFIG_ARM64
#include "mm/dma.h"
#endif

#include <linux/vmalloc.h>
#include "ion_profile.h"
#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif
#include <linux/seq_file.h>
#include "ion_priv.h"
#include "ion_drv_priv.h"
#include "mtk/mtk_ion.h"
#include "mtk/ion_drv.h"
#ifdef CONFIG_PM
#include <linux/fb.h>
#endif

#define ION_FUNC_ENTER
#define ION_FUNC_LEAVE

/* #pragma GCC optimize ("O0") */
#define DEFAULT_PAGE_SIZE 0x1000
#define PAGE_ORDER 12

struct ion_device *g_ion_device;
EXPORT_SYMBOL(g_ion_device);

#ifndef dmac_map_area
#define dmac_map_area __dma_map_area
#endif
#ifndef dmac_unmap_area
#define dmac_unmap_area __dma_unmap_area
#endif

#ifndef dmac_flush_range
#define dmac_flush_range __dma_flush_range
#endif

#define __ION_CACHE_SYNC_USER_VA_EN__

static int ion_cache_sync_kernel(unsigned long start, size_t size,
				 enum ION_CACHE_SYNC_TYPE sync_type)
{
	unsigned long end = start + size;

	start = (start / L1_CACHE_BYTES * L1_CACHE_BYTES);
	size =
	    (end - start + L1_CACHE_BYTES -
	     1) / L1_CACHE_BYTES * L1_CACHE_BYTES;
	/* L1 cache sync */
	if ((sync_type == ION_CACHE_CLEAN_BY_RANGE) ||
	    (sync_type == ION_CACHE_CLEAN_BY_RANGE_USE_VA)) {
		mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_CLEAN_RANGE],
				 MMPROFILE_FLAG_START, size, start);
		dmac_map_area((void *)start, size, DMA_TO_DEVICE);
	} else if ((sync_type == ION_CACHE_INVALID_BY_RANGE) ||
		   (sync_type == ION_CACHE_INVALID_BY_RANGE_USE_VA)) {
		mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_INVALID_RANGE],
				 MMPROFILE_FLAG_START, size, start);
		dmac_unmap_area((void *)start, size, DMA_FROM_DEVICE);
	} else if ((sync_type == ION_CACHE_FLUSH_BY_RANGE) ||
		   (sync_type == ION_CACHE_FLUSH_BY_RANGE_USE_VA)) {
		mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_FLUSH_RANGE],
				 MMPROFILE_FLAG_START, size, start);
#ifdef CONFIG_ARM64
		__dma_flush_area((void *)start, size);
#else
		dmac_flush_range((void *)start, (void *)(start + size - 1));
#endif
	}

	return 0;
}

static struct vm_struct *cache_map_vm_st;
static int ion_cache_sync_init(void)
{
	cache_map_vm_st = get_vm_area(PAGE_SIZE, VM_ALLOC);
	if (!cache_map_vm_st)
		return -ENOMEM;

	return 0;
}

static void *ion_cache_map_page_va(struct page *page)
{
	int ret;
	struct page **ppage = &page;

	ret = map_vm_area(cache_map_vm_st, PAGE_KERNEL, ppage);
	if (ret) {
		IONMSG("error to map page\n");
		return NULL;
	}
	return cache_map_vm_st->addr;
}

static void ion_cache_unmap_page_va(unsigned long va)
{
	unmap_kernel_range((unsigned long)cache_map_vm_st->addr, PAGE_SIZE);
}

/* lock to protect cache_map_vm_st */
static DEFINE_MUTEX(ion_cache_sync_lock);

static long ion_sys_cache_sync(struct ion_client *client,
			       struct ion_sys_cache_sync_param *param,
			       int from_kernel)
{
	enum ION_CACHE_SYNC_TYPE sync_type = param->sync_type;

	ION_FUNC_ENTER;
	if (sync_type < ION_CACHE_CLEAN_ALL) {
		/* By range operation */
		unsigned long start = -1;
		struct ion_handle *kernel_handle;

		kernel_handle = ion_drv_get_handle(client, param->handle,
						   param->kernel_handle,
						   from_kernel);
		if (IS_ERR(kernel_handle)) {
			IONMSG("%s fail!\n", __func__);
			return -EINVAL;
		}

		{
			struct ion_buffer *buffer;
			struct scatterlist *sg;
			int i, j;
			struct sg_table *table = NULL;
			int npages = 0;

			mutex_lock(&client->lock);

			buffer = kernel_handle->buffer;

			table = buffer->sg_table;
			npages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;
			mutex_lock(&ion_cache_sync_lock);

			if (!cache_map_vm_st) {
				IONMSG("%s warn: vm_struct is retry\n",
				       __func__);
				ion_cache_sync_init();
			}

			if (!cache_map_vm_st) {
				IONMSG("%s error: vm_struct is NULL!\n",
				       __func__);
				mutex_unlock(&ion_cache_sync_lock);
				mutex_unlock(&client->lock);
				ion_drv_put_kernel_handle(kernel_handle);
				return -ENOMEM;
			}

			for_each_sg(table->sgl, sg, table->nents, i) {
				int npages_this_entry =
				    PAGE_ALIGN(sg->length) / PAGE_SIZE;
				struct page *page = sg_page(sg);

				if (!page) {
					phys_addr_t pa = sg_dma_address(sg);

					if (!pa) {
						IONMSG("%s fail, no page\n",
						       __func__);
						mutex_unlock
						    (&ion_cache_sync_lock);
						mutex_unlock(&client->lock);
						ion_drv_put_kernel_handle
						    (kernel_handle);
						return -EFAULT;
					}
					page = phys_to_page(pa);
					if (!pfn_valid(page_to_pfn(page))) {
						IONMSG
						    ("%s fail, page invalid\n",
						     __func__);
						mutex_unlock
						    (&ion_cache_sync_lock);
						mutex_unlock(&client->lock);
						ion_drv_put_kernel_handle
						    (kernel_handle);
						return -EFAULT;
					}
				}

				if (i >= npages) {
					IONMSG("%s warn:pg is %d, npg=%d\n",
					       __func__, i, npages);
					break;
				}
				for (j = 0; j < npages_this_entry; j++) {
					start =
					    (unsigned long)
					    ion_cache_map_page_va(page++);

					if (IS_ERR_OR_NULL((void *)start)) {
						IONMSG
						    ("can't cache sync: %lu\n",
						     start);
						mutex_unlock
						    (&ion_cache_sync_lock);
						mutex_unlock(&client->lock);
						ion_drv_put_kernel_handle(kernel_handle);
						return -EFAULT;
					}

					ion_cache_sync_kernel(start,
							      PAGE_SIZE,
							      sync_type);

					ion_cache_unmap_page_va(start);
				}
			}

			mutex_unlock(&ion_cache_sync_lock);
			mutex_unlock(&client->lock);
		}

		ion_drv_put_kernel_handle(kernel_handle);

		if (sync_type == ION_CACHE_CLEAN_BY_RANGE)
			mmprofile_log_ex(ion_mmp_events
					 [PROFILE_DMA_CLEAN_RANGE],
					 MMPROFILE_FLAG_END, size, 0);
		else if (sync_type == ION_CACHE_INVALID_BY_RANGE)
			mmprofile_log_ex(ion_mmp_events
					 [PROFILE_DMA_INVALID_RANGE],
					 MMPROFILE_FLAG_END, size, 0);
		else if (sync_type == ION_CACHE_FLUSH_BY_RANGE)
			mmprofile_log_ex(ion_mmp_events
					 [PROFILE_DMA_FLUSH_RANGE],
					 MMPROFILE_FLAG_END, size, 0);

	} else {
		/* All cache operation */
		if (sync_type == ION_CACHE_CLEAN_ALL) {
			mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_CLEAN_ALL],
					 MMPROFILE_FLAG_START, 1, 1);
			smp_inner_dcache_flush_all();
			/* outer_clean_all(); */
			mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_CLEAN_ALL],
					 MMPROFILE_FLAG_END, 1, 1);
		} else if (sync_type == ION_CACHE_INVALID_ALL) {
			mmprofile_log_ex(ion_mmp_events
					 [PROFILE_DMA_INVALID_ALL],
					 MMPROFILE_FLAG_START, 1, 1);
			smp_inner_dcache_flush_all();
			/* outer_inv_all(); */
			/* outer_flush_all(); */
			mmprofile_log_ex(ion_mmp_events
					 [PROFILE_DMA_INVALID_ALL],
					 MMPROFILE_FLAG_END, 1, 1);
		} else if (sync_type == ION_CACHE_FLUSH_ALL) {
			mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_FLUSH_ALL],
					 MMPROFILE_FLAG_START, 1, 1);
			smp_inner_dcache_flush_all();
			/* outer_flush_all(); */
			mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_FLUSH_ALL],
					 MMPROFILE_FLAG_END, 1, 1);
		}
	}
	ION_FUNC_LEAVE;
	return 0;
}

int ion_sys_copy_client_name(const char *src, char *dst)
{
	int i;

	for (i = 0; i < ION_MM_DBG_NAME_LEN - 1; i++)
		dst[i] = src[i];

	dst[ION_MM_DBG_NAME_LEN - 1] = '\0';

	return 0;
}

void ion_cache_flush_all(void)
{
	mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_FLUSH_ALL],
			 MMPROFILE_FLAG_START, 1, 1);
	/* IONMSG("[ion_cache_flush_all]: ION cache flush all.\n"); */
	smp_inner_dcache_flush_all();
	/* outer_clean_all(); */
	mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_FLUSH_ALL],
			 MMPROFILE_FLAG_END, 1, 1);
}

static long ion_sys_ioctl(struct ion_client *client, unsigned int cmd,
			  unsigned long arg, int from_kernel)
{
	struct ion_sys_data param;
	long ret = 0;
	unsigned long ret_copy = 0;
	ion_phys_addr_t phy_addr;

	ION_FUNC_ENTER;
	if (!arg) {
		IONMSG("%s:err arg = NULL. %s(%s),%d, k:%d\n",
		       __func__, client->name, client->dbg_name,
		       client->pid, from_kernel);
		ret = -EINVAL;
		goto err;
	}

	if (from_kernel) {
		param = *(struct ion_sys_data *)arg;
	} else {
		ret_copy =
		    copy_from_user(&param, (void __user *)arg,
				   sizeof(struct ion_sys_data));
		if (ret_copy != 0) {
			IONMSG("%s:err arg copy failed, ret_copy = %lu. %s(%s),%d, k:%d\n",
			       __func__, ret_copy, client->name, client->dbg_name,
			       client->pid, from_kernel);
			ret = -EFAULT;
			goto err;
		}
	}

	switch (param.sys_cmd) {
	case ION_SYS_CACHE_SYNC:
		ret =
		    ion_sys_cache_sync(client, &param.cache_sync_param,
				       from_kernel);
		break;
	case ION_SYS_GET_PHYS:
		{
			struct ion_handle *kernel_handle;

			phy_addr = param.get_phys_param.phy_addr;
			kernel_handle =
			    ion_drv_get_handle(client,
					       param.get_phys_param.handle,
					       param.get_phys_param.
					       kernel_handle, from_kernel);
			if (IS_ERR(kernel_handle)) {
				IONMSG("ION_PHYS:err handle %s(%s),%d, k:%d\n",
				       client->name, client->dbg_name,
				       client->pid, from_kernel);
				ret = -EINVAL;
				break;
			}

			if (ion_phys(client, kernel_handle, &phy_addr,
				     (size_t *)&param.get_phys_param.len) <
			    0) {
				param.get_phys_param.phy_addr = 0;
				param.get_phys_param.len = 0;
				IONMSG(" %s: Error. Cannot get PA.\n",
				       __func__);
				ret = -EFAULT;
			}
			param.get_phys_param.phy_addr = (unsigned int)phy_addr;
			ion_drv_put_kernel_handle(kernel_handle);
		}
		break;
	case ION_SYS_SET_CLIENT_NAME:
		ret = ion_sys_copy_client_name(param.client_name_param.name,
					       client->dbg_name);
		break;
	default:
		IONMSG("[ion_sys_ioctl]: Error. Invalid command(%d).\n",
		       param.sys_cmd);
		ret = -EFAULT;
		break;
	}

	if (ret) {
		IONMSG("[%s]:failed to finish io-cmd(%d). %s(%s),%d, k:%d\n",
		       __func__, param.sys_cmd,
		       client->name, client->dbg_name,
		       client->pid, from_kernel);
		goto err;
	}

	if (from_kernel)
		*(struct ion_sys_data *)arg = param;
	else
		ret_copy =
		    copy_to_user((void __user *)arg, &param,
				 sizeof(struct ion_sys_data));
err:
	ION_FUNC_LEAVE;
	return ret;
}

static long _ion_ioctl(struct ion_client *client, unsigned int cmd,
		       unsigned long arg, int from_kernel)
{
	long ret = 0;

	ION_FUNC_ENTER;
	switch (cmd) {
	case ION_CMD_SYSTEM:
		ret = ion_sys_ioctl(client, cmd, arg, from_kernel);
		break;
	case ION_CMD_MULTIMEDIA:
		ret = ion_mm_ioctl(client, cmd, arg, from_kernel);
		break;
	}
	ION_FUNC_LEAVE;
	return ret;
}

long ion_kernel_ioctl(struct ion_client *client, unsigned int cmd,
		      unsigned long arg)
{
	return _ion_ioctl(client, cmd, arg, 1);
}
EXPORT_SYMBOL(ion_kernel_ioctl);

static long ion_custom_ioctl(struct ion_client *client, unsigned int cmd,
			     unsigned long arg)
{
	return _ion_ioctl(client, cmd, arg, 0);
}

#ifdef CONFIG_PM
/* FB event notifier */
static int ion_fb_event(struct notifier_block *notifier, unsigned long event,
			void *data)
{
	struct fb_event *fb_event = data;
	int blank;

	if (event != FB_EVENT_BLANK)
		return NOTIFY_DONE;

	blank = *(int *)fb_event->data;

	switch (blank) {
	case FB_BLANK_UNBLANK:
		break;
	case FB_BLANK_NORMAL:
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_POWERDOWN:
		IONMSG("%s: + screen-off +\n", __func__);
		shrink_ion_by_scenario(1);
		IONMSG("%s: - screen-off -\n", __func__);
		break;
	default:
		return -EINVAL;
	}

	return NOTIFY_OK;
}

static struct notifier_block ion_fb_notifier_block = {
	.notifier_call = ion_fb_event,
	.priority = 1,		/* Just exceeding 0 for higher priority */
};
#endif

struct ion_heap *ion_mtk_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_heap *heap = NULL;

	switch ((int)heap_data->type) {
	case ION_HEAP_TYPE_MULTIMEDIA:
		heap = ion_mm_heap_create(heap_data);
		break;
	case ION_HEAP_TYPE_FB:
		heap = ion_fb_heap_create(heap_data);
		break;
	case ION_HEAP_TYPE_MULTIMEDIA_SEC:
		heap = ion_sec_heap_create(heap_data);
		break;
	default:
		heap = ion_heap_create(heap_data);
	}

	if (IS_ERR_OR_NULL(heap)) {
		IONMSG("%s: error creat heap %s type %d base %lu size %zu\n",
		       __func__, heap_data->name, heap_data->type,
		       heap_data->base, heap_data->size);
		return ERR_PTR(-EINVAL);
	}

	heap->name = heap_data->name;
	heap->id = heap_data->id;
	return heap;
}

void ion_mtk_heap_destroy(struct ion_heap *heap)
{
	if (!heap)
		return;

	switch ((int)heap->type) {
	case ION_HEAP_TYPE_MULTIMEDIA:
		ion_mm_heap_destroy(heap);
		break;
	case ION_HEAP_TYPE_FB:
		ion_fb_heap_destroy(heap);
		break;
	case ION_HEAP_TYPE_MULTIMEDIA_SEC:
		ion_sec_heap_destroy(heap);
		break;
	default:
		ion_heap_destroy(heap);
	}
}

int ion_drv_create_heap(struct ion_platform_heap *heap_data)
{
	struct ion_heap *heap;

	heap = ion_mtk_heap_create(heap_data);
	if (IS_ERR_OR_NULL(heap)) {
		IONMSG("%s: %d heap is err or null.\n", __func__,
		       heap_data->id);
		return PTR_ERR(heap);
	}
	heap->name = heap_data->name;
	heap->id = heap_data->id;
	ion_device_add_heap(g_ion_device, heap);

	IONMSG("%s: create heap: %s\n", __func__, heap->name);
	return 0;
}

int ion_device_destroy_heaps(struct ion_device *dev)
{
	struct ion_heap *heap, *tmp;

	down_write(&dev->lock);

	plist_for_each_entry_safe(heap, tmp, &dev->heaps, node) {
		plist_del((struct plist_node *)heap, &dev->heaps);
		ion_mtk_heap_destroy(heap);
	}

	up_write(&dev->lock);

	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS) || IS_ENABLED(CONFIG_PROC_FS)
/*for clients ion mm heap summary size*/
static int ion_clients_summary_show(struct seq_file *s, void *unused)
{
	struct ion_device *dev = g_ion_device;
	struct rb_node *n, *m;
	int buffer_size = 0;
	unsigned int id = 0;
	enum mtk_ion_heap_type cam_heap = ION_HEAP_TYPE_MULTIMEDIA_FOR_CAMERA;
	enum mtk_ion_heap_type mm_heap = ION_HEAP_TYPE_MULTIMEDIA;

	if (!down_read_trylock(&dev->lock))
		return 0;
	seq_printf(s, "%-16.s %-8.s %-8.s\n", "client_name", "pid", "size");
	seq_puts(s, "------------------------------------------\n");
	for (n = rb_first(&dev->clients); n; n = rb_next(n)) {
		struct ion_client *client =
		    rb_entry(n, struct ion_client, node);
		{
			mutex_lock(&client->lock);
			for (m = rb_first(&client->handles); m;
			     m = rb_next(m)) {
				struct ion_handle *handle =
				    rb_entry(m, struct ion_handle,
					     node);
				id = handle->buffer->heap->id;

				if ((id == mm_heap || id == cam_heap) &&
				    (handle->buffer->handle_count) != 0)
					buffer_size +=
					    (int)(handle->buffer->size) /
					    (handle->buffer->handle_count);
			}
			if (!buffer_size) {
				mutex_unlock(&client->lock);
				continue;
			}
			seq_printf(s, "%-16s %-8d %-8d\n", client->name,
				   client->pid, buffer_size);
			buffer_size = 0;
			mutex_unlock(&client->lock);
		}
	}

	seq_puts(s, "-------------------------------------------\n");
	up_read(&dev->lock);

	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
static int ion_debug_client_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_clients_summary_show, inode->i_private);
}

static const struct file_operations debug_client_fops = {
	.open = ion_debug_client_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
static int ion_proc_client_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_clients_summary_show, PDE_DATA(inode));
}

static const struct file_operations proc_client_fops = {
	.open = ion_proc_client_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif
#endif

static int ion_drv_probe(struct platform_device *pdev)
{
	int i;
	struct ion_platform_data *pdata = pdev->dev.platform_data;
	unsigned int num_heaps = pdata->nr;

	IONMSG("ion_drv_probe() heap_nr=%d\n", pdata->nr);
	g_ion_device = ion_device_create(ion_custom_ioctl);
	if (IS_ERR_OR_NULL(g_ion_device)) {
		IONMSG("ion_device_create() error! device=%p\n", g_ion_device);
		return PTR_ERR(g_ion_device);
	}

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];
		struct ion_heap *heap;

		if (heap_data->type == ION_HEAP_TYPE_CARVEOUT &&
		    heap_data->base == 0) {
			/* reserve for carveout heap failed */
			heap_data->size = 0;
			continue;
		}

		heap = ion_mtk_heap_create(heap_data);

		if (IS_ERR_OR_NULL(heap))
			continue;

		ion_device_add_heap(g_ion_device, heap);
	}

	platform_set_drvdata(pdev, g_ion_device);

	#ifdef CONFIG_XEN
	g_ion_device->dev.this_device->archdata.dev_dma_ops = NULL;
	#endif
	arch_setup_dma_ops(g_ion_device->dev.this_device, 0, 0, NULL, false);

#if IS_ENABLED(CONFIG_DEBUG_FS)
	debugfs_create_file("clients_summary", 0644,
			    g_ion_device->clients_debug_root, NULL,
			    &debug_client_fops);
	debugfs_create_symlink("ion_mm_heap", g_ion_device->debug_root,
			       "./heaps/ion_mm_heap");
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
	proc_create("clients_summary", S_IFREG | 0664,
		    g_ion_device->clients_proc_root,
		    &proc_client_fops);
	proc_symlink("ion_mm_heap", g_ion_device->proc_root,
		     "./heaps/ion_mm_heap");
#endif

	ion_history_init();

	ion_profile_init();

	return 0;
}

int ion_drv_remove(struct platform_device *pdev)
{
	struct ion_device *idev = platform_get_drvdata(pdev);

	ion_device_destroy_heaps(idev);
	ion_device_destroy(idev);
	return 0;
}

static struct ion_platform_heap ion_drv_platform_heaps[] = {
	{
	 .type = (unsigned int)ION_HEAP_TYPE_SYSTEM_CONTIG,
	 .id = ION_HEAP_TYPE_SYSTEM_CONTIG,
	 .name = "ion_system_contig_heap",
	 .base = 0,
	 .size = 0,
	 .align = 0,
	 .priv = NULL,
	 },
	{
	 .type = (unsigned int)ION_HEAP_TYPE_MULTIMEDIA,
	 .id = ION_HEAP_TYPE_MULTIMEDIA,
	 .name = "ion_mm_heap",
	 .base = 0,
	 .size = 0,
	 .align = 0,
	 .priv = NULL,
	 },
	{
	 .type = (unsigned int)ION_HEAP_TYPE_MULTIMEDIA,
	 .id = ION_HEAP_TYPE_MULTIMEDIA_FOR_CAMERA,
	 .name = "ion_mm_heap_for_camera",
	 .base = 0,
	 .size = 0,
	 .align = 0,
	 .priv = NULL,
	 },
	{
	 .type = (unsigned int)ION_HEAP_TYPE_MULTIMEDIA_SEC,
	 .id = ION_HEAP_TYPE_MULTIMEDIA_SEC,
	 .name = "ion_sec_heap",
	 .base = 0,
	 .size = 0,
	 .align = 0,
	 .priv = NULL,
	 },
	{
	 .type = (unsigned int)ION_HEAP_TYPE_CARVEOUT,
	 .id = ION_HEAP_TYPE_CARVEOUT,
	 .name = "ion_carveout_heap",
	 .base = 0,
	 .size = 0,		/* reserve size, align to Mbytes; */
	 .align = 0x1000,	/* this must not be 0 if enable */
	 .priv = NULL,
	 },
};

struct ion_platform_data ion_drv_platform_data = {
	.nr = ARRAY_SIZE(ion_drv_platform_heaps),
	.heaps = ion_drv_platform_heaps,
};

static struct platform_driver ion_driver = {
		.probe = ion_drv_probe,
		.remove = ion_drv_remove,
		.driver = {
				.name = "ion-drv"
		}
};

static struct platform_device ion_device = {
	.name = "ion-drv",
	.id = 0,
	.dev = {
		.platform_data = &ion_drv_platform_data,
		},

};

static int __init ion_init(void)
{
	IONMSG("ion_init()\n");
	if (platform_device_register(&ion_device)) {
		IONMSG("%s platform device register failed.\n", __func__);
		return -ENODEV;
	}
	if (platform_driver_register(&ion_driver)) {
		platform_device_unregister(&ion_device);
		IONMSG("%s platform driver register failed.\n", __func__);
		return -ENODEV;
	}

#ifdef CONFIG_PM
	if (!fb_register_client(&ion_fb_notifier_block))
		IONMSG("%s fd register notifer fail\n", __func__);
#endif
	return 0;
}

static void __exit ion_exit(void)
{
	IONMSG("ion_exit()\n");
	platform_driver_unregister(&ion_driver);
	platform_device_unregister(&ion_device);
}

fs_initcall(ion_init);
__exitcall(ion_exit);
/*module_exit(ion_exit);*/
