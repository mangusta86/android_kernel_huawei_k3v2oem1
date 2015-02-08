#include <mach/gpio.h>
#include <mach/platform.h>


//#define CONFIG_HUAWEI_IRDA_GPIO_SIMULATION
#ifndef TRUE 
#define TRUE    1
#endif
#ifndef FALSE
#define FALSE   0
#endif
#define IRDA_BLOCK_NAME            "block_irda"
#define IRDA_REGULATER_NAME    "irda_gpio_simulation"
#define IRDA_CLK_NAME                 "clk_pwm0"
#define IRDA_RX_IRQ                      GPIO_14_6
#define IRDA_PWM_OUT                   GPIO_18_5
#define PWM_CLK_SOURCE_RATE  1625000
#define PWM_CLK_DEFAULT_DIV    0x2A
#define PWM_CLK_DEFAULT_WIDE 0x05

#define PWM0_OUT                       (REG_BASE_IOC_VIRT+0x154)
#define PWM0_OUT_CONFIG          (REG_BASE_IOC_VIRT+0xA80)
#define PWM0_OUT_EN                  REG_BASE_PWM0_VIRT
#define PWM0_OUT_DIV                (REG_BASE_PWM0_VIRT+0x008)
#define PWM0_OUT_WIDE             (REG_BASE_PWM0_VIRT+0x010)
#define PWM0_OUT_WARN            (REG_BASE_PWM0_VIRT+0x018)


struct irda_platform_data {
	const char *gpio_block_name;
	const char *regulater_name;
	const char *clk_name;
	unsigned int default_clk_rate;
	unsigned int irq_gpio;
	unsigned int pwm_mux_gpio;
};
