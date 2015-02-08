/*
 * synaptics_TOUCHKEY.c - SYNAPTICS TOUCHKEY ONETOUCH SENSOR CONTROLER
*/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <asm/gpio.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/leds.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mux.h>
#include <linux/kthread.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <hsad/config_interface.h>
#include "synaptics_SO340010.h"
#include <linux/rmi.h>
#include "tp_tk_regulator.h"

#define TOUCHKEY_DEBUG
#define TK_CMD_PENDING_FUNC

#define PRESS_KEY                               1
#define RELEASE_KEY                           0
#define TK_KEY_MAX                            4
#define TOUCHKEY_THREAD_WAKE     1

#define SENSITIVITY10_FINGER 		0x7A00
#define SENSITIVITY32_FINGER		0x727C
#define SENSITIVITY10_WET		0x6000
#define SENSITIVITY32_WET		0x5C6A
#define SENSITIVITY10_GLOVE 		0xCFCF
#define SENSITIVITY32_GLOVE		0xCFCF

#define TK_CMD_PENDING_TIME 1500
#define SYNAPTICS_I2C_RETRY_TIMES    5

extern atomic_t touch_is_pressed;    
extern u32 time_finger_up;    
atomic_t tk_is_pressed  = ATOMIC_INIT(0);
static atomic_t tk_power_state = ATOMIC_INIT(0);
 u32 synaptics_release_time = 0;
 EXPORT_SYMBOL(synaptics_release_time);

/*struct area begin*/
typedef enum touchkey_cmd_type_tag {
	TOUCHKEY_CMD_IRQ = 0,
	TOUCHKEY_CMD_SENSITIVITY = 1,
	TOUCHKEY_CMD_LED = 2,
	TOUCHKEY_CMD_TYPE_MAX = 3,
} touchkey_cmd_type;

#ifdef TK_CMD_PENDING_FUNC
typedef struct tk_cmd_pending_tag {
	bool is_cmd_pending[TOUCHKEY_CMD_TYPE_MAX];
	u32 cmd_pending_time[TOUCHKEY_CMD_TYPE_MAX];
} tk_cmd_pending_t;
#endif

struct touchkey_data {
	struct i2c_client *client;
	struct input_dev *input;
	struct early_suspend early_suspend;
	uint16_t key_matrix;
	unsigned short *keycodes;
	struct iomux_block *gpio_block;
	struct block_config *gpio_block_config;
	int keynum;
	int led_brightness_max;
	uint16_t reg_brightness;
	uint16_t reg_sensitivity_10;
	uint16_t reg_sensitivity_32;
	uint16_t defualt_sensitivity_10;
	uint16_t defualt_sensitivity_32;
	struct led_classdev synaptics_kp_bl_led;
	enum rmi_tp_status tp_status;
	struct mutex power_lock;
	struct task_struct *synaptics_task;
	unsigned long tk_cmd_flag;
	unsigned long  thread_waked;
	void (*cmd_type_handler[TOUCHKEY_CMD_TYPE_MAX])(struct touchkey_data *);
#ifdef TK_CMD_PENDING_FUNC
	tk_cmd_pending_t tk_cmd_pending;
#endif
	atomic_t enable_tk_flag;
};
struct touchkey_data *touchkey_info = NULL;

struct touchkey_debug_info {
	uint16_t irq_req;
	uint16_t led_req;
	uint16_t sensitivity_req;

	uint16_t irq_finished;
	uint16_t led_finished;
	uint16_t sensitivity_finished;

	uint16_t cmd_handler_cnt;
}g_tk_debug_info;


/*-----------------------------function definition begin-------------------------------------*/
static int touchkey_resume(struct i2c_client *client);
static int touchkey_suspend(struct i2c_client *client, pm_message_t mesg);
static void touchkey_early_suspend(struct early_suspend *h);
static void touchkey_later_resume(struct early_suspend *h);
static void touchkey_send_event(struct touchkey_data *tk_info,
										touchkey_cmd_type cmd_type);
static int i2c_synaptics_read( uint16_t address, uint16_t *data);
static int i2c_synaptics_write(uint16_t address, uint16_t data);
static uint16_t calc_reg_brightness(int led_brightness_max, enum led_brightness value);
static int set_reg_brightness(struct touchkey_data *tk_info);
static int calc_reg_sensitivity(struct touchkey_data *tk_info, uint8_t tp_status);
static int set_reg_sensitivity(struct touchkey_data *tk_info);

static int synaptics_set_initconfig(struct touchkey_data *syp_tk);
static void synaptics_thread_led_handler(struct touchkey_data *tk_info);
static void synaptics_thread_sensitivity_handler(struct touchkey_data *tk_info);

#ifdef TK_CMD_PENDING_FUNC
static void cmd_pending_add(struct touchkey_data *tk_info, touchkey_cmd_type cmd_type);
static void cmd_pending_init(struct touchkey_data *tk_info);
static void cmd_pending_process(struct touchkey_data *tk_info);
#endif

/*function definition end*/
#if 0
static void synaptics_i2c_error_processing(struct touchkey_data *tk_info)
{
	int ret;

	atomic_set(&tk_power_state, 0);

	TK_VCI_DISABLE();
	msleep(30);
	TK_VCI_ENABLE();
	msleep(70);

	ret = synaptics_set_initconfig(tk_info);
	if (ret < 0) {
		dev_err(&tk_info->client->dev,"%s: synaptics_set_initconfig failed\n",__func__);
	}
	atomic_set(&tk_power_state, 1);
}
#endif

static void synaptics_thread_led_handler(struct touchkey_data *tk_info)
{
	int ret;

	mutex_lock(&tk_info->power_lock);
	if ((atomic_read(&tk_power_state) == 0)) {
		dev_err(&tk_info->client->dev, "%s failed ,tk_power_state==0\n", __func__);
#ifdef TK_CMD_PENDING_FUNC
		cmd_pending_add(tk_info, TOUCHKEY_CMD_LED);
#endif
		goto out;
	} 
	ret = set_reg_brightness(tk_info);
	if (ret < 0) {
		dev_err(&tk_info->client->dev, "%s set_reg_brightness faildn",__func__);
	}

out:
	mutex_unlock(&tk_info->power_lock);
#ifdef TOUCHKEY_DEBUG
	g_tk_debug_info.led_finished++;
#endif
}

static void synaptics_thread_sensitivity_handler(struct touchkey_data *tk_info)
{
	int ret;

	mutex_lock(&tk_info->power_lock);
	if (atomic_read(&tk_power_state) == 0) {
		dev_err(&tk_info->client->dev, "%s failed ,tk_power_state==0\n", __func__);
		goto out;
	}
	if (set_reg_sensitivity(tk_info) < 0) {
		dev_err(&tk_info->client->dev, "%s set_reg_sensitivity failed\n", __func__);
	}

out:
	mutex_unlock(&tk_info->power_lock);
#ifdef TOUCHKEY_DEBUG
	g_tk_debug_info.sensitivity_finished++;
#endif
}

static void synaptics_thread_irq_handler(struct touchkey_data *tk_info)
{
	uint16_t reg_read = 0,status = 0, mask, keyval;
	uint16_t new_matrix, old_matrix;	
	u32 t, deltT;
	int ret;
	int i;

	mutex_lock(&tk_info->power_lock);

	if ((atomic_read(&tk_power_state) == 0)) {
		dev_err(&tk_info->client->dev, "%s failed ,tk_power_state==0\n", __func__);
		goto out;
	}

	ret = i2c_synaptics_read(GPIO_STATE, &reg_read);
	if(ret < 0) {
		dev_err(&tk_info->client->dev, 
			"%s:i2c_synaptics_read error: register[0x%x]\n", __func__, GPIO_STATE);
		//synaptics_i2c_error_processing(tk_info);
		goto out;
	}
	ret = i2c_synaptics_read(BUTTON_STATE, &reg_read);
	if(ret < 0) {
		dev_err(&tk_info->client->dev, 
			"%s:i2c_synaptics_read error: register[0x%x]\n", __func__, BUTTON_STATE);
		//synaptics_i2c_error_processing(tk_info);
		goto out;
	}
	dev_info(&tk_info->client->dev, "[%s],[reg_read:0x%x],[key_matrix:%d]\n",
		__func__, reg_read, tk_info->key_matrix);

	if (atomic_read(&tk_info->enable_tk_flag) == 0) {
		dev_err(&tk_info->client->dev, "%s TK is stopped by user layer\n", __func__);
		goto out;
	}

	t = (u32)ktime_to_ms(ktime_get());
	deltT = t - time_finger_up;
	if((((atomic_read(&touch_is_pressed) == 0) && ( deltT > 120))
	        || (atomic_read(&tk_is_pressed) == 1))) {
		new_matrix = reg_read & 0x000F;
		old_matrix = tk_info->key_matrix;
		mask = 1;
		for (i = 0; i < tk_info->keynum; ++i, mask <<= 1) {
			keyval = new_matrix & mask;
			if (((old_matrix & mask) != keyval) &&(tk_info->keycodes[i])) {
				input_report_key(tk_info->input, tk_info->keycodes[i], keyval);
				if (keyval) {
					atomic_set(&tk_is_pressed, 1);
				} else {
					atomic_set(&tk_is_pressed, 0);
					synaptics_release_time = (u32)ktime_to_ms(ktime_get());
				}
				break;
			}
		}
		input_sync(tk_info->input);
		tk_info->key_matrix = new_matrix;
	} else {
		dev_err(&tk_info->client->dev,
			"tk data not input, touch_is_pressed[%d],deltT[%d],tk_is_pressed[%d]\n",
			touch_is_pressed.counter, deltT, tk_is_pressed.counter);
	}

out:
	mutex_unlock(&tk_info->power_lock);
	enable_irq(tk_info->client->irq);
#ifdef TOUCHKEY_DEBUG
	g_tk_debug_info.irq_finished++;
#endif
}

static int synaptics_set_initconfig(struct touchkey_data *tk_info)
{
	int ret;
	uint16_t data;
	dev_info(&tk_info->client->dev, "%s called\n", __func__);

	ret = i2c_synaptics_write(BUTTON_ENABLE , 0x000E);
	if (ret < 0) {
		dev_err(&tk_info->client->dev, 
			"%s BUTTON_ENABLE failed\n",__func__);
		return ret;
	}
	ret = i2c_synaptics_write(GPIO_CONTROL , 0x0E0E);
	if (ret < 0) {
		dev_err(&tk_info->client->dev,
			"%s GPIO_CONTROL failed\n",__func__);
		return ret;
	}
	ret = i2c_synaptics_write(INTERFERENCE_THRESHOLD , 0x0080);
	if (ret < 0) {
		dev_err(&tk_info->client->dev,
			"%s INTERFERENCE_THRESHOLD failed\n",__func__);
		return ret;
	}
	ret = i2c_synaptics_write(BUTTON_TO_GPIO_MAPPING , 0x2000);
	if (ret < 0) {
		dev_err(&tk_info->client->dev,
			"%s BUTTON_TO_GPIO_MAPPING failed\n", __func__);
		return ret;
	}
	ret = i2c_synaptics_write(TIMER_CONTROL , 0xD000);
	if (ret < 0) {
		dev_err(&tk_info->client->dev,
			"%s TIMER_CONTROL failed\n",__func__);
		return ret;
	}
	ret = set_reg_sensitivity(tk_info);
	if (ret < 0) {
		dev_err(&tk_info->client->dev, "%s set_reg_brightness faildn", __func__);
	}

	ret = i2c_synaptics_write(INTERFACE_CONFIGUARATION , 0x0007);
	if (ret < 0) {
		dev_err(&tk_info->client->dev,"%s INTERFACE_CONFIGUARATION failed\n", __func__);
		return ret;
	}
	ret = i2c_synaptics_write(GENERAL_CONFIGURATION , 0x0060); //0060
	if(ret < 0) {
		dev_err(&tk_info->client->dev,"%s GENERAL_CONFIGURATION failed\n", __func__);
		return ret;
	}
	/*clean the possible irq which not handle*/
	ret = i2c_synaptics_read(GPIO_STATE, &data);
	if (ret <0) {
		dev_err(&tk_info->client->dev,"%s read GPIO_STATE failed\n",__func__);
		return ret;
	}
	ret = i2c_synaptics_read(BUTTON_STATE, &data);
	if(ret < 0) {
		dev_err(&tk_info->client->dev, 
			"%s:i2c_synaptics_read error: register[0x%x]\n", __func__, BUTTON_STATE);
		return ret;
	}

	return ret;
}

void set_tk_sensitivity(uint8_t tp_status)
{
	struct touchkey_data *tk_info = touchkey_info;

	if (NULL == touchkey_info) {
		dev_err(&tk_info->client->dev,
			"%s error NULL == touchkey_info\n",  __func__);
		return;
	}
	if ((atomic_read(&tk_power_state) == 0)) {
		dev_err(&tk_info->client->dev, "%s failed ,tk_power_state==0\n", __func__);
		return;
	}

	if (0 == calc_reg_sensitivity(tk_info, tp_status)) {
#ifdef TOUCHKEY_DEBUG
		g_tk_debug_info.sensitivity_req++;
#endif
		touchkey_send_event(tk_info, TOUCHKEY_CMD_SENSITIVITY);
	} else {
		dev_err(&tk_info->client->dev,
			"%s calc_reg_sensitivity failed\n", __func__);
	}
}

/* called in */
void touchkey_early_suspend_extern(void)
{
	if (touchkey_info) {
		touchkey_suspend(touchkey_info->client,PMSG_SUSPEND);
	} 
}

void touchkey_later_resume_extern(void)
{
	if (touchkey_info) {
		touchkey_resume(touchkey_info->client);
	}
}

/*---------------------------internal function------------------------------------------*/

static int touchkey_gpio_block_init(struct device *dev, struct touchkey_data *tk_info)
{
	int ret = 0;

	/* get gpio block*/
	tk_info->gpio_block = iomux_get_block(TOUCHKEY_GPIO_BOLCK_NAME);
	if (IS_ERR(tk_info->gpio_block)) {
		dev_err(dev, "%s: failed to get gpio block,iomux_get_block failed\n", __func__);
		ret = -EINVAL;
		return ret;
	}
	/* get gpio block config*/
	tk_info->gpio_block_config = iomux_get_blockconfig(TOUCHKEY_GPIO_BOLCK_NAME);
	if (IS_ERR(tk_info->gpio_block_config)) {
		dev_err(dev, "%s: failed to get gpio block config\n", __func__);
		ret = -EINVAL;
		goto err_block_config;
	}
	/* config gpio work mode*/
	ret = blockmux_set(tk_info->gpio_block, tk_info->gpio_block_config, NORMAL);
	if (ret) {
		dev_err(dev, "%s: failed to config gpio,blockmux_set failed\n", __func__);
		ret = -EINVAL;
		goto err_mux_set;
	}
	return ret;

err_mux_set:
	if (tk_info->gpio_block_config)
		tk_info->gpio_block_config = NULL;
err_block_config:
	if (tk_info->gpio_block)
		tk_info->gpio_block = NULL;

	return ret;

}

static int i2c_synaptics_write(uint16_t address, uint16_t data)
{
	struct touchkey_data *tk_info = touchkey_info;
	int retry = SYNAPTICS_I2C_RETRY_TIMES;
	int ret = 0;
	uint8_t buf[4];
	struct i2c_msg msg[] = {
		{
			.addr = touchkey_info->client->addr,
			.flags = 0,
			.len = 4,
			.buf = buf,
		}
	};
	buf[1] = address & 0xFF;
	buf[0] = (address >> 8) & 0xFF;
	buf[3] = data & 0xFF;
	buf[2] = (data >> 8) & 0xFF;

	while (retry--) {
		ret = i2c_transfer(touchkey_info->client->adapter, msg, 1);
		if(ret >= 0) {
			return 0;
		}
		dev_err(&tk_info->client->dev,
			"[TouchKey][reg 0x%x][data 0x%x] %s i2c transfer error\n",
			address,data,__func__);
		mdelay(10);
	}
	return ret;
}

static int i2c_synaptics_read( uint16_t address, uint16_t *data)
{
	struct touchkey_data *tk_info = touchkey_info;
	int retry = SYNAPTICS_I2C_RETRY_TIMES;
	int ret;
	uint8_t addr[2];
	uint8_t read_data[2];
	struct i2c_msg msg[] = {
		{
			.addr = touchkey_info->client->addr,
			.flags = 0,
			.len = 2,
			.buf = addr,
		},
		{
			.addr = touchkey_info->client->addr,
			.flags = I2C_M_RD,
			.len = 2,
			.buf = read_data,
		}
	};
	addr[1] = address & 0xFF;
	addr[0] = (address >> 8) & 0xFF;

	while (retry--) {
		ret = i2c_transfer(touchkey_info->client->adapter, msg, 2);
		if(ret >= 0) {
			(*data) = read_data[0]&0xFF;
			(*data) = (((*data) << 8) & 0xFF00) | read_data[1];
			return 0;
		}
		dev_err(&tk_info->client->dev,
			"[TouchKey] %s i2c transfer error\n", __func__);
		mdelay(10);
	}
	return ret;
}

static void touchkey_send_event(struct touchkey_data *tk_info,
										touchkey_cmd_type cmd_type)
{
	set_bit(cmd_type, &tk_info->tk_cmd_flag);
	set_bit(TOUCHKEY_THREAD_WAKE, &tk_info->thread_waked);
	wake_up_process(tk_info->synaptics_task);
}

static void touchkey_thread_cmd_handler(struct touchkey_data *tk_info)
{
	int i;

	for (i=0; i<TOUCHKEY_CMD_TYPE_MAX; i++) {
		if (test_and_clear_bit(i, &tk_info->tk_cmd_flag) && tk_info-> cmd_type_handler[i]) {
			tk_info-> cmd_type_handler[i](tk_info);
#ifdef TOUCHKEY_DEBUG
			g_tk_debug_info.cmd_handler_cnt++;
#endif
		}
	}
}

static int touchkey_thread_handler(void *p)
{
	struct touchkey_data *tk_info = p;
#if 0
	static const struct sched_param param = {
		.sched_priority = MAX_USER_RT_PRIO/2,
	};

	sched_setscheduler(current, SCHED_FIFO, &param);
#endif
	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (test_and_clear_bit(TOUCHKEY_THREAD_WAKE, &tk_info->thread_waked)) {
			set_current_state(TASK_RUNNING);
			touchkey_thread_cmd_handler(tk_info);
		}
		else {
			schedule();
		}
	}
	return 0;
}

static void touchkey_thread_cmd_init(struct touchkey_data *tk_info)
{
	tk_info->cmd_type_handler[TOUCHKEY_CMD_IRQ] = synaptics_thread_irq_handler;
	tk_info->cmd_type_handler[TOUCHKEY_CMD_SENSITIVITY] = synaptics_thread_sensitivity_handler;
	tk_info->cmd_type_handler[TOUCHKEY_CMD_LED] = synaptics_thread_led_handler;
	tk_info->tk_cmd_flag = 0;
	clear_bit(TOUCHKEY_THREAD_WAKE, &tk_info->thread_waked);
#ifdef TK_CMD_PENDING_FUNC
	cmd_pending_init(tk_info);
#endif
}

static irqreturn_t touchkey_irq_handler(int irq, void *dev_id)
{
	struct touchkey_data *tk_info = dev_id;

	disable_irq_nosync(tk_info->client->irq);
#ifdef TOUCHKEY_DEBUG
	g_tk_debug_info.irq_req++;
#endif
	touchkey_send_event(tk_info, TOUCHKEY_CMD_IRQ);
	return IRQ_HANDLED;
}

static uint16_t calc_reg_brightness(int led_brightness_max, enum led_brightness value)
{
	uint16_t brightness;
	uint16_t reg_brightness = 0;

	if (led_brightness_max < 0) {
		return reg_brightness;
	}
	brightness = (value*(led_brightness_max))/0xFF;
	if (brightness == 0) {
		reg_brightness = 0;
	} else {
		if (brightness < led_brightness_max) {
			brightness |= 0x80;
		} else {
			brightness = led_brightness_max|0x80;
		}
		 reg_brightness = (brightness<<8) + brightness;
	}

	return reg_brightness;
}

static int set_reg_brightness(struct touchkey_data *tk_info)
{
	int ret;
	if (tk_info->reg_brightness == 0) {
		ret = i2c_synaptics_write(LED_ENABLE, 0x0000);
		if (ret < 0) {
			dev_err(&tk_info->client->dev, "failed to disable led1 and led2\n");
		}
	} else {
		ret = i2c_synaptics_write(LED_CONTROL1 , tk_info->reg_brightness);
		if (ret < 0) {
			dev_err(&tk_info->client->dev,
				"%s LED_CONTROL1 failed\n", __func__);
		}
		ret = i2c_synaptics_write(LED_CONTROL2 , tk_info->reg_brightness);
		if (ret < 0) {
			dev_err(&tk_info->client->dev,
				"%s LED_CONTROL2 failed\n", __func__);
		}
		ret = i2c_synaptics_write(LED_ENABLE, 0x000E);
		if (ret < 0) {
			dev_err(&tk_info->client->dev,
				"%s LED_ENABLE failed\n", __func__);
		}
	}

	return ret;
}

static int calc_reg_sensitivity(struct touchkey_data *tk_info, uint8_t tp_status)
{
	int ret = 0;
	switch(tp_status) {
	case TP_FINGER:
		tk_info->reg_sensitivity_10 = SENSITIVITY10_FINGER;
		tk_info->reg_sensitivity_32 = SENSITIVITY32_FINGER;
		break;
	case TP_GLOVE:
		tk_info->reg_sensitivity_10 = SENSITIVITY32_GLOVE;
		tk_info->reg_sensitivity_32 = SENSITIVITY32_GLOVE;
		break;
	case TP_WET:
		tk_info->reg_sensitivity_10 = SENSITIVITY10_WET;
		tk_info->reg_sensitivity_32 = SENSITIVITY32_WET;
		break;
	case TP_NULL:
	default:
		dev_err(&tk_info->client->dev, "%s tp_status = %d\n", __func__, tp_status);
		ret = -EFAULT;
		break;
	}

	return ret;
}

static int set_reg_sensitivity(struct touchkey_data *tk_info)
{
	int ret;
	ret = i2c_synaptics_write(SENSOR_PIN10_SENSITIVITY , tk_info->reg_sensitivity_10);
	if (ret < 0) {
		dev_err(&tk_info->client->dev,
			"%s SENSOR_PIN10_SENSITIVITY[0x%x] failed\n", __func__, tk_info->reg_sensitivity_10);
		return ret;
	}
	ret = i2c_synaptics_write(SENSOR_PIN32_SENSITIVITY , tk_info->reg_sensitivity_32);
	if (ret < 0) {
		dev_err(&tk_info->client->dev,
			"%s SENSOR_PIN32_SENSITIVITY[0x%x] failed\n", __func__, tk_info->reg_sensitivity_32);
		return ret;
	}

	return 0;
}

static void touchkey_led_set(struct led_classdev *led_cdev,
        enum led_brightness value)
{
	struct touchkey_data *tk_info = 
		container_of(led_cdev,  struct touchkey_data, synaptics_kp_bl_led);
	uint16_t brightness;

	tk_info->reg_brightness = calc_reg_brightness(tk_info->led_brightness_max, value);
#ifdef TOUCHKEY_DEBUG
	g_tk_debug_info.led_req++;
#endif
	touchkey_send_event(tk_info, TOUCHKEY_CMD_LED);
}

#ifdef TK_CMD_PENDING_FUNC
static void cmd_pending_process(struct touchkey_data *tk_info)
{
	u32 cur_time;
	touchkey_cmd_type i;
	tk_cmd_pending_t *pending_info = (tk_cmd_pending_t *)&(tk_info->tk_cmd_pending);

	for (i=0; i<TOUCHKEY_CMD_TYPE_MAX; i++) {
		if (true == pending_info->is_cmd_pending[i]) {
			cur_time = (u32)ktime_to_ms(ktime_get());
			if (cur_time < pending_info->cmd_pending_time[i] + TK_CMD_PENDING_TIME) {
				touchkey_send_event(tk_info, i);
			}
			pending_info->is_cmd_pending[i] = false;
		}
	}
}

static void cmd_pending_add(struct touchkey_data *tk_info, touchkey_cmd_type cmd_type)
{
	tk_cmd_pending_t *pending_info = (tk_cmd_pending_t *)&(tk_info->tk_cmd_pending);

	pending_info->cmd_pending_time[cmd_type] = (u32)ktime_to_ms(ktime_get());
	pending_info->is_cmd_pending[cmd_type] = true;

}

static void cmd_pending_init(struct touchkey_data *tk_info)
{
	touchkey_cmd_type i;
	tk_cmd_pending_t *pending_info = (tk_cmd_pending_t *)&(tk_info->tk_cmd_pending);
	for (i=0; i<TOUCHKEY_CMD_TYPE_MAX; i++) {
		pending_info->cmd_pending_time[i] = 0;
		pending_info->is_cmd_pending[i] = false;
	}
}
#endif

#if 0
static int enable_power_for_device(struct synatics_touchkey_platform *pdata, struct i2c_client *client, bool enable)
{
	int  error = 0;

        
       if(enable)
	{ 
	     pdata->vbus = regulator_get(&client->dev, TOUCHKEY_DEVICE_NAME);
             if (IS_ERR(pdata->vbus)) {
        	dev_err(&client->dev, "%s: failed to get touchkey vbus\n", __func__);
        	return -EINVAL;
            }
            error = regulator_set_voltage(pdata->vbus,2850000,2850000);
            if(error < 0){
            	dev_err(&client->dev, "%s: failed to set touchkey vbus\n", __func__);
            	return -EINVAL;
            }
            
            error = regulator_enable(pdata->vbus);
            if (error < 0) {
            	dev_err(&client->dev, "%s: failed to enable touchkey vbus\n", __func__);
            	return -EINVAL;
            }
            mdelay(100);
	} else {
	    if( NULL !=pdata->vbus) {
               error = regulator_set_voltage(pdata->vbus, 0, 2850000);
		 if(error < 0) {
            	     dev_err(&client->dev, "%s: failed to set touchkey vbus\n", __func__);
            	     return -EINVAL;
               }
               error = regulator_disable(pdata->vbus);
	        if(error < 0) {
            	     dev_err(&client->dev, "%s: failed to disable touchkey vbus\n", __func__);
            	     return -EINVAL;
               }
                regulator_put(pdata->vbus);
               pdata->vbus = NULL;
	    }
	}
	return error;
}
#endif

#ifdef TOUCHKEY_DEBUG
static ssize_t touchkey_debug_info_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	uint16_t ret =0;
	int i;
	ssize_t count=0;

	ret = snprintf(buf, PAGE_SIZE, "cmd req     : irq[%d], led[%d], sensitivity[%d]\n", 
		g_tk_debug_info.irq_req, g_tk_debug_info.led_req, g_tk_debug_info.sensitivity_req);
	buf += ret;
	count += ret;
	ret = snprintf(buf, PAGE_SIZE, "cmd finished: irq[%d], led[%d], sensitivity[%d]\n", 
		g_tk_debug_info.irq_finished, g_tk_debug_info.led_finished, g_tk_debug_info.sensitivity_finished); 
	buf += ret;
	count += ret;
	ret = snprintf(buf, PAGE_SIZE, "cmd_handler_cnt[%d]\n", 
			g_tk_debug_info.cmd_handler_cnt);
	count += ret;
	buf += ret;
	
	return count;
}

static ssize_t touchkey_debug_info_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	memset(&g_tk_debug_info, 0, sizeof(g_tk_debug_info));
	return count;
}

static DEVICE_ATTR(debug_info, 0664, touchkey_debug_info_show,
		   touchkey_debug_info_store);
#endif

/*add for check the details of registers begin */
static ssize_t touchkey_reg_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct touchkey_data *tk_info = dev_get_drvdata(dev);
	u16 i,ret;
	int val;
	ssize_t count=0;
	uint16_t data = 0;
	uint16_t array_address[18] = {INTERFACE_CONFIGUARATION, GENERAL_CONFIGURATION, BUTTON_ENABLE,
		GPIO_CONTROL, INTERFERENCE_THRESHOLD, SENSOR_PIN10_SENSITIVITY, SENSOR_PIN32_SENSITIVITY,
		BUTTON_TO_GPIO_MAPPING, TIMER_CONTROL, LED_ENABLE, LED_EFFECT_PERIOD, LED_CONTROL1,
		LED_CONTROL2, GPIO_STATE, BUTTON_STATE, TIMER_STATE, PRESSURE_VALUES10, PRESSURE_VALUES32};
	disable_irq(tk_info->client->irq);
	for(i=0; i < 18; i++)
	{
		val = i2c_synaptics_read(array_address[i], &data);
		 ret = snprintf(buf, PAGE_SIZE, "register_address:[0x%04x]; val:[0x%04x] \n", array_address[i],data);
		 buf += ret;
		 count += ret;
	}
	enable_irq(tk_info->client->irq);
	return count;
}

static ssize_t touchkey_reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{

	struct touchkey_data *tk_info = dev_get_drvdata(dev);
	int regs,value;
	int ret;
	if (2 != sscanf(buf, "0x%4x 0x%4x", &regs, &value)) {
		dev_err(&tk_info->client->dev, "failed to reg store\n");
		return -EINVAL;
	}
	disable_irq(tk_info->client->irq);
	ret = i2c_synaptics_write(regs, value);
	enable_irq(tk_info->client->irq);
	return count;
}

static DEVICE_ATTR(reg, 0664, touchkey_reg_show,
		   touchkey_reg_store);

static ssize_t touchkey_enable_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	uint16_t ret =0;
	struct touchkey_data *tk_info = dev_get_drvdata(dev);
	ssize_t count=0;
	int enable_tk_flag = 0;

	if (atomic_read(&tk_info->enable_tk_flag) == 0) {
		enable_tk_flag = 0;
	} else {
		enable_tk_flag = 1;
	}

	ret = snprintf(buf, PAGE_SIZE, "%d\n", enable_tk_flag);

	count += ret;
	buf += ret;
	
	return count;
}

static ssize_t touchkey_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct touchkey_data *tk_info = dev_get_drvdata(dev);
	int enable_tk_flag;

	if (!sscanf(buf, "%d", &enable_tk_flag)) {
		dev_err(&tk_info->client->dev, "failed to enable store\n");
		return -EINVAL;
	}
	if (0 == enable_tk_flag ) {
		atomic_set(&tk_info->enable_tk_flag, 0);
	}else {
		atomic_set(&tk_info->enable_tk_flag, 1);
	}
	

	return count;
}

static DEVICE_ATTR(enable, 0664, touchkey_enable_show,
		   touchkey_enable_store);


static struct attribute *synaptics_attribute[] = {
	&dev_attr_reg.attr,
#ifdef TOUCHKEY_DEBUG
	&dev_attr_debug_info.attr,
#endif
	&dev_attr_enable.attr,
	NULL
};
static const struct attribute_group synaptics_attr_group = {
    .attrs = synaptics_attribute,
};
/*add for check the details of registers end */

static int __devinit touchkey_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct touchkey_data *tk_info = NULL;
	struct synatics_touchkey_platform *pdata;
	int ret, i;

	if (get_touchkey_enable() == false ) {
		dev_info(&client->dev, "touchkey not exit!\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	pdata = client->dev.platform_data;
	dev_info(&client->dev, "%s in\n", __func__);
	client->dev.init_name = TOUCHKEY_IO;
#if 0
	ret = enable_power_for_device(pdata, client,TRUE);
	if(ret < 0) {
	    dev_err(&client->dev, "%s adapter not supported\n",
	    dev_driver_string(&client->adapter->dev));
	    ret = -ENODEV;
	    goto err_check_functionality_failed;
	}
#endif
	 atomic_set(&tk_is_pressed, 0);
	/* Check functionality */
	msleep(10);
	ret = i2c_check_functionality(client->adapter, I2C_FUNC_I2C);
	if (!ret) {
		dev_err(&client->dev, "%s adapter not supported\n",
		dev_driver_string(&client->adapter->dev));
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}
	/* Chip is valid and active. Allocate structure */
	tk_info = kzalloc(sizeof(struct touchkey_data), GFP_KERNEL);
	if (tk_info == NULL) {
		dev_err(&client->dev, "%s: allocate synaptics_data failed\n", __func__);
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}
	touchkey_info = tk_info;
#ifdef TOUCHKEY_DEBUG
	memset(&g_tk_debug_info, 0, sizeof(g_tk_debug_info));
#endif
	mutex_init(&tk_info->power_lock);
	touchkey_thread_cmd_init(tk_info);
	tk_info->synaptics_task = kthread_create(touchkey_thread_handler, 
							tk_info, "touchkey_thread");
	if (IS_ERR(tk_info->synaptics_task)) {
		dev_err(&client->dev, "create thread failed!\n");
		goto err_cread_thread_failed;
	}

	tk_info->client = client;
	i2c_set_clientdata(client, tk_info);
	
	tk_info->key_matrix =  0 ;
	tk_info->keycodes = pdata->get_key_map();
	tk_info->reg_brightness = 0;
	tk_info->keynum = TK_KEY_MAX;
	tk_info->led_brightness_max = pdata->get_led_brightness();
	if (tk_info->led_brightness_max < 0) {
		dev_err(&client->dev, "%s : get_touchkey_led_brightness failed\n", __func__);
		ret = -EIO;
		goto err_cread_thread_failed;
	}

	ret = touchkey_gpio_block_init(&client->dev, tk_info);
	if(ret<0) {
		dev_err(&client->dev, "%s: failed to config gpio mode\n", __func__);
		goto err_request_gpio_failed;
	}

	if (pdata->touchkey_gpio_config) {
		pdata->touchkey_gpio_config(TK_GPIO_PROBE);
		client->irq = gpio_to_irq(pdata->attn_gpio);
		if (client->irq < 0) {
			dev_err(&client->dev, "irq gpio reguest failed\n");
			ret = -ENODEV;
			goto err_request_gpio_failed;
		}
	} else {
		dev_err(&client->dev, "no irq config func\n");
		ret = -ENODEV;
		goto err_request_gpio_failed;
	}

	if (calc_reg_sensitivity(tk_info, TP_FINGER)) {
		dev_err(&client->dev, "%s : calc_reg_sensitivity failed\n", __func__);
		ret = -EIO;
		goto err_init_config_failed;
	}

	ret = synaptics_set_initconfig(tk_info);
	if (ret < 0) {
		dev_err(&client->dev, "%s: Failed to set register value!\n", __func__);
		ret = -EIO;
		goto err_init_config_failed;
	}
	
	tk_info->input = input_allocate_device();
	if (!tk_info->input) {
		dev_err(&client->dev, "insufficient memory\n");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}
	dev_set_drvdata(&tk_info->input->dev, tk_info);
	tk_info->input->name = TOUCHKEY_DEVICE_NAME;
	tk_info->input->id.bustype = BUS_I2C;
	tk_info->input->keycode = tk_info->keycodes;
	tk_info->input->keycodesize = sizeof(tk_info->keycodes[0]);
	tk_info->input->keycodemax = TK_KEY_MAX;
	__set_bit(EV_KEY, tk_info->input->evbit);
	__clear_bit(EV_REP, tk_info->input->evbit);
	for (i = 0; i < TK_KEY_MAX; i++) {
		input_set_capability(tk_info->input, EV_KEY,
							tk_info->keycodes[i]);
	}
	__clear_bit(KEY_RESERVED, tk_info->input->keybit);
	ret = input_register_device(tk_info->input);
	if (ret) {
		dev_err(&client->dev,"Failed to register input device\n");
		ret = -ENOMEM;
		goto err_input_register_device_failed;
	}
	
	ret = request_irq(client->irq, touchkey_irq_handler,
			IRQF_TRIGGER_LOW, TOUCHKEY_DEVICE_NAME, tk_info);
	if (ret) {
		dev_err(&client->dev,"failed to allocate irq %d\n", client->irq);
		ret = EIO;
		goto err_request_irq_failed;
	}

	/*add for led control begin*/
	 tk_info->synaptics_kp_bl_led.name = "touchkey-led-bright";
	 tk_info->synaptics_kp_bl_led.brightness_set = touchkey_led_set;
	 tk_info->synaptics_kp_bl_led.brightness = LED_OFF;
	 ret = led_classdev_register(&client->dev, &tk_info->synaptics_kp_bl_led);
	 if (ret) {
		dev_err(&client->dev, "unable to register led class driver\n");
		ret = -ENOMEM;
		goto err_intput_reg;
	 }
	/*add for led control end*/

	 ret = sysfs_create_group(&client->dev.kobj, &synaptics_attr_group);

#ifdef CONFIG_HAS_EARLYSUSPEND
	tk_info->early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING + 1;
	tk_info->early_suspend.suspend =touchkey_early_suspend;
	tk_info->early_suspend.resume = touchkey_later_resume;
	register_early_suspend(&tk_info->early_suspend);
#endif

	atomic_set(&tk_power_state, 1);
	atomic_set(&tk_info->enable_tk_flag, 1);
	dev_info(&client->dev, "touchkey is successfully probed");
	return 0;

err_intput_reg:
err_request_irq_failed:
	input_unregister_device(tk_info->input);
err_input_register_device_failed:
	input_free_device(tk_info->input);
err_init_config_failed:
err_input_dev_alloc_failed:
	if(pdata->touchkey_gpio_config) {
		pdata->touchkey_gpio_config(TK_GPIO_SUSPEND);
	}
err_request_gpio_failed:
	//if kthread_create failed
err_cread_thread_failed:
	touchkey_info = NULL;
	kfree(tk_info);
err_alloc_data_failed:
err_check_functionality_failed:
	//enable_power_for_device(pdata, client,FALSE);
	return ret;
}

static int __devexit touchkey_remove(struct i2c_client *client)
{
	struct touchkey_data *tk_info;
	struct synatics_touchkey_platform *pdata;
	pdata = client->dev.platform_data;
	tk_info = i2c_get_clientdata(client);
	unregister_early_suspend(&tk_info->early_suspend);
	/* Release IRQ so no queue will be scheduled */
	if (client->irq) {
		free_irq(client->irq, tk_info);
	}
	input_unregister_device(tk_info->input);
	//enable_power_for_device(pdata, client,FALSE);
	touchkey_info = NULL;
	kfree(tk_info);
	return 0;
}

static int touchkey_suspend(struct i2c_client *client,pm_message_t mesg)
{
	int ret;
	uint16_t reg_read = 0;
	struct synatics_touchkey_platform *pdata = client->dev.platform_data;
	struct touchkey_data *tk_info = i2c_get_clientdata(client);

	/*the tk's suspend would have been called in LCD driver*/
	if ((atomic_read(&tk_power_state) == 0)) {
		dev_err(&tk_info->client->dev, "%s ,tk_power_state==0\n", __func__);
		return 0;
	}

	mutex_lock(&tk_info->power_lock);
	atomic_set(&tk_power_state, 0);
	atomic_set(&tk_is_pressed, 0);
	disable_irq_nosync(client->irq);

	/*clean the possible irq which not handle*/
	ret = i2c_synaptics_read(GPIO_STATE, &reg_read);
	if (ret < 0) {
		dev_err(&tk_info->client->dev, 
			"%s:i2c_synaptics_read error:  register[0x%x]\n", __func__, GPIO_STATE);		
	}
	ret = i2c_synaptics_read(BUTTON_STATE, &reg_read);
	if (ret < 0) {
		dev_err(&tk_info->client->dev, 
			"%s:i2c_synaptics_read error: register[0x%x]\n", __func__, BUTTON_STATE);
		//synaptics_i2c_error_processing(tk_info);
	}

	ret = blockmux_set(tk_info->gpio_block, tk_info->gpio_block_config, LOWPOWER);
	if (ret < 0) {
		dev_err(&tk_info->client->dev, "%s: failed to config gpio\n", __func__);
	}
	if (pdata->touchkey_gpio_config) {
		pdata->touchkey_gpio_config(TK_GPIO_SUSPEND);
	}

	mutex_unlock(&tk_info->power_lock);
	dev_info(&client->dev, "%s is suspend!\n", __func__);
	return 0;
}

static int touchkey_resume(struct i2c_client *client)
{
	int ret;
	uint16_t reg_read = 0;
	struct synatics_touchkey_platform *pdata = client->dev.platform_data;
	struct touchkey_data *tk_info = i2c_get_clientdata(client);

	if ((atomic_read(&tk_power_state) == 1)) {
		dev_err(&tk_info->client->dev, "%s ,tk_power_state==1\n", __func__);
		return 0;
	}

	mutex_lock(&tk_info->power_lock);

	if (pdata->touchkey_gpio_config) {
		pdata->touchkey_gpio_config(TK_GPIO_RESUME);
	}    
	ret = blockmux_set(tk_info->gpio_block, tk_info->gpio_block_config, NORMAL);
	if (ret<0) {
		dev_err(&client->dev, "%s: failed to config gpio\n", __func__);
	}

	ret = synaptics_set_initconfig(tk_info);
	if (ret < 0) {
		dev_err(&client->dev, "%s: Failed to set register value!\n",__func__);
	}

	enable_irq(client->irq);
	atomic_set(&tk_power_state, 1);
	mutex_unlock(&tk_info->power_lock);
#ifdef TK_CMD_PENDING_FUNC
	cmd_pending_process(tk_info);
#endif

	dev_info(&client->dev, "synaptics_touchkey is resume!\n");
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void touchkey_early_suspend(struct early_suspend *h)
{
	struct touchkey_data *tk_info;
	tk_info = container_of(h, struct touchkey_data, early_suspend);
	touchkey_suspend(tk_info->client,PMSG_SUSPEND);
}

static void touchkey_later_resume(struct early_suspend *h)
{
	struct touchkey_data *tk_info;
	tk_info = container_of(h, struct touchkey_data, early_suspend);
	touchkey_resume(tk_info->client);
}
#endif

#if 0
//just for case that the phone didn't close the LED:insert charger then power off the phone
static void synaptics_shutdown(struct i2c_client *client)
{
    struct touchkey_data *tk_info = i2c_get_clientdata(client);
    struct synatics_touchkey_platform *pdata = client->dev.platform_data;
    int ret;
    ret = i2c_synaptics_write(LED_ENABLE, 0x0000);//led off
    if(ret)
        dev_err(&client->dev, "synaptics-tk is  shutdown");
    if(pdata->touchkey_gpio_config) {
        pdata->touchkey_gpio_config(TK_GPIO_SUSPEND);
    }
}
#endif

static const struct i2c_device_id touchkey_idtable[] = {
	{ TOUCHKEY_DEVICE_NAME, 0, },
	{ }
};

MODULE_DEVICE_TABLE(i2c, touchkey_idtable);

static struct i2c_driver touchkey_driver = {
	.driver = {
		.name	= TOUCHKEY_DEVICE_NAME,
		.owner  = THIS_MODULE,
	},
	.id_table	= touchkey_idtable,
	.probe		= touchkey_probe,
	.remove		= touchkey_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = touchkey_suspend,
	.resume =  touchkey_resume,
#endif
};

static int __init touchkey_init(void)
{
	return i2c_add_driver(&touchkey_driver);
}
module_init(touchkey_init);

static void __exit touchkey_cleanup(void)
{
	i2c_del_driver(&touchkey_driver);
}
module_exit(touchkey_cleanup);


MODULE_AUTHOR("Willow.Wang willow.wanglei@huawei.com ");
MODULE_DESCRIPTION("Driver for TOUCHKEY");
MODULE_LICENSE("GPL");

