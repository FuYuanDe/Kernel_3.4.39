/*
 * main.c
 *
 * Copyright (C) 2013 Qiang Yu <yuq825@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <mach/sys_config.h>

#include "defs.h"
#include "nfc.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("li");

#define	DRIVER_NAME	"mtd-nand-sunxi"

struct sunxi_nand_info {
	struct mtd_info mtd;
	struct nand_chip nand;
};

#define SUNXI_NAND_PART_NUM         8

static struct mtd_partition sunxi_nand_parts[SUNXI_NAND_PART_NUM] =
{
    {
        name:       "fpga",
        size:       0x400000,
        offset:     0,
    },
    {
        name:       "fpga_bak",
        size:       0x400000,
        offset:     MTDPART_OFS_APPEND,
    },
    {
        name:       "firmware_kernel",
        size:       0x800000,
        offset:     MTDPART_OFS_APPEND,
    },
    {
        name:       "firmware_rootfs",
        size:       0x4000000,
        offset:     MTDPART_OFS_APPEND,
    },
    {
        name:       "firmwarebak_kernel",
        size:       0x800000,
        offset:     MTDPART_OFS_APPEND,
    },
    {
        name:       "firmwarebak_rootfs",
        size:       0x4000000,
        offset:     MTDPART_OFS_APPEND,
    },
    {
        name:       "rootfs_data",
        size:       0x8000000,
        offset:     MTDPART_OFS_APPEND,
    },
    {
        name:       "data",
        size:       MTDPART_SIZ_FULL,
        offset:     MTDPART_OFS_APPEND,
    }
};

static int __devinit nand_probe(struct platform_device *pdev)
{
	int err;
	struct sunxi_nand_info *info;

	if ((info = kzalloc(sizeof(*info), GFP_KERNEL)) == NULL) {
		ERR_INFO("alloc nand info fail\n");
		err = -ENOMEM;
		goto out;
	}

	info->mtd.priv = &info->nand;
	info->mtd.name = dev_name(&pdev->dev);
	info->mtd.owner = THIS_MODULE;

	if ((err = nfc_first_init(&info->mtd)) < 0) {
		ERR_INFO("nfc first inti fail\n");
		goto out_free_info;
	}

	// first scan to find the device and get the page size
	if ((err = nand_scan_ident(&info->mtd, 1, NULL)) < 0) {
		ERR_INFO("nand scan ident fail\n");
		goto out_nfc_exit;
	}

	// init NFC with flash chip info got from first scan
	if ((err = nfc_second_init(&info->mtd)) < 0) {
		ERR_INFO("nfc second init fail\n");
		goto out_nfc_exit;
	}

	// second phase scan
	if ((err = nand_scan_tail(&info->mtd)) < 0) {
		ERR_INFO("nand scan tail fail\n");
		goto out_nfc_exit;
	}

	if ((err = mtd_device_parse_register(&info->mtd, NULL, NULL, sunxi_nand_parts, SUNXI_NAND_PART_NUM)) < 0) {
		ERR_INFO("register mtd device fail\n");
		goto out_release_nand;
	}

	platform_set_drvdata(pdev, info);
	return 0;

out_release_nand:
	nand_release(&info->mtd);
out_nfc_exit:
	nfc_exit(&info->mtd);
out_free_info:
	kfree(info);
out:
	return err;
}

static int __devexit nand_remove(struct platform_device *pdev)
{
	struct sunxi_nand_info *info = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	mtd_device_unregister(&info->mtd);
	nand_release(&info->mtd);
	nfc_exit(&info->mtd);
	kfree(info);
	return 0;
}

static void nand_shutdown(struct platform_device *pdev)
{

}

static int nand_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int nand_resume(struct platform_device *pdev)
{
	return 0;
}

static void	nfc_dev_release(struct device *dev)
{

}

static struct platform_driver plat_driver = {
	.probe		= nand_probe,
	.remove		= nand_remove,
	.shutdown   = nand_shutdown,
	.suspend    = nand_suspend,
	.resume     = nand_resume,
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};

static struct platform_device plat_device = {
	.name = DRIVER_NAME,
	.id = 0,
	.dev = {
		.release = nfc_dev_release,
	},
};

int nand1k_init(void);
void nand1k_exit(void);

static int __init nand_init(void)
{
	int err;
	int nand_used = 0;
	script_item_u   val;
	script_item_value_type_e  type;

#if defined(PRODUCT_SBCUSER) || defined(PRODUCT_MTG2500USER) || \
    defined(PRODUCT_AG) || defined(PRODUCT_SBC300USER) || \
    defined(PRODUCT_SBC1000USER)
    return 0;
#endif
///-
    printk(KERN_ERR "%s %d------------------------\n", __FUNCTION__, __LINE__);

#if 0
    type = script_get_item("nand_para", "nand_used", &val);
    if(SCIRPT_ITEM_VALUE_TYPE_INT != type)
    {
    	ERR_INFO("nand init fetch emac using configuration failed\n");
    }
    else
    {
        nand_used = val.val;
    }
#else
    nand_used = 1;
#endif

    if(nand_used == 0) {
        DBG_INFO("nand driver is disabled \n");
        return 0;
    }

	DBG_INFO("nand driver, init.\n");

	if ((err = platform_driver_register(&plat_driver)) != 0) {
		ERR_INFO("platform_driver_register fail \n");
		return err;
	}
	DBG_INFO("nand driver, ok.\n");

	// add an NFC, may be should be done by platform driver
	if ((err = platform_device_register(&plat_device)) < 0) {
		ERR_INFO("platform_device_register fail\n");
		return err;
	}
	DBG_INFO("nand device, ok.\n");

///-
#if 0
	if (nand1k_init()) {
		ERR_INFO("nand1k module init fail\n");
	}
#endif

	return 0;
}

static void __exit nand_exit(void)
{
	int nand_used = 0;
	script_item_u   val;
	script_item_value_type_e  type;

///-
#if 0
    type = script_get_item("nand_para", "nand_used", &val);
    if(SCIRPT_ITEM_VALUE_TYPE_INT != type)
    {
    	ERR_INFO("nand init fetch emac using configuration failed\n");
    }
    else
    {
        nand_used = val.val;
    }
#else
    nand_used = 0;
#endif
    
    if(nand_used == 0) {
        DBG_INFO("nand driver is disabled \n");
        return;
    }

	nand1k_exit();
	platform_device_unregister(&plat_device);
	platform_driver_unregister(&plat_driver);
	DBG_INFO("nand driver : bye bye\n");
}

module_init(nand_init);
module_exit(nand_exit);




