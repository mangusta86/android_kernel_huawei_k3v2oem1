//include
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/mux.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/hrtimer.h>
#include <linux/irda_gpio_simulation.h>

//define
#define IRDA_GPIO_DEBUG
#define IRDA_BUFF_TYPE_INT
#define IRDA_HRTIME_OFFSET_VALUE 30
#define READ_START_TIMEOUT_LIMIT 20
#define READ_SINGLE_PULSE_TIMEOUT_LIMIT 1
#define IRDA_MAX_BUFF_NUM       1024
#define IRDA_DEFAULT_POWER_SUPPLY  2850000
#define IRDA_PWM_DEFAULT_DIV    0x2a
#define IRDA_PWM_DEFAULT_WIDE    0x0e

#define IRDA_ERR(format, arg...)  printk(KERN_ERR "[IRDA][%s]" format , __FUNCTION__ , ## arg)

#ifdef  IRDA_DEBUG_HIGH_LOW_TIME_LENGTH
static ktime_t debug_buff[IRDA_MAX_BUFF_NUM];
static int debug_index;
#endif

//typedef
#ifndef IRDA_BUFF_TYPE_INT
typedef unsigned short irda_buff_type;
#define irda_buff_type_config 0xFFFF
#else
typedef unsigned int irda_buff_type;
#define irda_buff_type_config 0xFFFFFFFF
#endif

//enum
enum irda_dev_state{
	IRDA_DEV_CLOSE = 0,
	IRDA_DEV_OPEN,
};

enum irda_state{
	IRDA_STATE_OFF = 0,
	IRDA_STATE_ON,
};

enum irda_pwm_state{
	PWM_FORCE_OFF = 0,
	PWM_FORCE_ON,
	PWM_AUTO_EXCHANGE,
};

enum irda_read_status{
	IRDA_READ_INIT = 0,//device probe update
	IRDA_READ_START, //cdev read update
	IRDA_READ_PROCESSING, //interrupt handler update
	IRDA_READ_STOP,//hrtimer callback update
	IRDA_READ_UNDEFINE,
};

struct irda_read_stat{
	enum irda_read_status rd_stat;
	spinlock_t rd_lock;
};

enum irda_work_mode {
	IRDA_CANCEL_STUDY = 0,
	IRDA_RX_MODE,
	IRDA_TX_MODE,
};

enum irda_ioctl_cmd {
	IRDA_SET_MODE = 0xFF000000,
	IRDA_SET_FREQUENCY,
	IRDA_SET_DUTY_PCTS,
	IRDA_SET_RECIEVE_BUFF_MAX_NUM,
};

enum irda_common_frequency{
	IRDA_FREQUENCY_30 = 0,
	IRDA_FREQUENCY_33,
	IRDA_FREQUENCY_36,
	IRDA_FREQUENCY_38,
	IRDA_FREQUENCY_40,
	IRDA_FREQUENCY_56,
};

enum irda_common_frequency_div{
	IRDA_FREQUENCY_30_DIV = 54,
	IRDA_FREQUENCY_33_DIV = 50,
	IRDA_FREQUENCY_36_DIV = 45,
	IRDA_FREQUENCY_38_DIV = 42,
	IRDA_FREQUENCY_40_DIV = 40,
	IRDA_FREQUENCY_56_DIV = 30,
};

enum irda_common_frequency_wide{
	IRDA_FREQUENCY_30_WIDE = 18,
	IRDA_FREQUENCY_33_WIDE = 17,
	IRDA_FREQUENCY_36_WIDE = 15,
	IRDA_FREQUENCY_38_WIDE = 14,
	IRDA_FREQUENCY_40_WIDE = 13,
	IRDA_FREQUENCY_56_WIDE = 10,
};
//struct
struct irda_pwm_config{
	int clk_div;
	int clk_wide;
};


struct irda_gpio_simulation_state{
	int dev_state;
	int power_state;
	int work_state;
	int clk_state;
	int irq_state;
};

struct irda_gpio_simulation_data{
	int clk_rate;
	int irda_irq;
	struct cdev irda_cdev;
	struct clk *irda_clk;
	struct completion irda_send;
	struct regulator *irda_power_supply;
	struct iomux_block *gpio_block;
	struct block_config *gpio_block_config;
	struct irda_gpio_simulation_state state;
	struct irda_pwm_config clk_config;
};

/*
* parameter  
*/
//buff
static irda_buff_type irda_tx_buff[IRDA_MAX_BUFF_NUM] = {0};
static irda_buff_type irda_rx_buff[IRDA_MAX_BUFF_NUM] = {0};

//tx&rx
static int irda_tx_index;
static int irda_tx_count;

static int irda_rx_max_num = IRDA_MAX_BUFF_NUM;
static int irda_rx_count = 0;
static ktime_t irda_delay[IRDA_MAX_BUFF_NUM];
static struct irda_read_stat rd_stats;
static struct hrtimer irda_hrtimer;
static struct hrtimer irda_read_hrtimer;
static struct irda_gpio_simulation_data *irdadrvdata;
//system
static dev_t dev_id_irda;
static struct device *irda_dev;
static struct class *irda_class;

//function
static inline unsigned int irda_gpio_reg_read(u32 reg)
{	
	return readl(reg);
}

static inline void irda_gpio_reg_write(unsigned int value, u32 reg)
{
	writel(value, reg);
	return;
}

static void irda_gpio_buff_print(irda_buff_type number[], int length)
{
	int index = 0;
	while(index < length && number[index]) {
		printk("%d\t", number[index]);
		index++;
	}
	printk("\n");
	return ;
}

static int irda_read_statu_update(enum irda_read_status st)
{
	unsigned long flag;
	int ret = 0;
	if (st < IRDA_READ_UNDEFINE) {
		spin_lock_irqsave(&rd_stats.rd_lock, flag);
		rd_stats.rd_stat = st;
		spin_unlock_irqrestore(&rd_stats.rd_lock,flag);
	} else {
		IRDA_ERR("read statu invaild parameter :%d\n", st);
		ret = -1;
	}
	return ret;
}

static inline void irda_read_statu_get(enum irda_read_status *st)
{

	unsigned long flag;
	spin_lock_irqsave(&rd_stats.rd_lock, flag);
	*st = rd_stats.rd_stat;
	spin_unlock_irqrestore(&rd_stats.rd_lock,flag);
	return;
}

static void irda_gpio_pwm_en(enum irda_pwm_state force_state)
{
	static int irda_tx_en_flag = PWM_FORCE_OFF;

	switch(force_state){
		case PWM_AUTO_EXCHANGE:
			irda_tx_en_flag = !irda_tx_en_flag;
			break;
		case PWM_FORCE_ON:
			irda_tx_en_flag = PWM_FORCE_ON;
			break;
		case PWM_FORCE_OFF:
			irda_tx_en_flag = PWM_FORCE_OFF;
			break;
		default:
			irda_tx_en_flag = PWM_FORCE_OFF;
			IRDA_ERR("force_state not vaild\n");
			break;			
	}

	if (irda_tx_en_flag) {
		//gpio_direction_output(IRDA_RX_IRQ, 1);
		irda_gpio_reg_write(PWM_FORCE_ON, PWM0_OUT_EN);
		//gpio_direction_output(IRDA_RX_IRQ, 0);
	} else {
		//gpio_direction_output(IRDA_RX_IRQ, 1);
		irda_gpio_reg_write(PWM_FORCE_OFF, PWM0_OUT_EN);
		//gpio_direction_output(IRDA_RX_IRQ, 0);
	}

	return ;
}

static int hrtime_ktime_set(ktime_t delay[], irda_buff_type buff[], int length)
{
	int index= 0;

	memset(delay, 0, length);
	for(; index<length && buff[index]; index++){
		if((index&0x01))
			buff[index] += IRDA_HRTIME_OFFSET_VALUE; //deal with hrtime offset value which cased by RTOS
		else if(buff[index] > IRDA_HRTIME_OFFSET_VALUE)
			buff[index] -= IRDA_HRTIME_OFFSET_VALUE;
		delay[index]= ktime_set(0, buff[index]* 1000);
	}

	return index;
}

static int irda_gpio_start_send(void)
{
	IRDA_ERR("start\n");
	preempt_disable();
	hrtimer_start(&irda_hrtimer,irda_delay[irda_tx_index++], HRTIMER_MODE_REL);
	preempt_enable();
	irda_gpio_pwm_en(PWM_FORCE_ON);
	return 0;
}

static int irda_gpio_start_read(void)
{
	IRDA_ERR("read start");
	irda_rx_count = 0;
	memset(irda_rx_buff, 0, sizeof(irda_rx_buff));
	irda_read_statu_update(IRDA_READ_START);
	enable_irq(irdadrvdata->irda_irq);

	return 0;
}

static int irda_gpio_irq_control(int flag)
{
	int ret = 0;
	if (flag) {
		enable_irq(irdadrvdata->irda_irq);
		irdadrvdata->state.irq_state= IRDA_STATE_ON;
	} else {
		disable_irq_nosync(irdadrvdata->irda_irq);
		irdadrvdata->state.irq_state = IRDA_STATE_OFF;
	}

	return ret;
}

static int irda_gpio_regulator_en(int flag)
{
	int ret = 0;
	if (flag) {
		ret = regulator_enable(irdadrvdata->irda_power_supply);
		if (ret) {
			IRDA_ERR("regu enable\n");
			regulator_put(irdadrvdata->irda_power_supply);
		}
		irdadrvdata->state.power_state = IRDA_STATE_ON;
	} else {
		regulator_disable(irdadrvdata->irda_power_supply);
		irdadrvdata->state.power_state = IRDA_STATE_OFF;
	}

	return ret;
}

static int irda_gpio_tx_clk_en(int flag)
{
	int ret = 0;
	if (flag){
		ret = clk_enable(irdadrvdata->irda_clk);
		if (ret) {
			IRDA_ERR("clk enable err\n");
			clk_put(irdadrvdata->irda_clk);
		}
		irdadrvdata->state.clk_state= IRDA_STATE_ON;
	} else {
		clk_disable(irdadrvdata->irda_clk);
		irdadrvdata->state.clk_state= IRDA_STATE_OFF;
	}

	return ret;
}

static ssize_t irda_txen_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 100, "[work_state:%d][irda_tx_index:%d][irda_tx_count:%d]\n", 
					irdadrvdata->state.work_state, irda_tx_index, irda_tx_count);
}

static ssize_t irda_txen_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	unsigned long state = 0;

	ret = strict_strtol(buf, 10, &state);
	if (ret < 0)
		return ret;

	if (state) {
		irda_gpio_buff_print(irda_tx_buff, sizeof(irda_tx_buff)/sizeof(irda_buff_type));
	}

	return size;
}
 

static ssize_t irda_rxen_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 50, "[work_state:%d][rx_count:%d]\n", irdadrvdata->state.work_state, irda_rx_count);
}
static ssize_t irda_rxen_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	unsigned long state = 0;
	
	ret = strict_strtol(buf, 10, &state);
	if (ret < 0)
		return ret;
	if (state) {
		irda_gpio_buff_print(irda_rx_buff, sizeof(irda_rx_buff)/sizeof(irda_buff_type));
	}

	return size;
}

static ssize_t irda_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 500, "[dev:%d]\n[power:%d]\n[clk:%d]\n[irq:%d]\n[rx read state:%d]\n[buff type:%d]\n", 
				irdadrvdata->state.dev_state, irdadrvdata->state.power_state, 
				irdadrvdata->state.clk_state, irdadrvdata->state.irq_state, 
				rd_stats.rd_stat, sizeof(irda_buff_type));
}

static ssize_t irda_state_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	unsigned long state = 0;
	
	ret = strict_strtol(buf, 10, &state);
	if (ret < 0)
		return ret;
	IRDA_ERR("[dev:%d]\n[power:%d]\n[clk:%d]\n[irq:%d]\n[rx read state:%d]\n[buff type:%d]\n", 
				irdadrvdata->state.dev_state, irdadrvdata->state.power_state, 
				irdadrvdata->state.clk_state, irdadrvdata->state.irq_state, 
				rd_stats.rd_stat, sizeof(irda_buff_type));

	return size;
}

static ssize_t irda_power_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 50, "power_state%d\n", irdadrvdata->state.power_state);
}

static ssize_t irda_power_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	unsigned long state = 0;
	
	ret = strict_strtol(buf, 10, &state);
	if (ret < 0)
		return ret;

	ret = irda_gpio_regulator_en(state);
	if (ret < 0)
		return ret;

	return size;
}

static ssize_t irda_irq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 50, "irq\n");
}
static ssize_t irda_irq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	unsigned long state = 0;
	
	ret = strict_strtol(buf, 10, &state);
	if (ret < 0)
		return ret;
	ret = irda_gpio_irq_control(state);
	if (ret < 0)
		return ret;
	return size;
}

static ssize_t irda_clk_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	irdadrvdata->clk_rate = clk_get_rate(irdadrvdata->irda_clk);
	return snprintf(buf, 50, "clk rat:%d,clk div:%d,clk wide%d\n", 
				irdadrvdata->clk_rate, irdadrvdata->clk_config.clk_div, irdadrvdata->clk_config.clk_wide);
}

static ssize_t irda_clk_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	unsigned long state = 0;
	
	ret = strict_strtol(buf, 10, &state);
	if (ret < 0)
		return ret;

	if (9*state >=  irdadrvdata->clk_config.clk_div && 
		2*state <=  irdadrvdata->clk_config.clk_div) {
		irda_gpio_reg_write(irdadrvdata->clk_config.clk_div/state, PWM0_OUT_WIDE);
	}
	if (state == 1) {
		irda_gpio_tx_clk_en(IRDA_STATE_ON);
		irda_gpio_reg_write(PWM_FORCE_ON, PWM0_OUT_EN);
	} else if (state == 2) {
		irda_gpio_reg_write(PWM_FORCE_OFF, PWM0_OUT_EN);
		irda_gpio_tx_clk_en(IRDA_STATE_OFF);
	}
	return size;
}

DEVICE_ATTR(rxen,0644,irda_rxen_show,irda_rxen_store);
DEVICE_ATTR(txen,0644,irda_txen_show,irda_txen_store);
DEVICE_ATTR(state,0644,irda_state_show,irda_state_store);
DEVICE_ATTR(power_state,0644,irda_power_show,irda_power_store);
DEVICE_ATTR(irq,0644,irda_irq_show,irda_irq_store);
DEVICE_ATTR(clk,0644,irda_clk_show,irda_clk_store);

static struct attribute *irda_attributes[] =
{
	&dev_attr_txen.attr,
	&dev_attr_rxen.attr,
	&dev_attr_power_state.attr,
	&dev_attr_irq.attr,
	&dev_attr_clk.attr,
	&dev_attr_state.attr,
	NULL,
};

static const struct attribute_group irda_attr_group = {
    .attrs = irda_attributes,
};

static int irda_create_file(void)
{	
	return sysfs_create_group(&irda_dev->kobj, &irda_attr_group);
}

static void irda_remove_file(struct irda_gpio_simulation_data *pdata)
{
	sysfs_remove_group(&irda_dev->kobj, &irda_attr_group);
}

static int irda_classdev_register(void)
{
	irda_class = class_create(THIS_MODULE, "irda");
	if (IS_ERR(irda_class))
		return PTR_ERR(irda_class);
	irda_dev = device_create(irda_class, NULL, dev_id_irda, NULL, "irda_device");
	if (IS_ERR(irda_dev))
		return PTR_ERR(irda_dev);

	return 0;
}

static void irda_classdev_unregister(void)
{
	kfree(irda_dev);
	kfree(irda_class);

	return;
}

static inline int irda_fill_read_buffer(int is_first_isr, struct timeval now) // 0 - buffer not full   1- buffer is full
{
	int ret = TRUE;
	long rd_threshold;
	static struct timeval prev;
	
	if(is_first_isr)
		goto end;
		
	rd_threshold =(now.tv_sec - prev.tv_sec)*1000000 + (now.tv_usec - prev.tv_usec);	
	irda_rx_buff[irda_rx_count++] = rd_threshold&irda_buff_type_config;
	if(irda_rx_count == irda_rx_max_num)
		ret = FALSE;
	
end:
	prev.tv_sec = now.tv_sec;
	prev.tv_usec = now.tv_usec;
	return ret;
}

irqreturn_t irda_gpio_handle_interrupts(int irq, void *dev_id)
{
	unsigned long flag;
	struct timeval now;
	do_gettimeofday(&now);
	spin_lock_irqsave(&rd_stats.rd_lock, flag);
	switch(rd_stats.rd_stat){
		case IRDA_READ_START:
			if(__gpio_get_value(IRDA_RX_IRQ)){
				IRDA_ERR("start from H-level, ingore\n");
				spin_unlock_irqrestore(&rd_stats.rd_lock,flag);
				break;
			}
			rd_stats.rd_stat = IRDA_READ_PROCESSING;
			spin_unlock_irqrestore(&rd_stats.rd_lock,flag);
			irda_fill_read_buffer(TRUE , now);
			hrtimer_start(&irda_read_hrtimer, ktime_set(READ_SINGLE_PULSE_TIMEOUT_LIMIT,0),HRTIMER_MODE_REL);
			break;
		case IRDA_READ_PROCESSING:
			if(!irda_fill_read_buffer(FALSE, now)){
				rd_stats.rd_stat = IRDA_READ_STOP;
				disable_irq_nosync(irdadrvdata->irda_irq);
			}
			spin_unlock_irqrestore(&rd_stats.rd_lock,flag);
			break;
		case IRDA_READ_STOP:
			spin_unlock_irqrestore(&rd_stats.rd_lock,flag);
			break;
		default:
			IRDA_ERR("unknown reason : %d interrupt occur\n",rd_stats.rd_stat);
			spin_unlock_irqrestore(&rd_stats.rd_lock,flag);
			break;
	}
	return IRQ_HANDLED;
}

static ssize_t irda_read(struct file *pfile, char *buff, size_t count, loff_t *poff)
{
	enum irda_read_status st;
	int buff_count = 0;

	IRDA_ERR("reading,count:%d\n", count);
	irda_read_statu_get(&st);
	if(st == IRDA_READ_STOP){
		buff_count = irda_rx_count*sizeof(irda_buff_type);
		buff_count = count < buff_count ? count : buff_count;
		if(copy_to_user(buff, irda_rx_buff, buff_count)) {
			buff_count = -EFAULT;
		}
	}	
	return buff_count;
}

static enum hrtimer_restart irda_send_callback(struct hrtimer *timer)
{
	enum hrtimer_restart ret = HRTIMER_NORESTART;

	if(irda_tx_index<irda_tx_count){
		irda_gpio_pwm_en(PWM_AUTO_EXCHANGE);
		hrtimer_forward_now(&irda_hrtimer, irda_delay[irda_tx_index++]);
		ret =  HRTIMER_RESTART;
	} else {
		irda_gpio_pwm_en(PWM_FORCE_OFF);
		complete(&irdadrvdata->irda_send);
		IRDA_ERR("stop\n");
	}
	return ret;
}

static enum hrtimer_restart irda_read_timeout_callback(struct hrtimer *timer)
{
	unsigned long flag;

	spin_lock_irqsave(&rd_stats.rd_lock, flag);
	switch(rd_stats.rd_stat){
		case IRDA_READ_PROCESSING:
			rd_stats.rd_stat = IRDA_READ_STOP;
			disable_irq_nosync(irdadrvdata->irda_irq);
			break;
		default:
			IRDA_ERR("irda_read_timeout_callback reason %d\n",rd_stats.rd_stat);
			break;
	}
	spin_unlock_irqrestore(&rd_stats.rd_lock,flag);	
	return HRTIMER_NORESTART;
}


static ssize_t irda_write(struct file *pfile, const char *buff, size_t count, loff_t *poff)
{
	int length = 0;
	int ret = 0;

	irda_tx_index = 0;
	length = sizeof(irda_tx_buff);
	memset(irda_tx_buff, 0, length);
	if (length > count)
		length = count;
	if (copy_from_user(irda_tx_buff, buff, length)) {
		ret = -ENOMEM;
		goto out;
	}
	
	irda_tx_count = length/sizeof(irda_buff_type);

	hrtime_ktime_set(irda_delay, irda_tx_buff, irda_tx_count);
	
	irda_gpio_start_send();
	if(!wait_for_completion_timeout(&irdadrvdata->irda_send, HZ*5)){
		IRDA_ERR("send time out:%d\n", irda_tx_index);
		hrtimer_cancel(&irda_hrtimer);
		ret = 0;
		goto out;
	}
	ret = count;
out:
	return ret;
}
static int irda_open(struct inode *pinode, struct file *pfile)
{
	int ret;

	if (IRDA_DEV_OPEN == irdadrvdata->state.dev_state) {
		IRDA_ERR("dev already open");
		return -1;
	}
	ret = blockmux_set(irdadrvdata->gpio_block, irdadrvdata->gpio_block_config, NORMAL);
	if (ret) {
		IRDA_ERR("gpio block set err");
		return -EINVAL;
	}

	if (0 == irdadrvdata->state.power_state) {
		ret = irda_gpio_regulator_en(IRDA_STATE_ON);
		if (ret) 
			return ret;
		irdadrvdata->state.power_state = IRDA_STATE_ON;
	}

	 irdadrvdata->state.dev_state = IRDA_DEV_OPEN;

	IRDA_ERR("dev open success\n");
	return 0;
}

static int irda_close(struct inode *pinode, struct file *pfile)
{
	int ret;

	if (IRDA_DEV_CLOSE == irdadrvdata->state.dev_state) {
		IRDA_ERR("dev already close !");
		return -1;
	}

	if (IRDA_STATE_ON == irdadrvdata->state.power_state) {
		ret = irda_gpio_regulator_en(IRDA_STATE_OFF);
		if (ret)
			return ret;
		irdadrvdata->state.power_state = IRDA_STATE_OFF;
	}

	if (IRDA_STATE_ON == irdadrvdata->state.clk_state) {
		ret = irda_gpio_tx_clk_en(IRDA_STATE_OFF);
		if (ret)
			return ret;
		irdadrvdata->state.clk_state = IRDA_STATE_OFF;
	}

	ret = blockmux_set(irdadrvdata->gpio_block, irdadrvdata->gpio_block_config, LOWPOWER);
	if (ret) {
		IRDA_ERR(" gpio block set error\n");
		return -EINVAL;
	}

	 irdadrvdata->state.dev_state = IRDA_DEV_CLOSE;

	IRDA_ERR("close successed\n");

	return 0;
}

static void irda_gpio_cancel_read(void)
{
	unsigned long flag;
	IRDA_ERR("running test ##");
	spin_lock_irqsave(&rd_stats.rd_lock, flag);
	switch(rd_stats.rd_stat){
		case IRDA_READ_START :
			rd_stats.rd_stat = IRDA_READ_STOP;
			disable_irq_nosync(irdadrvdata->irda_irq);
			break;
		case IRDA_READ_PROCESSING:
			rd_stats.rd_stat = IRDA_READ_STOP;
			disable_irq_nosync(irdadrvdata->irda_irq);
			break;
		case IRDA_READ_STOP :
			break;
		default:
			IRDA_ERR("irda_gpio_cancel_read reason %d\n",rd_stats.rd_stat);
			break;
	}
	spin_unlock_irqrestore(&rd_stats.rd_lock,flag);	
	return;
}

static int irda_gpio_set_mode(enum irda_work_mode mode)
{
	int ret = 0;

	switch(mode){
		case IRDA_TX_MODE :
			if (IRDA_STATE_OFF == irdadrvdata->state.clk_state) {
				ret = irda_gpio_tx_clk_en(IRDA_STATE_ON);
				if (ret)
					break;
				//PWM_OUT1_DIV
				irda_gpio_reg_write(irdadrvdata->clk_config.clk_div, PWM0_OUT_DIV);
				//PWM_OUT1_WIDE
				irda_gpio_reg_write(irdadrvdata->clk_config.clk_wide, PWM0_OUT_WIDE);
			}
			break;
		case IRDA_RX_MODE :
			if (irdadrvdata->state.clk_state== IRDA_STATE_ON) {
				irda_gpio_tx_clk_en(IRDA_STATE_OFF);
			}
			irda_gpio_start_read();
			break;
		case IRDA_CANCEL_STUDY :
			irda_gpio_cancel_read();
			irdadrvdata->state.work_state = IRDA_CANCEL_STUDY;
			break;
		default : 
			break;
	};
	irdadrvdata->state.work_state = mode;
	return ret;
}

static int irda_gpio_set_frequency(enum irda_common_frequency frequency)
{
	if (irdadrvdata->state.work_state == IRDA_TX_MODE){
		switch (frequency){
			case IRDA_FREQUENCY_30 : 
				irda_gpio_reg_write(IRDA_FREQUENCY_30_DIV, PWM0_OUT_DIV);
				irda_gpio_reg_write(IRDA_FREQUENCY_30_WIDE, PWM0_OUT_WIDE);
				irdadrvdata->clk_config.clk_div = IRDA_FREQUENCY_30_DIV;
				irdadrvdata->clk_config.clk_wide = IRDA_FREQUENCY_30_WIDE;
				break;
			case IRDA_FREQUENCY_33 : 
				irda_gpio_reg_write(IRDA_FREQUENCY_33_DIV, PWM0_OUT_DIV);
				irda_gpio_reg_write(IRDA_FREQUENCY_33_DIV, PWM0_OUT_WIDE);
				irdadrvdata->clk_config.clk_div = IRDA_FREQUENCY_33_DIV;
				irdadrvdata->clk_config.clk_wide = IRDA_FREQUENCY_33_WIDE;
				break;
			case IRDA_FREQUENCY_36 : 
				irda_gpio_reg_write(IRDA_FREQUENCY_36_DIV, PWM0_OUT_DIV);
				irda_gpio_reg_write(IRDA_FREQUENCY_36_DIV, PWM0_OUT_WIDE);
				irdadrvdata->clk_config.clk_div = IRDA_FREQUENCY_36_DIV;
				irdadrvdata->clk_config.clk_wide = IRDA_FREQUENCY_36_WIDE;
				break;
			case IRDA_FREQUENCY_38 : 
				irda_gpio_reg_write(IRDA_FREQUENCY_38_DIV, PWM0_OUT_DIV);
				irda_gpio_reg_write(IRDA_FREQUENCY_38_DIV, PWM0_OUT_WIDE);
				irdadrvdata->clk_config.clk_div = IRDA_FREQUENCY_38_DIV;
				irdadrvdata->clk_config.clk_wide = IRDA_FREQUENCY_38_WIDE;
				break;
			case IRDA_FREQUENCY_40 : 
				irda_gpio_reg_write(IRDA_FREQUENCY_40_DIV, PWM0_OUT_DIV);
				irda_gpio_reg_write(IRDA_FREQUENCY_40_DIV, PWM0_OUT_WIDE);
				irdadrvdata->clk_config.clk_div = IRDA_FREQUENCY_40_DIV;
				irdadrvdata->clk_config.clk_wide = IRDA_FREQUENCY_40_WIDE;
				break;
			case IRDA_FREQUENCY_56 : 
				irda_gpio_reg_write(IRDA_FREQUENCY_56_DIV, PWM0_OUT_DIV);
				irda_gpio_reg_write(IRDA_FREQUENCY_56_DIV, PWM0_OUT_WIDE);
				irdadrvdata->clk_config.clk_div = IRDA_FREQUENCY_56_DIV;
				irdadrvdata->clk_config.clk_wide = IRDA_FREQUENCY_56_WIDE;
				break;
			default : 
				break;

		};
	}
	return 0;
}

static int irda_gpio_set_duty_pcts(int wide)
{
	int clk_div;
	if (irdadrvdata->state.work_state == IRDA_TX_MODE){
		clk_div = irdadrvdata->clk_config.clk_div;
		if (wide > clk_div/9 && wide < clk_div/2 ) {
			irdadrvdata->clk_config.clk_wide = clk_div/wide+1;
		}
		irda_gpio_reg_write(irdadrvdata->clk_config.clk_wide, PWM0_OUT_WIDE);
	}
	return 0;
}

static long irda_ioctl(struct file *filp, unsigned int cmd,unsigned long arg)
{
	unsigned long value = arg;
	IRDA_ERR("value:%lu\n", value);
	switch(cmd){
		case IRDA_SET_MODE : 
			irda_gpio_set_mode(value);
			break;
		case IRDA_SET_FREQUENCY : 
			irda_gpio_set_frequency(value);
			break;
		case IRDA_SET_DUTY_PCTS : 
			irda_gpio_set_duty_pcts(value);
			break;
		case IRDA_SET_RECIEVE_BUFF_MAX_NUM : 
			if (value < IRDA_MAX_BUFF_NUM)
				irda_rx_max_num = value;
			break;
		default : break;
	};

	return 0;
}
 static struct file_operations irda_fops = {
	.owner = THIS_MODULE,
	.read  = irda_read,
	.write =irda_write,
	.open  = irda_open,
	.release  = irda_close,
	.unlocked_ioctl = irda_ioctl,
 };

static int irda_gpio_hw_init(void)
{
	int ret = 0;

	irdadrvdata->gpio_block = iomux_get_block(IRDA_BLOCK_NAME);
	if (!irdadrvdata->gpio_block) {
		IRDA_ERR("get gpio block err\n");
		return  -EINVAL;
	}
	irdadrvdata->gpio_block_config= iomux_get_blockconfig(IRDA_BLOCK_NAME);
	if (!irdadrvdata->gpio_block_config) {
		IRDA_ERR("get gpio block config err\n");
		ret = -EINVAL;
		goto err_block_config;
	}
	ret = blockmux_set(irdadrvdata->gpio_block, irdadrvdata->gpio_block_config, LOWPOWER);
	if (ret) {
		IRDA_ERR("set gpio block  err\n");
		ret = -EINVAL;
		goto err_mux_set;
	}

	ret = gpio_request(IRDA_RX_IRQ, "irda_irq");
	if (ret) {
		IRDA_ERR("IRDA gpio request IRDA_RESET fail.\n");
		goto err_mux_set;
	}
	ret = gpio_direction_input(IRDA_RX_IRQ);
	irdadrvdata->irda_irq = gpio_to_irq(IRDA_RX_IRQ);

	irda_read_statu_update(IRDA_READ_INIT);
	spin_lock_init(&rd_stats.rd_lock);
	ret = request_irq(irdadrvdata->irda_irq, irda_gpio_handle_interrupts, IRQ_TYPE_EDGE_BOTH, "irda_handler", NULL);
	if (ret) {
		IRDA_ERR("request irq err\n");
		goto err_mux_set;
	}
	disable_irq_nosync(irdadrvdata->irda_irq);
	irdadrvdata->state.irq_state = IRDA_STATE_OFF;
	irdadrvdata->irda_power_supply = regulator_get(NULL, "irda-gpio-simulation");
	if (IS_ERR(irdadrvdata->irda_power_supply)) { 
    		IRDA_ERR("regulater get err\n");
		goto err_mux_set;
	}

	ret = regulator_set_voltage(irdadrvdata->irda_power_supply, IRDA_DEFAULT_POWER_SUPPLY, IRDA_DEFAULT_POWER_SUPPLY);
	if (ret < 0) {
		IRDA_ERR("regulater set err\n");
		goto err_regulater;
	}

	irdadrvdata->irda_clk = clk_get(NULL,"clk_pwm0");
	if (IS_ERR(irdadrvdata->irda_clk)) {
		IRDA_ERR("clk get err\n");
		ret = PTR_ERR(irdadrvdata->irda_clk);
		goto err_regulater;
	}

	irda_gpio_tx_clk_en(IRDA_STATE_ON);
	clk_set_rate(irdadrvdata->irda_clk, PWM_CLK_SOURCE_RATE);
	irdadrvdata->clk_rate = clk_get_rate(irdadrvdata->irda_clk);

	//PWM_OUT1_DIV
	irda_gpio_reg_write(irdadrvdata->clk_config.clk_div, PWM0_OUT_DIV);
	//PWM_OUT1_WIDE
	irda_gpio_reg_write(irdadrvdata->clk_config.clk_wide, PWM0_OUT_WIDE);
	irda_gpio_tx_clk_en(IRDA_STATE_OFF);

	return 0;

err_regulater:
	regulator_put(irdadrvdata->irda_power_supply);
err_block_config:
	kfree(irdadrvdata->gpio_block);	
err_mux_set:
	kfree(irdadrvdata->gpio_block_config);

	return ret;
}

static int irda_cdev_init(void)
{
	int ret = 0;
	ret = alloc_chrdev_region(&dev_id_irda, 0, 1, "irda_gpio");
	if (ret < 0) {
		IRDA_ERR("cdev alloc err\n");
		return ret;
	}
	cdev_init(&irdadrvdata->irda_cdev, &irda_fops);
	irdadrvdata->irda_cdev.owner = THIS_MODULE;
	ret = cdev_add(&irdadrvdata->irda_cdev, dev_id_irda, 1);
	if (ret) {
		IRDA_ERR("cdev add err\n");
		goto unregister;
	}
	ret = irda_classdev_register();
	if (ret) {
		IRDA_ERR("irda register class err\n");
		goto irda_cdev_del;
	}
	ret = irda_create_file();
	if (ret) {
		IRDA_ERR("irda create file err\n");
		goto irda_class_unregister;
	}

	return 0;

irda_class_unregister:
	irda_classdev_unregister();
irda_cdev_del:
	cdev_del(&irdadrvdata->irda_cdev);
unregister:
	unregister_chrdev_region(dev_id_irda, 1);

	return ret;
}

static int irda_gpio_probe(struct platform_device *pdev)
{
	int ret = 0;

	irdadrvdata = kzalloc(sizeof(struct irda_gpio_simulation_data), GFP_KERNEL);
	if (NULL == irdadrvdata) {
		IRDA_ERR("alloc driver data error\n");
		return -ENOMEM;
	}
	irdadrvdata->clk_config.clk_div = IRDA_PWM_DEFAULT_DIV;
	irdadrvdata->clk_config.clk_wide = IRDA_PWM_DEFAULT_WIDE;
	
	irdadrvdata->state.clk_state = 0;
	irdadrvdata->state.dev_state = 0;
	irdadrvdata->state.irq_state = 0;
	irdadrvdata->state.power_state = 0;
	irdadrvdata->state.work_state = 0;

	ret = irda_gpio_hw_init();
	if (ret){
		IRDA_ERR("hardware init fail:%d\n", ret);
		goto err_free_drv_data;
	}

	ret = irda_cdev_init();
	if (ret) {
		IRDA_ERR("char device init fail:%d\n", ret);
		goto err_free_drv_data;
	}
	irda_read_statu_update(IRDA_READ_INIT);
	hrtimer_init(&irda_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	irda_hrtimer.function = irda_send_callback;
	hrtimer_init(&irda_read_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	irda_read_hrtimer.function = irda_read_timeout_callback;
	init_completion(&irdadrvdata->irda_send);

	IRDA_ERR("irda platform probe success\n");
	return 0;
	
err_free_drv_data:
	kfree(irdadrvdata);
	
	return ret;
}
static int irda_gpio_remove(struct platform_device *pdev)
{
	irda_remove_file(irdadrvdata);
	irda_classdev_unregister();
	class_destroy(irda_class);
	kfree(irdadrvdata);

	return 0;
}

static int irda_gpio_suspend(struct platform_device *dev, pm_message_t state)
{
    int ret = -1;

	if (IRDA_STATE_ON == irdadrvdata->state.clk_state) {
		if(irda_gpio_tx_clk_en(IRDA_STATE_OFF))
			return ret;
	}
	if (IRDA_STATE_ON == irdadrvdata->state.power_state) {
		
		if(irda_gpio_regulator_en(IRDA_STATE_OFF))
			return ret;
	}

	ret = blockmux_set(irdadrvdata->gpio_block, irdadrvdata->gpio_block_config, LOWPOWER);
	if (ret < 0) {
		IRDA_ERR( "%s: failed to config gpio\n", __FUNCTION__);
	}

    return 0;
}

static int irda_gpio_resume(struct platform_device *dev)
{
	int ret = -1;

	if(IRDA_DEV_OPEN == irdadrvdata->state.dev_state){
		ret = blockmux_set(irdadrvdata->gpio_block, irdadrvdata->gpio_block_config, NORMAL);
		if (ret) {
			IRDA_ERR("gpio block set err");
			return -EINVAL;
		}
		ret = irda_gpio_regulator_en(IRDA_STATE_ON);
		if (ret) 
			return ret;

		ret = irda_gpio_tx_clk_en(IRDA_STATE_ON);
		if (ret)
			return ret;
		//PWM_OUT1_DIV
		irda_gpio_reg_write(irdadrvdata->clk_config.clk_div, PWM0_OUT_DIV);
		//PWM_OUT1_WIDE
		irda_gpio_reg_write(irdadrvdata->clk_config.clk_wide, PWM0_OUT_WIDE);
	}

	return 0;
}
 static struct platform_device irda_plat_device = {
	.name = "irda_platform",	
};

 static struct platform_driver irda_gpio_driver = {
	.probe     = irda_gpio_probe,
	.remove    = irda_gpio_remove,
	.suspend  = irda_gpio_suspend,
	.resume   = irda_gpio_resume,
	.driver    = {
		.name  = "irda_platform",
		.owner = THIS_MODULE,
	},
};
static int __init irda_gpio_module_init(void)
{
	if(platform_device_register(&irda_plat_device))
		IRDA_ERR("platform dev register err\n");
	return platform_driver_register(&irda_gpio_driver);
}

static void __exit irda_gpio_module_exit(void)
{
	platform_device_unregister(&irda_plat_device);
	platform_driver_unregister(&irda_gpio_driver);
}

module_init(irda_gpio_module_init);
module_exit(irda_gpio_module_exit);

MODULE_AUTHOR("Huawei gpio");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("irda driver");
