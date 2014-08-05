/*
 * arch/arm/mach-tegra/eprj_sysfs_tools.c
 *   -- EternityProject sysfs Tools --
 *
 * Copyright (C) 2012, EternityProject Development
 *
 * Author:
 *      Angelo G. Del Regno <kholk11@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * History:
 * Date		| Comment					| Author
 * 08/06/2013     Add EternityProject SoundBoost Control          kholk <kholk11@gmail.com>
 * 22/05/2013     Add EternityProject LiveColor Control           kholk <kholk11@gmail.com>
 * 21/05/2013     Add USB Host Mode switch for LGE X3             kholk <kholk11@gmail.com>
 * 20/02/2013     Add FryMe EDPStop functionality (Beware!!)      kholk <kholk11@gmail.com>
 * 25/10/2012	  Add DVFS Voltage Control			  kholk <kholk11@gmail.com>
 * 21/10/2012	  Fix sysfs handling for DVFS			  lowjoel <joel@joelsplace.sg>
 * 16/10/2012	  Add DVFS Clocks Control			  kholk <kholk11@gmail.com>
 * 17/09/2012	  Optimize ClusterSwitch			  lowjoel <joel@joelsplace.sg>
 * 09/09/2012	  Cleanup, improved expandeability, better vars	  kholk <kholk11@gmail.com>
 * 09/09/2012	  Use for EternityProject HSMGR Modifications	  kholk <kholk11@gmail.com>
 * 01/09/2012	  Implement ChargerBoost functionality		  kholk <kholk11@gmail.com>
 * 08/08/2012	  Initial revision.				  kholk <kholk11@gmail.com>
 */

#include <linux/io.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wakelock.h>

#include <linux/cpufreq.h>

#include <asm/uaccess.h>
#include <mach/eternityproject.h>

#include <mach/dc.h>

#include "../pm.h"
#include "../cpu-tegra.h"
#include "../clock.h"
#include "../dvfs.h"

#define EPRJ_TAG "eprj-sysfs-tools:"
#define SYSFSMAN_REVISION 4

static DEFINE_MUTEX(eprj_sysfs_tools_mutex);

#define EPRJ_SYSFS_ENTRY(_id, _name, _mode, _value)	\
static struct eprj_sysfs _id = {			\
	.attr.name = _name,				\
	.attr.mode = _mode,				\
	.value     = _value,				\
};

#define EPRJ_FREQ_ENTRY(_id, _name, _mode)		\
static struct eprj_sysfs _id = {			\
	.attr.name = _name,				\
	.attr.mode = _mode,				\
};

/* Standard entries */
EPRJ_SYSFS_ENTRY(version, "version", 0444, SYSFSMAN_REVISION);
EPRJ_SYSFS_ENTRY(android_release, "android_apirev", 0644, ANDROID_API_JB);
EPRJ_SYSFS_ENTRY(pwrmode, "power_lock", 0666, 0); /* 0 = Don't lock in LP -- 1 = Lock in LP Mode */
EPRJ_SYSFS_ENTRY(chgbst, "charger_boost", 0666, 1); /* 0 = Normal operation -- 1 = Boost USB charge */
EPRJ_SYSFS_ENTRY(wlockblist, "wlock_blacklist", 0666, 0); /* It is a list of strings: No numeric values in there. */
EPRJ_SYSFS_ENTRY(eprj_friedeggs, "friedeggs", 0666, 0);

/* Frequency and voltage management */
EPRJ_FREQ_ENTRY(gpucontrol, "gpu_frequencies", 0666);
EPRJ_FREQ_ENTRY(lpcontrol, "lp_frequencies", 0666);
EPRJ_FREQ_ENTRY(gcontrol, "g_frequencies", 0666);
EPRJ_FREQ_ENTRY(corevoltctrl, "soc_voltage", 0666);
EPRJ_FREQ_ENTRY(t3_gvolt, "g_voltage", 0666);

/* Device-specific hacky entries */
#ifdef CONFIG_MACH_X3
EPRJ_SYSFS_ENTRY(eprj_x3_usbh, "usb_host_mode", 0666, 0);
EPRJ_SYSFS_ENTRY(colorenhance, "livecolor", 0666, 1);
#endif
#ifdef CONFIG_MACH_ENDEAVORU
EPRJ_SYSFS_ENTRY(eprj_enru_sndboost, "sndboost", 0666, 0);
#endif

struct attribute * eprjmanager[] = {
	&version.attr,
	&android_release.attr,
	&pwrmode.attr,
	&chgbst.attr,
	&wlockblist.attr,
	&gpucontrol.attr,
	&lpcontrol.attr,
	&gcontrol.attr,
	&corevoltctrl.attr,
	&t3_gvolt.attr,
	&eprj_friedeggs.attr,
#ifdef CONFIG_MACH_X3
	&eprj_x3_usbh.attr,
	&colorenhance.attr,
#endif
#ifdef CONFIG_MACH_ENDEAVORU
	&eprj_enru_sndboost.attr,
#endif
	NULL
};

typedef enum
{
	EPRJ_INVALID = 0,
	EPRJ_VERSION,
	HSMGR_APIREV,
	POWER_LOCK,
	CHARGE_BOOST,
	WAKELOCK_BLACKLIST,
	GPUCTRL,
	LPCLCTRL,
	GCLCTRL,
	SOCVOLT,
	GCLVOLT,
	EDPSTOP,
#ifdef CONFIG_MACH_X3
	X3USBH,
	LIVECOLOR,
#endif
#ifdef CONFIG_MACH_ENDEAVORU
	SNDBOOST,
#endif
} eprj_attribute;

#ifdef CONFIG_MACH_X3
static bool x3_usbh_mode = 0;
bool eprj_chargeboost = 0;
#else
bool eprj_chargeboost = 1;
#endif

#ifdef CONFIG_MACH_ENDEAVORU
bool eprj_sndboosted = 1;
#endif

bool eprj_fryme;

static eprj_attribute calling_attribute(const char *attrname)
{
	if (!strcmp(attrname, "version"))
		return EPRJ_VERSION;
	if (!strcmp(attrname, "android_apirev"))
		return HSMGR_APIREV;
	if (!strcmp(attrname, "power_lock"))
		return POWER_LOCK;
	if (!strcmp(attrname, "charger_boost"))
		return CHARGE_BOOST;
	if (!strcmp(attrname, "wlock_blacklist"))
		return WAKELOCK_BLACKLIST;
	if (!strcmp(attrname, "gpu_frequencies"))
		return GPUCTRL;
	if (!strcmp(attrname, "lp_frequencies"))
		return LPCLCTRL;
	if (!strcmp(attrname, "g_frequencies"))
		return GCLCTRL;
	if (!strcmp(attrname, "soc_voltage")) 
		return SOCVOLT;
	if (!strcmp(attrname, "g_voltage"))
		return GCLVOLT;
	if (!strcmp(attrname, "friedeggs"))
		return EDPSTOP;
#ifdef CONFIG_MACH_X3
	if (!strcmp(attrname, "usb_host_mode"))
		return X3USBH;
	if (!strcmp(attrname, "livecolor"))
		return LIVECOLOR;
#endif

#ifdef CONFIG_MACH_ENDEAVORU
	if (!strcmp(attrname, "sndboost"))
		return SNDBOOST;
#endif

	return EPRJ_INVALID;
}

static inline int eprjsysfs_listread(struct eprj_sysfs *entry, char *buf)
{
	unsigned int nchars = 0;
	uint8_t i = 0;
	char *temp = buf;

	do {
		nchars += scnprintf(temp, PAGE_SIZE, "%s", entry->strings[i]);
		i++;
	} while (i <= entry->value);

	buf = temp;

	return nchars;
}

static inline int eprjsysfs_seqwrite(struct eprj_sysfs *entry, const char *buf)
{
	unsigned int nchars = 0;
	uint8_t pos = 0;

	pos = is_blacklisted(buf);

	if (!pos) {
		entry->value++;
		nchars = sscanf(buf, "%s", entry->strings[entry->value]);
	} else {
		entry->strings[pos] = entry->strings[entry->value];
		entry->strings[entry->value] = 0;
		entry->value--;
	}
		/* Or, alternatively, we can really reorder it. IMHO, useless. 
		for (i = pos; i < entry->value; i++)
			entry->strings[i] == entry->strings[i + 1]
		entry->strings[entry->value] = NULL; */
		

	return nchars;
}

/*
 * Show the given clock, recognized by ClockID (cid)
 * The function is valid for Tegra 2 and Tegra 3 chips
 * with DVFS code by nVidia.
 */
static int tegra_clocks_show(char *buf, eprj_attribute cid)
{
	struct clk *mydev = NULL;
	char *temp = buf;
	uint8_t nfreqs = 0,
		i = 0;

	switch (cid)
	{
		case GPUCTRL:
			mydev = tegra_get_clock_by_name("3d");
			break;
		case LPCLCTRL:
		case SOCVOLT:
			mydev = tegra_get_clock_by_name("cpu_lp");
			break;
		case GCLCTRL:
		case GCLVOLT:
			mydev = tegra_get_clock_by_name("cpu_g");
			break;
		default:
			return -EINVAL;
	}
	if (mydev == NULL)
		return -EINVAL;

	nfreqs = mydev->dvfs->num_freqs;
	if (nfreqs == 0)
		return -EINVAL;

	for ( ; i < nfreqs; ++i) {
		unsigned long value;
		if (cid == SOCVOLT || cid == GCLVOLT)
			value = mydev->dvfs->millivolts[i];
		else
			value = mydev->dvfs->freqs[i] / 1000000;
		temp += sprintf(temp, "%lu ", value);
	}

	temp += sprintf(temp, "\n");
	return temp - buf;
}

#define MHZ(a)		 ((a) * 1000000)

/*
 * Set the given clock, recognized by ClockID (cid)
 * The fuction is valid for Tegra 2 and Tegra 3 chips
 * with DVFS code by nVidia.
 */
static int tegra_clocks_write(const char *buf, eprj_attribute cid)
{
	struct clk *mydev = NULL;
	struct clk *mydev2 = NULL;
	uint8_t nfreqs = 0;

	switch (cid)
	{
		case GPUCTRL:
			mydev = tegra_get_clock_by_name("3d");
			break;
		case LPCLCTRL:
		case SOCVOLT:
			mydev = tegra_get_clock_by_name("cpu_lp");
			break;
		case GCLCTRL:
		case GCLVOLT:
			mydev = tegra_get_clock_by_name("cpu_g");
			mydev2 = tegra_get_clock_by_name("cpu_0");
			break;
		default:
			return -EINVAL;
	}
	if (mydev == NULL)
		return -EINVAL;

	/* Note: if GPUCTRL, num_freqs is common between all the clocks. */
	nfreqs = mydev->dvfs->num_freqs;
	if (nfreqs == 0)
		return -EINVAL;

	{
		const char *i;
		uint8_t j;
		unsigned long newtbl[nfreqs];
		struct dvfs_rail* affected_rail;
		struct tegra_cpufreq_table_data* cpufreq_table =
			tegra_cpufreq_table_get();

		for (i = buf, j = 0; *i && j < nfreqs; ++j)
		{
			int read = 0;
			u32 newfreq = 0;
			sscanf(i, "%u%n", &newfreq, &read);
			if (!read)
			{
				break;
			}

			i += read;

			/*
			 * Paranoid check for sanity of the voltage value.
			 * We will STOP if we are trying to set a voltage
			 * higher than the MAX allowed or if it is a negative
			 * number, as we could trigger something like setting
			 * a billion volts (max is 5V but we don't want it).
			 */
			if ((cid == GCLVOLT || cid == SOCVOLT) && (
				newfreq > MAX_ALLOWED_VOLT || newfreq < 0)) {
				pr_err(EPRJ_TAG " CRITICAL ERROR: Voltage protection "
					"saved you. MAX Allowed: %d - You wanted to "
					"set: %d. \n", MAX_ALLOWED_VOLT, newfreq);

				return -EINVAL;
			}
			newtbl[j] = newfreq;
		}

		if (j != nfreqs) {
			pr_warn(EPRJ_TAG "not updating voltages: input must have exactly "
				"%d entries", (int)nfreqs);
			return -EINVAL;
		}

		mutex_lock(&eprj_sysfs_tools_mutex);
		for (j = 0; j < nfreqs; ++j) {
			switch (cid) {
				case GCLVOLT:
					/* Update cpu_0 DVFS and fall through */
					mydev2->dvfs->millivolts[j] = newtbl[j];
					if (j == nfreqs - 1) {
						mydev2->dvfs->max_millivolts = newtbl[j];
					}
					/* no break here */
				case SOCVOLT:
					mydev->dvfs->millivolts[j] = newtbl[j];
					if (j == nfreqs - 1) {
						mydev->dvfs->max_millivolts = newtbl[j];
					}
					break;

				case GPUCTRL:
				{
					struct clk *tdd, *vde, *mpe, *epp, *dud, *se, *cb;
					tdd = tegra_get_clock_by_name("3d2");
					vde = tegra_get_clock_by_name("vde");
					mpe = tegra_get_clock_by_name("mpe");
					epp = tegra_get_clock_by_name("epp");
					dud = tegra_get_clock_by_name("2d");
					se  = tegra_get_clock_by_name("se");
					cb  = tegra_get_clock_by_name("cbus");

					if (j == nfreqs - 1) {
						tdd->max_rate = min(tdd->max_rate, MHZ(newtbl[j]));
						vde->max_rate = min(vde->max_rate, MHZ(newtbl[j]));
						mpe->max_rate = min(mpe->max_rate, MHZ(newtbl[j]));
						epp->max_rate = min(epp->max_rate, MHZ(newtbl[j]));
						dud->max_rate = min(dud->max_rate, MHZ(newtbl[j]));
						se->max_rate  = min(se->max_rate,  MHZ(newtbl[j]));
						cb->max_rate  = min(cb->max_rate,  MHZ(newtbl[j]));
					}
					if (j == 0) {
						tdd->min_rate = min(tdd->max_rate, MHZ(newtbl[j]));
						vde->min_rate = min(vde->max_rate, MHZ(newtbl[j]));
						mpe->min_rate = min(mpe->max_rate, MHZ(newtbl[j]));
						epp->min_rate = min(epp->max_rate, MHZ(newtbl[j]));
						dud->min_rate = min(dud->max_rate, MHZ(newtbl[j]));
						se->min_rate  = min(se->max_rate, MHZ(newtbl[j]));
						cb->min_rate  = min(cb->max_rate,  MHZ(newtbl[j]));
					}

					tdd->dvfs->freqs[j] = MHZ(newtbl[j]);
					vde->dvfs->freqs[j] = MHZ(newtbl[j]);
					mpe->dvfs->freqs[j] = MHZ(newtbl[j]);
					epp->dvfs->freqs[j] = MHZ(newtbl[j]);
					dud->dvfs->freqs[j] = MHZ(newtbl[j]);
					se->dvfs->freqs[j]  = MHZ(newtbl[j]);
					cb->dvfs->freqs[j]  = MHZ(newtbl[j]);
					//fall through
					goto LPCLCTRL_FREQ_SET;
				}

				case GCLCTRL:
					pr_debug(EPRJ_TAG "Getting cpufreq[%d]=%u want to set %lu", j + CPU_LP_FREQ_ENTRIES,
						cpufreq_table->freq_table[j + CPU_LP_FREQ_ENTRIES].frequency, newtbl[j]);
					if (newtbl[j] >= cpufreq_table->freq_table[CPU_LP_FREQ_ENTRIES - 1].frequency / 1000) {
						pr_info(EPRJ_TAG "Setting cpufreq[%d]=%lu was %u", j + CPU_LP_FREQ_ENTRIES,
							newtbl[j] * 1000, cpufreq_table->freq_table[j + CPU_LP_FREQ_ENTRIES].frequency);
						cpufreq_table->freq_table[j + CPU_LP_FREQ_ENTRIES].
							frequency = newtbl[j] * 1000;
					}
					if (j == nfreqs - 1) {
						struct clk* cpu = clk_get_sys(NULL, "cpu");
						struct clk* cclk_g = tegra_get_clock_by_name("cpu_g");
						struct clk* cclk_0 = tegra_get_clock_by_name("cpu_0");
						pr_info(EPRJ_TAG "Updating %p (%s) with new rate %lu (old=%lu)",
							cpu, cpu->name, MHZ(newtbl[j]), cpu->max_rate);
						cpu->max_rate = cclk_0->max_rate =
						cclk_g->max_rate = MHZ(newtbl[j]);
					}
					//fall through

				case LPCLCTRL:
					LPCLCTRL_FREQ_SET:
					if (j == nfreqs - 1) {
						mydev->max_rate =
							min(mydev->max_rate, MHZ(newtbl[j]));
					}
					if (j == 0) {
						mydev->min_rate =
							max(mydev->min_rate, MHZ(newtbl[j]));
					}
					mydev->dvfs->freqs[j] = MHZ(newtbl[j]);

				default:
					break;
			}
		}
		mutex_unlock(&eprj_sysfs_tools_mutex);

		/* Let the new frequencies take effect. */
		if (cid == GCLCTRL) {
#ifndef CONFIG_MACH_OUYA
			unsigned int j;
			for_each_online_cpu(j) {
				struct cpufreq_policy *policy = cpufreq_cpu_get(j);
				bool no_freq_cap = policy->user_policy.max == policy->max;

				pr_info(EPRJ_TAG " Updating frequency for cpu%d", j);
				cpufreq_frequency_table_cpuinfo(policy, cpufreq_table->freq_table);
				if (no_freq_cap) {
					policy->user_policy.max = policy->max;
				}

				cpufreq_cpu_put(policy);
			}
#endif
		}

		/* Recalculate voltages all the clocks on the rail. */
		switch (cid) {
			case GCLVOLT:
				affected_rail = tegra_dvfs_get_rail_by_name("vdd_cpu");
				break;
			case SOCVOLT:
				affected_rail = tegra_dvfs_get_rail_by_name("vdd_core");
				break;
			default:
				affected_rail = NULL;
		}

		if (affected_rail) {
			struct dvfs* d;
			list_for_each_entry(d, &affected_rail->dvfs, reg_node) {
				pr_info(EPRJ_TAG "voltage setting: clock %s affected. rate=%lu, volt=%d (max %d)\n",
					d->clk_name, d->cur_rate, d->cur_millivolts, d->max_millivolts);
				tegra_dvfs_set_rate(tegra_get_clock_by_name(d->clk_name), d->cur_rate);
			}
		}

		return i - buf;
	}
}

static ssize_t eprjsysfs_show(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	struct eprj_sysfs *entry = container_of(attr, struct eprj_sysfs, attr);
	eprj_attribute mynameis = calling_attribute(entry->attr.name);

	switch (mynameis)
	{
		case EPRJ_VERSION:
			return scnprintf(buf, PAGE_SIZE, "%d\n", entry->value);
			break;
		case WAKELOCK_BLACKLIST:
			return eprjsysfs_listread(entry, buf);
			break;
		case GPUCTRL:
		case LPCLCTRL:
		case GCLCTRL:
			return tegra_clocks_show(buf, mynameis);
			break;
		case SOCVOLT:
		case GCLVOLT:
			return tegra_clocks_show(buf, mynameis);
			break;

		case EDPSTOP:
			return scnprintf(buf, PAGE_SIZE, "%d\n", entry->value);
			break;
#ifdef CONFIG_MACH_X3
		case X3USBH:
			sscanf(buf, "%d", &entry->value);
			x3_usbh_mode = entry->value;
			return scnprintf(buf, PAGE_SIZE, "%d\n", entry->value);
			break;
		case LIVECOLOR:
			return scnprintf(buf, PAGE_SIZE, "%d\n", entry->value);
			break;
#endif
#ifdef CONFIG_MACH_ENDEAVORU
		case SNDBOOST:
			return scnprintf(buf, PAGE_SIZE, "%d\n", entry->value);
			break;
#endif
		default:
			return scnprintf(buf, PAGE_SIZE, "%d\n", entry->value);
	}
}

/*
 * \return The number of characters consumed from the input buffer.
 *         We always return \paramref len because we do not like to be
 *         called twice (sysfs will keep calling us until we empty the
 *         input buffer)
 */
static ssize_t eprjsysfs_store(struct kobject *kobj, struct attribute *attr,
				 const char *buf, size_t len)
{
	struct eprj_sysfs *entry = container_of(attr, struct eprj_sysfs, attr);
	eprj_attribute mynameis = calling_attribute(entry->attr.name);
#ifdef CONFIG_MACH_X3
	extern void eprj_x3_usbhost_switch(bool enb);
#endif

	switch (mynameis)
	{
		case HSMGR_APIREV:
#ifdef CONFIG_MACH_ENDEAVORU
			sscanf(buf, "%d", &entry->value);
			switch (entry->value) {
				case ANDROID_API_JB:
					EPRJ_PRINT("EternityProject HSMGR: Android Jellybean detected.\n");
					break;
				case ANDROID_API_ICS:
					EPRJ_PRINT("EternityProject HSMGR: Android Ice Cream Sandwich detected.\n");
					break;
				default:
					EPRJ_PRINT("EternityProject HSMGR: Android -WHAT THE HELL DID YOU WRITE?- not detected. ERROR.\n");
					EPRJ_PRINT("Doing nothing. Check what you wrote.\n");
					EPRJ_PRINT("Note: Actually we only support ICS and JB!\n");
					return;
			}
				eprj_hsmgr_35mm_os(entry->value);
#endif
			break;

		case POWER_LOCK:
			sscanf(buf, "%d", &entry->value);
			eprj_extreme_powersave(entry->value);
			break;

		case CHARGE_BOOST:
			sscanf(buf, "%d", &entry->value);
			printk("EternityProject Charger Boost -- %s", entry->value ? "ACTIVATED." : "DEACTIVATED.");
			eprj_chargeboost = (entry->value);
			break;

		case WAKELOCK_BLACKLIST:
			/* Manage the store function for blacklist here. */
			eprjsysfs_seqwrite(entry, buf);
			printk("EPRJ sysfs Tools: WLOCK_BLIST NOT SUPPORTED.\n");
			break;

		case GPUCTRL:
		case LPCLCTRL:
		case GCLCTRL:
			tegra_clocks_write(buf, mynameis);
			break;

		case SOCVOLT:
		case GCLVOLT:
			tegra_clocks_write(buf, mynameis);
			break;

		case EDPSTOP:
			sscanf(buf, "%d", &entry->value);
			printk("EternityProject: We %s fry your eggs.\n", entry->value? "are ready to" : "won't");
			eprj_fryme = entry->value;
			break;
#ifdef CONFIG_MACH_X3
		case X3USBH:
			if ((entry->value < 0) ||
			    (entry->value > 1))
				return -EINVAL;
			sscanf(buf, "%d", &entry->value);
			x3_usbh_mode = entry->value;
			eprj_x3_usbhost_switch(entry->value);
			break;
		case LIVECOLOR:
			if ((entry->value < 0) ||
			    (entry->value > 1))
				return -EINVAL;
			sscanf(buf, "%d", &entry->value);
			if (entry->value)
				cmdlineRGBvalue.table_type = GAMMA_NV_ETERNITYPROJECT;
			else
				cmdlineRGBvalue.table_type = GAMMA_NV_DISABLED;
			eprj_livecolor_apply(entry->value);
			break;
#endif

#ifdef CONFIG_MACH_ENDEAVORU
		case SNDBOOST:
			if ((entry->value < 0) ||
			    (entry->value > 1))
				return -EINVAL;
			sscanf(buf, "%d", &entry->value);
			eprj_sndboosted = entry->value;
			break;
#endif
		default:
			break;
	}

	return len;
}

static struct sysfs_ops eprjsysfs_ops = {
	.show = eprjsysfs_show,
	.store = eprjsysfs_store,
};

static struct kobj_type eprjsysfs_type = {
	.sysfs_ops = &eprjsysfs_ops,
	.default_attrs = eprjmanager,
};

/*
 * Tegra 3 - Force LP Cluster
 * TODO: Make a better implementation of that mess.
 */
void eprj_extreme_powersave(bool active)
{
	/* We can use set_cpu_possible(cpu, possible) for enforcing that, if needed. */
	if (active) {
		eprj_tegra_auto_hotplug_set_enabled_clusters(EPRJ_TEGRA3_LP_CLUSTER);
		msleep(1000);

		tegra_auto_hotplug_exit();
		disable_nonboot_cpus();
	} else {
		enable_nonboot_cpus();
		eprj_tegra_auto_hotplug_reinit();
		eprj_tegra_auto_hotplug_set_enabled_clusters(
			EPRJ_TEGRA3_LP_CLUSTER | EPRJ_TEGRA3_G_CLUSTER);
	};
};

/* Warning: Blind function! */
void eprjsysfs_initstrings(struct attribute *attr)
{
	struct eprj_sysfs *entry = container_of(attr, struct eprj_sysfs, attr);
	uint8_t i;

	for (i = 0; i < MAXSTRINGS; i++)
		entry->strings[i] = 0;
}

/*
 * Wakelock Blacklist Management
 */
uint8_t __eprjsearchlock(char *lock)
{
	struct attribute *attr = &wlockblist.attr;
	struct eprj_sysfs *entry = container_of(attr, struct eprj_sysfs, attr);
	uint8_t ret = -1;

	if (entry->strings[0])
		return 0;

	do {
		ret++;
		if (!entry->strings[ret])
			break;
	} while ( strcmp(entry->strings[ret], lock) || (ret <= (entry->value) ) ) ;

	if (ret > entry->value)
		return 0;

	return ret;
}

#ifdef CONFIG_MACH_X3
bool is_eprj_usbh(void)
{
	return x3_usbh_mode;
}
#endif

/*
 * Actual implementation. Start the sysfs entry.
 */
struct kobject *eprj_sysfs_tools;
static int __init eprj_sysfs_tools_init(void)
{
	int ret = -1;
	printk("EternityProject sysfs Tools: Initialization...\n");
	eprj_sysfs_tools = kzalloc(sizeof(*eprj_sysfs_tools), GFP_KERNEL);
	if (likely(eprj_sysfs_tools)) {
		kobject_init(eprj_sysfs_tools, &eprjsysfs_type);
		if (kobject_add(eprj_sysfs_tools, NULL, "%s", "eprjmanager")) {
			printk("EternityProject sysfs Tools: creation failed.\n");
			kobject_put(eprj_sysfs_tools);
			eprj_sysfs_tools = NULL;
		}
	eprjsysfs_initstrings(&wlockblist.attr);
	ret = 0;
	eprj_fryme = 0;
	}
	if (!ret)
		printk("EternityProject sysfs Tools: Initialized!\n");
	return ret;
}

static void __exit eprj_sysfs_tools_exit(void)
{
	if (eprj_sysfs_tools) {
		kobject_put(eprj_sysfs_tools);
		kfree(eprj_sysfs_tools);
	}
}

late_initcall(eprj_sysfs_tools_init);
module_exit(eprj_sysfs_tools_exit);

MODULE_DESCRIPTION("EternityProject sysfs Tools for HTC One X");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Angelo G. Del Regno - kholk - <kholk11@gmail.com>");
