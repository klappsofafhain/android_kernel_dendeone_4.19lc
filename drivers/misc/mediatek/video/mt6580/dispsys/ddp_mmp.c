// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 *
 */


#define LOG_TAG "MMP"

#include "m4u.h"
#include "DpDataType.h"
#include "ddp_mmp.h"
#include "ddp_reg.h"
#include "disp_log.h"

static struct DDP_MMP_Events_t DDP_MMP_Events;

void init_ddp_mmp_events(void)
{
	if (DDP_MMP_Events.DDP == 0) {
		DDP_MMP_Events.DDP = mmprofile_register_event(MMP_ROOT_EVENT, "Display");
		DDP_MMP_Events.primary_Parent =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "primary_disp");
		DDP_MMP_Events.primary_trigger =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "trigger");
		DDP_MMP_Events.primary_config =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "ovl_config");
		DDP_MMP_Events.primary_rdma_config =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "rdma_config");
		DDP_MMP_Events.primary_wdma_config =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "wdma_config");
		DDP_MMP_Events.primary_set_dirty =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "set_dirty");
		DDP_MMP_Events.primary_cmdq_flush =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "cmdq_flush");
		DDP_MMP_Events.primary_cmdq_done =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "cmdq_done");
		DDP_MMP_Events.primary_display_cmd =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "display_io");
		DDP_MMP_Events.primary_suspend =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "suspend");
		DDP_MMP_Events.primary_resume =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "resume");
		DDP_MMP_Events.primary_cache_sync =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "cache_sync");
		DDP_MMP_Events.primary_wakeup =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "wakeup");
		DDP_MMP_Events.interface_trigger =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "interface_trigger");
		DDP_MMP_Events.primary_switch_mode =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "switch_session_mode");

		DDP_MMP_Events.dsi_frame_done =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "DSI_FRAME_DONE");
		DDP_MMP_Events.dsi_lfr_switch =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "DSI_LFR_SWITCH");
		DDP_MMP_Events.Dsi_Update =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "DSI_LFR_UPDATE");

		DDP_MMP_Events.primary_seq_insert =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "seq_insert");
		DDP_MMP_Events.primary_seq_config =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "seq_config");
		DDP_MMP_Events.primary_seq_trigger =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "seq_trigger");
		DDP_MMP_Events.primary_seq_rdma_irq =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "seq_rdma_irq");
		DDP_MMP_Events.primary_seq_release =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "seq_release");

		DDP_MMP_Events.primary_ovl_fence_release =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "ovl_fence_r");
		DDP_MMP_Events.primary_wdma_fence_release =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "wdma_fence_r");

		DDP_MMP_Events.present_fence_release =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "preset_fence_release");
		DDP_MMP_Events.present_fence_get =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "preset_fence_get");
		DDP_MMP_Events.present_fence_set =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "preset_fence_set");

		DDP_MMP_Events.idlemgr =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "idlemgr");
		DDP_MMP_Events.primary_error =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "primary_error");

#ifdef CONFIG_MTK_HDMI_SUPPORT
		DDP_MMP_Events.Extd_Parent = mmprofile_register_event(DDP_MMP_Events.DDP, "ext_disp");
		DDP_MMP_Events.Extd_State =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_State");
		DDP_MMP_Events.Extd_DevInfo =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_DevInfo");
		DDP_MMP_Events.Extd_ErrorInfo =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_ErrorInfo");
		DDP_MMP_Events.Extd_Mutex =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_Mutex");
		DDP_MMP_Events.Extd_ImgDump =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_ImgDump");
		DDP_MMP_Events.Extd_IrqStatus =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_IrqStatus");
		DDP_MMP_Events.Extd_UsedBuff =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_UsedBuf");
		DDP_MMP_Events.Extd_trigger =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_trigger");
		DDP_MMP_Events.Extd_config =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_config");
		DDP_MMP_Events.Extd_set_dirty =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_set_dirty");
		DDP_MMP_Events.Extd_cmdq_flush =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_cmdq_flush");
		DDP_MMP_Events.Extd_cmdq_done =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_cmdq_done");

#endif

		DDP_MMP_Events.primary_display_aalod_trigger =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "primary_aal_trigger");
		DDP_MMP_Events.ESD_Parent = mmprofile_register_event(DDP_MMP_Events.DDP, "ESD");
		DDP_MMP_Events.esd_check_t =
		    mmprofile_register_event(DDP_MMP_Events.ESD_Parent, "ESD_Check");
		DDP_MMP_Events.esd_recovery_t =
		    mmprofile_register_event(DDP_MMP_Events.ESD_Parent, "ESD_Recovery");
		DDP_MMP_Events.esd_extte =
		    mmprofile_register_event(DDP_MMP_Events.esd_check_t, "ESD_Check_EXT_TE");
		DDP_MMP_Events.esd_rdlcm =
		    mmprofile_register_event(DDP_MMP_Events.esd_check_t, "ESD_Check_RD_LCM");
		DDP_MMP_Events.esd_vdo_eint =
		    mmprofile_register_event(DDP_MMP_Events.esd_extte, "ESD_Vdo_EINT");
		DDP_MMP_Events.primary_set_bl =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "set_BL_LCM");
		DDP_MMP_Events.dprec_cpu_write_reg = DDP_MMP_Events.MutexParent =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "dprec_cpu_write_reg");
		DDP_MMP_Events.session_Parent =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "session");

		DDP_MMP_Events.ovl_trigger =
		    mmprofile_register_event(DDP_MMP_Events.session_Parent, "ovl2mem");
		DDP_MMP_Events.layerParent = mmprofile_register_event(DDP_MMP_Events.DDP, "Layer");
		DDP_MMP_Events.layer[0] =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "Layer0");
		DDP_MMP_Events.layer[1] =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "Layer1");
		DDP_MMP_Events.layer[2] =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "Layer2");
		DDP_MMP_Events.layer[3] =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "Layer3");

		DDP_MMP_Events.ovl1_layer[0] =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "Ovl1_Layer0");
		DDP_MMP_Events.ovl1_layer[1] =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "Ovl1_Layer1");
		DDP_MMP_Events.ovl1_layer[2] =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "Ovl1_Layer2");
		DDP_MMP_Events.ovl1_layer[3] =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "Ovl1_Layer3");

		DDP_MMP_Events.layer_dump_parent =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "layerBmpDump");
		DDP_MMP_Events.layer_dump[0] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "layer0_dump");
		DDP_MMP_Events.layer_dump[1] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "Layer1_dump");
		DDP_MMP_Events.layer_dump[2] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "Layer2_dump");
		DDP_MMP_Events.layer_dump[3] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "Layer3_dump");

		DDP_MMP_Events.ovl1layer_dump[0] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "ovl1layer0_dump");
		DDP_MMP_Events.ovl1layer_dump[1] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "ovl1layer1_dump");
		DDP_MMP_Events.ovl1layer_dump[2] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "ovl1layer2_dump");
		DDP_MMP_Events.ovl1layer_dump[3] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "ovl1layer3_dump");

		DDP_MMP_Events.wdma_dump[0] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "wdma0_dump");
		DDP_MMP_Events.wdma_dump[1] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "wdma1_dump");

		DDP_MMP_Events.rdma_dump[0] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "rdma0_dump");
		DDP_MMP_Events.rdma_dump[1] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "rdma1_dump");

		DDP_MMP_Events.ovl_enable =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "ovl_enable_config");
		DDP_MMP_Events.ovl_disable =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "ovl_disable_config");
		DDP_MMP_Events.cascade_enable =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "cascade_enable");
		DDP_MMP_Events.cascade_disable =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "cascade_disable");
		DDP_MMP_Events.ovl1_status =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "ovl1_status");
		DDP_MMP_Events.dpmgr_wait_event_timeout =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "wait_event_timeout");
		DDP_MMP_Events.cmdq_rebuild =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "cmdq_rebuild");
		DDP_MMP_Events.dsi_te = mmprofile_register_event(DDP_MMP_Events.DDP, "dsi_te");

		DDP_MMP_Events.DDP_IRQ = mmprofile_register_event(DDP_MMP_Events.DDP, "DDP_IRQ");
		DDP_MMP_Events.MutexParent =
		    mmprofile_register_event(DDP_MMP_Events.DDP_IRQ, "Mutex");
		DDP_MMP_Events.MUTEX_IRQ[0] =
		    mmprofile_register_event(DDP_MMP_Events.MutexParent, "Mutex0");
		DDP_MMP_Events.MUTEX_IRQ[1] =
		    mmprofile_register_event(DDP_MMP_Events.MutexParent, "Mutex1");
		DDP_MMP_Events.MUTEX_IRQ[2] =
		    mmprofile_register_event(DDP_MMP_Events.MutexParent, "Mutex2");
		DDP_MMP_Events.MUTEX_IRQ[3] =
		    mmprofile_register_event(DDP_MMP_Events.MutexParent, "Mutex3");
		DDP_MMP_Events.MUTEX_IRQ[4] =
		    mmprofile_register_event(DDP_MMP_Events.MutexParent, "Mutex4");
		DDP_MMP_Events.OVL_IRQ_Parent =
		    mmprofile_register_event(DDP_MMP_Events.DDP_IRQ, "OVL_IRQ");
		DDP_MMP_Events.OVL_IRQ[0] =
		    mmprofile_register_event(DDP_MMP_Events.OVL_IRQ_Parent, "OVL_IRQ_0");
		DDP_MMP_Events.OVL_IRQ[1] =
		    mmprofile_register_event(DDP_MMP_Events.OVL_IRQ_Parent, "OVL_IRQ_1");
		DDP_MMP_Events.WDMA_IRQ_Parent =
		    mmprofile_register_event(DDP_MMP_Events.DDP_IRQ, "WDMA_IRQ");
		DDP_MMP_Events.WDMA_IRQ[0] =
		    mmprofile_register_event(DDP_MMP_Events.WDMA_IRQ_Parent, "WDMA_IRQ_0");
		DDP_MMP_Events.WDMA_IRQ[1] =
		    mmprofile_register_event(DDP_MMP_Events.WDMA_IRQ_Parent, "WDMA_IRQ_1");
		DDP_MMP_Events.RDMA_IRQ_Parent =
		    mmprofile_register_event(DDP_MMP_Events.DDP_IRQ, "RDMA_IRQ");
		DDP_MMP_Events.RDMA_IRQ[0] =
		    mmprofile_register_event(DDP_MMP_Events.RDMA_IRQ_Parent, "RDMA_IRQ_0");
		DDP_MMP_Events.RDMA_IRQ[1] =
		    mmprofile_register_event(DDP_MMP_Events.RDMA_IRQ_Parent, "RDMA_IRQ_1");
		DDP_MMP_Events.RDMA_IRQ[2] =
		    mmprofile_register_event(DDP_MMP_Events.RDMA_IRQ_Parent, "RDMA_IRQ_2");
		DDP_MMP_Events.ddp_abnormal_irq =
		    mmprofile_register_event(DDP_MMP_Events.DDP_IRQ, "ddp_abnormal_irq");
		DDP_MMP_Events.SCREEN_UPDATE[0] =
		    mmprofile_register_event(DDP_MMP_Events.session_Parent, "SCREEN_UPDATE_0");
		DDP_MMP_Events.SCREEN_UPDATE[1] =
		    mmprofile_register_event(DDP_MMP_Events.RDMA_IRQ_Parent, "SCREEN_UPDATE_1");
		DDP_MMP_Events.SCREEN_UPDATE[2] =
		    mmprofile_register_event(DDP_MMP_Events.RDMA_IRQ_Parent, "SCREEN_UPDATE_2");
		DDP_MMP_Events.DSI_IRQ_Parent =
		    mmprofile_register_event(DDP_MMP_Events.DDP_IRQ, "DSI_IRQ");
		DDP_MMP_Events.DSI_IRQ[0] =
		    mmprofile_register_event(DDP_MMP_Events.DSI_IRQ_Parent, "DSI_IRQ_0");
		DDP_MMP_Events.DSI_IRQ[1] =
		    mmprofile_register_event(DDP_MMP_Events.DSI_IRQ_Parent, "DSI_IRQ_1");
		DDP_MMP_Events.primary_sw_mutex =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "primary_sw_mutex");
		DDP_MMP_Events.session_release =
		    mmprofile_register_event(DDP_MMP_Events.session_Parent, "session_release");

		DDP_MMP_Events.MonitorParent =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "Monitor");
		DDP_MMP_Events.trigger_delay =
		    mmprofile_register_event(DDP_MMP_Events.MonitorParent, "Trigger Delay");
		DDP_MMP_Events.release_delay =
		    mmprofile_register_event(DDP_MMP_Events.MonitorParent, "Release Delay");
		DDP_MMP_Events.cg_mode =
		    mmprofile_register_event(DDP_MMP_Events.MonitorParent, "SPM CG Mode");
		DDP_MMP_Events.power_down_mode =
		    mmprofile_register_event(DDP_MMP_Events.MonitorParent, "SPM Power Down Mode");
		DDP_MMP_Events.sodi_disable =
		    mmprofile_register_event(DDP_MMP_Events.MonitorParent, "Request CG");
		DDP_MMP_Events.sodi_enable =
		    mmprofile_register_event(DDP_MMP_Events.MonitorParent, "Request Power Down");
		DDP_MMP_Events.vsync_count =
		    mmprofile_register_event(DDP_MMP_Events.MonitorParent, "Vsync Ticket");
		DDP_MMP_Events.LFR_NUM =
		    mmprofile_register_event(DDP_MMP_Events.MonitorParent, "LFR_CURRENT_SKIP_NUM");

		DDP_MMP_Events.dal_clean = mmprofile_register_event(DDP_MMP_Events.DDP, "DAL Clean");
		DDP_MMP_Events.dal_printf =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "DAL Printf");


		DDP_MMP_Events.DSI_IRQ_Parent =
		    mmprofile_register_event(DDP_MMP_Events.DDP_IRQ, "DSI_IRQ");
		DDP_MMP_Events.DSI_IRQ[0] =
		    mmprofile_register_event(DDP_MMP_Events.DSI_IRQ_Parent, "DSI_IRQ_0");
		DDP_MMP_Events.DSI_IRQ[1] =
		    mmprofile_register_event(DDP_MMP_Events.DSI_IRQ_Parent, "DSI_IRQ_1");

		mmprofile_enable_event(DDP_MMP_Events.DDP, 1);
		mmprofile_enable_event(DDP_MMP_Events.layerParent, 1);
		mmprofile_enable_event(DDP_MMP_Events.MutexParent, 1);
		mmprofile_enable_event(DDP_MMP_Events.DDP_IRQ, 1);

		mmprofile_enable_event(DDP_MMP_Events.primary_sw_mutex, 0);
		mmprofile_enable_event(DDP_MMP_Events.primary_seq_insert, 0);
		mmprofile_enable_event(DDP_MMP_Events.primary_seq_config, 0);
		mmprofile_enable_event(DDP_MMP_Events.primary_seq_trigger, 0);
		mmprofile_enable_event(DDP_MMP_Events.primary_seq_rdma_irq, 0);
		mmprofile_enable_event(DDP_MMP_Events.primary_seq_release, 0);
	}
}

void ddp_mmp_ovl_layer(struct OVL_CONFIG_STRUCT *pLayer, unsigned int down_sample_x,
		       unsigned int down_sample_y,
		       unsigned int session /*1:primary, 2:external, 3:memory */)
{
	struct mmp_metadata_bitmap_t Bitmap;
	struct mmp_metadata_t meta;
	int raw = 0;

	if (session == 1)
		mmprofile_log_ex(DDP_MMP_Events.layer_dump_parent, MMPROFILE_FLAG_START, pLayer->layer,
			       pLayer->layer_en);
	else if (session == 2)
		mmprofile_log_ex(DDP_MMP_Events.Extd_layer_dump_parent, MMPROFILE_FLAG_START,
			       pLayer->layer, pLayer->layer_en);

	if (pLayer->layer_en) {
		Bitmap.data1 = pLayer->vaddr;
		Bitmap.width = pLayer->dst_w;
		Bitmap.height = pLayer->dst_h;
		switch (pLayer->fmt) {
		case eRGB565:
		case eBGR565:
			Bitmap.format = MMPROFILE_BITMAP_RGB565;
			Bitmap.bpp = 16;
			break;
		case eRGB888:
			Bitmap.format = MMPROFILE_BITMAP_RGB888;
			Bitmap.bpp = 24;
			break;
		case eBGRA8888:
			Bitmap.format = MMPROFILE_BITMAP_BGRA8888;
			Bitmap.bpp = 32;
			break;
		case eBGR888:
			Bitmap.format = MMPROFILE_BITMAP_BGR888;
			Bitmap.bpp = 24;
			break;
		case eRGBA8888:
			Bitmap.format = MMPROFILE_BITMAP_RGBA8888;
			Bitmap.bpp = 32;
			break;
		case eABGR8888:
			Bitmap.format = MMPROFILE_BITMAP_RGBA8888;
			Bitmap.bpp = 32;
			break;
		default:
			DISPERR("ddp_mmp_ovl_layer(), unknown fmt=0x%x,dump raw\n", pLayer->fmt);
			raw = 1;
		}
		if (!raw) {
			Bitmap.start_pos = 0;
			Bitmap.pitch = pLayer->src_pitch;
			Bitmap.data_size = Bitmap.pitch * Bitmap.height;
			Bitmap.down_sample_x = down_sample_x;
			Bitmap.down_sample_y = down_sample_y;
			if (m4u_mva_map_kernel
			    (pLayer->addr, Bitmap.data_size, (unsigned long *)&Bitmap.p_data,
			     &Bitmap.data_size) == 0) {
				if (session == 1)
					mmprofile_log_meta_bitmap(DDP_MMP_Events.layer_dump
							       [pLayer->layer], MMPROFILE_FLAG_PULSE,
							       &Bitmap);
				else if (session == 2)
					mmprofile_log_meta_bitmap(DDP_MMP_Events.ovl1layer_dump
							       [pLayer->layer], MMPROFILE_FLAG_PULSE,
							       &Bitmap);
				m4u_mva_unmap_kernel(pLayer->addr, Bitmap.data_size,
						     (unsigned long)Bitmap.p_data);
			} else {
				DISPERR("ddp_mmp_ovl_layer(),fail to dump rgb(0x%x)\n", pLayer->fmt);
			}
		} else {
			meta.data_type = MMPROFILE_META_RAW;
			meta.size = pLayer->src_pitch * pLayer->src_h;
			if (m4u_mva_map_kernel
			    (pLayer->addr, meta.size, (unsigned long *)&meta.p_data,
			     &meta.size) == 0) {
				if (session == 1)
					mmprofile_log_meta(DDP_MMP_Events.layer_dump[pLayer->layer],
							 MMPROFILE_FLAG_PULSE, &meta);
				else if (session == 2)
					mmprofile_log_meta(DDP_MMP_Events.ovl1layer_dump
							 [pLayer->layer], MMPROFILE_FLAG_PULSE,
							 &meta);
				m4u_mva_unmap_kernel(pLayer->addr, meta.size,
						     (unsigned long)meta.p_data);
			} else {
				DISPERR("ddp_mmp_ovl_layer(),fail to dump raw(0x%x)\n", pLayer->fmt);
			}
		}
	}

	if (session == 1)
		mmprofile_log_ex(DDP_MMP_Events.layer_dump_parent, MMPROFILE_FLAG_END, pLayer->fmt,
			       pLayer->addr);
	else if (session == 2)
		mmprofile_log_ex(DDP_MMP_Events.Extd_layer_dump_parent, MMPROFILE_FLAG_END, pLayer->fmt,
			       pLayer->addr);
}

void ddp_mmp_wdma_layer(struct WDMA_CONFIG_STRUCT *wdma_layer, unsigned int wdma_num,
			unsigned int down_sample_x, unsigned int down_sample_y)
{
	struct mmp_metadata_bitmap_t Bitmap;
	struct mmp_metadata_t meta;
	int raw = 0;

	if (wdma_num > 1) {
		DISPERR("dprec_mmp_dump_wdma_layer is error %d\n", wdma_num);
		return;
	}

	Bitmap.data1 = wdma_layer->dstAddress;
	Bitmap.width = wdma_layer->srcWidth;
	Bitmap.height = wdma_layer->srcHeight;
	switch (wdma_layer->outputFormat) {
	case eRGB565:
	case eBGR565:
		Bitmap.format = MMPROFILE_BITMAP_RGB565;
		Bitmap.bpp = 16;
		break;
	case eRGB888:
		Bitmap.format = MMPROFILE_BITMAP_RGB888;
		Bitmap.bpp = 24;
		break;
	case eBGRA8888:
		Bitmap.format = MMPROFILE_BITMAP_BGRA8888;
		Bitmap.bpp = 32;
		break;
	case eBGR888:
		Bitmap.format = MMPROFILE_BITMAP_BGR888;
		Bitmap.bpp = 24;
		break;
	case eRGBA8888:
		Bitmap.format = MMPROFILE_BITMAP_RGBA8888;
		Bitmap.bpp = 32;
		break;
	default:
		DISPERR("dprec_mmp_dump_wdma_layer(), unknown fmt=%d, dump raw\n",
		       wdma_layer->outputFormat);
		raw = 1;
	}
	if (!raw) {
		Bitmap.start_pos = 0;
		Bitmap.pitch = wdma_layer->dstPitch;
		Bitmap.data_size = Bitmap.pitch * Bitmap.height;
		Bitmap.down_sample_x = down_sample_x;
		Bitmap.down_sample_y = down_sample_y;
		if (m4u_mva_map_kernel
		    (wdma_layer->dstAddress, Bitmap.data_size, (unsigned long *)&Bitmap.p_data,
		     &Bitmap.data_size) == 0) {
			mmprofile_log_meta_bitmap(DDP_MMP_Events.wdma_dump[wdma_num],
					       MMPROFILE_FLAG_PULSE, &Bitmap);
			m4u_mva_unmap_kernel(wdma_layer->dstAddress, Bitmap.data_size,
					     (unsigned long)Bitmap.p_data);
		} else {
			DISPERR("dprec_mmp_dump_wdma_layer(),fail to dump rgb(0x%x)\n",
			       wdma_layer->outputFormat);
		}
	} else {
		meta.data_type = MMPROFILE_META_RAW;
		meta.size = wdma_layer->dstPitch * wdma_layer->srcHeight;
		if (m4u_mva_map_kernel
		    (wdma_layer->dstAddress, meta.size, (unsigned long *)&meta.p_data,
		     &meta.size) == 0) {
			mmprofile_log_meta(DDP_MMP_Events.wdma_dump[wdma_num], MMPROFILE_FLAG_PULSE,
					 &meta);
		} else {
			DISPERR("dprec_mmp_dump_wdma_layer(),fail to dump raw(0x%x)\n",
			       wdma_layer->outputFormat);
		}
	}

}

void ddp_mmp_rdma_layer(struct RDMA_CONFIG_STRUCT *rdma_layer, unsigned int rdma_num,
			unsigned int down_sample_x, unsigned int down_sample_y)
{
	struct mmp_metadata_bitmap_t Bitmap;
	struct mmp_metadata_t meta;
	int raw = 0;

	if (rdma_num > 1) {
		DISPERR("dump_rdma_layer is error %d\n", rdma_num);
		return;
	}

	Bitmap.data1 = rdma_layer->address;
	Bitmap.width = rdma_layer->width;
	Bitmap.height = rdma_layer->height;
	switch (rdma_layer->inputFormat) {
	case eRGB565:
	case eBGR565:
		Bitmap.format = MMPROFILE_BITMAP_RGB565;
		Bitmap.bpp = 16;
		break;
	case eRGB888:
		Bitmap.format = MMPROFILE_BITMAP_RGB888;
		Bitmap.bpp = 24;
		break;
	case eBGRA8888:
		Bitmap.format = MMPROFILE_BITMAP_BGRA8888;
		Bitmap.bpp = 32;
		break;
	case eBGR888:
		Bitmap.format = MMPROFILE_BITMAP_BGR888;
		Bitmap.bpp = 24;
		break;
	case eRGBA8888:
		Bitmap.format = MMPROFILE_BITMAP_RGBA8888;
		Bitmap.bpp = 32;
		break;
	default:
		DISPERR("dump_rdma_layer(), unknown fmt=%d, dump raw\n", rdma_layer->inputFormat);
		raw = 1;
	}
	if (!raw) {
		Bitmap.start_pos = 0;
		Bitmap.pitch = rdma_layer->pitch;
		Bitmap.data_size = Bitmap.pitch * Bitmap.height;
		Bitmap.down_sample_x = down_sample_x;
		Bitmap.down_sample_y = down_sample_y;
		if (m4u_mva_map_kernel
		    (rdma_layer->address, Bitmap.data_size, (unsigned long int *)&Bitmap.p_data,
		     &Bitmap.data_size) == 0) {
			mmprofile_log_meta_bitmap(DDP_MMP_Events.rdma_dump[rdma_num],
					       MMPROFILE_FLAG_PULSE, &Bitmap);
			m4u_mva_unmap_kernel(rdma_layer->address, Bitmap.data_size,
					     (unsigned long)Bitmap.p_data);
		} else {
			DISPERR("dump_rdma_layer(),fail to dump rgb(0x%x)\n",
			       rdma_layer->inputFormat);
		}
	} else {
		meta.data_type = MMPROFILE_META_RAW;
		meta.size = rdma_layer->pitch * rdma_layer->height;
		if (m4u_mva_map_kernel
		    (rdma_layer->address, meta.size, (unsigned long int *)&meta.p_data,
		     &meta.size) == 0) {
			mmprofile_log_meta(DDP_MMP_Events.rdma_dump[rdma_num], MMPROFILE_FLAG_PULSE,
					 &meta);
			m4u_mva_unmap_kernel(rdma_layer->address, meta.size,
					     (unsigned long)meta.p_data);
		} else {
			DISPERR("dprec_mmp_dump_rdma_layer(),fail to dump raw(0x%x)\n",
			       rdma_layer->inputFormat);
		}
	}
}


struct DDP_MMP_Events_t *ddp_mmp_get_events(void)
{
	return &DDP_MMP_Events;
}

void ddp_mmp_init(void)
{
#ifdef DEFAULT_MMP_ENABLE
	DISPMSG("ddp_mmp_init\n");
	MMProfileEnable(1);
	init_ddp_mmp_events();
	MMProfileStart(1);
#endif
}
