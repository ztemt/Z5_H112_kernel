/******************** (C) COPYRIGHT 2011 STMicroelectronics ********************
 *
 * File Name          : lsm330dlc_acc.c
 * Authors            : MSH - Motion Mems BU - Application Team
 *		      : Matteo Dameno (matteo.dameno@st.com)
 *		      : Carmine Iascone (carmine.iascone@st.com)
 *		      : Both authors are willing to be considered the contact
 *		      : and update points for the driver.
 * Version            : V.1.0.10
 * Date               : 2011/Aug/16
 * Description        : LSM330DLC accelerometer sensor API
 *
 *******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
 * OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
 * PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
 * AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
 * INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
 * CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
 * INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 ******************************************************************************
 Revision 1.0.6 15/11/2010
  first revision
  supports sysfs;
  no more support for ioctl;
 Revision 1.0.7 26/11/2010
  checks for availability of interrupts pins
  correction on FUZZ and FLAT values;
 Revision 1.0.8 2010/Apr/01
  corrects a bug in interrupt pin management in 1.0.7
 Revision 1.0.9: 2011/May/23
  update_odr func correction;
 Revision 1.0.10: 2011/Aug/16
  introduces default_platform_data, i2c_read and i2c_write funnction rewritten,
  manages smbus beside i2c
 Revision 1.0.10.1: 2012/Mar/21
  change the sysfs attribute for ancroid CTS
 ******************************************************************************/

#include	<linux/err.h>
#include	<linux/errno.h>
#include	<linux/delay.h>
#include	<linux/fs.h>
#include	<linux/i2c.h>
#include	<linux/input.h>
#include	<linux/uaccess.h>
#include	<linux/workqueue.h>
#include	<linux/irq.h>
#include	<linux/gpio.h>
#include	<linux/interrupt.h>
#include	<linux/slab.h>
#include	<linux/i2c/lsm330dlc.h>
#include    <linux/regulator/consumer.h>


//#define CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

#ifdef CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

#ifdef CONFIG_HAS_EARLYSUSPEND

#include <linux/earlysuspend.h>

#endif //end of CONFIG_HAS_EARLYSUSPEND

#include <linux/fs.h>
#include <linux/wakelock.h>
#include <linux/delay.h>

#endif //end of CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

#define  LOG_TAG "SENSOR_G_SENSOR"

#define DEBUG_ON //DEBUG SWITCH

#define SENSOR_LOG_FILE__ strrchr(__FILE__, '/') ? (strrchr(__FILE__, '/')+1) : __FILE__

#ifdef  CONFIG_FEATURE_ZTEMT_SENSORS_LOG_ON
#define SENSOR_LOG_ERROR(fmt, args...) printk(KERN_ERR   "[%s] [%s: %d] "  fmt,\
                                              LOG_TAG,__FUNCTION__, __LINE__, ##args)
    #ifdef  DEBUG_ON
#define SENSOR_LOG_INFO(fmt, args...)  printk(KERN_INFO  "[%s] [%s: %d] "  fmt,\
                                              LOG_TAG,__FUNCTION__, __LINE__, ##args)
                                              
#define SENSOR_LOG_DEBUG(fmt, args...) printk(KERN_DEBUG "[%s] [%s: %d] "  fmt,\
                                              LOG_TAG,__FUNCTION__, __LINE__, ##args)
    #else
#define SENSOR_LOG_INFO(fmt, args...)
#define SENSOR_LOG_DEBUG(fmt, args...)
    #endif

#else
#define SENSOR_LOG_ERROR(fmt, args...)
#define SENSOR_LOG_INFO(fmt, args...)
#define SENSOR_LOG_DEBUG(fmt, args...)
#endif


#define	DEBUG	0
#define	G_MAX		16000


#define SENSITIVITY_2G		1	/**	mg/LSB	*/
#define SENSITIVITY_4G		2	/**	mg/LSB	*/
#define SENSITIVITY_8G		4	/**	mg/LSB	*/
#define SENSITIVITY_16G		12	/**	mg/LSB	*/




/* Accelerometer Sensor Operating Mode */
#define LSM330DLC_ACC_ENABLE	0x01
#define LSM330DLC_ACC_DISABLE	0x00

#define	HIGH_RESOLUTION		0x08

#define	AXISDATA_REG		0x28
#define WHOAMI_LSM330DLC_ACC	0x33	/*	Expctd content for WAI	*/

/*	CONTROL REGISTERS	*/
#define WHO_AM_I		0x0F	/*	WhoAmI register		*/
#define	TEMP_CFG_REG		0x1F	/*	temper sens control reg	*/
/* ctrl 1: ODR3 ODR2 ODR ODR0 LPen Zenable Yenable Zenable */
#define	CTRL_REG1		0x20	/*	control reg 1		*/
#define	CTRL_REG2		0x21	/*	control reg 2		*/
#define	CTRL_REG3		0x22	/*	control reg 3		*/
#define	CTRL_REG4		0x23	/*	control reg 4		*/
#define	CTRL_REG5		0x24	/*	control reg 5		*/
#define	CTRL_REG6		0x25	/*	control reg 6		*/

#define	REFERENCE_REG   0x26    /*  reference reg       */

#define	FIFO_CTRL_REG		0x2E	/*	FiFo control reg	*/

#define	INT_CFG1		0x30	/*	interrupt 1 config	*/
#define	INT_SRC1		0x31	/*	interrupt 1 source	*/
#define	INT_THS1		0x32	/*	interrupt 1 threshold	*/
#define	INT_DUR1		0x33	/*	interrupt 1 duration	*/


#define	TT_CFG			0x38	/*	tap config		*/
#define	TT_SRC			0x39	/*	tap source		*/
#define	TT_THS			0x3A	/*	tap threshold		*/
#define	TT_LIM			0x3B	/*	tap time limit		*/
#define	TT_TLAT			0x3C	/*	tap time latency	*/
#define	TT_TW			0x3D	/*	tap time window		*/
/*	end CONTROL REGISTRES	*/


#define ENABLE_HIGH_RESOLUTION	1

#define LSM330DLC_ACC_PM_OFF		0x00
#define LSM330DLC_ACC_ENABLE_ALL_AXES	0x07


#define PMODE_MASK			0x08
#define ODR_MASK			0XF0

#define ODR1		0x10  /* 1Hz output data rate */
#define ODR10		0x20  /* 10Hz output data rate */
#define ODR25		0x30  /* 25Hz output data rate */
#define ODR50		0x40  /* 50Hz output data rate */
#define ODR100		0x50  /* 100Hz output data rate */
#define ODR200		0x60  /* 200Hz output data rate */
#define ODR400		0x70  /* 400Hz output data rate */
#define ODR1250		0x90  /* 1250Hz output data rate */



#define	IA			0x40
#define	ZH			0x20
#define	ZL			0x10
#define	YH			0x08
#define	YL			0x04
#define	XH			0x02
#define	XL			0x01
/* */
/* CTRL REG BITS*/
#define	CTRL_REG3_I1_AOI1	0x40
#define	CTRL_REG6_I2_TAPEN	0x80
#define	CTRL_REG6_HLACTIVE	0x02
/* */
#define NO_MASK			0xFF
#define INT1_DURATION_MASK	0x7F
#define	INT1_THRESHOLD_MASK	0x7F
#define TAP_CFG_MASK		0x3F
#define	TAP_THS_MASK		0x7F
#define	TAP_TLIM_MASK		0x7F
#define	TAP_TLAT_MASK		NO_MASK
#define	TAP_TW_MASK		NO_MASK


/* TAP_SOURCE_REG BIT */
#define	DTAP			0x20
#define	STAP			0x10
#define	SIGNTAP			0x08
#define	ZTAP			0x04
#define	YTAP			0x02
#define	XTAZ			0x01


#define	FUZZ			0
#define	FLAT			0
#define	I2C_RETRY_DELAY		5
#define	I2C_RETRIES		5
#define	I2C_AUTO_INCREMENT	0x80

/* RESUME STATE INDICES */
#define	RES_CTRL_REG1		0
#define	RES_CTRL_REG2		1
#define	RES_CTRL_REG3		2
#define	RES_CTRL_REG4		3
#define	RES_CTRL_REG5		4
#define	RES_CTRL_REG6		5

#define	RES_INT_CFG1		6
#define	RES_INT_THS1		7
#define	RES_INT_DUR1		8

#define	RES_TT_CFG		9
#define	RES_TT_THS		10
#define	RES_TT_LIM		11
#define	RES_TT_TLAT		12
#define	RES_TT_TW		13

#define	RES_TEMP_CFG_REG	14
#define	RES_REFERENCE_REG	15
#define	RES_FIFO_CTRL_REG	16

#define	RESUME_ENTRIES		17
/* end RESUME STATE INDICES */

#ifdef CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

struct lsm330dlc_acc_data *p_acc_data_global;
unsigned char flag_enable_miss_event     = false;
unsigned char flag_have_miss_event       = false;
unsigned char flag_enable_sleep_clock    = false;
unsigned char flag_enable_irq            = false;
unsigned char flag_lock_wakelock         = false;
unsigned char flag_enable_record         = false;
unsigned char flag_enable_irq_respond    = false;

#endif //end of CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

struct {
	unsigned int cutoff_ms;
	unsigned int mask;
} lsm330dlc_acc_odr_table[] = {
		{    1, ODR1250 },
		{    3, ODR400  },
		{    5, ODR200  },
		{   10, ODR100  },
		{   20, ODR50   },
		{   40, ODR25   },
		{  100, ODR10   },
		{ 1000, ODR1    },
};

static int use_smbus = 0;

struct lsm330dlc_acc_data {
	struct i2c_client *client;
	struct lsm330dlc_acc_platform_data *pdata;

	struct mutex lock;
	struct delayed_work input_work;

	struct input_dev *input_dev;

	int hw_initialized;
	/* hw_working=-1 means not tested yet */
	int hw_working;
	atomic_t enabled;
	int on_before_suspend;

	u8 sensitivity;

	u8 resume_state[RESUME_ENTRIES];

	int irq1;
	struct work_struct irq1_work;
	struct workqueue_struct *irq1_work_queue;
	int irq2;
	struct work_struct irq2_work;
	struct workqueue_struct *irq2_work_queue;

#ifdef CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

    #ifdef CONFIG_HAS_EARLYSUSPEND

	struct early_suspend early_suspend;

    #endif //end of CONFIG_HAS_EARLYSUSPEND

    struct wake_lock lsm330dlc_acc_wake_lock;

#endif //end of CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

#ifdef DEBUG
	u8 reg_addr;
#endif
};

static bool flag_debug = false;

static void lsm330dlc_acc_report_values(struct lsm330dlc_acc_data *acc,	int *xyz);


#ifdef CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

static ssize_t read_single_reg(struct device *dev, char *buf, u8 reg);
static unsigned int lsm330dlc_acc_disable_irq_config(struct lsm330dlc_acc_data *acc);
static unsigned int lsm330dlc_acc_enable_irq_config(struct lsm330dlc_acc_data *acc);
static void lsm330dlc_acc_vibrator_work(int time_ms);

#ifdef CONFIG_ZTEMT_VIBRATOR_DRV2605
extern void extern_linear_vibrator_enable(int mode, int time);
#endif

#ifdef CONFIG_PMIC8XXX_VIBRATOR
extern void extern_nolinear_vibrator_enable(int time_ms);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND

void lsm330dlc_acc_earlysuspend_suspend(struct early_suspend *h);
void lsm330dlc_acc_earlysuspend_resume(struct early_suspend *h);

#endif //end of CONFIG_HAS_EARLYSUSPEND

//#define ENABLE_EVENT_VIBRATOR_PATH "/data/data/ztemt_feature_enable_event_vibrator.txt"
//#define HAVE_MISS_EVENT_PATH       "/data/data/ztemt_feature_have_miss_event.txt"

#define ENABLE_EVENT_VIBRATOR_PATH  "/data/data/com.ztemt.missevent.enable"
#define HAVE_MISS_EVENT_PATH        "/data/data/com.ztemt.missevent.have"
#define ENABLE_SLEEP_CLOCK_PATH     "/data/data/com.ztemt.sleepclock.enable"


#endif //end of CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

int lsm330dlc_acc_read_file(char *file_path)
{
    struct file *file_p;
    int vfs_read_retval = 0;
    mm_segment_t old_fs; 
    char read_buf[32];
    unsigned short read_value;

    if (NULL==file_path)
    {
        SENSOR_LOG_ERROR("file_path is NULL\n");
        goto error;
    }

    memset(read_buf, 0, 32);

    file_p = filp_open(file_path, O_RDONLY |O_LARGEFILE, 0);
    if (IS_ERR(file_p))
    {
        SENSOR_LOG_ERROR("[open file <%s>failed]\n",file_path);
        goto error;
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);
    
    vfs_read_retval = vfs_read(file_p, (char*)read_buf, 16, &file_p->f_pos);
    if (vfs_read_retval < 0)
    {
        SENSOR_LOG_ERROR("[read file <%s>failed]\n",file_path);
        goto error;
    }

    set_fs(old_fs);
    filp_close(file_p, NULL);

    if (kstrtou16(read_buf, 10, &read_value) < 0)
    {
        SENSOR_LOG_ERROR("[kstrtou16 %s failed]\n",read_buf);
        goto error;
    }
    
    SENSOR_LOG_ERROR("[the content of %s is %s]\n", file_path, read_buf);

    return read_value;

error:
    return 0;
}

int lsm330dlc_acc_write_file(char *file_path, char *data)
{
    struct file *dest_file_p = NULL;
    int vfs_write_retval = 0;
    mm_segment_t old_fs; 

    dest_file_p   = filp_open(file_path, O_RDWR |O_LARGEFILE, 0);
    if (IS_ERR(dest_file_p))
    {
        SENSOR_LOG_ERROR(" file <%s> not exist",file_path);
        SENSOR_LOG_ERROR(" create file <%s>",file_path);
        dest_file_p   = filp_open(file_path, O_CREAT | O_RDWR |O_LARGEFILE | FMODE_LSEEK, 0660);
        if (IS_ERR(dest_file_p))
        {
            SENSOR_LOG_ERROR(" create file <%s> failed",file_path);
            goto error;
        }
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    SENSOR_LOG_ERROR(" data is %s",data);
    
    vfs_write_retval = vfs_write(dest_file_p, data, 16, &dest_file_p->f_pos);

    set_fs(old_fs);
    dest_file_p->f_pos = 0;
    filp_close(dest_file_p, NULL);

    return 0;

error:
    return -1;
}

static const struct lsm330dlc_acc_platform_data default_lsm330dlc_acc_pdata = {
	.g_range = LSM330DLC_ACC_G_2G,
	.axis_map_x = 0,
	.axis_map_y = 1,
	.axis_map_z = 2,
	.negate_x = 0,
	.negate_y = 0,
	.negate_z = 0,
	.poll_interval = 100,
	.min_interval = 10,
	.gpio_int1 = -EINVAL,
	.gpio_int2 = -EINVAL,
};


static int lsm330dlc_acc_i2c_read(struct lsm330dlc_acc_data *acc,
				u8 * buf, int len)
{
	int ret;
	u8 reg = buf[0];
	u8 cmd = reg;

/*
	if( len > sizeof(buf) )
			dev_err(&acc->client->dev, 
				"read error insufficient buffer length: "
				"len:%d, buf size=%d\n",
				len, sizeof(buf));
*/

	if (use_smbus) {
		if (len == 1) {
			ret = i2c_smbus_read_byte_data(acc->client, cmd);
			buf[0] = ret & 0xff;
#if DEBUG
			dev_warn(&acc->client->dev, 
				"i2c_smbus_read_byte_data: ret=0x%02x, len:%d ,"
				"command=0x%02x, buf[0]=0x%02x\n",
				ret, len, cmd , buf[0]);
#endif
		} else if ( len > 1) {
			//cmd =  = I2C_AUTO_INCREMENT | reg;
			ret = i2c_smbus_read_i2c_block_data(acc->client, 
								cmd, len, buf);
#if DEBUG
			dev_warn(&acc->client->dev, 
				"i2c_smbus_read_i2c_block_data: ret:%d len:%d, "
				"command=0x%02x, ",
				ret, len, cmd);
			char ii;
			for(ii=0;ii<len;ii++){
				printk("buf[%d]=0x%02x,",ii,buf[ii]);
			}
			printk("\n");
#endif
		} else ret = -1;
		if (ret < 0) {
			dev_err(&acc->client->dev, 
				"read transfer error: len:%d, command=0x%02x\n",
				len, cmd );
			return 0; // failure
		}
		return len; // success
	}

	//cmd =  = I2C_AUTO_INCREMENT | reg;
	ret = i2c_master_send(acc->client, &cmd, sizeof(cmd));
	if (ret != sizeof(cmd))
		return ret;

	return i2c_master_recv(acc->client, buf, len);
}

static int lsm330dlc_acc_i2c_write(struct lsm330dlc_acc_data *acc, u8 * buf,
								int len)
{
	int ret;
	u8 reg, value;

	reg = buf[0];
	value = buf[1];

	if (use_smbus) {
		if (len == 1) {
			ret = i2c_smbus_write_byte_data(acc->client, reg, value);
#if DEBUG
			dev_warn(&acc->client->dev, 
				"i2c_smbus_write_byte_data: ret=%d, len:%d, "
				"command=0x%02x, value=0x%02x\n",
				ret, len, reg , value);
#endif
			return ret;
		} else if (len > 1) {
			ret = i2c_smbus_write_i2c_block_data(acc->client, 
							reg, len, buf + 1 );
#if DEBUG
			dev_warn(&acc->client->dev, 
				"i2c_smbus_write_i2c_block_data: ret=%d, "
				"len:%d, command=0x%02x, ", 
				ret, len, reg );
			char ii;
			for(ii=0;ii<len+1;ii++){
				printk("value[%d]=0x%02x,",ii,buf[ii]);
			}
			printk("\n");
#endif
			return ret;
		}		
	}

	ret = i2c_master_send(acc->client, buf, len+1);
    return (ret == len+1) ? ret : -1;
}


static int lsm330dlc_acc_hw_init(struct lsm330dlc_acc_data *acc)
{
	int err = -1;
	u8 buf[7];

	SENSOR_LOG_INFO( "%s: hw init start\n", LSM330DLC_ACC_DEV_NAME);

	buf[0] = WHO_AM_I;
	err = lsm330dlc_acc_i2c_read(acc, buf, 1);
	if (err < 0) {
		dev_warn(&acc->client->dev, "Error reading WHO_AM_I: is device "
		"available/working?\n");
		goto err_firstread;
	} else
		acc->hw_working = 1;
	
	if (buf[0] != WHOAMI_LSM330DLC_ACC) {
	dev_err(&acc->client->dev,
		"device unknown. Expected: 0x%02x,"
		" Replies: 0x%02x\n", WHOAMI_LSM330DLC_ACC, buf[0]);
		err = -1; /* choose the right coded error */
		goto err_unknown_device;
	}


	buf[0] = CTRL_REG1;
	buf[1] = acc->resume_state[RES_CTRL_REG1];
	err = lsm330dlc_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = TEMP_CFG_REG;
	buf[1] = acc->resume_state[RES_TEMP_CFG_REG];
	err = lsm330dlc_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = FIFO_CTRL_REG;
	buf[1] = acc->resume_state[RES_FIFO_CTRL_REG];
	err = lsm330dlc_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | TT_THS);
	buf[1] = acc->resume_state[RES_TT_THS];
	buf[2] = acc->resume_state[RES_TT_LIM];
	buf[3] = acc->resume_state[RES_TT_TLAT];
	buf[4] = acc->resume_state[RES_TT_TW];
	err = lsm330dlc_acc_i2c_write(acc, buf, 4);
	if (err < 0)
		goto err_resume_state;
	buf[0] = TT_CFG;
	buf[1] = acc->resume_state[RES_TT_CFG];
	err = lsm330dlc_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | INT_THS1);
	buf[1] = acc->resume_state[RES_INT_THS1];
	buf[2] = acc->resume_state[RES_INT_DUR1];
	err = lsm330dlc_acc_i2c_write(acc, buf, 2);
	if (err < 0)
		goto err_resume_state;
	buf[0] = INT_CFG1;
	buf[1] = acc->resume_state[RES_INT_CFG1];
	err = lsm330dlc_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;


	buf[0] = (I2C_AUTO_INCREMENT | CTRL_REG2);
	buf[1] = acc->resume_state[RES_CTRL_REG2];
	buf[2] = acc->resume_state[RES_CTRL_REG3];
	buf[3] = acc->resume_state[RES_CTRL_REG4];
	buf[4] = acc->resume_state[RES_CTRL_REG5];
	buf[5] = acc->resume_state[RES_CTRL_REG6];
	err = lsm330dlc_acc_i2c_write(acc, buf, 5);
	if (err < 0)
		goto err_resume_state;

	acc->hw_initialized = 1;
	SENSOR_LOG_INFO( "%s: hw init done\n", LSM330DLC_ACC_DEV_NAME);
	return 0;

err_firstread:
	acc->hw_working = 0;
err_unknown_device:
err_resume_state:
	acc->hw_initialized = 0;
	dev_err(&acc->client->dev, "hw init error 0x%02x,0x%02x: %d\n", buf[0],
			buf[1], err);
	return err;
}

static void lsm330dlc_acc_device_power_off(struct lsm330dlc_acc_data *acc)
{

#ifdef CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

	if (acc->hw_initialized) 
    {
		acc->hw_initialized = 0;
	}

#else

	int err;
	u8 buf[2] = { CTRL_REG1, LSM330DLC_ACC_PM_OFF };

	err = lsm330dlc_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		dev_err(&acc->client->dev, "soft power off failed: %d\n", err);

	if (acc->pdata->power_off) {
		if(acc->pdata->gpio_int1 >= 0)
			disable_irq_nosync(acc->irq1);
		if(acc->pdata->gpio_int2 >= 0)
			disable_irq_nosync(acc->irq2);
		acc->pdata->power_off();
		acc->hw_initialized = 0;
	}
	if (acc->hw_initialized) {
		if(acc->pdata->gpio_int1 >= 0)
			disable_irq_nosync(acc->irq1);
		if(acc->pdata->gpio_int2 >= 0)
			disable_irq_nosync(acc->irq2);
		acc->hw_initialized = 0;
	}

#endif //end of CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

}

static int lsm330dlc_acc_device_power_on(struct lsm330dlc_acc_data *acc)
{
	int err = -1;
    SENSOR_LOG_ERROR("enter\n");  
    
#ifdef CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

	if (!acc->hw_initialized)
    {
        SENSOR_LOG_ERROR("\n");
		err = lsm330dlc_acc_hw_init(acc);
		if (acc->hw_working == 1 && err < 0) {
			lsm330dlc_acc_device_power_off(acc);
			return err;
		}
	}

#else

	if (acc->pdata->power_on) {
		err = acc->pdata->power_on();
		if (err < 0) {
			dev_err(&acc->client->dev,
					"power_on failed: %d\n", err);
			return err;
		}
		if(acc->pdata->gpio_int1 >= 0)
			enable_irq(acc->irq1);
		if(acc->pdata->gpio_int2 >= 0)
			enable_irq(acc->irq2);
	}

	if (!acc->hw_initialized) {
		err = lsm330dlc_acc_hw_init(acc);
		if (acc->hw_working == 1 && err < 0) {
			lsm330dlc_acc_device_power_off(acc);
			return err;
		}
	}

	if (acc->hw_initialized) {
		if(acc->pdata->gpio_int1 >= 0)
			enable_irq(acc->irq1);
		if(acc->pdata->gpio_int2 >= 0)
			enable_irq(acc->irq2);
	}

#endif //end of CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

    SENSOR_LOG_ERROR("exit\n");
	return 0;
}

static irqreturn_t lsm330dlc_acc_isr1(int irq, void *dev)
{
#ifdef CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT    

	struct lsm330dlc_acc_data *acc = dev;

    SENSOR_LOG_ERROR("enter\n");
    SENSOR_LOG_ERROR("flag_enable_irq_respond is %d",flag_enable_irq_respond);

    if (flag_enable_irq_respond)
    {
        if (false == flag_lock_wakelock)
        {
            SENSOR_LOG_ERROR("wake_lock");
            wake_lock(&acc->lsm330dlc_acc_wake_lock);
            flag_lock_wakelock = true;
        }

        flag_enable_irq = false;
    	disable_irq_nosync(irq);
    	queue_work(acc->irq1_work_queue, &acc->irq1_work);
    	SENSOR_LOG_ERROR("isr1 queued\n");
        if (true == flag_lock_wakelock)
        {
            SENSOR_LOG_ERROR("wake_unlock");
            wake_unlock(&acc->lsm330dlc_acc_wake_lock);
            flag_lock_wakelock = false;
        }
    }
    
    SENSOR_LOG_ERROR("exit\n");

	return IRQ_HANDLED;

#else

    struct lsm330dlc_acc_data *acc = dev;

	disable_irq_nosync(irq);
	queue_work(acc->irq1_work_queue, &acc->irq1_work);
	SENSOR_LOG_INFO( "%s: isr1 queued\n", LSM330DLC_ACC_DEV_NAME);

	return IRQ_HANDLED;

#endif //end of CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT
}

static irqreturn_t lsm330dlc_acc_isr2(int irq, void *dev)
{
	struct lsm330dlc_acc_data *acc = dev;

	disable_irq_nosync(irq);
	queue_work(acc->irq2_work_queue, &acc->irq2_work);
	SENSOR_LOG_INFO( "%s: isr2 queued\n", LSM330DLC_ACC_DEV_NAME);

	return IRQ_HANDLED;
}

static void lsm330dlc_acc_irq1_work_func(struct work_struct *work)
{
#ifdef CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

    struct lsm330dlc_acc_data *acc = container_of(work, struct lsm330dlc_acc_data, irq1_work);
    unsigned char retry_times = 0;
    int xyz_miss_event[3]  = {1024,1024,1024};
    int xyz_sleep_clock[3] = {2048,2048,2048};
    int ret;
    SENSOR_LOG_ERROR("enter\n");
    wake_lock(&acc->lsm330dlc_acc_wake_lock);

    if (flag_enable_miss_event || flag_enable_sleep_clock)
    {
        mdelay(50);
        for (retry_times=0; retry_times<100; retry_times++)
        {
            ret = lsm330dlc_acc_disable_irq_config(acc);
            if (ret >= 0)
            {
                break;
            }   
            mdelay(20);
        }     
        SENSOR_LOG_ERROR("disable retry_times = %d\n",retry_times);

        if (flag_have_miss_event && flag_enable_miss_event)
        {
            lsm330dlc_acc_report_values(acc, xyz_miss_event);
            lsm330dlc_acc_vibrator_work(100);
        }
        else
        {
            if (flag_enable_sleep_clock)
            {
                lsm330dlc_acc_report_values(acc, xyz_sleep_clock);
            }
        }
    }

    wake_unlock(&acc->lsm330dlc_acc_wake_lock);
    SENSOR_LOG_ERROR("exit\n");

#else

	struct lsm330dlc_acc_data *acc =
	container_of(work, struct lsm330dlc_acc_data, irq1_work);
	/* TODO  add interrupt service procedure.
		 ie:lsm330dlc_acc_get_int1_source(acc); */
	;
	/*  */
	SENSOR_LOG_INFO( "%s: IRQ1 triggered\n", LSM330DLC_ACC_DEV_NAME);
//exit:
	enable_irq(acc->irq1);

#endif //end of CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT
}

static void lsm330dlc_acc_irq2_work_func(struct work_struct *work)
{

	struct lsm330dlc_acc_data *acc =
	container_of(work, struct lsm330dlc_acc_data, irq2_work);
	/* TODO  add interrupt service procedure.
		 ie:lsm330dlc_acc_get_tap_source(acc); */
	;
	/*  */

	SENSOR_LOG_INFO( "%s: IRQ2 triggered\n", LSM330DLC_ACC_DEV_NAME);
//exit:
	enable_irq(acc->irq2);
}

static int lsm330dlc_acc_update_g_range(struct lsm330dlc_acc_data *acc,
							u8 new_g_range)
{
	int err=-1;

	u8 sensitivity;
	u8 buf[2];
	u8 updated_val;
	u8 init_val;
	u8 new_val;
	u8 mask = LSM330DLC_ACC_FS_MASK | HIGH_RESOLUTION;

	switch (new_g_range) {
	case LSM330DLC_ACC_G_2G:

		sensitivity = SENSITIVITY_2G;
		break;
	case LSM330DLC_ACC_G_4G:

		sensitivity = SENSITIVITY_4G;
		break;
	case LSM330DLC_ACC_G_8G:

		sensitivity = SENSITIVITY_8G;
		break;
	case LSM330DLC_ACC_G_16G:

		sensitivity = SENSITIVITY_16G;
		break;
	default:
		dev_err(&acc->client->dev, "invalid g range requested: %u\n",
				new_g_range);
		return -EINVAL;
	}

	if (atomic_read(&acc->enabled)) {
		/* Updates configuration register 4,
		* which contains g range setting */
		buf[0] = CTRL_REG4;
		err = lsm330dlc_acc_i2c_read(acc, buf, 1);
		if (err < 0)
			goto error;
		init_val = buf[0];
		acc->resume_state[RES_CTRL_REG4] = init_val;
		new_val = new_g_range | HIGH_RESOLUTION;
		updated_val = ((mask & new_val) | ((~mask) & init_val));
		buf[1] = updated_val;
		buf[0] = CTRL_REG4;
		err = lsm330dlc_acc_i2c_write(acc, buf, 1);
		if (err < 0)
			goto error;
		acc->resume_state[RES_CTRL_REG4] = updated_val;
		acc->sensitivity = sensitivity;
	}


	return err;
error:
	dev_err(&acc->client->dev, "update g range failed 0x%02x,0x%02x: %d\n",
			buf[0], buf[1], err);

	return err;
}

static int lsm330dlc_acc_update_odr(struct lsm330dlc_acc_data *acc,
							int poll_interval_ms)
{
	int err = -1;
	int i;
	u8 config[2];

	/* Following, looks for the longest possible odr interval scrolling the
	 * odr_table vector from the end (shortest interval) backward (longest
	 * interval), to support the poll_interval requested by the system.
	 * It must be the longest interval lower then the poll interval.*/
	for (i = ARRAY_SIZE(lsm330dlc_acc_odr_table) - 1; i >= 0; i--) {
		if ((lsm330dlc_acc_odr_table[i].cutoff_ms <= poll_interval_ms)
								|| (i == 0))
        {
			break;   
        }
	}
	config[1] = lsm330dlc_acc_odr_table[i].mask;

	config[1] |= LSM330DLC_ACC_ENABLE_ALL_AXES;

	/* If device is currently enabled, we need to write new
	 *  configuration out to it */
	if (atomic_read(&acc->enabled)) {
		config[0] = CTRL_REG1;
		err = lsm330dlc_acc_i2c_write(acc, config, 1);
		if (err < 0)
			goto error;
		acc->resume_state[RES_CTRL_REG1] = config[1];
	}

	return err;

error:
	dev_err(&acc->client->dev, "update odr failed 0x%02x,0x%02x: %d\n",
			config[0], config[1], err);

	return err;
}



static int lsm330dlc_acc_register_write(struct lsm330dlc_acc_data *acc,
					u8 *buf, u8 reg_address, u8 new_value)
{
	int err = -1;

		/* Sets configuration register at reg_address
		 *  NOTE: this is a straight overwrite  */
		buf[0] = reg_address;
		buf[1] = new_value;
		err = lsm330dlc_acc_i2c_write(acc, buf, 1);
		if (err < 0)
			return err;
	return err;
}

#if 0
static int lsm330dlc_acc_register_read(struct lsm330dlc_acc_data *acc,
							u8 *buf, u8 reg_address)
{

	int err = -1;
	buf[0] = (reg_address);
	err = lsm330dlc_acc_i2c_read(acc, buf, 1);
	return err;
}

#endif
/*
static int lsm330dlc_acc_register_update(struct lsm330dlc_acc_data *acc,
		u8 *buf, u8 reg_address, u8 mask, u8 new_bit_values)
{
	int err = -1;
	u8 init_val;
	u8 updated_val;
	err = lsm330dlc_acc_register_read(acc, buf, reg_address);
	if (!(err < 0)) {
		init_val = buf[1];
		updated_val = ((mask & new_bit_values) | ((~mask) & init_val));
		err = lsm330dlc_acc_register_write(acc, buf, reg_address,
				updated_val);
	}
	return err;
}
*/


static int lsm330dlc_acc_get_acceleration_data(struct lsm330dlc_acc_data *acc,
		int *xyz)
{
	int err = -1;
	/* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	u8 acc_data[6];
	/* x,y,z hardware data */
	s16 hw_d[3] = { 0 };

	acc_data[0] = (I2C_AUTO_INCREMENT | AXISDATA_REG);
	err = lsm330dlc_acc_i2c_read(acc, acc_data, 6);
	if (err < 0)
		return err;

	hw_d[0] = (((s16) ((acc_data[1] << 8) | acc_data[0])) >> 4);
	hw_d[1] = (((s16) ((acc_data[3] << 8) | acc_data[2])) >> 4);
	hw_d[2] = (((s16) ((acc_data[5] << 8) | acc_data[4])) >> 4);



	hw_d[0] = hw_d[0] * acc->sensitivity;
	hw_d[1] = hw_d[1] * acc->sensitivity;
	hw_d[2] = hw_d[2] * acc->sensitivity;


	xyz[0] = ((acc->pdata->negate_x) ? (-hw_d[acc->pdata->axis_map_x])
		   : (hw_d[acc->pdata->axis_map_x]));
	xyz[1] = ((acc->pdata->negate_y) ? (-hw_d[acc->pdata->axis_map_y])
		   : (hw_d[acc->pdata->axis_map_y]));
	xyz[2] = ((acc->pdata->negate_z) ? (-hw_d[acc->pdata->axis_map_z])
		   : (hw_d[acc->pdata->axis_map_z]));

#if DEBUG
/*
		printk(KERN_INFO "%s read x=%d, y=%d, z=%d\n",
			LSM330DLC_ACC_DEV_NAME, xyz[0], xyz[1], xyz[2]);
*/
#endif
	return err;
}

static void lsm330dlc_acc_report_values(struct lsm330dlc_acc_data *acc,
					int *xyz)
{
    if (flag_debug)
    {
        SENSOR_LOG_ERROR("acc data: x = %d, y = %d, z = %d\n",xyz[0],xyz[1],xyz[2]);
    }
	input_report_abs(acc->input_dev, ABS_X, xyz[0]);
	input_report_abs(acc->input_dev, ABS_Y, xyz[1]);
	input_report_abs(acc->input_dev, ABS_Z, xyz[2]);
	input_sync(acc->input_dev);
}

static int lsm330dlc_acc_enable(struct lsm330dlc_acc_data *acc)
{
	int err;

	if (!atomic_cmpxchg(&acc->enabled, 0, 1)) {
		err = lsm330dlc_acc_device_power_on(acc);
		if (err < 0) {
			atomic_set(&acc->enabled, 0);
			return err;
		}
		schedule_delayed_work(&acc->input_work,
			msecs_to_jiffies(acc->pdata->poll_interval));
	}

	return 0;
}

static int lsm330dlc_acc_disable(struct lsm330dlc_acc_data *acc)
{
	if (atomic_cmpxchg(&acc->enabled, 1, 0)) {
		cancel_delayed_work_sync(&acc->input_work);
		lsm330dlc_acc_device_power_off(acc);
	}

	return 0;
}


static ssize_t read_single_reg(struct device *dev, char *buf, u8 reg)
{
	ssize_t ret;
	struct lsm330dlc_acc_data *acc = dev_get_drvdata(dev);
	int err;

	u8 data = reg;
	err = lsm330dlc_acc_i2c_read(acc, &data, 1);
	if (err < 0)
		return err;
	ret = sprintf(buf, "0x%02x\n", data);
	return ret;

}

static int write_reg(struct device *dev, const char *buf, u8 reg,
		u8 mask, int resumeIndex)
{
	int err = -1;
	struct lsm330dlc_acc_data *acc = dev_get_drvdata(dev);
	u8 x[2];
	u8 new_val;
	unsigned long val;

	if (strict_strtoul(buf, 16, &val))
		return -EINVAL;

	new_val=((u8) val & mask);
	x[0] = reg;
	x[1] = new_val;
	err = lsm330dlc_acc_register_write(acc, x,reg,new_val);
	if (err < 0)
		return err;
	acc->resume_state[resumeIndex] = new_val;
	return err;
}

static ssize_t attr_get_polling_rate(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int val;
	struct lsm330dlc_acc_data *acc = dev_get_drvdata(dev);
	mutex_lock(&acc->lock);
	val = acc->pdata->poll_interval;
	mutex_unlock(&acc->lock);
	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_polling_rate(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct lsm330dlc_acc_data *acc = dev_get_drvdata(dev);
	unsigned long interval_ms;

	if (strict_strtoul(buf, 10, &interval_ms))
		return -EINVAL;
	if (!interval_ms)
		return -EINVAL;
	interval_ms = max((unsigned int)interval_ms,acc->pdata->min_interval);
	mutex_lock(&acc->lock);
	acc->pdata->poll_interval = interval_ms;
	lsm330dlc_acc_update_odr(acc, interval_ms);
	mutex_unlock(&acc->lock);
	return size;
}

static ssize_t attr_get_range(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	char val;
	struct lsm330dlc_acc_data *acc = dev_get_drvdata(dev);
	char range = 2;
	mutex_lock(&acc->lock);
	val = acc->pdata->g_range ;
	switch (val) {
	case LSM330DLC_ACC_G_2G:
		range = 2;
		break;
	case LSM330DLC_ACC_G_4G:
		range = 4;
		break;
	case LSM330DLC_ACC_G_8G:
		range = 8;
		break;
	case LSM330DLC_ACC_G_16G:
		range = 16;
		break;
	}
	mutex_unlock(&acc->lock);
	return sprintf(buf, "%d\n", range);
}

static ssize_t attr_set_range(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct lsm330dlc_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	mutex_lock(&acc->lock);
	acc->pdata->g_range = val;
	lsm330dlc_acc_update_g_range(acc, val);
	mutex_unlock(&acc->lock);
	return size;
}

static ssize_t attr_get_enable(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct lsm330dlc_acc_data *acc = dev_get_drvdata(dev);
	int val = atomic_read(&acc->enabled);
	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_enable(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct lsm330dlc_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val)
		lsm330dlc_acc_enable(acc);
	else
		lsm330dlc_acc_disable(acc);

	return size;
}

static ssize_t attr_set_intconfig1(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, INT_CFG1, NO_MASK, RES_INT_CFG1);
}

static ssize_t attr_get_intconfig1(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, INT_CFG1);
}

#ifdef CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

static ssize_t attr_set_reference(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
    SENSOR_LOG_ERROR("enter\n");
    SENSOR_LOG_ERROR("buf is %s\n",buf);
	return write_reg(dev, buf, REFERENCE_REG, NO_MASK, RES_REFERENCE_REG);
    SENSOR_LOG_ERROR("exit\n");
}

static ssize_t attr_get_reference(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, REFERENCE_REG);
}


static ssize_t attr_set_crtl_reg1(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
    SENSOR_LOG_ERROR("enter\n");
    SENSOR_LOG_ERROR("buf is %s\n",buf);
	return write_reg(dev, buf, CTRL_REG1, NO_MASK, RES_CTRL_REG1);
    SENSOR_LOG_ERROR("exit\n");
}

static ssize_t attr_get_crtl_reg1(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, CTRL_REG1);
}

static ssize_t attr_set_crtl_reg2(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
    SENSOR_LOG_ERROR("enter\n");
    SENSOR_LOG_ERROR("buf is %s\n",buf);
	return write_reg(dev, buf, CTRL_REG2, NO_MASK, RES_CTRL_REG2);
    SENSOR_LOG_ERROR("exit\n");
}

static ssize_t attr_get_crtl_reg2(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, CTRL_REG2);
}


static ssize_t attr_set_crtl_reg3(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
    SENSOR_LOG_ERROR("enter\n");
    SENSOR_LOG_ERROR("buf is %s\n",buf);
	return write_reg(dev, buf, CTRL_REG3, NO_MASK, RES_CTRL_REG3);
    SENSOR_LOG_ERROR("exit\n");
}

static ssize_t attr_get_crtl_reg3(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, CTRL_REG3);
}

static ssize_t attr_set_crtl_reg4(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
    SENSOR_LOG_ERROR("enter\n");
    SENSOR_LOG_ERROR("buf is %s\n",buf);
	return write_reg(dev, buf, CTRL_REG4, NO_MASK, RES_CTRL_REG4);
    SENSOR_LOG_ERROR("exit\n");
}

static ssize_t attr_get_crtl_reg4(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, CTRL_REG4);
}

static ssize_t attr_set_crtl_reg5(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
    SENSOR_LOG_ERROR("enter\n");
    SENSOR_LOG_ERROR("buf is %s\n",buf);
	return write_reg(dev, buf, CTRL_REG5, NO_MASK, RES_CTRL_REG5);
    SENSOR_LOG_ERROR("exit\n");
}

static ssize_t attr_get_crtl_reg5(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, CTRL_REG5);
}


static ssize_t attr_set_crtl_reg6(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
    SENSOR_LOG_ERROR("enter\n");
    SENSOR_LOG_ERROR("buf is %s\n",buf);
	return write_reg(dev, buf, CTRL_REG6, NO_MASK, RES_CTRL_REG6);
    SENSOR_LOG_ERROR("exit\n");
}

static ssize_t attr_get_crtl_reg6(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, CTRL_REG6);
}

static ssize_t attr_set_debug(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
    unsigned long val;
    SENSOR_LOG_ERROR("enter\n");
    if (strict_strtoul(buf, 10, &val))
        return -EINVAL;
    if (val)
    {
        flag_debug = true;
        
    }
    else
    {
        flag_debug = false;
    }
    SENSOR_LOG_ERROR("exit\n");
	return size;
}

static ssize_t attr_get_debug(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return sprintf(buf, "%d\n", flag_debug);
}


static ssize_t attr_set_gpio_int_1(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
    unsigned long val;
    SENSOR_LOG_ERROR("enter\n");
    if (strict_strtoul(buf, 10, &val))
        return -EINVAL;
    if (val)
    {
        SENSOR_LOG_ERROR("set to 1\n");
        gpio_set_value(GPIO_G_SENSOR_INT1, 1);   
        SENSOR_LOG_ERROR("now is %d\n",gpio_get_value(GPIO_G_SENSOR_INT1));
    }
    else
    {
        SENSOR_LOG_ERROR("set to 0\n");
        gpio_set_value(GPIO_G_SENSOR_INT1, 0);   
        SENSOR_LOG_ERROR("now is %d\n",gpio_get_value(GPIO_G_SENSOR_INT1));  
    }
    SENSOR_LOG_ERROR("exit\n");
	return size;
}

static ssize_t attr_get_gpio_int_1(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return sprintf(buf, "%d\n", gpio_get_value(GPIO_G_SENSOR_INT1));
}


static ssize_t attr_set_irq_enable(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
    unsigned long val;
    SENSOR_LOG_ERROR("enter\n");
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if (val)
    {
        enable_irq(p_acc_data_global->irq1);
        
    }
    else
    {
        disable_irq(p_acc_data_global->irq1);
    }
    
    SENSOR_LOG_ERROR("exit\n");
    return 1;
}

static ssize_t attr_get_irq_enable(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return sprintf(buf, "%d\n", flag_enable_irq);
}


static ssize_t attr_set_file_event_vibrator(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
    unsigned long val;
    char data[16];
    SENSOR_LOG_ERROR("enter\n");
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if (val)
    {
        sprintf(data, "1");
        lsm330dlc_acc_write_file(ENABLE_EVENT_VIBRATOR_PATH, data);   
        flag_enable_miss_event = true;
    }
    else
    {
        sprintf(data, "0");
        lsm330dlc_acc_write_file(ENABLE_EVENT_VIBRATOR_PATH, data); 
        flag_enable_miss_event = false;
    }
    
    SENSOR_LOG_ERROR("exit\n");
    return 1;
}

static ssize_t attr_get_file_event_vibrator(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return sprintf(buf, "%d\n", lsm330dlc_acc_read_file(ENABLE_EVENT_VIBRATOR_PATH));
}


static ssize_t attr_set_file_have_miss_event(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
    unsigned long val;
    char data[16];
    SENSOR_LOG_ERROR("enter\n");
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if (val)
    {
        sprintf(data, "1");
        lsm330dlc_acc_write_file(HAVE_MISS_EVENT_PATH, data);   
        flag_have_miss_event = true;
    }
    else
    {
        sprintf(data, "0");
        lsm330dlc_acc_write_file(HAVE_MISS_EVENT_PATH, data); 
        flag_have_miss_event = false;
    }
    
    SENSOR_LOG_ERROR("exit\n");
    return 1;
}

static ssize_t attr_get_file_have_miss_event(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return sprintf(buf, "%d\n", lsm330dlc_acc_read_file(HAVE_MISS_EVENT_PATH));
}


static ssize_t attr_set_have_miss_event(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
    unsigned long val;
    SENSOR_LOG_ERROR("enter\n");
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if (val)
    {
        flag_have_miss_event = true;
        
    }
    else
    {
        flag_have_miss_event = false;
    }
    
    SENSOR_LOG_ERROR("exit\n");
    return 1;
}

static ssize_t attr_get_have_miss_event(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return sprintf(buf, "%d\n", flag_have_miss_event);
}

static ssize_t attr_set_enable_event_vibrator(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
    unsigned long val;
    SENSOR_LOG_ERROR("enter\n");
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if (val)
    {
        flag_enable_miss_event= true;
        
    }
    else
    {
        flag_enable_miss_event = false;
    }
    
    SENSOR_LOG_ERROR("exit\n");
    return 1;
}

static ssize_t attr_get_enable_event_vibrator(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return sprintf(buf, "%d\n", flag_enable_miss_event);
}

#endif //end of CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

static ssize_t attr_set_duration1(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, INT_DUR1, INT1_DURATION_MASK, RES_INT_DUR1);
}

static ssize_t attr_get_duration1(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev,buf,INT_DUR1);
}

static ssize_t attr_set_thresh1(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev,buf, INT_THS1, INT1_THRESHOLD_MASK,RES_INT_THS1);
}

static ssize_t attr_get_thresh1(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev,buf,INT_THS1);
}

static ssize_t attr_get_source1(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return read_single_reg(dev,buf,INT_SRC1);
}

static ssize_t attr_set_click_cfg(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, TT_CFG, TAP_CFG_MASK, RES_TT_CFG);
}

static ssize_t attr_get_click_cfg(struct device *dev,
		struct device_attribute *attr,	char *buf)
{

	return read_single_reg(dev, buf, TT_CFG);
}

static ssize_t attr_get_click_source(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, TT_SRC);
}

static ssize_t attr_set_click_ths(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, TT_THS, TAP_THS_MASK, RES_TT_THS);
}

static ssize_t attr_get_click_ths(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, TT_THS);
}

static ssize_t attr_set_click_tlim(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, TT_LIM, TAP_TLIM_MASK, RES_TT_LIM);
}

static ssize_t attr_get_click_tlim(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, TT_LIM);
}

static ssize_t attr_set_click_tlat(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, TT_TLAT, TAP_TLAT_MASK, RES_TT_TLAT);
}

static ssize_t attr_get_click_tlat(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, TT_TLAT);
}

static ssize_t attr_set_click_tw(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, TT_TLAT, TAP_TW_MASK, RES_TT_TLAT);
}

static ssize_t attr_get_click_tw(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, TT_TLAT);
}


#if DEBUG
/* PAY ATTENTION: These DEBUG funtions don't manage resume_state */
static ssize_t attr_reg_set(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	int rc;
	struct lsm330dlc_acc_data *acc = dev_get_drvdata(dev);
	u8 x[2];
	unsigned long val;

	if (strict_strtoul(buf, 16, &val))
		return -EINVAL;
	mutex_lock(&acc->lock);
	x[0] = acc->reg_addr;
	mutex_unlock(&acc->lock);
	x[1] = val;
	rc = lsm330dlc_acc_i2c_write(acc, x, 1);
	/*TODO: error need to be managed */
	return size;
}

static ssize_t attr_reg_get(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	ssize_t ret;
	struct lsm330dlc_acc_data *acc = dev_get_drvdata(dev);
	int rc;
	u8 data;

	mutex_lock(&acc->lock);
	data = acc->reg_addr;
	mutex_unlock(&acc->lock);
	rc = lsm330dlc_acc_i2c_read(acc, &data, 1);
	/*TODO: error need to be managed */
	ret = sprintf(buf, "0x%02x\n", data);
	return ret;
}

static ssize_t attr_addr_set(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct lsm330dlc_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val;
	if (strict_strtoul(buf, 16, &val))
		return -EINVAL;
	mutex_lock(&acc->lock);
	acc->reg_addr = val;
	mutex_unlock(&acc->lock);
	return size;
}
#endif

static struct device_attribute attributes[] = {

	__ATTR(pollrate_ms, 0664, attr_get_polling_rate, attr_set_polling_rate),
	__ATTR(range, 0644, attr_get_range, attr_set_range),
	__ATTR(enable_device, 0664, attr_get_enable, attr_set_enable),
	__ATTR(int1_config, 0644, attr_get_intconfig1, attr_set_intconfig1),

#ifdef CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

    __ATTR(reference_config, 0644, attr_get_reference, attr_set_reference),
    __ATTR(crtl_reg1, 0644, attr_get_crtl_reg1, attr_set_crtl_reg1),
    __ATTR(crtl_reg2, 0644, attr_get_crtl_reg2, attr_set_crtl_reg2),
    __ATTR(crtl_reg3, 0644, attr_get_crtl_reg3, attr_set_crtl_reg3),
    __ATTR(crtl_reg4, 0644, attr_get_crtl_reg4, attr_set_crtl_reg4),
    __ATTR(crtl_reg5, 0644, attr_get_crtl_reg5, attr_set_crtl_reg5),
    __ATTR(crtl_reg6, 0644, attr_get_crtl_reg6, attr_set_crtl_reg6),
    __ATTR(enable_debug, 0644, attr_get_debug, attr_set_debug),
    __ATTR(ctrl_gpio_int_1, 0644, attr_get_gpio_int_1, attr_set_gpio_int_1),
    __ATTR(crtl_irq_enable, 0644, attr_get_irq_enable, attr_set_irq_enable),
    __ATTR(crtl_event_vibrator, 0644, attr_get_enable_event_vibrator, attr_set_enable_event_vibrator),
    __ATTR(crtl_have_miss_event, 0644, attr_get_have_miss_event, attr_set_have_miss_event),
    __ATTR(crtl_file_have_miss_event, 0644, attr_get_file_have_miss_event, attr_set_file_have_miss_event),
    __ATTR(crtl_file_event_vibrator, 0644, attr_get_file_event_vibrator, attr_set_file_event_vibrator),

#endif //end of CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

	__ATTR(int1_duration, 0644, attr_get_duration1, attr_set_duration1),
	__ATTR(int1_threshold, 0644, attr_get_thresh1, attr_set_thresh1),
	__ATTR(int1_source, 0444, attr_get_source1, NULL),
	__ATTR(click_config, 0644, attr_get_click_cfg, attr_set_click_cfg),
	__ATTR(click_source, 0444, attr_get_click_source, NULL),
	__ATTR(click_threshold, 0644, attr_get_click_ths, attr_set_click_ths),
	__ATTR(click_timelimit, 0644, attr_get_click_tlim, attr_set_click_tlim),
	__ATTR(click_timelatency, 0644, attr_get_click_tlat,
							attr_set_click_tlat),
	__ATTR(click_timewindow, 0644, attr_get_click_tw, attr_set_click_tw),

#if DEBUG
	__ATTR(reg_value, 0600, attr_reg_get, attr_reg_set),
	__ATTR(reg_addr, 0200, NULL, attr_addr_set),
#endif
};

static int create_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		if (device_create_file(dev, attributes + i))
			goto error;
	return 0;

error:
	for ( ; i >= 0; i--)
		device_remove_file(dev, attributes + i);
	dev_err(dev, "%s:Unable to create interface\n", __func__);
	return -1;
}

static int remove_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);
	return 0;
}

static void lsm330dlc_acc_input_work_func(struct work_struct *work)
{
	struct lsm330dlc_acc_data *acc;

	int xyz[3] = { 0 };
	int err;

	acc = container_of((struct delayed_work *)work,
			struct lsm330dlc_acc_data, input_work);

	mutex_lock(&acc->lock);
	err = lsm330dlc_acc_get_acceleration_data(acc, xyz);
	if (err < 0)
		dev_err(&acc->client->dev, "get_acceleration_data failed\n");
	else
		lsm330dlc_acc_report_values(acc, xyz);

	schedule_delayed_work(&acc->input_work, msecs_to_jiffies(
			acc->pdata->poll_interval));
	mutex_unlock(&acc->lock);
}


static int lsm330dlc_acc_validate_pdata(struct lsm330dlc_acc_data *acc)
{
	/* checks for correctness of minimal polling period */
	acc->pdata->min_interval =
		max((unsigned int)LSM330DLC_ACC_MIN_POLL_PERIOD_MS,
						acc->pdata->min_interval);

	acc->pdata->poll_interval = max(acc->pdata->poll_interval,
			acc->pdata->min_interval);

	if (acc->pdata->axis_map_x > 2 ||
		acc->pdata->axis_map_y > 2 ||
		 acc->pdata->axis_map_z > 2) {
		dev_err(&acc->client->dev, "invalid axis_map value "
			"x:%u y:%u z%u\n", acc->pdata->axis_map_x,
				acc->pdata->axis_map_y, acc->pdata->axis_map_z);
		return -EINVAL;
	}

	/* Only allow 0 and 1 for negation boolean flag */
	if (acc->pdata->negate_x > 1 || acc->pdata->negate_y > 1
			|| acc->pdata->negate_z > 1) {
		dev_err(&acc->client->dev, "invalid negate value "
			"x:%u y:%u z:%u\n", acc->pdata->negate_x,
				acc->pdata->negate_y, acc->pdata->negate_z);
		return -EINVAL;
	}

	/* Enforce minimum polling interval */
	if (acc->pdata->poll_interval < acc->pdata->min_interval) {
		dev_err(&acc->client->dev, "minimum poll interval violated\n");
		return -EINVAL;
	}

	return 0;
}

static int lsm330dlc_acc_input_init(struct lsm330dlc_acc_data *acc)
{
	int err;

	INIT_DELAYED_WORK(&acc->input_work, lsm330dlc_acc_input_work_func);
	acc->input_dev = input_allocate_device();
	if (!acc->input_dev) {
		err = -ENOMEM;
		dev_err(&acc->client->dev, "input device allocation failed\n");
		goto err0;
	}

	acc->input_dev->name = LSM330DLC_ACC_DEV_NAME;
	//acc->input_dev->name = "accelerometer";
	acc->input_dev->id.bustype = BUS_I2C;
	acc->input_dev->dev.parent = &acc->client->dev;

	input_set_drvdata(acc->input_dev, acc);

	set_bit(EV_ABS, acc->input_dev->evbit);
	/*	next is used for interruptA sources data if the case */
	set_bit(ABS_MISC, acc->input_dev->absbit);
	/*	next is used for interruptB sources data if the case */
	set_bit(ABS_WHEEL, acc->input_dev->absbit);

	input_set_abs_params(acc->input_dev, ABS_X, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(acc->input_dev, ABS_Y, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(acc->input_dev, ABS_Z, -G_MAX, G_MAX, FUZZ, FLAT);
	/*	next is used for interruptA sources data if the case */
	input_set_abs_params(acc->input_dev, ABS_MISC, INT_MIN, INT_MAX, 0, 0);
	/*	next is used for interruptB sources data if the case */
	input_set_abs_params(acc->input_dev, ABS_WHEEL, INT_MIN, INT_MAX, 0, 0);


	err = input_register_device(acc->input_dev);
	if (err) {
		dev_err(&acc->client->dev,
				"unable to register input device %s\n",
				acc->input_dev->name);
		goto err1;
	}

	return 0;

err1:
	input_free_device(acc->input_dev);
err0:
	return err;
}

static void lsm330dlc_acc_input_cleanup(struct lsm330dlc_acc_data *acc)
{
	input_unregister_device(acc->input_dev);
	input_free_device(acc->input_dev);
}

static int lsm330dlc_acc_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{

	struct lsm330dlc_acc_data *acc;

	u32 smbus_func = I2C_FUNC_SMBUS_BYTE_DATA | 
			I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_I2C_BLOCK ;

	int err = -1;

    SENSOR_LOG_ERROR("enter\n");

	/* Support for both I2C and SMBUS adapter interfaces. */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_warn(&client->dev, "client not i2c capable\n");
		if (i2c_check_functionality(client->adapter, smbus_func)){
		use_smbus = 1;
		dev_warn(&client->dev, "client using SMBUS\n");
		} else {
			err = -ENODEV;
			dev_err(&client->dev, "client nor SMBUS capable\n");
			goto exit_check_functionality_failed;
		}
	}


	acc = kzalloc(sizeof(struct lsm330dlc_acc_data), GFP_KERNEL);
	if (acc == NULL) 
    {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for module data: "
					"%d\n", err);
		goto exit_check_functionality_failed;
	}

#ifdef CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

    else
    {
        p_acc_data_global = acc;
    }

#endif //end of CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT


	mutex_init(&acc->lock);
	mutex_lock(&acc->lock);

	acc->client = client;
	i2c_set_clientdata(client, acc);

	acc->pdata = kmalloc(sizeof(*acc->pdata), GFP_KERNEL);
	if (acc->pdata == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for pdata: %d\n",
				err);
		goto err_mutexunlock;
	}

	if (client->dev.platform_data == NULL) {	
		memcpy(acc->pdata, &default_lsm330dlc_acc_pdata,
							sizeof(*acc->pdata));	
	} else {
		memcpy(acc->pdata, client->dev.platform_data,
							sizeof(*acc->pdata));
	}

	err = lsm330dlc_acc_validate_pdata(acc);
	if (err < 0) {
		dev_err(&client->dev, "failed to validate platform data\n");
		goto exit_kfree_pdata;
	}
    
    /*
	if (acc->pdata->init) {
		err = acc->pdata->init(client);
		if (err < 0) {
			dev_err(&client->dev, "init failed: %d\n", err);
			goto err_pdata_init;
		}
	}
    */

    err = acc->pdata->gpio_init();
    if (err)
    {
        SENSOR_LOG_ERROR("sensor_acc_lsm330d_goio_init failed!\n");
        goto err_pdata_init;
    }
    else
    {
        SENSOR_LOG_ERROR("sensor_acc_lsm330d_goio_init success!\n");
    }

	if(acc->pdata->gpio_int1 >= 0)
    {
		acc->irq1 = gpio_to_irq(acc->pdata->gpio_int1);

        SENSOR_LOG_ERROR("%s: has set irq1 to irq: %d "
						"mapped on gpio:%d\n",
		LSM330DLC_ACC_DEV_NAME, acc->irq1,
						acc->pdata->gpio_int1);
	}

	if(acc->pdata->gpio_int2 >= 0){
		acc->irq2 = gpio_to_irq(acc->pdata->gpio_int2);
		SENSOR_LOG_INFO( "%s: %s has set irq2 to irq: %d "
							"mapped on gpio:%d\n",
			LSM330DLC_ACC_DEV_NAME, __func__, acc->irq2,
							acc->pdata->gpio_int2);
	}

	memset(acc->resume_state, 0, ARRAY_SIZE(acc->resume_state));

	acc->resume_state[RES_CTRL_REG1] = LSM330DLC_ACC_ENABLE_ALL_AXES;
	acc->resume_state[RES_CTRL_REG2] = 0x00;
	acc->resume_state[RES_CTRL_REG3] = 0x40;//0x00;
	acc->resume_state[RES_CTRL_REG4] = 0x00;
	acc->resume_state[RES_CTRL_REG5] = 0x00;
	acc->resume_state[RES_CTRL_REG6] = 0x00;

	acc->resume_state[RES_TEMP_CFG_REG] = 0x00;
	acc->resume_state[RES_FIFO_CTRL_REG] = 0x00;
	acc->resume_state[RES_INT_CFG1] = 0x00;
	acc->resume_state[RES_INT_THS1] = 0x7f;
	acc->resume_state[RES_INT_DUR1] = 0x7f;

	acc->resume_state[RES_TT_CFG] = 0x00;
	acc->resume_state[RES_TT_THS] = 0x00;
	acc->resume_state[RES_TT_LIM] = 0x00;
	acc->resume_state[RES_TT_TLAT] = 0x00;
	acc->resume_state[RES_TT_TW] = 0x00;

	err = lsm330dlc_acc_device_power_on(acc);
	if (err < 0) {
		dev_err(&client->dev, "power on failed: %d\n", err);
		goto err_pdata_init;
	}

	atomic_set(&acc->enabled, 1);

	err = lsm330dlc_acc_update_g_range(acc, acc->pdata->g_range);
	if (err < 0) {
		dev_err(&client->dev, "update_g_range failed\n");
		goto  err_power_off;
	}

	err = lsm330dlc_acc_update_odr(acc, acc->pdata->poll_interval);
	if (err < 0) {
		dev_err(&client->dev, "update_odr failed\n");
		goto  err_power_off;
	}

	err = lsm330dlc_acc_input_init(acc);
	if (err < 0) {
		dev_err(&client->dev, "input init failed\n");
		goto err_power_off;
	}


	err = create_sysfs_interfaces(&client->dev);
	if (err < 0) {
		dev_err(&client->dev,
		   "device LSM330DLC_ACC_DEV_NAME sysfs register failed\n");
		goto err_input_cleanup;
	}


	lsm330dlc_acc_device_power_off(acc);

	/* As default, do not report information */
	atomic_set(&acc->enabled, 0);

	if(acc->pdata->gpio_int1 >= 0)
    {
		INIT_WORK(&acc->irq1_work, lsm330dlc_acc_irq1_work_func);
		acc->irq1_work_queue =
			create_singlethread_workqueue("lsm330dlc_acc_wq1");
		if (!acc->irq1_work_queue) {
			err = -ENOMEM;
			dev_err(&client->dev,
					"cannot create work queue1: %d\n", err);
			goto err_remove_sysfs_int;
		}
		err = request_irq(acc->irq1, lsm330dlc_acc_isr1,
			IRQF_TRIGGER_RISING, "lsm330dlc_acc_irq1", acc);
		if (err < 0) {
			dev_err(&client->dev, "request irq1 failed: %d\n", err);
			goto err_destoyworkqueue1;
		}

#ifdef CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

		disable_irq_nosync(acc->irq1);
        err = enable_irq_wake(acc->irq1);
    	if(err < 0)
        {
            SENSOR_LOG_ERROR("enable_irq_wake failed\n");
            return err;
        }

#endif //end of CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

	}

	if(acc->pdata->gpio_int2 >= 0){
		INIT_WORK(&acc->irq2_work, lsm330dlc_acc_irq2_work_func);
		acc->irq2_work_queue =
			create_singlethread_workqueue("lsm330dlc_acc_wq2");
		if (!acc->irq2_work_queue) {
			err = -ENOMEM;
			dev_err(&client->dev,
					"cannot create work queue2: %d\n", err);
			goto err_free_irq1;
		}
		err = request_irq(acc->irq2, lsm330dlc_acc_isr2,
			IRQF_TRIGGER_RISING, "lsm330dlc_acc_irq2", acc);
		if (err < 0) {
			dev_err(&client->dev, "request irq2 failed: %d\n", err);
			goto err_destoyworkqueue2;
		}
		disable_irq_nosync(acc->irq2);
	}
    
#ifdef CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

    #ifdef CONFIG_HAS_EARLYSUSPEND
	acc->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	acc->early_suspend.suspend = lsm330dlc_acc_earlysuspend_suspend;
	acc->early_suspend.resume =  lsm330dlc_acc_earlysuspend_resume;
	register_early_suspend(&acc->early_suspend);
    #endif //end of CONFIG_HAS_EARLYSUSPEND

    wake_lock_init(&acc->lsm330dlc_acc_wake_lock, WAKE_LOCK_SUSPEND, "lsm330dlc_acc_wake_lock");

#endif //end of CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

	mutex_unlock(&acc->lock);

    SENSOR_LOG_ERROR("exit\n");
	return 0;

#ifdef CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

    wake_lock_destroy(&acc->lsm330dlc_acc_wake_lock);

#endif //end of CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

err_destoyworkqueue2:
	if(acc->pdata->gpio_int2 >= 0)
		destroy_workqueue(acc->irq2_work_queue);
err_free_irq1:
	free_irq(acc->irq1, acc);
err_destoyworkqueue1:
	if(acc->pdata->gpio_int1 >= 0)
		destroy_workqueue(acc->irq1_work_queue);
err_remove_sysfs_int:
	remove_sysfs_interfaces(&client->dev);
err_input_cleanup:
	lsm330dlc_acc_input_cleanup(acc);
err_power_off:
	lsm330dlc_acc_device_power_off(acc);
err_pdata_init:
	if (acc->pdata->exit)
		acc->pdata->exit();
exit_kfree_pdata:
	kfree(acc->pdata);
err_mutexunlock:
	mutex_unlock(&acc->lock);
//err_freedata:
	kfree(acc);
exit_check_functionality_failed:
	printk(KERN_ERR "%s: Driver Init failed\n", LSM330DLC_ACC_DEV_NAME);
	return err;
}

static int __devexit lsm330dlc_acc_remove(struct i2c_client *client)
{

	struct lsm330dlc_acc_data *acc = i2c_get_clientdata(client);

	if(acc->pdata->gpio_int1 >= 0){
		free_irq(acc->irq1, acc);
		gpio_free(acc->pdata->gpio_int1);
		destroy_workqueue(acc->irq1_work_queue);
	}

	if(acc->pdata->gpio_int2 >= 0){
		free_irq(acc->irq2, acc);
		gpio_free(acc->pdata->gpio_int2);
		destroy_workqueue(acc->irq2_work_queue);
	}

#ifdef CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

    #ifdef    CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&acc->early_suspend);
    #endif //end of CONFIG_HAS_EARLYSUSPEND

	lsm330dlc_acc_input_cleanup(acc);
	lsm330dlc_acc_device_power_off(acc);
	remove_sysfs_interfaces(&client->dev);

#endif //end of CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

	if (acc->pdata->exit)
		acc->pdata->exit();
	kfree(acc->pdata);
	kfree(acc);

	return 0;
}


#ifdef CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

static unsigned int lsm330dlc_acc_enable_irq_config(struct lsm330dlc_acc_data *acc)
{
    int ret = 0;
    unsigned char buf[2] = {0,0};

    SENSOR_LOG_ERROR("enter\n");
    
    //enable high pass filter
    buf[0] = CTRL_REG2;
    buf[1] = 0x09;
    ret = lsm330dlc_acc_i2c_write(acc, buf, 1);
    if (ret < 0)
    {
        SENSOR_LOG_ERROR("enable high pass filter failed\n");
        return ret;
    }
    
    //enable int int aio mode
    buf[0] = CTRL_REG3;
    buf[1] = 0x40;
    ret = lsm330dlc_acc_i2c_write(acc, buf, 1);
    if (ret < 0)
    {
        SENSOR_LOG_ERROR("disable int int aio mode failed\n");
        return ret;
    }
   
    //set int_duration
    buf[0] = INT_DUR1;
    buf[1] = 0x03;
    ret = lsm330dlc_acc_i2c_write(acc, buf, 1);
    if (ret < 0)
    {
        SENSOR_LOG_ERROR("set int_duration failed\n");
        return ret;
    }

    //set int_threshold
    buf[0] = INT_THS1;
    buf[1] = 0x08; //500mg
    ret = lsm330dlc_acc_i2c_write(acc, buf, 1);
    if (ret < 0)
    {
        SENSOR_LOG_ERROR("set int_threshold failed\n");
        return ret;
    }
   
    //read the int_src
    buf[0] = INT_SRC1;
    ret = lsm330dlc_acc_i2c_read(acc, buf, 1);
    SENSOR_LOG_ERROR("INI1_SRC = 0x%02X\n",ret);

    //set int_cfg1
    buf[0] = INT_CFG1;
    buf[1] = 0x2a; //XH or YH or ZH
    ret = lsm330dlc_acc_i2c_write(acc, buf, 1);
    if (ret < 0)
    {
        SENSOR_LOG_ERROR("set int_cfg1 failed\n");
        return ret;
    }

    SENSOR_LOG_ERROR("flag_enable_irq = %d\n",flag_enable_irq);

    //enable irq
    if (false == flag_enable_irq)
    {
        flag_enable_irq = true;
        enable_irq(p_acc_data_global->irq1);
        SENSOR_LOG_ERROR("enable irq\n");
    }
    
    SENSOR_LOG_ERROR("exit\n");

    return 0;
    
}

static unsigned int lsm330dlc_acc_disable_irq_config(struct lsm330dlc_acc_data *acc)
{
    int ret;
    unsigned char buf[2] = {0,0};

    SENSOR_LOG_ERROR("enter\n");

    //set int_cfg1
    buf[0] = INT_CFG1;
    buf[1] = 0x00;
    ret = lsm330dlc_acc_i2c_write(acc, buf, 1);
    if (ret < 0)
    {
        SENSOR_LOG_ERROR("set int_cfg1 failed\n");
        return ret;
    }

    //disable high pass filter
    buf[0] = CTRL_REG2;
    buf[1] = 0x00;
    ret = lsm330dlc_acc_i2c_write(acc, buf, 1);
    if (ret < 0)
    {
        SENSOR_LOG_ERROR("set high pass filter failed\n");
        return ret;
    }

    //disable int int aio mode
    buf[0] = CTRL_REG3;
    buf[1] = 0x00;
    ret = lsm330dlc_acc_i2c_write(acc, buf, 1);
    if (ret < 0)
    {
        SENSOR_LOG_ERROR("disable int int aio mode failed\n");
        return ret;
    }
   
    //set int_duration
    buf[0] = INT_DUR1;
    buf[1] = 0x7f;
    ret = lsm330dlc_acc_i2c_write(acc, buf, 1);
    if (ret < 0)
    {
        SENSOR_LOG_ERROR("set int_duration failed\n");
        return ret;
    }

    //set int_threshold
    buf[0] = INT_THS1;
    buf[1] = 0x7f; //0mg
    ret = lsm330dlc_acc_i2c_write(acc, buf, 1);
    if (ret < 0)
    {
        SENSOR_LOG_ERROR("set int_threshold failed\n");
        return ret;
    }
    
    SENSOR_LOG_ERROR("exit\n");

    return 0;
}


#ifdef CONFIG_HAS_EARLYSUSPEND

void lsm330dlc_acc_earlysuspend_suspend(struct early_suspend *h)
{
    int ret = 0;
    struct lsm330dlc_acc_data *acc = container_of(h, struct lsm330dlc_acc_data, early_suspend);
    
    flag_enable_irq_respond = false;

    flag_enable_miss_event  = lsm330dlc_acc_read_file(ENABLE_EVENT_VIBRATOR_PATH);
    flag_enable_sleep_clock = lsm330dlc_acc_read_file(ENABLE_SLEEP_CLOCK_PATH);
    flag_have_miss_event    = lsm330dlc_acc_read_file(HAVE_MISS_EVENT_PATH);

    if (flag_enable_miss_event || flag_enable_sleep_clock)
    {
        SENSOR_LOG_ERROR("acc->enabled = %d and %s to disable poll\n",atomic_read(&acc->enabled),atomic_read(&acc->enabled)? "now" : "not");
        if (atomic_read(&acc->enabled)) 
        {     
            lsm330dlc_acc_disable(acc);
            flag_enable_record = true;
        }

        ret = lsm330dlc_acc_enable_irq_config(acc);
        
        if (ret < 0)
        {
            SENSOR_LOG_ERROR("enable_irq_config failed\n");
        }
        else
        {
            SENSOR_LOG_ERROR("enable_irq_config succes\n");
        }

        msleep(50);
        flag_enable_irq_respond = true;
    }
}

void lsm330dlc_acc_earlysuspend_resume(struct early_suspend *h)
{
    int ret = 0;
    struct lsm330dlc_acc_data *acc = container_of(h, struct lsm330dlc_acc_data, early_suspend);

    if (flag_enable_miss_event || flag_enable_sleep_clock)
    {
        ret = lsm330dlc_acc_disable_irq_config(acc);
        SENSOR_LOG_ERROR("flag_enable_record = %d and %s to enable poll\n",flag_enable_record,atomic_read(&acc->enabled)? "now" : "not");
        if (flag_enable_record) 
        {            
            flag_enable_record = false;
			atomic_set(&acc->enabled, 0);
            SENSOR_LOG_ERROR("enable acc\n");
            lsm330dlc_acc_enable(acc);
        }
       
        if (ret < 0)
        {
            SENSOR_LOG_ERROR("disable_irq_config failed\n");
        }
        else
        {
            SENSOR_LOG_ERROR("disable_irq_config succes\n");
        }
    }
}
#endif //end of CONFIG_HAS_EARLYSUSPEND

void lsm330dlc_acc_vibrator_work(int time_ms)
{
    #ifdef CONFIG_ZTEMT_VIBRATOR_DRV2605
    extern_linear_vibrator_enable(0, time_ms);
    #endif

    #ifdef CONFIG_PMIC8XXX_VIBRATOR
    extern_nolinear_vibrator_enable(time_ms);
    #endif

    mdelay(time_ms);

    #ifdef CONFIG_ZTEMT_VIBRATOR_DRV2605
    extern_linear_vibrator_enable(0, 0);
    #endif

    #ifdef CONFIG_PMIC8XXX_VIBRATOR
    extern_nolinear_vibrator_enable(0);
    #endif
}

#endif //end of CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

#ifdef CONFIG_PM
static int lsm330dlc_acc_resume(struct i2c_client *client)
{
#ifndef CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

	struct lsm330dlc_acc_data *acc = i2c_get_clientdata(client);
	if (acc->on_before_suspend)
		return lsm330dlc_acc_enable(acc);

#else

#endif //end of CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

    return 0;
}

static int lsm330dlc_acc_suspend(struct i2c_client *client, pm_message_t mesg)
{
#ifndef CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT

	struct lsm330dlc_acc_data *acc = i2c_get_clientdata(client);

	acc->on_before_suspend = atomic_read(&acc->enabled);
	return lsm330dlc_acc_disable(acc);

#else

    return 0;
    
#endif //end of CONFIG_FEATURE_ZTEMT_VIBRATOR_HAVE_MISS_EVENT
}
#else
#define lsm330dlc_acc_suspend	NULL
#define lsm330dlc_acc_resume	NULL
#endif /* CONFIG_PM */

static const struct i2c_device_id lsm330dlc_acc_id[]
		= { { LSM330DLC_ACC_DEV_NAME, 0 }, { }, };

MODULE_DEVICE_TABLE(i2c, lsm330dlc_acc_id);


static struct i2c_driver lsm330dlc_acc_driver = {
	.driver = {
			.owner = THIS_MODULE,
			.name = LSM330DLC_ACC_DEV_NAME,
		  },
	.probe = lsm330dlc_acc_probe,
	.remove = __devexit_p(lsm330dlc_acc_remove),
	.suspend = lsm330dlc_acc_suspend,
	.resume = lsm330dlc_acc_resume,
	.id_table = lsm330dlc_acc_id,
};

static int __init lsm330dlc_acc_init(void)
{
	SENSOR_LOG_INFO( "%s   lsm330dlc_acc_init accelerometer driver: init\n",LSM330DLC_ACC_DEV_NAME);
	return i2c_add_driver(&lsm330dlc_acc_driver);
}

static void __exit lsm330dlc_acc_exit(void)
{
#ifdef DEBUG
	SENSOR_LOG_INFO( "%s accelerometer driver exit\n",
						LSM330DLC_ACC_DEV_NAME);
#endif /* DEBUG */
	i2c_del_driver(&lsm330dlc_acc_driver);
	return;
}

module_init(lsm330dlc_acc_init);
module_exit(lsm330dlc_acc_exit);

MODULE_DESCRIPTION("lsm330dlc accelerometer section sysfs driver");
MODULE_AUTHOR("Matteo Dameno, Carmine Iascone, STMicroelectronics");
MODULE_LICENSE("GPL");

