/*
 *  linux/arch/arm/mach-k3v2/cpu-k3v2.c
 *
 *  CPU frequency scaling for K3V2
 *
 *  Copyright (C) 2005 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <asm/system.h>
#include <linux/pm_qos_params.h>
#include <linux/ipps.h>
#include <linux/cpufreq-k3v2.h>
#include <mach/boardid.h>
#include <linux/switch.h>

struct switch_dev policy_switch;

static struct ipps_device *gpipps_device;
static struct ipps_client ipps_client;

/*cpufreq table must be sorted by asc*/
struct cpufreq_frequency_table *cpufreq_table;

/*gpufreq table must be sorted by asc*/
struct cpufreq_frequency_table *gpufreq_table;

/*ddrfreq table must be sorted by asc*/
struct cpufreq_frequency_table *ddrfreq_table;

static struct pm_qos_request_list g_ippspolicy;

struct ipps_param gipps_param;

#ifdef CONFIG_DEBUG_FS
extern struct ipps_param gdbgipps_param;
#endif

/********************Qos notifier begin*********************/
static int cpumaxprofile_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	PARAM_MAX(cpu) = l;

	pr_debug("[%s] in=%lu, profile=%d\n", __func__, l, PARAM_MAX(cpu));

#ifdef CONFIG_DEBUG_FS
	if (gdbgipps_param.cpu.max_freq != 0)
		ipps_set_parameter(&ipps_client, IPPS_OBJ_CPU, &gdbgipps_param);
	else
		ipps_set_parameter(&ipps_client, IPPS_OBJ_CPU, &gipps_param);
#else
	ipps_set_parameter(&ipps_client, IPPS_OBJ_CPU, &gipps_param);
#endif

	return 0;
}

static struct notifier_block cpumaxprofile_notifier = {
	.notifier_call = cpumaxprofile_notify,
};

static int cpuminprofile_notify(struct notifier_block *b, unsigned long l,
	void *v)
{
	PARAM_MIN(cpu) = l;

	pr_debug("[%s] cpu_min=%d\n", __func__, PARAM_MIN(cpu));

#ifdef CONFIG_DEBUG_FS
	if (gdbgipps_param.cpu.min_freq != 0)
		ipps_set_parameter(&ipps_client, IPPS_OBJ_CPU, &gdbgipps_param);
	else
		ipps_set_parameter(&ipps_client, IPPS_OBJ_CPU, &gipps_param);
#else
	ipps_set_parameter(&ipps_client, IPPS_OBJ_CPU, &gipps_param);
#endif

	return 0;
}

static struct notifier_block cpuminprofile_notifier = {
	.notifier_call = cpuminprofile_notify,
};

static int cpublockprofile_notify(struct notifier_block *b, unsigned long l,
	void *v)
{
	PARAM_BLOCK(cpu) = l;

	pr_debug("[%s] cpu_block=%d.\n", __func__, PARAM_BLOCK(cpu));

#ifdef CONFIG_DEBUG_FS
	if (gdbgipps_param.cpu.max_freq != 0 || gdbgipps_param.cpu.min_freq != 0)
		ipps_set_parameter(&ipps_client, IPPS_OBJ_CPU, &gdbgipps_param);
	else
		ipps_set_parameter(&ipps_client, IPPS_OBJ_CPU, &gipps_param);
#else
	ipps_set_parameter(&ipps_client, IPPS_OBJ_CPU, &gipps_param);
#endif

	return 0;
}

static struct notifier_block cpublockprofile_notifier = {
	.notifier_call = cpublockprofile_notify,
};

static int cpusafeprofile_notify(struct notifier_block *b, unsigned long l,
	void *v)
{
	pr_debug("[%s] in=%lu\n", __func__, l);

	PARAM_SAFE(cpu) = l;

	pr_debug("[%s] cpu_safe=%d.\n", __func__, PARAM_SAFE(cpu));

#ifdef CONFIG_DEBUG_FS
	if (gdbgipps_param.cpu.safe_freq != 0)
		ipps_set_parameter(&ipps_client, IPPS_OBJ_CPU, &gdbgipps_param);
	else
		ipps_set_parameter(&ipps_client, IPPS_OBJ_CPU, &gipps_param);
#else
	ipps_set_parameter(&ipps_client, IPPS_OBJ_CPU, &gipps_param);
#endif


	return 0;
}

static struct notifier_block cpusafeprofile_notifier = {
	.notifier_call = cpusafeprofile_notify,
};

static int gpumaxprofile_notify(struct notifier_block *b, unsigned long l,
	void *v)
{
	pr_debug("[%s] in=%lu\n", __func__, l);

	PARAM_MAX(gpu) = l;

	pr_debug("[%s] gpu_max=%d\n", __func__, PARAM_MAX(gpu));

#ifdef CONFIG_DEBUG_FS
	if (gdbgipps_param.gpu.max_freq != 0)
		ipps_set_parameter(&ipps_client, IPPS_OBJ_GPU, &gdbgipps_param);
	else
		ipps_set_parameter(&ipps_client, IPPS_OBJ_GPU, &gipps_param);
#else
	ipps_set_parameter(&ipps_client, IPPS_OBJ_GPU, &gipps_param);
#endif

	return 0;
}

static struct notifier_block gpumaxprofile_notifier = {
	.notifier_call = gpumaxprofile_notify,
};

static int gpuminprofile_notify(struct notifier_block *b, unsigned long l,
	void *v)
{
	pr_debug("[%s] in=%lu\n", __func__, l);

	PARAM_MIN(gpu) = l;

	pr_debug("[%s] gpu_min=%d\n", __func__, PARAM_MIN(gpu));

#ifdef CONFIG_DEBUG_FS
	if (gdbgipps_param.gpu.min_freq != 0)
		ipps_set_parameter(&ipps_client, IPPS_OBJ_GPU, &gdbgipps_param);
	else
		ipps_set_parameter(&ipps_client, IPPS_OBJ_GPU, &gipps_param);
#else
	ipps_set_parameter(&ipps_client, IPPS_OBJ_GPU, &gipps_param);
#endif
	return 0;
}

static struct notifier_block gpuminprofile_notifier = {
	.notifier_call = gpuminprofile_notify,
};

static int gpublockprofile_notify(struct notifier_block *b, unsigned long l,
	void *v)
{
	pr_debug("[%s] in=%lu\n", __func__, l);

	PARAM_BLOCK(gpu) = l;

	pr_debug("[%s] gpu_block=%d\n", __func__, PARAM_BLOCK(gpu));

#ifdef CONFIG_DEBUG_FS
	if (gdbgipps_param.gpu.max_freq != 0 || gdbgipps_param.gpu.min_freq != 0)
		ipps_set_parameter(&ipps_client, IPPS_OBJ_GPU, &gdbgipps_param);
	else
		ipps_set_parameter(&ipps_client, IPPS_OBJ_GPU, &gipps_param);
#else
	ipps_set_parameter(&ipps_client, IPPS_OBJ_GPU, &gipps_param);
#endif
	return 0;
}

static struct notifier_block gpublockprofile_notifier = {
	.notifier_call = gpublockprofile_notify,
};

static int gpusafeprofile_notify(struct notifier_block *b, unsigned long l,
	void *v)
{
	pr_debug("[%s] in=%lu\n", __func__, l);

	PARAM_SAFE(gpu) = l;

	pr_debug("[%s] gpu_safe=%d\n", __func__, PARAM_SAFE(gpu));

#ifdef CONFIG_DEBUG_FS
	if (gdbgipps_param.gpu.safe_freq != 0)
		ipps_set_parameter(&ipps_client, IPPS_OBJ_GPU, &gdbgipps_param);
	else
		ipps_set_parameter(&ipps_client, IPPS_OBJ_GPU, &gipps_param);
#else
	ipps_set_parameter(&ipps_client, IPPS_OBJ_GPU, &gipps_param);
#endif
	return 0;
}

static struct notifier_block gpusafeprofile_notifier = {
	.notifier_call = gpusafeprofile_notify,
};

static int ddrmaxprofile_notify(struct notifier_block *b, unsigned long l,
	void *v)
{
	pr_debug("[%s] in=%lu\n", __func__, l);

	PARAM_MAX(ddr) = l;

	pr_debug("[%s] ddr_max=%d\n", __func__, PARAM_MAX(ddr));

#ifdef CONFIG_DEBUG_FS
	if (gdbgipps_param.ddr.max_freq != 0)
		ipps_set_parameter(&ipps_client, IPPS_OBJ_DDR, &gdbgipps_param);
	else
		ipps_set_parameter(&ipps_client, IPPS_OBJ_DDR, &gipps_param);
#else
	ipps_set_parameter(&ipps_client, IPPS_OBJ_DDR, &gipps_param);
#endif

	return 0;
}

static struct notifier_block ddrmaxprofile_notifier = {
	.notifier_call = ddrmaxprofile_notify,
};

static int ddrminprofile_notify(struct notifier_block *b, unsigned long l,
	void *v)
{
	pr_debug("[%s] in=%lu\n", __func__, l);

	PARAM_MIN(ddr) = l;

	pr_debug("[%s] ddr_min=%d\n", __func__, PARAM_MIN(ddr));

#ifdef CONFIG_DEBUG_FS
	if (gdbgipps_param.ddr.min_freq != 0)
		ipps_set_parameter(&ipps_client, IPPS_OBJ_DDR, &gdbgipps_param);
	else
		ipps_set_parameter(&ipps_client, IPPS_OBJ_DDR, &gipps_param);
#else
	ipps_set_parameter(&ipps_client, IPPS_OBJ_DDR, &gipps_param);
#endif

	return 0;
}

static struct notifier_block ddrminprofile_notifier = {
	.notifier_call = ddrminprofile_notify,
};

static int ddrblockprofile_notify(struct notifier_block *b, unsigned long l,
	void *v)
{
	pr_debug("[%s] in=%lu\n", __func__, l);

	PARAM_BLOCK(ddr) = l;

	pr_debug("[%s] ddr_block=%d\n", __func__, PARAM_BLOCK(ddr));

#ifdef CONFIG_DEBUG_FS
	if (gdbgipps_param.ddr.max_freq != 0 || gdbgipps_param.ddr.min_freq != 0)
		ipps_set_parameter(&ipps_client, IPPS_OBJ_DDR, &gdbgipps_param);
	else
		ipps_set_parameter(&ipps_client, IPPS_OBJ_DDR, &gipps_param);
#else
	ipps_set_parameter(&ipps_client, IPPS_OBJ_DDR, &gipps_param);
#endif
	return 0;
}

static struct notifier_block ddrblockprofile_notifier = {
	.notifier_call = ddrblockprofile_notify,
};

static int ddrsafeprofile_notify(struct notifier_block *b, unsigned long l,
	void *v)
{
	pr_debug("[%s] in=%lu\n", __func__, l);

	PARAM_SAFE(ddr) = l;

	pr_debug("[%s] ddr_safe=%d\n", __func__, PARAM_SAFE(ddr));

#ifdef CONFIG_DEBUG_FS
	if (gdbgipps_param.ddr.safe_freq != 0)
		ipps_set_parameter(&ipps_client, IPPS_OBJ_DDR, &gdbgipps_param);
	else
		ipps_set_parameter(&ipps_client, IPPS_OBJ_DDR, &gipps_param);
#else
	ipps_set_parameter(&ipps_client, IPPS_OBJ_DDR, &gipps_param);
#endif
	return 0;
}

static struct notifier_block ddrsafeprofile_notifier = {
	.notifier_call = ddrsafeprofile_notify,
};

static int ippspolicy_notify(struct notifier_block *b, unsigned long l,
	void *v)
{
	unsigned int upolicy = 0;

	upolicy = l;

	if (PM_QOS_IPPS_POLICY_MAX <  upolicy) {
		upolicy = PM_QOS_IPPS_POLICY_SPECIAL0B;
		WARN(1, "Policy overflow =%x\n", l);
	}

	pr_info("%s l=%d, upolicy=%x\n", __func__, __LINE__, upolicy);

	switch_set_state(&policy_switch, upolicy);

	upolicy = (upolicy&0xF) << 4;

	ipps_set_current_policy(&ipps_client, IPPS_OBJ_CPU|IPPS_OBJ_GPU|IPPS_OBJ_DDR, &upolicy);
	return 0;
}

static struct notifier_block ippspolicy_notifier = {
	.notifier_call = ippspolicy_notify,
};

static void k3v2_qos_remove_notifier(void)
{
	int ret = 0;

	QOS_REMOVE_NOTIFY(PM_QOS_CPU_MAX_PROFILE, cpumaxprofile_notifier);

	QOS_REMOVE_NOTIFY(PM_QOS_CPU_MIN_PROFILE, cpuminprofile_notifier);

	QOS_REMOVE_NOTIFY(PM_QOS_CPU_PROFILE_BLOCK, cpublockprofile_notifier);

	QOS_REMOVE_NOTIFY(PM_QOS_CPU_PROFILE_SAFE, cpusafeprofile_notifier);

	QOS_REMOVE_NOTIFY(PM_QOS_GPU_MAX_PROFILE, gpumaxprofile_notifier);

	QOS_REMOVE_NOTIFY(PM_QOS_GPU_MIN_PROFILE, gpuminprofile_notifier);

	QOS_REMOVE_NOTIFY(PM_QOS_GPU_PROFILE_BLOCK, gpublockprofile_notifier);

	QOS_REMOVE_NOTIFY(PM_QOS_GPU_PROFILE_SAFE, gpusafeprofile_notifier);

	QOS_REMOVE_NOTIFY(PM_QOS_DDR_MAX_PROFILE, ddrmaxprofile_notifier);

	QOS_REMOVE_NOTIFY(PM_QOS_DDR_MIN_PROFILE, ddrminprofile_notifier);

	QOS_REMOVE_NOTIFY(PM_QOS_DDR_PROFILE_BLOCK, ddrblockprofile_notifier);

	QOS_REMOVE_NOTIFY(PM_QOS_DDR_PROFILE_SAFE, ddrsafeprofile_notifier);

	QOS_REMOVE_NOTIFY(PM_QOS_IPPS_POLICY, ippspolicy_notifier);
}


static void k3v2_qos_add_notifier(void)
{
	int ret = 0;

	QOS_ADD_NOTIFY(PM_QOS_CPU_MAX_PROFILE, cpumaxprofile_notifier);

	QOS_ADD_NOTIFY(PM_QOS_CPU_MIN_PROFILE, cpuminprofile_notifier);

	QOS_ADD_NOTIFY(PM_QOS_CPU_PROFILE_BLOCK, cpublockprofile_notifier);

	QOS_ADD_NOTIFY(PM_QOS_CPU_PROFILE_SAFE, cpusafeprofile_notifier);

	QOS_ADD_NOTIFY(PM_QOS_GPU_MAX_PROFILE, gpumaxprofile_notifier);

	QOS_ADD_NOTIFY(PM_QOS_GPU_MIN_PROFILE, gpuminprofile_notifier);

	QOS_ADD_NOTIFY(PM_QOS_GPU_PROFILE_BLOCK, gpublockprofile_notifier);

	QOS_ADD_NOTIFY(PM_QOS_GPU_PROFILE_SAFE, gpusafeprofile_notifier);

	QOS_ADD_NOTIFY(PM_QOS_DDR_MAX_PROFILE, ddrmaxprofile_notifier);

	QOS_ADD_NOTIFY(PM_QOS_DDR_MIN_PROFILE, ddrminprofile_notifier);

	QOS_ADD_NOTIFY(PM_QOS_DDR_PROFILE_BLOCK, ddrblockprofile_notifier);

	QOS_ADD_NOTIFY(PM_QOS_DDR_PROFILE_SAFE, ddrsafeprofile_notifier);

	QOS_ADD_NOTIFY(PM_QOS_IPPS_POLICY, ippspolicy_notifier);

	return;

ERROR:
	k3v2_qos_remove_notifier();

}

/*******************Qos notifier end    ******************/


/*******************freq attr setting begin*****************************/

/**
 * show_available_freqs - show available frequencies for the specified GPU
 */
static ssize_t show_gpu_available_freqs(struct cpufreq_policy *policy,
	char *buf)
{
	unsigned int i = 0;
	ssize_t count = 0;

	for (i = 0; (gpufreq_table[i].frequency != CPUFREQ_TABLE_END); i++) {

		if (gpufreq_table[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;

		count += sprintf(&buf[count], "%d ", gpufreq_table[i].frequency);
	}

	count += sprintf(&buf[count], "\n");

	return count;
}

static struct freq_attr gpufreq_freq_attr_scaling_available_freqs = {
	.attr = { .name = "scaling_available_gpufrequencies",
		  .mode = 0444,
		},
	.show = show_gpu_available_freqs,
};

/**
 * show_available_freqs - show available frequencies for the specified GPU
 */
static ssize_t show_ddr_available_freqs(struct cpufreq_policy *policy,
	char *buf)
{
	unsigned int i = 0;
	ssize_t count = 0;

	for (i = 0; (ddrfreq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
		if (ddrfreq_table[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;

		count += sprintf(&buf[count], "%d ", ddrfreq_table[i].frequency);
	}
	count += sprintf(&buf[count], "\n");

	return count;

}

static struct freq_attr ddrfreq_freq_attr_scaling_available_freqs = {
	.attr = { .name = "scaling_available_ddrfrequencies",
		  .mode = 0444,
		},
	.show = show_ddr_available_freqs,
};

/**
 * show_available_policy - show available policy
 */
static ssize_t show_available_policy(struct cpufreq_policy *policy,
	char *buf)
{
	ssize_t count = 0;

	count += sprintf(&buf[count], "powersave normal performance\n");

	return count;
}

static struct freq_attr attr_scaling_available_policy = {
	.attr = { .name = "scaling_available_policies",
		  .mode = 0444,
		},
	.show = show_available_policy,
};

static ssize_t show_scaling_policy(struct cpufreq_policy *policy, char *buf)
{
	/*first read current policy from mcu*/
	unsigned int upolicy = 0;
	ipps_get_current_policy(&ipps_client, IPPS_OBJ_CPU, &upolicy);

	/*then return to user*/
	switch (upolicy) {
	case 0x00:
		return sprintf(buf, "powersave\n");
	case 0x10:
		return sprintf(buf, "normal\n");
	case 0x20:
		return sprintf(buf, "performance\n");
	default:
		return sprintf(buf, "powersave\n");
	}
}

static ssize_t store_scaling_policy(struct cpufreq_policy *policy,
					const char *buf, size_t count)
{
	char	str_policy[16];
	unsigned int ret = -EINVAL;
	unsigned int upolicy = 0;

	ret = sscanf(buf, "%15s", str_policy);
	if (ret != 1)
		return -EINVAL;

	printk("%s %d %s\n", __func__, __LINE__, str_policy);

	if (!strnicmp(str_policy, "powersave",	CPUFREQ_NAME_LEN)) {
		/*powersave mode*/
		printk("powersave called.\n");
		upolicy = 0x0;
	} else if (!strnicmp(str_policy, "performance",	CPUFREQ_NAME_LEN)) {
		/*performance mode*/
		printk("performance called.\n");
		upolicy = 0x2;
	} else if (!strnicmp(str_policy, "normal", CPUFREQ_NAME_LEN)) {
		/*normal mode*/
		printk("normal called.\n");
		upolicy = 0x1;
	} else if (!strnicmp(str_policy, "special01", CPUFREQ_NAME_LEN)) {
		/*special01 mode*/
		printk("special01 called.\n");
		upolicy = 0x3;
	} else if (!strnicmp(str_policy, "special02", CPUFREQ_NAME_LEN)) {
		/*special02 mode*/
		printk("special02 called.\n");
		upolicy = 0x4;
	} else if (!strnicmp(str_policy, "special03", CPUFREQ_NAME_LEN)) {
		/*special03 mode*/
		printk("special03 called.\n");
		upolicy = 0x5;
	} else {
		/*powersave mode*/
		printk("powersave called.\n");
		upolicy = 0x0;
	}

	pm_qos_update_request(&g_ippspolicy, upolicy);
	return count;
}

cpufreq_freq_attr_rw(scaling_policy);

/*******************freq attr setting end *****************************/

/*******************hotplug cpu operation ****************************/

static inline void wait_cpu_idle(unsigned int cpu)
{
	 unsigned int cnt = 0;
	 unsigned int tmp = 0;

	 tmp = readl(IO_ADDRESS(0xFC8020FC));
	 while ((1<<(8+cpu)) != (tmp & (1<<(8+cpu)))) {
		msleep(1);
		cnt++;
		tmp = readl(IO_ADDRESS(0xFC8020FC));

		/*wait 1s*/
		if (cnt > 1000) {
			WARN(1, "cpu %d wfi state is error, please check!\r\n", cpu);
			break;
		}
	}
}

static int __cpuinit hotcpu_k3_prepare(struct notifier_block *nfb,
					unsigned long action, void *hcpu)
{
	unsigned int umode = 0;
	unsigned int cpu = (unsigned long)hcpu;

	switch (action & 0xF) {
	case CPU_UP_PREPARE:

		umode = IPPS_DISABLE;
		ipps_set_mode(&ipps_client, IPPS_OBJ_CPU, &umode);

		/* MTCMOS */
		if (cpu > 1)
			writel((0x01 << (cpu + 3)), IO_ADDRESS(0xFC8020D0));
		udelay(100);

		/* Enable core */
		writel((0x01 << cpu), IO_ADDRESS(0xFC8020F4));

		/* Unreset */
		if (cpu > 1)
			writel((0x1011 << cpu), IO_ADDRESS(0xfc802414));

		/* reset */
		if (cpu > 1)
			writel((0x1011 << cpu), IO_ADDRESS(0xfc802410));

		/* ISO disable */
		if (cpu > 1)
			writel((0x01 << (cpu + 3)), IO_ADDRESS(0xFC8020C4));
		udelay(1);

		/* WFI Mask */
		writel(((~(0x1 << (cpu+28)))&readl(IO_ADDRESS(0xFC802200))), IO_ADDRESS(0xfc802200));

		/* Unreset */
		writel((0x1011 << cpu), IO_ADDRESS(0xfc802414));

		break;

	case CPU_DOWN_PREPARE:
		umode = IPPS_DISABLE;
		ipps_set_mode(&ipps_client, IPPS_OBJ_CPU, &umode);
		break;

	default:
		break;
	}

	return NOTIFY_OK;
}


static int __cpuinit hotcpu_k3_later(struct notifier_block *nfb,
					unsigned long action, void *hcpu)
{
	unsigned int umode = 0;
	unsigned int cpu = (unsigned long)hcpu;

	switch (action & 0xF) {
	case CPU_ONLINE:
	case CPU_DOWN_FAILED:
		umode = IPPS_ENABLE;
		ipps_set_mode(&ipps_client, IPPS_OBJ_CPU, &umode);
		break;

	case CPU_UP_CANCELED:
	case CPU_DEAD:

		/*wait cpu wfi*/
		wait_cpu_idle(cpu);

		/* WFI Mask */
		writel(((0x1 << (cpu+28))|readl(IO_ADDRESS(0xFC802200))), IO_ADDRESS(0xfc802200));

		/* Disable core */
		writel((0x01 << cpu), IO_ADDRESS(0xFC8020F8));

		/* ISO enable */
		if (cpu > 1)
			writel((0x01 << (cpu + 3)), IO_ADDRESS(0xFC8020C0));
		udelay(1);

		/* Reset */
		writel((0x1011 << cpu), IO_ADDRESS(0xfc802410));

		/* MTCMOS */
		if (cpu > 1)
			writel((0x01 << (cpu + 3)), IO_ADDRESS(0xFC8020D4));
		udelay(100);

		umode = IPPS_ENABLE;
		ipps_set_mode(&ipps_client, IPPS_OBJ_CPU, &umode);
		break;

	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block __refdata hotcpu_prepare_notifier = {
	.notifier_call = hotcpu_k3_prepare,
	.priority      = INT_MAX,
};

static struct notifier_block __refdata hotcpu_later_notifier = {
	.notifier_call = hotcpu_k3_later,
	.priority      = INT_MIN,
};

/*******************hotplug cpu operation ****************************/


/*******************Cpufreq_driver interface *************/
static int k3v2_verify_freq(struct cpufreq_policy *policy)
{
	if (cpufreq_table)
		return cpufreq_frequency_table_verify(policy, cpufreq_table);

	if (policy->cpu)
		return -EINVAL;

	cpufreq_verify_within_limits(policy, policy->cpuinfo.min_freq,
				     policy->cpuinfo.max_freq);
	return 0;
}

static unsigned int k3v2_getfreq(unsigned int cpu)
{
	unsigned int rate = 0;

	pr_debug("[%s] %d cpu\n", __func__, __LINE__);

	if (cpu)
		return 0;

	ipps_get_current_freq(&ipps_client, IPPS_OBJ_CPU, &rate);

	pr_debug("[%s] %d cur=%d.\n", __func__, __LINE__, rate);

	return rate;
}

static int k3v2_target(struct cpufreq_policy *policy,
		       unsigned int target_freq,
		       unsigned int relation)
{
	struct cpufreq_freqs freqs;
	int ret = 0;
	unsigned int dest_freq = target_freq;

	/*check if we have limits on the cpufreqs*/
	if (target_freq > PARAM_MAX(cpu))
		target_freq = PARAM_MAX(cpu);

	if (target_freq < PARAM_MIN(cpu))
		target_freq = PARAM_MIN(cpu);

	freqs.old = k3v2_getfreq(0);
	freqs.new = target_freq;
	freqs.cpu = 0;

	if (freqs.old == freqs.new)
		return ret;

	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	/*set cpu freq.*/
	ipps_set_current_freq(&ipps_client, IPPS_OBJ_CPU, &dest_freq);
	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	return ret;
}

static int __cpuinit k3v2_cpu_init(struct cpufreq_policy *policy)
{
	int result = 0;

	if (policy->cpu != 0) {
		pr_err("[%s] %d cpu=%d\n", __func__, __LINE__, policy->cpu);
	}

	policy->cpuinfo.transition_latency = 1 * 1000;

	if (!cpufreq_table) {
		cpufreq_table = kmalloc(sizeof(struct cpufreq_frequency_table) * 16 , GFP_KERNEL);

		if (!cpufreq_table) {
			pr_err("[%s] %d cpufreq_table kmalloc err\n", __func__, __LINE__);
			goto ERROR;
		}
	}

	if (!gpufreq_table) {
		gpufreq_table = kmalloc(sizeof(struct cpufreq_frequency_table) * 16 , GFP_KERNEL);

		if (!gpufreq_table) {
			pr_err("[%s] %d gpufreq_table kmalloc err\n", __func__, __LINE__);
			goto ERROR;
		}
	}

	if (!ddrfreq_table) {
		ddrfreq_table = kmalloc(sizeof(struct cpufreq_frequency_table) * 16 , GFP_KERNEL);

		if (!ddrfreq_table) {
			pr_err("[%s] %d ddrfreq_table kmalloc err\n", __func__, __LINE__);
			goto ERROR;
		}
	}

	if (policy->cpu == 0) {
		/*get freq_table*/
		ipps_get_freqs_table(&ipps_client, IPPS_OBJ_CPU, cpufreq_table);
		ipps_get_freqs_table(&ipps_client, IPPS_OBJ_DDR, ddrfreq_table);
		ipps_get_freqs_table(&ipps_client, IPPS_OBJ_GPU, gpufreq_table);

		/*get freq limits*/
		ipps_get_parameter(&ipps_client, IPPS_OBJ_DDR|IPPS_OBJ_GPU|IPPS_OBJ_CPU, &gipps_param);

		policy_switch.name = "ippspolicy";
		result = switch_dev_register(&policy_switch);
		if (0 != result) {
			pr_err("%s line=%d, switch_dev_register err=%x\n", __func__, __LINE__, result);
			goto ERROR;
		}
	}

	/*get current profile*/
	policy->cur = k3v2_getfreq(0);
	if (cpufreq_table) {
		result = cpufreq_frequency_table_cpuinfo(policy, cpufreq_table);
		if (!result)
			cpufreq_frequency_table_get_attr(cpufreq_table,
							policy->cpu);
	} else {
		pr_err("freq_table does not exist.\n");
	}

	policy->min = PARAM_MIN(cpu);
	policy->max = PARAM_MAX(cpu);

	if (policy->cpu == 0) {
#ifdef CONFIG_K3V2_DVFSEN
	if (!(RUNMODE_FLAG_FACTORY == runmode_is_factory() && 1 == get_boot_into_recovery_flag())) {
		unsigned int umode = IPPS_ENABLE;
		ipps_set_mode(&ipps_client, IPPS_OBJ_CPU, &umode);
	}
#endif
		/*add qos notifier*/
		k3v2_qos_add_notifier();

		pm_qos_add_request(&g_ippspolicy, PM_QOS_IPPS_POLICY,
			PM_QOS_IPPS_POLICY_DEFAULT_VALUE);
	}

	return 0;

ERROR:

	if (cpufreq_table) {
		kfree(cpufreq_table);
		cpufreq_table = NULL;
	}

	if (ddrfreq_table) {
		kfree(ddrfreq_table);
		ddrfreq_table = NULL;
	}

	if (gpufreq_table) {
		kfree(gpufreq_table);
		gpufreq_table = NULL;
	}

	printk("[%s] %d leave %x\n", __func__, __LINE__, EINVAL);

	return -EINVAL;
}

static int k3v2_cpu_exit(struct cpufreq_policy *policy)
{
	if (policy->cpu != 0) {
		pr_err("[%s] %d cpu=%d\n", __func__, __LINE__, policy->cpu);
		return -EINVAL;
	}

	pm_qos_remove_request(&g_ippspolicy);

	/*remove notifier*/
	k3v2_qos_remove_notifier();

	if (cpufreq_table) {
		kfree(cpufreq_table);
		cpufreq_table = NULL;
	}

	if (ddrfreq_table) {
		kfree(ddrfreq_table);
		ddrfreq_table = NULL;
	}

	if (gpufreq_table) {
		kfree(gpufreq_table);
		gpufreq_table = NULL;
	}

	switch_dev_unregister(&policy_switch);

	return 0;
}

static struct freq_attr *k3v2_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	&gpufreq_freq_attr_scaling_available_freqs,
	&ddrfreq_freq_attr_scaling_available_freqs,
	&attr_scaling_available_policy,
	&scaling_policy,
	NULL,
};


static struct cpufreq_driver k3v2_driver = {
	.flags		= CPUFREQ_STICKY,
	.verify		= k3v2_verify_freq,
	.target		= k3v2_target,
	.get		= k3v2_getfreq,
	.init		= k3v2_cpu_init,
	.exit		= k3v2_cpu_exit,
	.name		= "k3v2cpufreq",
	.attr		= k3v2_cpufreq_attr,
};

/******************cpufreq_driver interface end*****************************/


/******************ipps driver interface begin*****************************/

static void ippsclient_add(struct ipps_device *device)
{
	int ret = 0;

	gpipps_device = device;

	ret = cpufreq_register_driver(&k3v2_driver);
	if (0 != ret)
		pr_err("[%s] cpufreq_register_driver err=%x\n", __func__, ret);

	register_hotcpu_notifier(&hotcpu_prepare_notifier);
	register_hotcpu_notifier(&hotcpu_later_notifier);
}

static void ippsclient_remove(struct ipps_device *device)
{
	int ret = 0;

	ret = cpufreq_unregister_driver(&k3v2_driver);
	if (ret != 0)
		pr_err("cpufreq_unregister_driver err=%x\n", ret);

	unregister_hotcpu_notifier(&hotcpu_later_notifier);
	unregister_hotcpu_notifier(&hotcpu_prepare_notifier);

	gpipps_device = NULL;
}

static struct ipps_client ipps_client = {
	.name   = "k3v2cpufreq",
	.add    = ippsclient_add,
	.remove = ippsclient_remove
};

/*****************ipps driver interface end ****************************/

static int __init k3v2_cpufreq_init(void)
{
	int ret = 0;

	ret = ipps_register_client(&ipps_client);
	if (ret != 0)
		pr_err("%s ipps_register_client err=%x\n",
			__func__, ret);

	return ret;
}

static void __exit k3v2_cpufreq_exit(void)
{
	ipps_unregister_client(&ipps_client);
}

MODULE_AUTHOR("s00107748");
MODULE_DESCRIPTION("Cpufreq driver for k3v2 processors.");
MODULE_LICENSE("GPL");

arch_initcall(k3v2_cpufreq_init);
module_exit(k3v2_cpufreq_exit);
