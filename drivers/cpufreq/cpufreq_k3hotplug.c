/*
 * CPUFreq k3hotplug governor
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/reboot.h>
#include <linux/pm_qos_params.h>
#include <linux/ipps.h>
#include <linux/cpufreq-k3v2.h>
#include <mach/boardid.h>
#include <linux/suspend.h>

/* pm_qos interface global val*/
struct pm_qos_lst {
	struct pm_qos_request_list *lst;
	int qos_class;
	s32 dvalue;
};

static struct pm_qos_request_list g_cpumaxlimits;
static struct pm_qos_request_list g_cpuminlimits;
static struct pm_qos_request_list g_cpublocklimits;
static struct pm_qos_request_list g_cpusafelimits;
static struct pm_qos_request_list g_gpumaxlimits;
static struct pm_qos_request_list g_gpuminlimits;
static struct pm_qos_request_list g_gpublocklimits;
static struct pm_qos_request_list g_gpusafelimits;
static struct pm_qos_request_list g_ddrmaxlimits;
static struct pm_qos_request_list g_ddrminlimits;
static struct pm_qos_request_list g_ddrblocklimits;
static struct pm_qos_request_list g_ddrsafelimits;
static struct pm_qos_request_list g_qoscpulock;
static struct pm_qos_request_list g_qoscpumax;
static struct pm_qos_request_list g_qoscpumin;
static struct pm_qos_request_list g_qoscpusafe;

static struct pm_qos_lst pm_qos_list[] = {
{&g_cpumaxlimits, PM_QOS_CPU_MAX_PROFILE, PM_QOS_CPU_MAXPROFILE_DEFAULT_VALUE},
{&g_cpuminlimits, PM_QOS_CPU_MIN_PROFILE, PM_QOS_CPU_MINPROFILE_DEFAULT_VALUE},
{&g_cpublocklimits, PM_QOS_CPU_PROFILE_BLOCK, PM_QOS_CPU_BLKPROFILE_DEFAULT_VALUE},
{&g_cpusafelimits, PM_QOS_CPU_PROFILE_SAFE, PM_QOS_CPU_SAFEPROFILE_DEFAULT_VALUE},
{&g_gpumaxlimits, PM_QOS_GPU_MAX_PROFILE, PM_QOS_GPU_MAXPROFILE_DEFAULT_VALUE},
{&g_gpuminlimits, PM_QOS_GPU_MIN_PROFILE, PM_QOS_GPU_MINPROFILE_DEFAULT_VALUE},
{&g_gpublocklimits, PM_QOS_GPU_PROFILE_BLOCK, PM_QOS_GPU_BLKPROFILE_DEFAULT_VALUE},
{&g_gpusafelimits, PM_QOS_GPU_PROFILE_SAFE, PM_QOS_GPU_SAFEPROFILE_DEFAULT_VALUE},
{&g_ddrmaxlimits, PM_QOS_DDR_MAX_PROFILE, PM_QOS_DDR_MAXPROFILE_DEFAULT_VALUE},
{&g_ddrminlimits, PM_QOS_DDR_MIN_PROFILE, PM_QOS_DDR_MINPROFILE_DEFAULT_VALUE},
{&g_ddrblocklimits, PM_QOS_DDR_PROFILE_BLOCK, PM_QOS_DDR_BLKPROFILE_DEFAULT_VALUE},
{&g_ddrsafelimits, PM_QOS_DDR_PROFILE_SAFE, PM_QOS_DDR_SAFEPROFILE_DEFAULT_VALUE},
{&g_qoscpulock, PM_QOS_CPU_NUMBER_LOCK, PM_QOS_CPU_NUMBER_LOCK_DEFAULT_VALUE},
{&g_qoscpumax, PM_QOS_CPU_NUMBER_MAX, PM_QOS_CPU_NUMBER_MAX_DEFAULT_VALUE},
{&g_qoscpumin, PM_QOS_CPU_NUMBER_MIN, PM_QOS_CPU_NUMBER_MIN_DEFAULT_VALUE},
{&g_qoscpusafe, PM_QOS_CPU_NUMBER_SAFE, PM_QOS_CPU_NUMBER_SAFE_DEFAULT_VALUE},
};


static int cpufreq_governor_dbs(struct cpufreq_policy *policy,
		unsigned int event);

struct cpufreq_governor cpufreq_gov_k3hotplug = {
	.name                   = "k3hotplug",
	.governor               = cpufreq_governor_dbs,
	.owner                  = THIS_MODULE,
};

struct cpu_dbs_info_s {
	struct cpufreq_policy *cur_policy;
	struct cpufreq_frequency_table *freq_table;
	int cpu;
};
static DEFINE_PER_CPU(struct cpu_dbs_info_s, hp_cpu_dbs_info);

static unsigned int dbs_enable;	/* number of CPUs using this policy */

/*
 * dbs_mutex protects data in gippslimit from concurrent changes on
 * different CPUs. It protects dbs_enable in governor start/stop.
 */
static DEFINE_MUTEX(dbs_mutex);

/************************** sysfs interface ************************/

struct cpu_num_limit gcpu_num_limit = {
	.max = NR_CPUS,
	.min = 1,
	.block = 0,
};

/* cpufreq_hotplug Governor Tunables */
#define show_gpu_one(file_name, object)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)		\
{									\
	unsigned gpufreq =	PARAM_VAL(gpu, object);		\
	pr_info("[%s] %d\n", __func__, PARAM_VAL(gpu, object));	\
	return sprintf(buf, "%u\n", gpufreq);		\
}

show_gpu_one(gpu_max_profile, max_freq);
show_gpu_one(gpu_min_profile, min_freq);
show_gpu_one(gpu_profile_block, block_freq);
show_gpu_one(gpu_safe_profile, safe_freq);

#define show_ddr_one(file_name, object)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)		\
{									\
	unsigned ddrfreq = PARAM_VAL(ddr, object); \
	pr_debug("[%s] %d\n", __func__, PARAM_VAL(ddr, object));	\
	return sprintf(buf, "%u\n", ddrfreq);		\
}

show_ddr_one(ddr_max_profile, max_freq);
show_ddr_one(ddr_min_profile, min_freq);
show_ddr_one(ddr_profile_block, block_freq);
show_ddr_one(ddr_safe_profile, safe_freq);

#define show_cpu_one(file_name, object)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)		\
{									\
	return sprintf(buf, "%u\n", gcpu_num_limit.object);		\
}

show_cpu_one(cpu_lock, block);
show_cpu_one(cpu_max, max);
show_cpu_one(cpu_min, min);
show_cpu_one(cpu_safe, safe);


#define show_scaling_one(file_name, object)	\
static ssize_t show_##file_name	\
(struct kobject *kobj, struct attribute *attr, char *buf)	\
{	\
	unsigned int freq = PARAM_VAL(cpu, object);\
	pr_debug("[%s] %d\n", __func__, PARAM_VAL(cpu, object));	\
	return sprintf(buf, "%u\n", freq);	\
}

show_scaling_one(cpu_profile_block, block_freq);
show_scaling_one(cpu_safe_profile, safe_freq);


#define store_one(file_name, object, pm_qos) \
static ssize_t store_##file_name	\
(struct kobject *a, struct attribute *b,	\
				   const char *buf, size_t count)	\
{	\
	unsigned int input;	\
	int ret = sscanf(buf, "%u", &input);	\
	if (ret != 1)		\
		return -EINVAL;	\
						\
	pm_qos_update_request(&pm_qos, input);	\
						\
	return count;	\
}					\

store_one(cpu_profile_block, block_limit, g_cpublocklimits);
store_one(cpu_safe_profile, safe_limit, g_cpusafelimits);
store_one(gpu_max_profile, max_limit, g_gpumaxlimits);
store_one(gpu_min_profile, min_limit, g_gpuminlimits);
store_one(gpu_profile_block, block_limit, g_gpublocklimits);
store_one(gpu_safe_profile, safe_limit, g_gpusafelimits);
store_one(ddr_max_profile, max_limit, g_ddrmaxlimits);
store_one(ddr_min_profile, min_limit, g_ddrminlimits);
store_one(ddr_profile_block, block_limit, g_ddrblocklimits);
store_one(ddr_safe_profile, safe_limit, g_ddrsafelimits);
store_one(cpu_lock, cpu_lock, g_qoscpulock);
store_one(cpu_max, max_limit, g_qoscpumax);
store_one(cpu_min, min_limit, g_qoscpumin);
store_one(cpu_safe, safe_limit, g_qoscpusafe);


define_one_global_rw(cpu_profile_block);
define_one_global_rw(cpu_safe_profile);
define_one_global_rw(gpu_max_profile);
define_one_global_rw(gpu_min_profile);
define_one_global_rw(gpu_profile_block);
define_one_global_rw(gpu_safe_profile);
define_one_global_rw(ddr_max_profile);
define_one_global_rw(ddr_min_profile);
define_one_global_rw(ddr_profile_block);
define_one_global_rw(ddr_safe_profile);
define_one_global_rw(cpu_lock);
define_one_global_rw(cpu_max);
define_one_global_rw(cpu_min);
define_one_global_rw(cpu_safe);


static struct attribute *dbs_attributes[] = {
	&cpu_profile_block.attr,
	&cpu_safe_profile.attr,
	&gpu_max_profile.attr,
	&gpu_min_profile.attr,
	&gpu_profile_block.attr,
	&gpu_safe_profile.attr,
	&ddr_max_profile.attr,
	&ddr_min_profile.attr,
	&ddr_profile_block.attr,
	&ddr_safe_profile.attr,
	&cpu_lock.attr,
	&cpu_max.attr,
	&cpu_min.attr,
	&cpu_safe.attr,
	NULL
};

static struct attribute_group dbs_attr_group = {
	.attrs = dbs_attributes,
	.name = "k3hotplug",
};

/************************** sysfs end ************************/

/***************************cpu hotplug*************************/
#ifndef NO_CPU_HOTPLUG

#define DEFAULT_HOTPLUG_IN_LOAD			(95)
#define DEFAULT_HOTPLUG_OUT_LOAD		(3)
#define DEFAULT_DIFFERENTIAL			(10)

#define DEFAULT_SAMPLING_PERIOD			(50000)
/* 20s booting not hotplug */
#define BOOTING_SAMPLING_PERIOD			(20000)

/*hotplug task running*/
#define DEFAULT_HOTPLUG_IN_RUN			(3)
#define DEFAULT_HOTPLUG_OUT_RUN			(6)

/*task running threshold*/
#define TASK_THRESHOLD_H				(199)
#define TASK_THRESHOLD_L				(110)

/*CPU NUM WATERSHED*/
#define CPU_NUM_WATERSHED				(2)

/* default number of sampling periods to average before hotplug-in decision */
#define DEFAULT_HOTPLUG_IN_SAMPLING_PERIODS		(3)

/* default number of sampling periods to average before hotplug-out decision */
#define DEFAULT_HOTPLUG_OUT_SAMPLING_PERIODS	(20)
#define DEFAULT_HOTPLUG_OUT_SAMPLING_PERIODS1	(200)

struct cpu_info_s {
	cputime64_t prev_cpu_idle;
	cputime64_t prev_cpu_wall;
	cputime64_t prev_cpu_nice;
};
static DEFINE_PER_CPU(struct cpu_info_s, hp_cpu_info);

static struct delayed_work k_work;

/*average nr_running on cpu*/
static int g_iavruning = 0;
static int g_iavraddcnt = 0;
static int g_iavrsubcnt = 0;
static int hotplug_in_count = 0;
static int hotplug_out_count = 0;


static inline cputime64_t get_cpu_idle_time(unsigned int cpu, cputime64_t *wall)
{
	u64 idle_time;
	u64 iowait_time;

	/* cpufreq-hotplug always assumes CONFIG_NO_HZ */
	idle_time = get_cpu_idle_time_us(cpu, wall);

	iowait_time = get_cpu_iowait_time_us(cpu, wall);

	/* cpufreq-hotplug always assumes CONFIG_NO_HZ */
	if (iowait_time != -1ULL && idle_time >= iowait_time)
		idle_time -= iowait_time;

	return idle_time;
}

static void auto_hotplug(void)
{
	/* single largest CPU load percentage*/
	unsigned int max_load = 0;
	unsigned int min_load = 100;
	int cpun = 0;
	unsigned int j;

	int cpufreq = cpufreq_get(0);

	/*
	 * cpu load accounting
	 * get highest load, total load and average load across all CPUs
	 */
	for_each_online_cpu(j) {
		unsigned int load;
		unsigned int idle_time, wall_time;
		cputime64_t cur_wall_time = 0, cur_idle_time;
		struct cpu_info_s *j_info;

		j_info = &per_cpu(hp_cpu_info, j);

		/* update both cur_idle_time and cur_wall_time */
		cur_idle_time = get_cpu_idle_time(j, &cur_wall_time);

		/* how much wall time has passed since last iteration? */
		wall_time = (unsigned int) cputime64_sub(cur_wall_time,
				j_info->prev_cpu_wall);
		j_info->prev_cpu_wall = cur_wall_time;

		/* how much idle time has passed since last iteration? */
		idle_time = (unsigned int) cputime64_sub(cur_idle_time,
				j_info->prev_cpu_idle);

		j_info->prev_cpu_idle = cur_idle_time;

		if (unlikely(!wall_time || wall_time < idle_time))
			continue;

		/* load is the percentage of time not spent in idle */
		load = 100 * (wall_time - idle_time) / wall_time;

		/* keep track of highest single load across all CPUs */
		if (load > max_load)
			max_load = load;
		if (load < min_load)
			min_load = load;
	}

	cpun = num_online_cpus();

	/*avg running task on each cpu*/
	g_iavruning = nr_running() * 100 / cpun;

	if (g_iavruning > TASK_THRESHOLD_H) {
		g_iavraddcnt ++ ;
		g_iavrsubcnt = 0 ;
	} else if (g_iavruning < TASK_THRESHOLD_L) {
		g_iavraddcnt = 0 ;
		g_iavrsubcnt ++ ;
	}

	/*min_load bigger than (95-(cpu number-1)*10)*/
	if (min_load > (DEFAULT_HOTPLUG_IN_LOAD - (cpun-1) * DEFAULT_DIFFERENTIAL)) {
		hotplug_in_count++;
		hotplug_out_count = 0;
	} else if ((cpun > CPU_NUM_WATERSHED)
		&& ((max_load + min_load) < DEFAULT_HOTPLUG_IN_LOAD
			|| min_load < cpun*DEFAULT_HOTPLUG_OUT_LOAD)) {
		/*max+min load lower than 95 or min load lower than cpun * 5*/
		hotplug_out_count++;
		hotplug_in_count = 0;
	} else if ((cpun <= CPU_NUM_WATERSHED) && (max_load + min_load) < DEFAULT_HOTPLUG_IN_LOAD) {
		/*max+min load lower than 95*/
		hotplug_out_count++;
		hotplug_in_count = 0;
	}

	if ((g_iavraddcnt >= DEFAULT_HOTPLUG_IN_RUN)
		&& (hotplug_in_count >= DEFAULT_HOTPLUG_IN_SAMPLING_PERIODS)
		&& (gcpu_num_limit.max > cpun)) {
#ifdef CONFIG_HOTPLUG_CPU
		cpu_up(num_online_cpus());
#endif
		hotplug_out_count = 0;
		hotplug_in_count = hotplug_in_count/2;
		g_iavraddcnt = 0 ;
		g_iavrsubcnt = 0 ;

		return;
	}

	if (((g_iavrsubcnt >= DEFAULT_HOTPLUG_OUT_RUN)
		&& (gcpu_num_limit.min < cpun))
		&& (((cpun > CPU_NUM_WATERSHED) && (hotplug_out_count >= DEFAULT_HOTPLUG_OUT_SAMPLING_PERIODS))
			|| ((cpun <= CPU_NUM_WATERSHED) && (hotplug_out_count >= DEFAULT_HOTPLUG_OUT_SAMPLING_PERIODS1)))) {

#ifdef CONFIG_HOTPLUG_CPU
		cpu_down(num_online_cpus()-1);
#endif
		hotplug_in_count  = 0;
		hotplug_out_count = 0;
		g_iavraddcnt = 0 ;
		g_iavrsubcnt = 0 ;

		return;
	}
}

static void do_dbs_timer(struct work_struct *work)
{
	int delay = usecs_to_jiffies(DEFAULT_SAMPLING_PERIOD);
	delay -= jiffies % delay;
	auto_hotplug();
	schedule_delayed_work_on(0, &k_work, delay);
}

static inline void dbs_timer_init(void)
{
	INIT_DELAYED_WORK_DEFERRABLE(&k_work, do_dbs_timer);

	if (RUNMODE_FLAG_NORMAL == runmode_is_factory())
		schedule_delayed_work_on(0, &k_work, usecs_to_jiffies(DEFAULT_SAMPLING_PERIOD));
}

static inline void dbs_timer_exit(void)
{
	cancel_delayed_work_sync(&k_work);
}

#endif

/********************cpu hotplug end**************************/

/******************** PM QOS NOTIFY***************************/

/**
*  cpu max number lock handle.
*/
static inline void cpumax_handle(void)
{
	int cpu_online_num = num_online_cpus();

	if (gcpu_num_limit.block != 0)
		return;

#ifdef CONFIG_HOTPLUG_CPU
	while (gcpu_num_limit.max < cpu_online_num) {
		cpu_down(cpu_online_num-1);
		cpu_online_num = num_online_cpus();
	}
#endif
}

static int cpunumbermax_notify(struct notifier_block *b, unsigned long l,
	void *v)
{
	int cpumax = 0;

	pr_debug("[%s] in=%lu\n", __func__, l);

	cpumax = (int)l;

	if (cpumax > num_possible_cpus())
		cpumax = num_possible_cpus();

	if (cpumax < 1)
		cpumax = 1;

	if (gcpu_num_limit.max != cpumax) {
		gcpu_num_limit.max = cpumax;
		/*check if we need to hotplug out cpu*/
		cpumax_handle();
	}

	return 0;
}

static struct notifier_block cpunumbermax_notifier = {
	.notifier_call = cpunumbermax_notify,
};

/***
* cpu min lock handle
*/
static inline void cpumin_handle(void)
{
	int cpu_online_num = num_online_cpus();

	if (gcpu_num_limit.block != 0)
		return;

#ifdef CONFIG_HOTPLUG_CPU
	while (gcpu_num_limit.min > cpu_online_num) {
		cpu_up(cpu_online_num);
		cpu_online_num = num_online_cpus();
	}
#endif
}

static int cpunumbermin_notify(struct notifier_block *b, unsigned long l,
	void *v)
{
	int cpumin = 0;

	pr_debug("[%s] in=%lu\n", __func__, l);

	cpumin = (int)l;

	if (cpumin > num_possible_cpus())
		cpumin = num_possible_cpus();

	if (cpumin < 1)
		cpumin = 1;

	if (gcpu_num_limit.min != cpumin) {
		gcpu_num_limit.min = cpumin;

		/*check if we need to hotplug cpu*/
		cpumin_handle();
	}

	return 0;
}

static struct notifier_block cpunumbermin_notifier = {
	.notifier_call = cpunumbermin_notify,
};

/**
*  cpu lock number handler.
**/
static inline void cpu_lock_handle(void)
{
	int cpu_online_num = num_online_cpus();

	if (gcpu_num_limit.block == 0) {
		cpumax_handle();
		cpumin_handle();

		/*reeable hotplug*/
		dbs_timer_init();
		return;
	}

	/*close hotplug*/
	dbs_timer_exit();

#ifdef CONFIG_HOTPLUG_CPU
	while (gcpu_num_limit.block != cpu_online_num) {

		if (gcpu_num_limit.block < cpu_online_num)
			cpu_down(cpu_online_num - 1);
		else if (gcpu_num_limit.block > cpu_online_num)
			cpu_up(cpu_online_num);

		cpu_online_num = num_online_cpus();
	}
#endif
}

static int cpunumberlock_notify(struct notifier_block *b, unsigned long l,
	void *v)
{
	int cpulock = 0;

	pr_debug("[%s] in=%lu\n", __func__, l);

	cpulock = (int)l;

	/*if the cpu number is larger than possible number,
	* we use the possible number.*/
	if (cpulock > num_possible_cpus())
		cpulock = num_possible_cpus();

	if (gcpu_num_limit.block != cpulock) {

		gcpu_num_limit.block = cpulock;

		/*check if we need to hotplug cpu*/
		cpu_lock_handle();
	}

	return 0;
}

static struct notifier_block cpunumberlock_notifier = {
	.notifier_call = cpunumberlock_notify,
};

static int cpunumbersafe_notify(struct notifier_block *b, unsigned long l,
	void *v)
{
	int isafe = 0;

	pr_debug("[%s] in=%lu\n", __func__, l);

	isafe = (int)l;

	if (isafe > num_possible_cpus())
		isafe = num_possible_cpus();

	if (isafe <= 1)
		isafe = 1;

	if (gcpu_num_limit.safe != isafe)
		gcpu_num_limit.safe = isafe;

	return 0;
}


static struct notifier_block cpunumbersafe_notifier = {
	.notifier_call = cpunumbersafe_notify,
};

static void k3hotplug_qos_remove_notifier(void)
{
	int ret = 0;

	QOS_REMOVE_NOTIFY(PM_QOS_CPU_NUMBER_LOCK, cpunumberlock_notifier);

	QOS_REMOVE_NOTIFY(PM_QOS_CPU_NUMBER_MAX, cpunumbermax_notifier);

	QOS_REMOVE_NOTIFY(PM_QOS_CPU_NUMBER_MIN, cpunumbermin_notifier);

	QOS_REMOVE_NOTIFY(PM_QOS_CPU_NUMBER_SAFE, cpunumbersafe_notifier);
}

static void k3hotplug_qos_add_notifier(void)
{
	int ret = 0;

	QOS_ADD_NOTIFY(PM_QOS_CPU_NUMBER_LOCK, cpunumberlock_notifier);

	QOS_ADD_NOTIFY(PM_QOS_CPU_NUMBER_MAX, cpunumbermax_notifier);

	QOS_ADD_NOTIFY(PM_QOS_CPU_NUMBER_MIN, cpunumbermin_notifier);

	QOS_ADD_NOTIFY(PM_QOS_CPU_NUMBER_SAFE, cpunumbersafe_notifier);

	return;

ERROR:
	k3hotplug_qos_remove_notifier();

}

/******************** PM QOS NOTIFY***************************/


void k3hotplug_pm_qos_add(void)
{
	int i = 0;
	int ilength = sizeof(pm_qos_list)/sizeof(struct pm_qos_lst);

	for (i = 0; i < ilength; i++) {
		pm_qos_add_request(pm_qos_list[i].lst, pm_qos_list[i].qos_class,
			pm_qos_list[i].dvalue);
	}
}

void k3hotplug_pm_qos_remove(void)
{
	int i = 0;
	int ilength = sizeof(pm_qos_list)/sizeof(struct pm_qos_lst);

	for (i = 0; i < ilength; i++)
		pm_qos_remove_request(pm_qos_list[i].lst);
}

static int cpufreq_pm_notify(struct notifier_block *nfb, unsigned long action, void *ignored)
{
	pr_info("%s %ld +\n", __func__, action);
	switch (action)
	{
	case PM_SUSPEND_PREPARE:
	case PM_HIBERNATION_PREPARE:
		dbs_timer_exit();
		break;
	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
		dbs_timer_init();
		break;
	default:
		pr_warn("%s %d %ld\n", __func__, __LINE__, action);
		break;
	}

	pr_info("%s -\n", __func__);

	return NOTIFY_OK;
}

static struct notifier_block hotplug_pm_nb = {
	.notifier_call = cpufreq_pm_notify,
};

static int cpufreq_governor_dbs(struct cpufreq_policy *policy,
				   unsigned int event)
{
	unsigned int cpu = policy->cpu;
	struct cpu_dbs_info_s *this_dbs_info = NULL;
	int rc = 0;

	this_dbs_info = &per_cpu(hp_cpu_dbs_info, cpu);

	switch (event) {
	case CPUFREQ_GOV_START:
		if ((!cpu_online(cpu)) || (!policy->cur))
			return -EINVAL;

		mutex_lock(&dbs_mutex);
		dbs_enable++;

		this_dbs_info->cpu = cpu;
		this_dbs_info->freq_table = cpufreq_frequency_get_table(cpu);

		/*
		 * Start the timerschedule work, when this governor
		 * is used for first time
		 */
		if (dbs_enable == 1) {

			dbs_timer_init();

			rc = sysfs_create_group(cpufreq_global_kobject,
						&dbs_attr_group);
			if (rc) {
				pr_err("[%s] %d rc=%x\n", __func__, __LINE__, rc);
				mutex_unlock(&dbs_mutex);
				return rc;
			}

			k3hotplug_qos_add_notifier();
			k3hotplug_pm_qos_add();
			register_pm_notifier(&hotplug_pm_nb);
		}

		mutex_unlock(&dbs_mutex);

		break;

	case CPUFREQ_GOV_STOP:

		mutex_lock(&dbs_mutex);

		dbs_enable--;

		if (!dbs_enable) {

			dbs_timer_exit();

			k3hotplug_pm_qos_remove();
			k3hotplug_qos_remove_notifier();

			sysfs_remove_group(cpufreq_global_kobject,
					   &dbs_attr_group);

			unregister_pm_notifier(&hotplug_pm_nb);
		}

		mutex_unlock(&dbs_mutex);

		break;

	case CPUFREQ_GOV_LIMITS:
		if (cpu == 0) {
			mutex_lock(&dbs_mutex);
			pm_qos_update_request(&g_cpumaxlimits, policy->max);
			pm_qos_update_request(&g_cpuminlimits, policy->min);
			mutex_unlock(&dbs_mutex);
		}
		break;
	default:
		pr_err("[%s] defualt run=%x\n", __func__, event);
		break;
	}
	return 0;
}

static int hotplug_reboot_notify(struct notifier_block *nb,
				unsigned long code, void *unused)
{
	if ((code == SYS_RESTART) || (code == SYS_POWER_OFF) ||
		(code == SYS_HALT)) {
		printk("hotplug_reboot_notify code 0x%lx stop hotplug\n", code);
		dbs_timer_exit();
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static struct notifier_block hotplug_reboot_nb = {
	.notifier_call	= hotplug_reboot_notify,
	.next		= NULL,
	.priority	= INT_MAX, /* before any real devices */
};

static int __init cpufreq_gov_dbs_init(void)
{
	int err = 0;
	dbs_enable = 0;

	err = cpufreq_register_governor(&cpufreq_gov_k3hotplug);
	if (err)
		pr_err("cpufreq_gov_k3hotplug register err=%d\n", err);

	register_reboot_notifier(&hotplug_reboot_nb);
	return err;
}

static void __exit cpufreq_gov_dbs_exit(void)
{
	unregister_reboot_notifier(&hotplug_reboot_nb);
	cpufreq_unregister_governor(&cpufreq_gov_k3hotplug);
}

MODULE_AUTHOR("s00107748");
MODULE_DESCRIPTION("'cpufreq_k3hotplug' - cpufreq governor for hotplug");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_K3HOTPLUG
fs_initcall(cpufreq_gov_dbs_init);
#else
module_init(cpufreq_gov_dbs_init);
#endif
module_exit(cpufreq_gov_dbs_exit);
