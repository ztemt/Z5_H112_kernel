#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/sysdev.h>
#include <linux/timer.h>
#include <linux/err.h>
#include <linux/ctype.h>


//rex add for mdp color tmp

#include "mdp.h"
#include "mdp4.h"
//rex add for mdp color end


//rex add for color temperature
static unsigned int v5_color_temperature = 24;
//rex add for color temperature end

/*added by congshan 20121127 start */
extern unsigned int v5_intensity_value;
#if defined( CONFIG_ZTEMT_LCD_Z5MINI_SIZE_4P7)
void zte_mipi_disp_inc_nt35590(unsigned int state);
#endif
#if defined( CONFIG_ZTEMT_LCD_Z5MINI2_SIZE_4P7)
#include <linux/nubia_lcd_ic_type.h>
void zte_mipi_disp_inc_r69431(unsigned int state);
void zte_mipi_disp_inc_tianma(unsigned int state);
#endif
void zte_mipi_disp_inc(unsigned int state);

static ssize_t intensity_show(struct kobject *kobj, struct kobj_attribute *attr,
   char *buf)
{
	return 0;
}
static ssize_t intensity_store(struct kobject *kobj, struct kobj_attribute *attr,
    const char *buf, size_t size)
{
	ssize_t ret = 0;
	int val;
	static bool is_firsttime = true;
	sscanf(buf, "%d", &val);
	pr_err("sss %s state=%d size=%d\n", __func__, (int)val, (int)size);
	v5_intensity_value = val;
	if (is_firsttime) {
		is_firsttime = false;
		return ret;
	}
#if defined( CONFIG_ZTEMT_LCD_Z5MINI_SIZE_4P7)
	zte_mipi_disp_inc_nt35590(val);
#elif defined( CONFIG_ZTEMT_LCD_Z5MINI2_SIZE_4P7)
	if (SHARP_R69431 == mini2_get_lcd_type()) {
		zte_mipi_disp_inc_r69431(val);
	} else {
		zte_mipi_disp_inc_tianma(val);
	}
#else
	zte_mipi_disp_inc(val);
#endif
	#if 0
	char *after;
	unsigned int state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;
	pr_err("sss %s state=%d size=%d\n", __func__, (int)state, (int)size);
	if (isspace(*after))
		count++;

	if (count == size) {
		ret = count;
		zte_mipi_disp_inc(state);
	}
	#endif
	return ret;


}


//rex add for color temperature
#ifdef CONFIG_ZTEMT_LCD_Z5MINI2_SIZE_4P7
static struct mdp_pcc_cfg_data mpcd_sharp[] =
{
//static struct mdp_pcc_cfg_data mpcd_sharp_warm = 
	{
		.block = MDP_BLOCK_DMA_P,
		.ops = MDP_PP_OPS_WRITE | MDP_PP_OPS_ENABLE,
		{
			.r = 0x8000,
		},
		{
			.g = 0x8000,
		},
		{
			.b = 0x7990,
		},
	},
//static struct mdp_pcc_cfg_data mpcd_sharp_normal = 
	{
		.block = MDP_BLOCK_DMA_P,
		.ops = MDP_PP_OPS_WRITE | MDP_PP_OPS_ENABLE,
		{
			.r = 0x8000,
		},
		{
			.g = 0x8000,
		},
		{
			.b = 0x8000,
		},
	},
//static struct mdp_pcc_cfg_data mpcd_sharp_cool = 
	{
		.block = MDP_BLOCK_DMA_P,
		.ops = MDP_PP_OPS_WRITE | MDP_PP_OPS_ENABLE,
		{
			.r = 0x7990,
		},
		{
			.g = 0x8000,
		},
		{
			.b = 0x8000,
		},
	},
};
#else
static struct mdp_pcc_cfg_data mpcd_NX50X[] =
{
//static struct mdp_pcc_cfg_data mpcd_sharp_warm = 
	{
		.block = MDP_BLOCK_DMA_P,
		.ops = MDP_PP_OPS_WRITE | MDP_PP_OPS_ENABLE,
		{
			.r = 0x8000,
		},
		{
			.g = 0x8000,
		},
		{
			.b = 0x7990,
		},
	},
//static struct mdp_pcc_cfg_data mpcd_sharp_normal = 
	{
		.block = MDP_BLOCK_DMA_P,
		.ops = MDP_PP_OPS_WRITE,
		{
			.r = 0x8000,
		},
		{
			.g = 0x8000,
		},
		{
			.b = 0x8000,
		},
	},
//static struct mdp_pcc_cfg_data mpcd_sharp_cool = 
	{
		.block = MDP_BLOCK_DMA_P,
		.ops = MDP_PP_OPS_WRITE | MDP_PP_OPS_ENABLE,
		{
			.r = 0x7990,
		},
		{
			.g = 0x8000,
		},
		{
			.b = 0x8000,
		},
	},
};
#endif
enum{
	COLOR_TMP_START =24,
	COLOR_TMP_WARM = 24,
	COLOR_TMP_NORL,
	COLOR_TMP_COOL,
	COLOR_TMP_END
};
static ssize_t colortmp_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
	//u32 copyback = 0;
	int ret = 0;
	struct mdp_pcc_cfg_data mpcd = {0};

	memset(&mpcd,sizeof(mpcd),0);
	mpcd.block = MDP_BLOCK_DMA_P;//MDP_LOGICAL_BLOCK_DISP_0;
	mpcd.ops = MDP_PP_OPS_READ;
	ret = mdp4_pcc_cfg(&mpcd);
	if(ret )	
	{
		pr_err("pcc_cfg_show error num: %x\n", ret);
		sprintf(buf,"get mdp pcc cfg is zero\n");
	}else
	{
		sprintf(buf,"colortmp:%d r:0x%x g:0x%x b:0x%x\n",v5_color_temperature,mpcd.r.r,mpcd.g.g,mpcd.b.b);
	}

	return ret;
}
static struct mdp_pcc_cfg_data* get_pcc_cfg_data( int colortmp_val)
{
	struct mdp_pcc_cfg_data* pmpcd = NULL;

	if ((colortmp_val < COLOR_TMP_START) || (colortmp_val >= COLOR_TMP_END))
	{
		return NULL;
	}
#ifdef CONFIG_ZTEMT_LCD_Z5MINI2_SIZE_4P7
	if (SHARP_R69431 == mini2_get_lcd_type())
	{
		pmpcd = mpcd_sharp;
	}
	else
	{
		pmpcd = mpcd_sharp;
	}
#else
	pmpcd = mpcd_NX50X;
#endif
	return (pmpcd + (colortmp_val - COLOR_TMP_START));
}
static ssize_t colortmp_store(struct kobject *kobj, struct kobj_attribute *attr,
		    const char *buf, size_t size)
{
	ssize_t ret = 0;
	int val;
	struct mdp_pcc_cfg_data* pmpcd = NULL;


	sscanf(buf, "%d", &val);
	
	pmpcd = get_pcc_cfg_data(val);
	
	if(NULL != pmpcd)
	{
		ret = mdp4_pcc_cfg(pmpcd);
	}
	v5_color_temperature = val;

	return ret;
}

int colortmp_turnon(void)
{
	struct mdp_pcc_cfg_data* pmpcd = NULL;
	int ret = 0;

	pmpcd = get_pcc_cfg_data(v5_color_temperature);
	if(NULL != pmpcd)
	{
		ret = mdp4_pcc_cfg(pmpcd);
	}

	return ret;
}
//rex add for color temperature end

static struct kobj_attribute intensity_attribute =
 __ATTR(intensity, 0664, intensity_show, intensity_store);

//rex add for color temperature
static struct kobj_attribute colortmp_attribute =
 __ATTR(colortmp, 0664, colortmp_show, colortmp_store);
//rex add for color temperature end

 static struct attribute *attrs[] = {
  &intensity_attribute.attr,
  &colortmp_attribute.attr,//rex add for color temperature
  NULL, /* need to NULL terminate the list of attributes */
 };
 static struct attribute_group attr_group = {
  .attrs = attrs,
 };
 
 static struct kobject *id_kobj;
 
 static int __init id_init(void)
 {
  int retval;
 
  id_kobj = kobject_create_and_add("lcd_intensity", kernel_kobj);
  if (!id_kobj)
   return -ENOMEM;
 
  /* Create the files associated with this kobject */
  retval = sysfs_create_group(id_kobj, &attr_group);
  if (retval)
   kobject_put(id_kobj);
 
  return retval;
 }
 
 static void __exit id_exit(void)
 {
  kobject_put(id_kobj);
 }
 
 module_init(id_init);
 module_exit(id_exit);
 
 /*added by congshan 20121127 end */

