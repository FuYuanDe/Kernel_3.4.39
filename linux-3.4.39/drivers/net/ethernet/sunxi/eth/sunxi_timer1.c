/*
 * Allwinner A1X SoCs timer handling.
 *
 * Copyright (C) 2012 Maxime Ripard
 *
 * Maxime Ripard <maxime.ripard@free-electrons.com>
 *
 * Based on code from
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Benn Huang <benn@allwinnertech.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/clk.h>
#include <linux/clockchips.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/sunxi_timer.h>
#include <linux/clk/sunxi.h>
#include <linux/delay.h>
#include <mach/platform.h>
#include "sunxi_geth.h"

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)

#define TIMER_CTL_REG		0x00
#define TIMER_CTL_ENABLE		(1 << 1)
#define TIMER_IRQ_ST_REG	0x04

#define TIMER1_CTL_REG		0x20
#define TIMER1_CTL_ENABLE		(1 << 0)
#define TIMER1_CTL_AUTORELOAD		(1 << 1)
#define TIMER1_CTL_ONESHOT		(1 << 7)
#define TIMER1_INTVAL_REG	0x24
#define TIMER1_CNTVAL_REG	0x28


#ifdef CONFIG_EVB_PLATFORM
#define TIMER_SCAL		16
#else /* it is proved by test that clk-src=32000, prescale=1 on fpga */
#define TIMER_SCAL		1
#endif

static void __iomem *timer_base;
static spinlock_t timer1_spin_lock;

#if 1
static void sunxi_clkevt_mode(enum clock_event_mode mode)
{
	u32 u = readl(timer_base + TIMER1_CTL_REG);

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		u &= ~(TIMER1_CTL_ONESHOT);
		writel(u | TIMER1_CTL_ENABLE, timer_base + TIMER1_CTL_REG);
		break;

	case CLOCK_EVT_MODE_ONESHOT:
		writel(u | TIMER1_CTL_ONESHOT, timer_base + TIMER1_CTL_REG);
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
	default:
		writel(u & ~(TIMER1_CTL_ENABLE), timer_base + TIMER1_CTL_REG);
		break;
	}
}

static int sunxi_clkevt_next_event(unsigned long evt)
{
	unsigned long flags;
	volatile u32 ctrl = 0;
	
	spin_lock_irqsave(&timer1_spin_lock,flags);

	/*disable timer0*/
	ctrl = readl(timer_base + TIMER1_CTL_REG);
	ctrl &=(~(1<<0));
	writel(ctrl,(timer_base+TIMER1_CTL_REG));
	udelay(1);

	/* set timer intervalue */
	writel(evt,(timer_base+TIMER1_INTVAL_REG));
	udelay(1);

    /*reload the timer intervalue*/
	ctrl = readl(timer_base + TIMER1_CTL_REG);
	ctrl |= (1<<1);
	writel(ctrl,(timer_base + TIMER1_CTL_REG));
	udelay(1);

    /*enable timer1*/
	ctrl = readl(timer_base + TIMER1_CTL_REG);
	ctrl |= (1<<0);
	writel(ctrl,(timer_base + TIMER1_CTL_REG));
	spin_unlock_irqrestore(&timer1_spin_lock,flags);

	return 0;
}

#endif

static irqreturn_t sunxi_timer_interrupt(int irq, void *dev_id)
{
	writel(0x2, timer_base + TIMER_IRQ_ST_REG);
	geth_rcv_hardtimer_func(dev_id);
	//printk(KERN_ERR"time1 at %ld\n",jiffies);
	return IRQ_HANDLED;
}

static struct irqaction sunxi_timer_irq = {
	.name = "sunxi_timer1",
	.flags = IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler = sunxi_timer_interrupt,
	.dev_id = NULL,
};


void  sunxi_timer_init(unsigned long data)
{
	unsigned long rate = 0;
	int ret, irq;
	u32 val;


	timer_base = (void __iomem *)SUNXI_TIMER_VBASE;
	irq = SUNXI_IRQ_TIMER1;
#if defined(CONFIG_ARCH_SUN8IW3P1) && defined(CONFIG_FPGA_V4_PLATFORM)
	rate = 32000; /* it is proved by test that clk-src=32000, prescale=1 on fpga */
#else
	rate = 24000000;
#endif

	writel(rate / (TIMER_SCAL * HZ),
	       timer_base + TIMER1_INTVAL_REG);

	/* set clock source to HOSC, 16 pre-division */
	val = readl(timer_base + TIMER1_CTL_REG);
	val &= ~(0x07 << 4);
	val &= ~(0x03 << 2);
	val |= (4 << 4) | (1 << 2);
	writel(val, timer_base + TIMER1_CTL_REG);

	/* set mode to auto reload */
	val = readl(timer_base + TIMER1_CTL_REG);
	writel(val | TIMER1_CTL_AUTORELOAD, timer_base + TIMER1_CTL_REG);

	sunxi_timer_irq.dev_id = data;
	ret = setup_irq(irq, &sunxi_timer_irq);
	if (ret)
		pr_warn("failed to setup irq %d\n", irq);

	/* Enable timer1 interrupt */
	val = readl(timer_base + TIMER_CTL_REG);
	writel(val | TIMER_CTL_ENABLE, timer_base + TIMER_CTL_REG);

	sunxi_clkevt_mode(CLOCK_EVT_MODE_PERIODIC);
	sunxi_clkevt_next_event(200);		// 66*2 us
}


void  sunxi_timer_exit(void)
{
	sunxi_clkevt_mode(CLOCK_EVT_MODE_SHUTDOWN);
}

#endif
