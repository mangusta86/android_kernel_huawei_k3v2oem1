/*
 * arch/arm/mach-tegra/eprj_t3_cpumanager.c
 *
 * EternityProject CPU Manager for Tegra3 CPUs:
 * A precise manager with hotplug and cluster
 * management capabilities.
 *
 * Copyright (c) 2012-2013, EternityProject Developers
 *
 * Angelo G. Del Regno <kholk11@gmail.com>
 * Joel Low <joel@joelsplace.sg>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/cpu.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/hardirq.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <mach/eternityproject.h>

#include "../pm.h"
#include "../cpu-tegra.h"
#include "../clock.h"

#define INITIAL_STATE               TEGRA_HP_IDLE
#define UP2G0_DELAY_MS              50
#define UP2Gn_DELAY_MS              100
/* Don't put CPU down prior 3x eprj_scheduling samplings */
#ifdef CONFIG_MACH_OUYA
#define DOWN_DELAY_MAX_MS           960
#else
#define DOWN_DELAY_MAX_MS           720
#endif
#define TRANSITION_AWAKE_DELAY_MS   250
#define TRANSITION_SUSPEND_DELAY_MS 2000

#ifdef CONFIG_MACH_OUYA
#define ENABLED_CLUSTERS	    EPRJ_TEGRA3_G_CLUSTER
#else
#define ENABLED_CLUSTERS 	    EPRJ_TEGRA3_G_CLUSTER | EPRJ_TEGRA3_LP_CLUSTER;
#endif

#define MESSAGE_TAG                 "[eprj_cpumanager] "

static struct mutex *tegra3_cpu_lock = NULL;

static struct workqueue_struct *hotplug_wq;
static struct delayed_work hotplug_work;

static uint enabled_clusters;
module_param(enabled_clusters, uint, 0644);

static unsigned long up2gn_delay;
static unsigned long up2g0_delay;
static unsigned long down_delay;
module_param(up2gn_delay, ulong, 0644);
module_param(up2g0_delay, ulong, 0644);
module_param(down_delay, ulong, 0644);

static unsigned int lp_top_freq;
static unsigned int hybrid_bottom_freq;
static unsigned int g_bottom_freq;
module_param(lp_top_freq, uint, 0644);
module_param(hybrid_bottom_freq, uint, 0644);
module_param(g_bottom_freq, uint, 0644);

static int mp_overhead = 10;
module_param(mp_overhead, int, 0644);

static struct clk *cpu_clk;
static struct clk *cpu_g_clk;
static struct clk *cpu_lp_clk;

static unsigned int transition_request_threshold_awake;
static unsigned int transition_request_threshold_suspend;

static unsigned int last_cpu_freq;
static unsigned long last_cpu_freq_time;
static unsigned long last_change_time;
static u64 last_sleep_time;


/* Stores whether we are currently in deep sleep. */
static bool in_deep_sleep;

/* Stores whether we are currently in early suspend mode. */
static bool in_suspend;

/* Stores whether we have currently initialized CPU Manager.
   TODO: Find a cleaner way to solve this. */
static bool initialized;

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend early_suspend;
#endif

static struct {
	cputime64_t time_up_total;
	u64 last_update;
	unsigned int up_down_count;
} hp_stats[CONFIG_NR_CPUS + 1];	/* Append LP CPU entry at the end */

static void hp_init_stats(void)
{
	int i;
	u64 cur_jiffies = get_jiffies_64();

	for (i = 0; i <= CONFIG_NR_CPUS; i++) {
		hp_stats[i].time_up_total = 0;
		hp_stats[i].last_update = cur_jiffies;

		hp_stats[i].up_down_count = 0;
		if (is_lp_cluster()) {
			if (i == CONFIG_NR_CPUS)
				hp_stats[i].up_down_count = 1;
		} else {
			if ((i < nr_cpu_ids) && cpu_online(i))
				hp_stats[i].up_down_count = 1;
		}
	}
}

static void hp_stats_update(unsigned int cpu, bool up)
{
	u64 cur_jiffies = get_jiffies_64();
	bool was_up = hp_stats[cpu].up_down_count & 0x1;

	if (was_up)
		hp_stats[cpu].time_up_total += cur_jiffies - hp_stats[cpu].last_update;

	if (was_up != up) {
		hp_stats[cpu].up_down_count++;
		if ((hp_stats[cpu].up_down_count & 0x1) != up) {
			/* FIXME: sysfs user space CPU control breaks stats */
			pr_err(MESSAGE_TAG "Tegra Hotplug stats out of sync "
				"with %s CPU%d\n",
			       (cpu < CONFIG_NR_CPUS) ? "G" : "LP",
			       (cpu < CONFIG_NR_CPUS) ?  cpu : 0);
			hp_stats[cpu].up_down_count ^=  0x1;
		}
	}
	hp_stats[cpu].last_update = cur_jiffies;
}


enum {
	/* Hotplugging is disabled. */
	TEGRA_HP_DISABLED = 0,

	/* Nothing to do for the Hotplug implementation. This is when the active
	 * cluster is LP where no CPU hotplugging is possible. */
	/* TODO: remove? */
	TEGRA_HP_IDLE,

	/* This is when the CPU composition needs to be actively determined at
	 * runtime. */
	TEGRA_HP_ACTIVE,

	/* This is when we are in deep sleep and all transition decisions have
	 * been delegated to the Tegra PM framework. */
	TEGRA_HP_SLEEP
};
static int hp_state;

static int hp_state_set(const char *arg, const struct kernel_param *kp)
{
	int ret = 0;
	int old_state;

	if (!tegra3_cpu_lock)
		return ret;

	mutex_lock(tegra3_cpu_lock);

	old_state = hp_state;
	ret = param_set_bool(arg, kp);	/* set idle or disabled only */

	if (ret == 0) {
		if ((hp_state == TEGRA_HP_DISABLED) &&
		    (old_state != TEGRA_HP_DISABLED)) {
			mutex_unlock(tegra3_cpu_lock);
			cancel_delayed_work_sync(&hotplug_work);
			mutex_lock(tegra3_cpu_lock);
			pr_info(MESSAGE_TAG "Tegra auto-hotplug disabled\n");
		} else if (hp_state != TEGRA_HP_DISABLED) {
			if (old_state == TEGRA_HP_DISABLED) {
				pr_info(MESSAGE_TAG "Tegra auto-hotplug enabled\n");
				hp_init_stats();
			}
			/* catch-up with governor target speed */
			tegra_cpu_set_speed_cap(NULL);
		}
	} else
		pr_warn(MESSAGE_TAG "%s: unable to set tegra hotplug state %s\n",
				__func__, arg);

	mutex_unlock(tegra3_cpu_lock);
	return ret;
}

static int hp_state_get(char *buffer, const struct kernel_param *kp)
{
	return param_get_int(buffer, kp);
}

static struct kernel_param_ops tegra_hp_state_ops = {
	.set = hp_state_set,
	.get = hp_state_get,
};
module_param_cb(auto_hotplug, &tegra_hp_state_ops, &hp_state, 0644);


enum {
	/* Brings one more CPU core on-line */
	TEGRA_CPU_UPCORE,

	/* Keeps CPU core composition unchanged */
	TEGRA_CPU_FAIR,

	/* Remove CPU core off-line */
	TEGRA_CPU_DOWNCORE,
};

/*
 * These only apply to core transitions when we are in G.
 *
 * There are two ladders here for hysteresis. We do not want to offline
 * and online a CPU repeatedly. Thus, the down thresholds are *lower*
 * than the corresponding up threshold.
 *
 * NOTE: The CONFIG_NR_CPUS'th value isn't being used with the same logic of
 *       the first 3, therefore, it could also be set at UINT_MAX.
 *       On the other hand, the final value is the runqueue threshold for
 *       transitions from LP to G
 */
#ifdef CONFIG_MACH_OUYA
static const unsigned int nr_run_thresholds[CONFIG_NR_CPUS + 1] = {
	190, 220, 280, UINT_MAX, 300
};
static const unsigned int nr_down_run_thresholds[CONFIG_NR_CPUS] = {
	0, 70, 100, 150
};
#else
static const unsigned int nr_run_thresholds[CONFIG_NR_CPUS + 1] = {
	240, 350, 480, UINT_MAX, 700
};
static const unsigned int nr_down_run_thresholds[CONFIG_NR_CPUS] = {
	0, 180, 280, 380
};
#endif

/*
 * This is the brains of the hotplug implementation.
 *
 * This function is called by tegra_auto_hotplug_work_func to determine if
 * the CPU composition needs to be changed.
 */
static int tegra_auto_hotplug_decider(void)
{
	unsigned int nr_cpus = num_online_cpus();
	unsigned int max_cpus = pm_qos_request(PM_QOS_MAX_ONLINE_CPUS) ? : CONFIG_NR_CPUS;
	unsigned int min_cpus = pm_qos_request(PM_QOS_MIN_ONLINE_CPUS);
	unsigned int avg_nr_run = eprj_get_nr_run_avg();
	unsigned int nr_run;

	if (is_lp_cluster()) {
		/* This is an unrolled loop for LP, since the index is
		   at the end of the runqueue thresholds. */
		nr_run = 1;
		if (avg_nr_run > nr_run_thresholds[CONFIG_NR_CPUS]) {
			nr_run = 2;
		}
	} else {
		/* First use the up thresholds to see if we need to bring CPUs online. */
		pr_debug(MESSAGE_TAG "Current core count max runqueue: %d\n",
			 nr_run_thresholds[nr_cpus - 1]);
		for (nr_run = nr_cpus; nr_run < CONFIG_NR_CPUS; ++nr_run) {
			if (avg_nr_run <= nr_run_thresholds[nr_run - 1])
				break;
		}

		/* Then use the down thresholds to see if we can offline CPUs.
		   This looks counter-intuitive, but it does work. The first loop
		   handles the case where there are more things running than our up
		   thresholds. If we have now too many CPUs online for too few tasks,
		   the loop above would be a no-op, and we will fall through to this
		   down threshold comparison loop.*/
		for ( ; nr_run > 1; --nr_run) {
			if (avg_nr_run >= nr_down_run_thresholds[nr_run - 1]) {
				/* We have fewer things running than our down threshold.
				   Use one less CPU. */
				break;
			}
		}
	}

	/* Consider when to take a CPU offline. We either need to have G
	   disabled, or at least have more than minimum online currently,
	   and either of: */
	if (enabled_clusters == EPRJ_TEGRA3_LP_CLUSTER ||
		(nr_cpus > min_cpus && (

		/* More CPUs than policy allows */
		(nr_cpus > max_cpus) ||

		/* At least one extra idle CPU */
		(tegra_count_slow_cpus(hybrid_bottom_freq) >= 2) ||

		/* Runqueue has been emptied */
		(nr_run < nr_cpus) ||

		/* To satisfy EDP equation. */
		tegra_cpu_edp_favor_down(nr_cpus, mp_overhead)))
	) {
		return TEGRA_CPU_DOWNCORE;
	}

	/* Consider when we may want to keep CPU composition unchanged. We need
	   to have at least the minimum number of CPUs as required by policy, and: */
	if ((nr_cpus >= min_cpus) && (
		/* Already at maximum CPUs allowed by policy. */
		(nr_cpus == max_cpus) ||

		/* Enough CPUs to process runqueue. */
		(nr_run <= nr_cpus) ||

		/* Satisfy EDP equation. */
		(!tegra_cpu_edp_favor_up(nr_cpus, mp_overhead)))
	) {
		return TEGRA_CPU_FAIR;
	}

	/* Otherwise, we have to online one more CPU */
	return TEGRA_CPU_UPCORE;
}

/*
 * This decides whether we should be on G or on LP. This can be called
 * from any cluster state.
 *
 * The problem this is supposed to fix is that if we have a min frequency
 * within the Hybrid region, then we will be switching between G and LP
 * ad nauseum.
 *
 * \return Returns true if we should be on G, or false if we should be
 *         on LP.
 */
static bool tegra_auto_hotplug_cluster_decider(void)
{
	/* These variables store the last time cpufreq requested a low
	   frequency. */
	static unsigned int low_request_freq_freq = 0;
	static unsigned int low_request_freq_time = 0;

	/* If we disabled switching, forcing a CPU cluster to be used, */
	if (enabled_clusters !=
		(EPRJ_TEGRA3_G_CLUSTER | EPRJ_TEGRA3_LP_CLUSTER)) {
		/* Reset our statistics. */
		low_request_freq_freq = 0;
		low_request_freq_time = 0;

		/* If we are forced to use LP, make it so. */
		if (enabled_clusters == EPRJ_TEGRA3_LP_CLUSTER) {
			return false;
		} else {
			/* Also if we are forced to use G, don't bother. */
			return true;
		}
	}

	/* Or, if we have a floor to the number of cores online at any one
	   time, */
	if (pm_qos_request(PM_QOS_MIN_ONLINE_CPUS) > 1) {
		return true;
	}

	/* Otherwise, both G and LP are enabled. We handle the trivial case,
	   when the CPU frequency is at the bottom of the G ladder. Switch to
	   LP. */
	if (last_cpu_freq <= g_bottom_freq) {
		low_request_freq_freq = 0;
		low_request_freq_time = 0;
		return false;
	}
	/* If cpufreq requested a frequency lower than the lower limit of LP,
	   and we have waited for the Awake transition delay, switch to LP
	   regardless of runqueue because that means all the many tasks are
	   simple. */
	else if (last_cpu_freq < hybrid_bottom_freq) {
		unsigned int transition_request_threshold = in_suspend ?
			transition_request_threshold_suspend :
			transition_request_threshold_awake;
		unsigned long now = jiffies;

		if (low_request_freq_time == 0) {
			low_request_freq_freq = last_cpu_freq;
			low_request_freq_time = now;
		}
		if (low_request_freq_freq < last_cpu_freq) {
			/* If we arrive here the frequency requested is still within
			   the LP envelope, but higher than before. Restart the timer
			   since load is no longer going down. */
			low_request_freq_freq = last_cpu_freq;
			low_request_freq_time = now;
		}

		if ((low_request_freq_time != 0 &&
			now - low_request_freq_time >= transition_request_threshold)) {
			/* cpufreq requested it to be a low frequency. Change to LP. */
			low_request_freq_freq = 0;
			low_request_freq_time = 0;
			return false;
		}
	}

	return !(num_online_cpus() == 1 && (
			tegra_cpu_lowest_speed() <= lp_top_freq
		)
	);
}

/*
 * This prepares the CPU cluster for a transition by reparenting the clocks.
 *
 * This also updates transition statistics.
 *
 * \remark Remember to lock tegra3_cpu_lock, otherwise we may end up with a race
 *         condition.
 * \return Returns success (0) or negative errno.
 */
static int tegra_auto_hotplug_prepare_cluster_transition(struct clk* new_parent)
{
	bool new_parent_is_lp = new_parent == cpu_lp_clk;
	int result;

	if (new_parent_is_lp) {
		/* Make sure our cluster switch will succeed. First, offline
		   all non-boot CPUs. */
		WARN(in_interrupt() || in_atomic(),
		     "cannot transition clusters in atomic or interrupt context");
		while (num_online_cpus() > 1) {
			mutex_unlock(tegra3_cpu_lock);
			result = disable_nonboot_cpus();
			mutex_lock(tegra3_cpu_lock);

			if (result) {
				pr_err(MESSAGE_TAG "%s: cannot shut down non-boot "
					"CPUs (err=%d)\n",
					__func__, result);
				return result;
			}

			pr_info(MESSAGE_TAG "%s: waiting for cpus to go offline: "
				"%d more\n", __func__, num_online_cpus() - 1);
			msleep(up2gn_delay);
		}

		/* Then cap the CPU frequency. */
		if (tegra_cpu_lowest_speed() > lp_top_freq) {
			tegra_update_cpu_speed(lp_top_freq);
		}
	}

	result = clk_set_parent(cpu_clk, new_parent);
	if (!result) {
		hp_stats_update(CONFIG_NR_CPUS, new_parent_is_lp);
		hp_stats_update(0, !new_parent_is_lp);
	}

	return result;
}

/*
 * This function manages CPU transitions when in G mode.
 *
 * \remark tegra3_cpu_lock must be held before this function is called.
 * \return Returns the number of jiffies to wait before the work function
 *         should be triggered again.
 */
static unsigned long __cpuinit tegra_auto_hotplug_work_func_g(void)
{
	bool up = false;
	int action = 0;
	unsigned int cpu = nr_cpu_ids;
	unsigned int online_cpus = num_online_cpus();
	unsigned long current_down_delay = online_cpus == 1 ?
		down_delay / CONFIG_NR_CPUS : /* Only one core in G. Try LP. */
		down_delay / (online_cpus - 1); /* Multiple CPUs online. */
	unsigned long now = jiffies;

	/* So we are now in G mode, and we need to decide what to do with our
	   core composition. */
	action = tegra_auto_hotplug_decider();
	switch (action) {
		case TEGRA_CPU_UPCORE:
			cpu = cpumask_next_zero(0, cpu_online_mask);
			if (cpu < nr_cpu_ids)
				up = true;
			break;

		case TEGRA_CPU_DOWNCORE:
			cpu = tegra_get_slowest_cpu_n();
			if (cpu < nr_cpu_ids)
				up = false;
			break;

		case TEGRA_CPU_FAIR:
			break;

		default:
			pr_err(MESSAGE_TAG "%s: invalid hotplug decision %d\n",
				__func__, action);
			WARN(1, KERN_ERR "Hotplug may be broken from now on!");
			break;
	}

	/* Do not perform hotplugging too often. */
	if (!up && ((now - last_change_time) < current_down_delay))
		cpu = nr_cpu_ids;

	if (cpu < nr_cpu_ids) {
		/* Record statistics. */
		last_change_time = now;
		hp_stats_update(cpu, up);

		/* Perform the hotplugging proper. */
		mutex_unlock(tegra3_cpu_lock);
		if (up)
			cpu_up(cpu);
		else
			cpu_down(cpu);
		mutex_lock(tegra3_cpu_lock);

		/* If we just put another CPU online, reduce the frequency of
		   the CPU since we have increased the overall computational
		   power of the processor. */
		if (up) {
			pr_debug(MESSAGE_TAG "Reducing CPU speed from %lu to %lu (%u cores)\n",
				tegra_cpu_lowest_speed(), tegra_cpu_lowest_speed() *
				(online_cpus + 1) / (online_cpus + 2), online_cpus
			);
			tegra_update_cpu_speed(tegra_cpu_lowest_speed() *
				(online_cpus + 1) / (online_cpus + 2));
		}
	}
	/* Decide if we need and if we're ready to shift to LP */
	else if (action != TEGRA_CPU_UPCORE &&
		((now - last_change_time) >= current_down_delay) &&
		!tegra_auto_hotplug_cluster_decider()) {
		pr_debug(MESSAGE_TAG "%s Transitioning to LP\n", __func__);

		last_change_time = now;
		if (!tegra_auto_hotplug_prepare_cluster_transition(cpu_lp_clk)) {
			/* catch-up with governor target speed */
			tegra_cpu_set_speed_cap(NULL);
		}
	}

	return up2gn_delay;
}

/*
 * This function manages CPU transitions when in LP mode.
 *
 * \remark tegra3_cpu_lock must be held before this function is called.
 * \return Returns the number of jiffies to wait before the work function
 *         should be triggered again.
 */
static unsigned long __cpuinit tegra_auto_hotplug_work_func_lp(void)
{
	static unsigned int high_request_freq_freq = 0;
	static unsigned int high_request_freq_time = 0;
	unsigned int now = jiffies;

	/* Are we forced to use LP? */
	if (enabled_clusters == EPRJ_TEGRA3_LP_CLUSTER) {
		return up2g0_delay * 4;
	}

	/* Are we forced to use G? */
	if (enabled_clusters == EPRJ_TEGRA3_G_CLUSTER) {
		last_change_time = now;
		if (!tegra_auto_hotplug_prepare_cluster_transition(cpu_g_clk)) {
			/* catch-up with governor target speed */
			tegra_cpu_set_speed_cap(NULL);
		}

		/* Reset our statistics. */
		high_request_freq_freq = 0;
		high_request_freq_time = 0;

		/* Schedule an analysis since we are going to G. */
		return up2gn_delay;
	}

	/* Are we in the hybrid frequency range? */
	if (last_cpu_freq > hybrid_bottom_freq) {
		/* Many slightly busy threads */
		if (tegra_auto_hotplug_decider() == TEGRA_CPU_UPCORE) {
			pr_debug(MESSAGE_TAG "%s: Transitioning to G cluster (runqueue: %u)\n",
				__func__, eprj_get_nr_run_avg());

			last_change_time = now;
			if (!tegra_auto_hotplug_prepare_cluster_transition(cpu_g_clk)) {
				unsigned int cpu;

				/* catch-up with governor target speed */
				tegra_cpu_set_speed_cap(NULL);

				/* force secondary CPU to be started */
				cpu = cpumask_next_zero(0, cpu_online_mask);
				hp_stats_update(cpu, true);
				mutex_unlock(tegra3_cpu_lock);
				cpu_up(cpu);
				mutex_lock(tegra3_cpu_lock);
			}

			/* Reset our statistics. */
			high_request_freq_freq = 0;
			high_request_freq_time = 0;

			/* Schedule an analysis since we are going to G. */
			return up2gn_delay;
		}
	}

	/* One busy thread. */
	if (last_cpu_freq > lp_top_freq) {
		/* The number of jiffies the CPU must be at the higher frequency
		   before we switch to G. */
		unsigned int transition_request_threshold = in_suspend ?
			transition_request_threshold_suspend :
			transition_request_threshold_awake;

		/* See what the runqueue average has been during the last sampling
		   interval. */
		unsigned int runqueue_average = eprj_get_nr_run_avg();

		/* Track how many jiffies the CPU has been requested to be at
		   the same high or higher frequency (to prevent spikes from
		   unnecessarily switching) */
		if (high_request_freq_time == 0) {
			high_request_freq_freq = last_cpu_freq;
			high_request_freq_time = now;
		}
		if (high_request_freq_freq > last_cpu_freq) {
			/* If we arrive here the frequency requested is higher than
			   the LP envelope, but lower than before. Restart the timer
			   since load is going down. */
			high_request_freq_freq = last_cpu_freq;
			high_request_freq_time = now;
		}

		pr_debug(MESSAGE_TAG "%s: Transition opportunity: %u / %u ms (%u start) ago "
			"(require %u ms); freq %u\n", __func__,
			high_request_freq_freq,
			jiffies_to_msecs(now - high_request_freq_time),
			high_request_freq_time,
			jiffies_to_msecs(transition_request_threshold),
			last_cpu_freq);

		/* Transition to G if we are called with the higher frequency longer
		   than our threshold. */
		if ((high_request_freq_time != 0 &&
			now - high_request_freq_time >= transition_request_threshold)) {
			/* Transition to the G cluster */
			pr_debug(MESSAGE_TAG "%s: Transitioning to G cluster (%u kHz requested, "
				"%u ms ago, runqueue %u)\n", __func__, last_cpu_freq,
				jiffies_to_msecs(now - high_request_freq_time), runqueue_average);

			last_change_time = now;
			if (!tegra_auto_hotplug_prepare_cluster_transition(cpu_g_clk)) {
				/* catch-up with governor target speed */
				tegra_cpu_set_speed_cap(NULL);
			}

			/* Reset our statistics. */
			high_request_freq_freq = 0;
			high_request_freq_time = 0;

			/* Schedule an analysis since we are going to G. */
			return up2gn_delay;
		} else {
			/* Try to schedule after the next CPU transition time.
			   The conditional is required because the number of jiffies
			   may wrap around 0 because of overflow. */
			return now < last_cpu_freq_time ?
				up2g0_delay :
				up2g0_delay + last_cpu_freq_time - now;
		}
	} else {
		if (tegra_auto_hotplug_cluster_decider()) {
			/* Forced transition to the G cluster */
			pr_info(MESSAGE_TAG "%s: Forced transition to G cluster\n", __func__);

			last_change_time = now;
			if (!tegra_auto_hotplug_prepare_cluster_transition(cpu_g_clk)) {
				/* catch-up with governor target speed */
				tegra_cpu_set_speed_cap(NULL);
			}
		}

		/* We time how long since the cpufreq driver tried to scale us
		   into G territory. If we come here, we need to reset the timer otherwise
		   the next time we cross the boundary, the threshold for core switching
		   is reduced. */
		high_request_freq_freq = 0;
		high_request_freq_time = 0;
	}

	return in_suspend ? up2g0_delay * 4 : up2g0_delay;
}

/*
 * This is the main machinery for the hotplug implementation.
 *
 * This function will call itself at the end of approximately every up2gn_delay
 * jiffies, even when in LP, because there are a range of frequencies where
 * one-core operation is handled by LP, but with a possibility of multi-core
 * operation (that will be the range between g_bottom_freq and lp_top_freq)
 *
 * If the hp_state variable is set to TEGRA_HP_DISABLED (user-disabled) this
 * function will also no longer run and will depend on
 * tegra_auto_hotplug_governor to change the hp_state and to trigger this
 * work item again.
 *
 * The decision to power up or power down a CPU is made by
 * tegra_auto_hotplug_decider.
 */
static void __cpuinit tegra_auto_hotplug_work_func(struct work_struct *work)
{
	unsigned long delay = 0;

	/* Bail out if we are idle or disabled. */
	mutex_lock(tegra3_cpu_lock);
	{
		if (hp_state == TEGRA_HP_IDLE || hp_state == TEGRA_HP_SLEEP) {
			//Do nothing.
			goto out;
		}

		if (hp_state == TEGRA_HP_DISABLED) {
			pr_warn(MESSAGE_TAG "%s: Hotplug disable happened without "
				"stopping worker.\n", __func__);

			goto out;
		}
	}

	if (is_lp_cluster()) {
		delay = tegra_auto_hotplug_work_func_lp();
	} else {
		delay = tegra_auto_hotplug_work_func_g();
	}
	queue_delayed_work(hotplug_wq, &hotplug_work, delay);

out:
	mutex_unlock(tegra3_cpu_lock);
	return;
}

/*
 * This functions listens for suspend events from cpu-tegra.c. When we are in
 * suspend, disable the hotplugging, since the Tegra PM component will take
 * over cluster management.
 *
 * \remark tegra3_cpu_lock must already be locked when we are called. Because
 *         this function WILL affect CPU composition, DO NOT call this from an
 *         interrupt or atomic context.
 * \param[in] suspend True if we are entering suspend; False when exiting
 *                    suspend.
 * \return Zero if the suspend operation succeeded. A standard Linux error
 *         code otherwise.
 */
int tegra_auto_hotplug_suspend(bool suspend)
{
	int retval = 0;

	/* If we are currently disabled, nothing else to do. */
	if (hp_state == TEGRA_HP_DISABLED) {
		goto out;
	}

	if (suspend) {
		/* If we are entering deep sleep, we need to give up ownership of
		   CPU hotplugging to the Tegra PM framework. */
		int old_hp_state = hp_state;
		hp_state = TEGRA_HP_SLEEP;

		/* Release our CPU lock so that should the hotplug worker run while
		   we are cancelling it, we won't deadlock. */
		mutex_unlock(tegra3_cpu_lock);
		cancel_delayed_work_sync(&hotplug_work);
		mutex_lock(tegra3_cpu_lock);

		/* Nonetheless, the framework assumes that we are already in LP. So
		   switch to LP. */
		retval = tegra_auto_hotplug_prepare_cluster_transition(cpu_lp_clk);
		if (!retval) {
			/* catch-up with governor target speed */
			tegra_cpu_set_speed_cap(NULL);

			/* set the time we entered suspend; we will use it to maintain
			   hotplug stats by excluding time for deep sleep. */
			last_sleep_time = get_jiffies_64();

			pr_info(MESSAGE_TAG "%s: in suspend: %u cpus, is_lp=%u\n",
				__func__, num_online_cpus(), is_lp_cluster());
		} else {
			/* We could not suspend: we need to be in LP but there are
			   we can't make it in. Deny the suspend, and re-enable the
			   worker. */
			hp_state = old_hp_state;
			queue_delayed_work(hotplug_wq, &hotplug_work, up2g0_delay);

			pr_err(MESSAGE_TAG "%s: could not enter suspend: %u cpus, "
				"is_lp=%u\n", __func__, num_online_cpus(),
				is_lp_cluster());
			return retval;
		}
	} else {
		/* If we are waking up, re-enable hotplug. This works because if we
		   come here, the state was either HP_STATE_IDLE or HP_STATE_ACTIVE */
		hp_state = TEGRA_HP_ACTIVE;

		/* Get the governor workqueue running again. */
		queue_delayed_work(hotplug_wq, &hotplug_work, up2g0_delay);

		/* exclude the time we spent in suspend; we will use it to maintain
		   hotplug stats by excluding time for deep sleep. */
		hp_stats[CONFIG_NR_CPUS].last_update += get_jiffies_64() - last_sleep_time;
		last_sleep_time = 0;
	}

out:
	in_deep_sleep = suspend;
	if (in_deep_sleep && !in_suspend) {
		in_suspend = true;
	}

	return retval;
}

/*
 * This functions listens for early suspend events. When we are in early
 * suspend, we are to make LP => G transitions less likely.
 *
 * \param[in] h Unused.
 */
static void tegra_auto_hotplug_early_suspend(struct early_suspend *h)
{
	in_suspend = true;
	pr_info(MESSAGE_TAG "%s: entering suspend: %u cpus, is_lp=%u\n",
		__func__, num_online_cpus(), is_lp_cluster());
}

static void tegra_auto_hotplug_late_resume(struct early_suspend *h)
{
	in_suspend = false;
	pr_info(MESSAGE_TAG "%s: leaving suspend: %u cpus, is_lp=%u\n",
		__func__, num_online_cpus(), is_lp_cluster());
}

/*
 * This function caches the last requested CPU frequency for
 * tegra_auto_hotplug_work_func. It also starts the work queue again
 * when we transition from TEGRA_HP_DISABLED to any other state.
 *
 * This is called from cpu-tegra.c in response to changes in CPU frequency.
 *
 * DO NOT lock the tegra3_cpu_lock. It is already locked when we are called.
 * The kernel will not boot (by deadlocking) if we lock this mutex.
 *
 * \param[in] cpu_freq The frequency of the highest-frequency cpu
 * \param[in] suspend  True if the system is going into suspend.
 */
void tegra_auto_hotplug_governor(unsigned int cpu_freq)
{
	pr_debug(MESSAGE_TAG "%s: %u / %d / %d\n", __func__,
		cpu_freq, is_lp_cluster(), hp_state);
	/* If we are disabled, don't bother. */
	if (hp_state == TEGRA_HP_DISABLED)
		return;

	/* Update the frequency which is currently being requested. */
	last_cpu_freq = cpu_freq;
	last_cpu_freq_time = jiffies;

	if (hp_state == TEGRA_HP_IDLE) {
		/* We need to run our code to handle CPU transitions. Start it. */
		hp_state = TEGRA_HP_ACTIVE;
		queue_delayed_work(hotplug_wq, &hotplug_work, up2g0_delay);
	}
}

static int eprj_cpuman_probe(struct platform_device *pdev)
{
	pr_debug(MESSAGE_TAG "Probing...\n");

	/* Do not allow probing to succeed if we have not been initialized
	   beforehand. */
	if (!initialized) {
		pr_warn(MESSAGE_TAG "Blocking probe: not initialized.\n");
		return -EINVAL;
	}

	INIT_DELAYED_WORK(&hotplug_work, tegra_auto_hotplug_work_func);
	hp_state = INITIAL_STATE;
	hp_init_stats();

	pr_info(MESSAGE_TAG "EternityProject Tegra 3 CPU Manager initialized: %s\n",
		(hp_state == TEGRA_HP_DISABLED) ? "disabled" : "enabled");

	pr_debug(MESSAGE_TAG "Probe successful.\n");
	return 0;
}

int __cpuinit tegra_auto_hotplug_init(struct mutex *cpu_lock)
{
	/*
	 * Not bound to the issuer CPU (=> high-priority), has rescue worker
	 * task, single-threaded, freezable.
	 */
	hotplug_wq = alloc_workqueue(
		"cpu-tegra3", WQ_UNBOUND | WQ_RESCUER | WQ_FREEZABLE, 1);
	if (!hotplug_wq)
		return -ENOMEM;

	cpu_clk = clk_get_sys(NULL, "cpu");
	cpu_g_clk = clk_get_sys(NULL, "cpu_g");
	cpu_lp_clk = clk_get_sys(NULL, "cpu_lp");
	if (IS_ERR(cpu_clk) || IS_ERR(cpu_g_clk) || IS_ERR(cpu_lp_clk))
		return -ENOENT;

	lp_top_freq = clk_get_max_rate(cpu_lp_clk) / 1000;
	g_bottom_freq = clk_get_min_rate(cpu_g_clk) / 1000;
	hybrid_bottom_freq = lp_top_freq * 3 / 4;

	last_cpu_freq = 0;
	last_cpu_freq_time = 0;
	last_change_time = 0;

	transition_request_threshold_awake =
		msecs_to_jiffies(TRANSITION_AWAKE_DELAY_MS);
	transition_request_threshold_suspend =
		msecs_to_jiffies(TRANSITION_SUSPEND_DELAY_MS);

	in_suspend = false;
	in_deep_sleep = false;

	enabled_clusters = ENABLED_CLUSTERS;

	up2g0_delay = msecs_to_jiffies(UP2G0_DELAY_MS);
	up2gn_delay = msecs_to_jiffies(UP2Gn_DELAY_MS);
	down_delay = msecs_to_jiffies(DOWN_DELAY_MAX_MS);

#ifdef CONFIG_HAS_EARLYSUSPEND
	early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	early_suspend.suspend = tegra_auto_hotplug_early_suspend;
	early_suspend.resume = tegra_auto_hotplug_late_resume;
	register_early_suspend(&early_suspend);
#endif

	tegra3_cpu_lock = cpu_lock;
	hp_state = TEGRA_HP_DISABLED;
	initialized = true;

	return 0;
}

static int eprj_cpuman_remove(struct platform_device *pdev)
{
	initialized = false;

	return 0;
}

#ifdef CONFIG_DEBUG_FS

static struct dentry *hp_debugfs_root;

struct pm_qos_request min_cpu_req;
struct pm_qos_request max_cpu_req;

static int hp_stats_show(struct seq_file *s, void *data)
{
	int i;
	u64 cur_jiffies = get_jiffies_64();

	mutex_lock(tegra3_cpu_lock);
	if (hp_state != TEGRA_HP_DISABLED) {
		for (i = 0; i <= CONFIG_NR_CPUS; i++) {
			bool was_up = (hp_stats[i].up_down_count & 0x1);
			hp_stats_update(i, was_up);
		}
	}
	mutex_unlock(tegra3_cpu_lock);

	seq_printf(s, "%-15s ", "cpu:");
	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		seq_printf(s, "G%-9d ", i);
	}
	seq_printf(s, "LP\n");

	seq_printf(s, "%-15s ", "transitions:");
	for (i = 0; i <= CONFIG_NR_CPUS; i++) {
		seq_printf(s, "%-10u ", hp_stats[i].up_down_count);
	}
	seq_printf(s, "\n");

	seq_printf(s, "%-15s ", "time plugged:");
	for (i = 0; i <= CONFIG_NR_CPUS; i++) {
		seq_printf(s, "%-10u ",
			   jiffies_to_msecs(hp_stats[i].time_up_total));
	}
	seq_printf(s, "\n");

	seq_printf(s, "%-15s %u\n", "time-stamp:",
		   jiffies_to_msecs(cur_jiffies));

	return 0;
}

static int hp_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, hp_stats_show, inode->i_private);
}

static const struct file_operations hp_stats_fops = {
	.open		= hp_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int min_cpus_get(void *data, u64 *val)
{
	*val = pm_qos_request(PM_QOS_MIN_ONLINE_CPUS);
	return 0;
}
static int min_cpus_set(void *data, u64 val)
{
	pm_qos_update_request(&min_cpu_req, (s32)val);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(min_cpus_fops, min_cpus_get, min_cpus_set, "%llu\n");

static int max_cpus_get(void *data, u64 *val)
{
	*val = pm_qos_request(PM_QOS_MAX_ONLINE_CPUS);
	return 0;
}
static int max_cpus_set(void *data, u64 val)
{
	pm_qos_update_request(&max_cpu_req, (s32)val);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(max_cpus_fops, max_cpus_get, max_cpus_set, "%llu\n");

static int __init tegra_auto_hotplug_debug_init(void)
{
	if (!tegra3_cpu_lock)
		return -ENOENT;

	hp_debugfs_root = debugfs_create_dir("tegra_hotplug", NULL);
	if (!hp_debugfs_root)
		return -ENOMEM;

	pm_qos_add_request(&min_cpu_req, PM_QOS_MIN_ONLINE_CPUS,
			   PM_QOS_DEFAULT_VALUE);
	pm_qos_add_request(&max_cpu_req, PM_QOS_MAX_ONLINE_CPUS,
			   PM_QOS_DEFAULT_VALUE);

	if (!debugfs_create_file(
		"min_cpus", S_IRUGO, hp_debugfs_root, NULL, &min_cpus_fops))
		goto err_out;

	if (!debugfs_create_file(
		"max_cpus", S_IRUGO, hp_debugfs_root, NULL, &max_cpus_fops))
		goto err_out;

	if (!debugfs_create_file(
		"stats", S_IRUGO, hp_debugfs_root, NULL, &hp_stats_fops))
		goto err_out;

	return 0;

err_out:
	debugfs_remove_recursive(hp_debugfs_root);
	pm_qos_remove_request(&min_cpu_req);
	pm_qos_remove_request(&max_cpu_req);
	return -ENOMEM;
}

late_initcall(tegra_auto_hotplug_debug_init);
#endif

void eprj_tegra_auto_hotplug_set_enabled_clusters(unsigned char clusters)
{
	if (clusters != 0)
		enabled_clusters = clusters;
}

void eprj_tegra_auto_hotplug_reinit(void)
{
	pr_info(MESSAGE_TAG "%s: CPU Manager: Resuming...\n", __func__);

	if (hp_state != TEGRA_HP_DISABLED) {
		pr_err(MESSAGE_TAG "%s: Cannot resume CPU Manager\n",
			__func__);
		return;
	}

	mutex_lock(tegra3_cpu_lock);

	hp_state = TEGRA_HP_IDLE;
	hp_init_stats();
	tegra_cpu_set_speed_cap(NULL);

	mutex_unlock(tegra3_cpu_lock);
}

void tegra_auto_hotplug_exit(void)
{
	pr_info(MESSAGE_TAG "%s: CPU Manager: Suspending...\n", __func__);

	if (hp_state == TEGRA_HP_DISABLED) {
		pr_err(MESSAGE_TAG "%s: Cannot suspend CPU Manager\n",
		       __func__);
		return;
	}

	mutex_lock(tegra3_cpu_lock);
	hp_state = TEGRA_HP_DISABLED;
	mutex_unlock(tegra3_cpu_lock);

	cancel_delayed_work_sync(&hotplug_work);
}

static const struct of_device_id cpumanager_dt[] = {
	{ .compatible = "nvidia,tegra30" },
	{ }
};

static struct platform_driver eprj_cpumanager = {
	.driver = {
		.name = "eprj-cpuman",
		.owner = THIS_MODULE,
		.of_match_table = cpumanager_dt,
	},
	.probe	= eprj_cpuman_probe,
	.remove	= __devexit_p(eprj_cpuman_remove),
};

static int __init cpuman_register(void)
{
	platform_driver_register(&eprj_cpumanager);
	return 0;
}
module_init(cpuman_register);
