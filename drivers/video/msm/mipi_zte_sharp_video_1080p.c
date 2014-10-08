/******************************************************************************

  Copyright (C), 2001-2012, ZTE. Co., Ltd.

 ******************************************************************************
  File Name     : mipi_zte_sharp.c
  Version       : Initial Draft
  Author        : congshan 
  Created       : 2012/9/22
  Last Modified :
  Description   : This is lcd driver board file
  Function List :
  History       :
  1.Date        :  2012/9/22
    Author      : congshan 
    Modification: Created file

*****************************************************************************/


#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_zte_sharp.h"
#define MAX_BL_LEVEL 70

static struct msm_panel_info pinfo;



static struct mipi_dsi_phy_ctrl dsi_video_mode_phy_db = {
	 		/* regulator */
	  {0x09, 0x08, 0x05, 0x00, 0x20},
	  /* timing */
	  {0xdf, 0x5b, 0x29, 0x00, 0x77, 0x79, 0x2e, 0x5f,
		  0x3f, 0x03, 0x04, 0xa0},
	  
	  /* phy ctrl */
	  {0x5f, 0x00, 0x00, 0x10},
	  /* strength */
	  {0xff, 0x00, 0x06, 0x00},
		/* pll control */
	  {0x0, 0xb0, 0x31, 0xda, 0x00, 0x20, 0x07, 0x62,
	   0x40, 0x07, 0x01,
	   0x00, 0x14, 0x03, 0x00, 0x02, 0x00, 0x20, 0x00, 0x01 },

};

static int __init mipi_video_sharp_init(void)
{
	int ret;
	if (msm_fb_detect_client("mipi_zte_sharp"))
		return 0;

	/* Landscape */
	pinfo.xres = 1080;
	pinfo.yres = 1920;
	pinfo.lcdc.xres_pad = 0;
	pinfo.lcdc.yres_pad = 0;

	pinfo.type = MIPI_VIDEO_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24;
	pinfo.lcdc.h_back_porch = 58;
	pinfo.lcdc.h_front_porch = 100;
	pinfo.lcdc.h_pulse_width = 10;
	pinfo.lcdc.v_back_porch = 4;
	pinfo.lcdc.v_front_porch = 4;
	pinfo.lcdc.v_pulse_width = 2;

	pinfo.lcdc.border_clr = 0;	/* blk */
	pinfo.lcdc.underflow_clr = 0xff;	/* blue */
	pinfo.lcdc.hsync_skew = 0;
	pinfo.bl_max = MAX_BL_LEVEL;
	pinfo.bl_min = 1;
	pinfo.fb_num = 2;

	pinfo.mipi.mode = DSI_VIDEO_MODE;
	pinfo.mipi.pulse_mode_hsa_he = TRUE;
	pinfo.mipi.hfp_power_stop = FALSE;
	pinfo.mipi.hbp_power_stop = FALSE;
	pinfo.mipi.hsa_power_stop = TRUE;
	pinfo.mipi.eof_bllp_power_stop = TRUE;
	pinfo.mipi.bllp_power_stop = TRUE;
	pinfo.mipi.traffic_mode = DSI_NON_BURST_SYNCH_EVENT;
	pinfo.mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
	pinfo.mipi.vc = 0;
	pinfo.mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	pinfo.mipi.data_lane0 = TRUE;
	pinfo.mipi.data_lane1 = TRUE;
	pinfo.mipi.data_lane2 = TRUE;
	pinfo.mipi.data_lane3 = TRUE;
	pinfo.mipi.t_clk_post = 0x07;
	pinfo.mipi.t_clk_pre = 0x2f;
	pinfo.mipi.stream = 0; /* dma_p */
	pinfo.mipi.mdp_trigger = 0;
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.frame_rate = 60;
	pinfo.mipi.dsi_phy_db = &dsi_video_mode_phy_db;
	pinfo.mipi.tx_eot_append = TRUE;
	pinfo.mipi.esc_byte_ratio = 8;

	ret = mipi_sharp_device_register(&pinfo, MIPI_DSI_PRIM,
					      MIPI_DSI_PANEL_WXGA);
	if (ret)
		pr_err("%s: failed to register device!\n", __func__);

	return ret;
}

module_init(mipi_video_sharp_init);
