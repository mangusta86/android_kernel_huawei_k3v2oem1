/*
 * arch/arm/mach-tegra/eprj_scheduling.c
 *
 * Copyright (C) 2013, EternityProject Development
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
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/reboot.h>

#include <mach/eternityproject.h>

#define RQ_SAMPLE_TIME_NS	250000000
#define RQ_AVG_TIMER_RATE_NS	10000000
#define RQ_SAMPLE_CAPACITY	(RQ_SAMPLE_TIME_NS / RQ_AVG_TIMER_RATE_NS)

struct runqueue_sample {
	int64_t sample_time;
	unsigned long nr_run;
};

struct runqueue_data {
	unsigned int update_rate;

	/* Circular buffer. */
	struct runqueue_sample nr_run_samples[RQ_SAMPLE_CAPACITY];

	/* Index of the head of the circular buffer = the newest element. */
	uint8_t nr_run_sample_head;

	/* Index of the tail of the circular buffer = oldest element. */
	uint8_t nr_run_sample_tail;

	/* Number of samples in the buffer. */
	uint8_t nr_run_sample_count;

	struct delayed_work work;
	struct workqueue_struct *nr_run_wq;
	spinlock_t lock;
};

static struct runqueue_data *rq_data;

static void rq_work_fn(struct work_struct *work)
{
	unsigned long flags = 0;
	int64_t cur_time = ktime_to_ns(ktime_get());
	struct runqueue_sample* sample = NULL;

	spin_lock_irqsave(&rq_data->lock, flags);
	if (unlikely(rq_data->nr_run_sample_count != RQ_SAMPLE_CAPACITY)) {
		/* Buffer still growing. */
		rq_data->nr_run_sample_head = rq_data->nr_run_sample_count;
		rq_data->nr_run_sample_tail = 0;
		++rq_data->nr_run_sample_count;
	} else {
		/* Buffer already full. We will be clobbering old samples. */
		rq_data->nr_run_sample_head =
			rq_data->nr_run_sample_head == RQ_SAMPLE_CAPACITY - 1 ?
				0 :
				rq_data->nr_run_sample_head + 1;
		rq_data->nr_run_sample_tail =
			rq_data->nr_run_sample_tail == RQ_SAMPLE_CAPACITY - 1 ?
				0 :
				rq_data->nr_run_sample_tail + 1;
	}
	sample = rq_data->nr_run_samples + (rq_data->nr_run_sample_head);

	/* Store the sample. */
	sample->sample_time = cur_time;
	sample->nr_run = nr_running();

	if (likely(rq_data->update_rate != 0))
		queue_delayed_work(rq_data->nr_run_wq, &rq_data->work,
				   msecs_to_jiffies(rq_data->update_rate));

	spin_unlock_irqrestore(&rq_data->lock, flags);
}

unsigned int eprj_get_nr_run_avg(void)
{
	struct runqueue_sample nr_run_samples[RQ_SAMPLE_CAPACITY];
	uint8_t nr_run_sample_head;
	uint8_t nr_run_sample_count;

	size_t j;
	struct runqueue_sample* i;
	unsigned int nr_run = 0;
	unsigned long flags = 0;
	int64_t after_time = ktime_to_ns(ktime_get()) - RQ_SAMPLE_TIME_NS;

	spin_lock_irqsave(&rq_data->lock, flags);
	memcpy(nr_run_samples, rq_data->nr_run_samples, sizeof(nr_run_samples));
	nr_run_sample_head = rq_data->nr_run_sample_head;
	nr_run_sample_count = rq_data->nr_run_sample_count;
	spin_unlock_irqrestore(&rq_data->lock, flags);

	for (i = nr_run_samples + nr_run_sample_head, j = 0;
	     j < nr_run_sample_count; ++j) {
		if (i->sample_time < after_time) {
			/* This sample is older than the ones we find relevant. */
			break;
		}

		nr_run += i->nr_run;

		/* Get the previous element in the circular buffer. */
		if (unlikely(i == nr_run_samples)) {
			/* Wrap around. */
			i = nr_run_samples + (RQ_SAMPLE_CAPACITY - 1);
		} else {
			--i;
		}
	}

	if (unlikely(j == 0))
		return 0;

	return nr_run * 100 / j;
}

static void start_rq_work(void)
{
	if (rq_data->nr_run_wq == NULL)
		rq_data->nr_run_wq =
			create_freezable_workqueue("nr_run_avg");

	queue_delayed_work(rq_data->nr_run_wq, &rq_data->work,
			   msecs_to_jiffies(rq_data->update_rate));
	return;
}

static void stop_rq_work(void)
{
	if (rq_data->nr_run_wq)
		cancel_delayed_work(&rq_data->work);
	return;
}

static int __init eprj_scheduling_open(void)
{
	rq_data = kzalloc(sizeof(struct runqueue_data), GFP_KERNEL);
	if (rq_data == NULL) {
		pr_err("%s cannot allocate memory\n", __func__);
		return -ENOMEM;
	}
	spin_lock_init(&rq_data->lock);
	rq_data->nr_run_sample_head = (uint8_t)-1;
	rq_data->nr_run_sample_tail = (uint8_t)-1;
	rq_data->nr_run_sample_count = 0;
	rq_data->update_rate = (RQ_AVG_TIMER_RATE_NS / 1000000);
	INIT_DELAYED_WORK_DEFERRABLE(&rq_data->work, rq_work_fn);

	start_rq_work();

	return 0;
}
subsys_initcall(eprj_scheduling_open);

static void __exit eprj_scheduling_close(void)
{
	stop_rq_work();
	kfree(rq_data);
}
module_exit(eprj_scheduling_close)
