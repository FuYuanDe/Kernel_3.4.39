/*
 * Amlogic Watchdog Timer Driver for Meson Chip
 *
 * Author: Bobby Yang <bo.yang@amlogic.com>
 *
 * Copyright (C) 2011 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/watchdog.h>
#include <linux/of.h>
#include <asm/delay.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/resource.h>
#include <linux/io.h>
#include <mach/platform.h>
#include <linux/kern_watchdog.h>
#include <linux/gpio.h>


#define V_MAX_TIMEOUT       60
#define V_DEF_TIMEOUT       30
#define MAX_TIMEOUT            500
#define DRV_NAME	"extern_wdt"

#define SWITCH_GPIO_BIT   4
#define WDI_GPIO_BIT   12
struct aml_wdt_dev {
    unsigned int min_timeout,max_timeout,default_timeout,reset_watchdog_time,shutdown_timeout;
    unsigned int firmware_timeout,suspend_timeout,timeout;
    unsigned int one_second;
    struct device *dev;
    struct mutex	lock;
    unsigned int reset_watchdog_method;
    //struct delayed_work boot_queue;
    struct timer_list v_timer;
    struct timer_list r_timer;
    unsigned long v_restart_time;
};
struct aml_wdt_dev *awdtv=NULL;
void __iomem *wdt_reg;
static int gpio_switch = 235; //PB9---ph11  235 41
static int gpio_wdi = 227;//PH3
const char *name_switch = "gpio_switch";
const char *name_wdi = "gpio_wdi";
static int wdi_levelflag = 1;
static struct platform_device *platform_device;

static struct resource sunxi_wdt_res[] = {
#if 0
	{
	       .start	= SUNXI_PIO_PBASE + 0x28,
		.end	= SUNXI_PIO_PBASE + 0x100, 
		.flags	= IORESOURCE_MEM,
	},
#endif
};

static inline void reset_watchdog(void)
{
    if(wdi_levelflag)
    {
        gpio_set_value(gpio_wdi, 0);
        wdi_levelflag = 0;
    }
    else
    {
        gpio_set_value(gpio_wdi, 1);
        wdi_levelflag = 1;
    }
}
static int disable_watchdog(void)
{
    //del_timer(&awdtv->v_timer);
    //del_timer(&awdtv->r_timer);
    //gpio_direction_input(gpio_switch); 
    mod_timer(&awdtv->r_timer, jiffies + MAX_TIMEOUT* HZ/1000);
    return 0;
}

static void disable_v_watchdog(void)
{
    if((awdtv->timeout )* HZ> MAX_TIMEOUT* HZ/1000)
    {
        del_timer(&awdtv->v_timer);      
        disable_watchdog();
    }
    else
    {
        disable_watchdog();
    }
}

static void enable_v_watchdog(unsigned int timeout)
{

    printk("** enable v_watchdog\n");
    printk("%s %d timeout:%d\n", __FUNCTION__, __LINE__, timeout);
    
    //gpio_direction_output(gpio_wdi, 0);	
    //gpio_direction_output(gpio_switch, 0);
    //gpio_set_value(gpio_wdi, 0);
    if(timeout* HZ > MAX_TIMEOUT* HZ/1000)
    {
        mod_timer(&awdtv->v_timer, jiffies + timeout* HZ);
        awdtv->v_restart_time = jiffies;	      
    }
    awdtv->timeout = timeout;
}

static void reset_v_watchdog(void)
{
    if((awdtv->timeout)* HZ > MAX_TIMEOUT* HZ/1000)
    {
        mod_timer(&awdtv->v_timer,jiffies + (awdtv->timeout) * HZ);
        awdtv->v_restart_time = jiffies;
    }
    else
    {
        reset_watchdog();
    }
}

static int aml_wdt_start(struct watchdog_device *wdog)
{
    struct aml_wdt_dev *wdev = watchdog_get_drvdata(wdog);
    mutex_lock(&wdev->lock);
///-

#if 0
	if(wdog->timeout==0xffffffff)
		enable_watchdog(wdev->default_timeout * wdev->one_second);
	else
		enable_watchdog(wdog->timeout* wdev->one_second);
#else
    if(wdog->timeout==0xffffffff)
    {
        enable_v_watchdog(wdev->default_timeout);
    }
    else
    {
        enable_v_watchdog(wdog->timeout);
    }
#endif
	mutex_unlock(&wdev->lock);
#if 0
	if(wdev->boot_queue)
		cancel_delayed_work(&wdev->boot_queue);
#endif
    return 0;
}

static int aml_wdt_stop(struct watchdog_device *wdog)
{
    struct aml_wdt_dev *wdev = watchdog_get_drvdata(wdog);
    mutex_lock(&wdev->lock);
///-
#if 0
	disable_watchdog();
#else
    disable_v_watchdog();
#endif
    mutex_unlock(&wdev->lock);
    return 0;
}

static int aml_wdt_ping(struct watchdog_device *wdog)
{
    struct aml_wdt_dev *wdev = watchdog_get_drvdata(wdog);
    mutex_lock(&wdev->lock);
///-
#if 0
	reset_watchdog();
#else
    reset_v_watchdog();
#endif
    mutex_unlock(&wdev->lock);

    return 0;
}

static int aml_wdt_set_timeout(struct watchdog_device *wdog,
				unsigned int timeout)
{
    struct aml_wdt_dev *wdev = watchdog_get_drvdata(wdog);

    mutex_lock(&wdev->lock);
    wdog->timeout = timeout;
    wdev->timeout = timeout;
    mutex_unlock(&wdev->lock);

    return 0;
}

static unsigned int aml_wdt_get_timeleft(struct watchdog_device *wdog)
{
    unsigned int timeout_t;
    struct aml_wdt_dev *wdev = watchdog_get_drvdata(wdog);
    mutex_lock(&wdev->lock);
///-
#if 0
	timeleft=read_watchdog_time();
#else
    timeout_t = wdev->timeout;
#endif
    mutex_unlock(&wdev->lock);
///-
#if 0
	return timeleft/wdev->one_second;
#else
 //   return timeleft;
#endif
    return timeout_t;
}

static void enable_wdt_timer(unsigned long data)
{
    //struct aml_wdt_dev *wdev=container_of(work,struct aml_wdt_dev,boot_queue.work);
    mod_timer(&awdtv->r_timer, jiffies + MAX_TIMEOUT * HZ/1000); 
    
    //schedule_delayed_work(&awdtv->boot_queue,
	//				 round_jiffies(msecs_to_jiffies(awdtv->reset_watchdog_time)));
    reset_watchdog();
}

static const struct watchdog_info aml_wdt_info = {
///-
	//.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING,
    .options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
    .identity = "aml Watchdog",
};

static const struct watchdog_ops aml_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= aml_wdt_start,
	.stop		= aml_wdt_stop,
	.ping		= aml_wdt_ping,
	.set_timeout	= aml_wdt_set_timeout,
	.get_timeleft   = aml_wdt_get_timeleft,
};
void aml_init_pdata(struct aml_wdt_dev *wdev)
{
///-
#if 0
	int ret;
	ret=of_property_read_u32(wdev->dev->of_node, "default_timeout", &wdev->default_timeout);
	if(ret){
		dev_err(wdev->dev, "dt probe default_timeout failed: %d using default value\n", ret);
		wdev->default_timeout=5;
	}
	ret=of_property_read_u32(wdev->dev->of_node, "reset_watchdog_method", &wdev->reset_watchdog_method);
	if(ret){
		dev_err(wdev->dev, "dt probe reset_watchdog_method failed: %d using default value\n", ret);
		wdev->reset_watchdog_method=1;
	}
	ret=of_property_read_u32(wdev->dev->of_node, "reset_watchdog_time", &wdev->reset_watchdog_time);
	if(ret){
		dev_err(wdev->dev, "dt probe reset_watchdog_time failed: %d using default value\n", ret);
		wdev->reset_watchdog_time=2;
	}
	
	ret=of_property_read_u32(wdev->dev->of_node, "shutdown_timeout", &wdev->shutdown_timeout);
	if(ret){
		dev_err(wdev->dev, "dt probe shutdown_timeout failed: %d using default value\n", ret);
		wdev->shutdown_timeout=10;
	}
	
	ret=of_property_read_u32(wdev->dev->of_node, "firmware_timeout", &wdev->firmware_timeout);
	if(ret){
		dev_err(wdev->dev, "dt probe firmware_timeout failed: %d using default value\n", ret);
		wdev->firmware_timeout=6;
	}
	
	ret=of_property_read_u32(wdev->dev->of_node, "suspend_timeout", &wdev->suspend_timeout);
	if(ret){
		dev_err(wdev->dev, "dt probe suspend_timeout failed: %d using default value\n", ret);
		wdev->suspend_timeout=6;
	}
#else
    wdev->default_timeout=V_DEF_TIMEOUT;
    wdev->reset_watchdog_method=0;
    wdev->reset_watchdog_time=MAX_TIMEOUT;
    wdev->shutdown_timeout=10;
    wdev->firmware_timeout=6;
    wdev->suspend_timeout=6;
#endif

	
    wdev->one_second= 1;
///-
	//wdev->max_timeout=MAX_TIMEOUT;
    wdev->max_timeout=V_MAX_TIMEOUT;
    wdev->min_timeout=1;
	
    printk("one-secod=%d,min_timeout=%d,max_timeout=%d,default_timeout=%d,reset_watchdog_method=%d,reset_watchdog_time=%d,shutdown_timeout=%d,firmware_timeout=%d,suspend_timeout=%d\n",
        wdev->one_second,wdev->min_timeout,wdev->max_timeout,
        wdev->default_timeout,wdev->reset_watchdog_method,
        wdev->reset_watchdog_time,wdev->shutdown_timeout,
        wdev->firmware_timeout,wdev->suspend_timeout);

    return;
}
#if 0
static int aml_wtd_pm_notify(struct notifier_block *nb, unsigned long event,
	void *dummy)
{
	
	if (event == PM_SUSPEND_PREPARE) {
		printk("set watch dog suspend timeout %d seconds\n",awdtv->suspend_timeout);
///-
#if 0
		enable_watchdog(awdtv->suspend_timeout*awdtv->one_second);
#else
        enable_v_watchdog(awdtv->suspend_timeout);
#endif
	} 
	if (event == PM_POST_SUSPEND){
		printk("resume watch dog finish\n");
///-
#if 0
		if(awdtv->timeout==0xffffffff)
			enable_watchdog(awdtv->default_timeout * awdtv->one_second);
		else
			enable_watchdog(awdtv->timeout* awdtv->one_second);
#else
        if(awdtv->timeout==0xffffffff)
            enable_v_watchdog(awdtv->default_timeout);
        else
            enable_v_watchdog(awdtv->timeout);
#endif
	}
	return NOTIFY_OK;
	
}

static int aml_wtd_reboot_notify(struct notifier_block *nb, unsigned long event,
	void *dummy)
{
	if (event == SYS_POWER_OFF) {
		printk("set watch dog shut down timeout %d seconds\n",awdtv->suspend_timeout);
		enable_watchdog(awdtv->shutdown_timeout*awdtv->one_second);
		aml_write_reg32(P_AO_RTI_STATUS_REG1, MESON_CHARGING_REBOOT);
	} 
	return NOTIFY_OK;
}


static struct notifier_block aml_wdt_pm_notifier = {
	.notifier_call = aml_wtd_pm_notify,
};
static struct notifier_block aml_wdt_reboot_notifier = {
	.notifier_call = aml_wtd_reboot_notify,
};
#endif
static void cancel_real_timer(unsigned long data)
{
    printk(KERN_EMERG "%s %d wtd timeout\n", __FUNCTION__, __LINE__);
    del_timer(&awdtv->r_timer);  
}

static int aml_wdt_probe(struct platform_device *pdev)
{
    int ret;
    struct watchdog_device *aml_wdt;
    struct aml_wdt_dev *wdev;

    aml_wdt = devm_kzalloc(&pdev->dev, sizeof(*aml_wdt), GFP_KERNEL);
    if (!aml_wdt)
    {
    	 ret = -ENOMEM;
        goto err_devm_kzalloc_aml_wdt;
    }
    wdev = devm_kzalloc(&pdev->dev, sizeof(*wdev), GFP_KERNEL);
    if (!wdev)
    {
	ret = -ENOMEM;
	goto err_devm_kzalloc_wdev;
    }
    wdev->dev		= &pdev->dev;
    mutex_init(&wdev->lock);
    aml_init_pdata(wdev);

    aml_wdt->info	      = &aml_wdt_info;
    aml_wdt->ops	      = &aml_wdt_ops;
    aml_wdt->min_timeout = wdev->min_timeout;
    aml_wdt->max_timeout = wdev->max_timeout;
    aml_wdt->timeout=0xffffffff;
    wdev->timeout=0xffffffff;
       
    ret = gpio_request(gpio_switch, name_switch);
    if (ret < 0)
    	goto err_switch;

    ret = gpio_request(gpio_wdi, name_wdi);
    if (ret < 0)
    	goto err_wdi;
    	
    ret = gpio_direction_output(gpio_switch, 1);
    if (ret < 0)
    	goto err_gpio;

    ret = gpio_direction_output(gpio_wdi, 1);
    if (ret < 0)
    	goto err_gpio;
    watchdog_set_drvdata(aml_wdt, wdev);
    platform_set_drvdata(pdev, aml_wdt);

///-
#if 0
	if(wdev->reset_watchdog_method==1)
	{
        INIT_DELAYED_WORK(&wdev->boot_queue, boot_moniter_work);
		mod_delayed_work(system_freezable_wq, &wdev->boot_queue,
					 round_jiffies(msecs_to_jiffies(wdev->reset_watchdog_time*1000)));
		enable_watchdog(wdev->default_timeout * wdev->one_second);
		printk("creat work queue for watch dog\n");
	}
#endif
    //INIT_DELAYED_WORK(&wdev->boot_queue, boot_moniter_work);
    ret = watchdog_register_device(aml_wdt);
    if (ret) 
    	goto err_gpio;
    awdtv=wdev;
///-
    
    init_timer(&wdev->v_timer);
    wdev->v_timer.function = cancel_real_timer;
    wdev->v_timer.expires = V_DEF_TIMEOUT* HZ/1000;

    init_timer(&wdev->r_timer);
    wdev->r_timer.function = enable_wdt_timer;
    wdev->r_timer.expires = MAX_TIMEOUT * HZ/1000;
    //pr_info("AML Watchdog Timer probed done \n");
    printk(KERN_ERR"AML Watchdog Timer probed done\n");
    
    /***关闭内核中启动的看门狗，定时器***/
    wdt_del_timer_and_disable(); 
    
    gpio_direction_output(gpio_wdi, 0);	
    gpio_direction_output(gpio_switch, 0);
    /**初始化中启动看门狗，是因为在主备间心跳的发送
         需要拉低gpio_switch引脚，此时必须启动看门狗，否则
         发生复位，且在系统运行时需一直保持gpio_switch为低
         ，当应用层关闭看门狗时，内核定时器接管看门狗*/
    mod_timer(&awdtv->r_timer, jiffies + MAX_TIMEOUT* HZ/1000);
      
    return ret;
err_gpio:
    gpio_free(gpio_wdi); 
err_wdi:
    gpio_free(gpio_switch);
err_switch:
    devm_kfree(&pdev->dev,wdev);
err_devm_kzalloc_wdev:
    devm_kfree(&pdev->dev,aml_wdt); 
err_devm_kzalloc_aml_wdt:
    return ret;  
}

static void aml_wdt_shutdown(struct platform_device *pdev)
{
	struct watchdog_device *wdog = platform_get_drvdata(pdev);
	struct aml_wdt_dev *wdev = watchdog_get_drvdata(wdog);
///-
#if 0
	if(wdev->reset_watchdog_method==1)
		cancel_delayed_work(&wdev->boot_queue);
	reset_watchdog();
#else
    reset_v_watchdog();
#endif
}

static int aml_wdt_remove(struct platform_device *pdev)
{
    struct watchdog_device *wdog = platform_get_drvdata(pdev);

    aml_wdt_stop(wdog);
    gpio_free(gpio_switch);
    gpio_free(gpio_wdi);
    watchdog_unregister_device(wdog);

    return 0;
}

#ifdef	CONFIG_PM
static int aml_wdt_suspend(struct platform_device *pdev, pm_message_t state)
{
///-
#if 0
	reset_watchdog();
#else
    reset_v_watchdog();
#endif
    return 0; 
}

static int aml_wdt_resume(struct platform_device *pdev)
{
///-
#if 0
	reset_watchdog();
#else
    reset_v_watchdog();
#endif
    return 0;
}

#else
#define	aml_wdt_suspend	NULL
#define	aml_wdt_resume		NULL
#endif

//MODULE_DEVICE_TABLE(of, aml_wdt_of_match);

static struct platform_driver aml_wdt_driver = {
	.probe		= aml_wdt_probe,
	.remove		= aml_wdt_remove,
	.shutdown	= aml_wdt_shutdown,
	.suspend	= aml_wdt_suspend,
	.resume		= aml_wdt_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "extern_wdt",
	},
};
static int __init aml_wdt_driver_init(void) 
{
    int err;

    /*避免启动看门狗失效*/
    //disable_watchdog();
    err = platform_driver_register(&aml_wdt_driver);
    if(err)
    	goto err_driver_register;

    platform_device = platform_device_register_simple(DRV_NAME, -1, sunxi_wdt_res, ARRAY_SIZE(sunxi_wdt_res));
    if(IS_ERR(platform_device)) {
    	err = PTR_ERR(platform_device);
    	goto err_platform_device;
    }
      
    return err;

err_platform_device:
    platform_driver_unregister(&aml_wdt_driver);
err_driver_register:
    return err;
} 
module_init(aml_wdt_driver_init); 
static void __exit aml_wdt_driver_exit(void) 
{ 
    platform_device_unregister(platform_device);
    platform_driver_unregister(&(aml_wdt_driver)); 	
} 
module_exit(aml_wdt_driver_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:aml_wdt");


