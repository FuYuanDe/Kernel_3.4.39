#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <mach/platform.h>


#define BANK_MEM_SIZE		0x24
#define MUX_REGS_OFFSET		0x0
#define DATA_REGS_OFFSET	0x10
#define DLEVEL_REGS_OFFSET	0x14
#define PULL_REGS_OFFSET	0x1c

#define PINS_PER_BANK		32
#define MUX_PINS_PER_REG	8
#define MUX_PINS_BITS		4
#define MUX_PINS_MASK		0x0f
#define DATA_PINS_PER_REG	32
#define DATA_PINS_BITS		1
#define DATA_PINS_MASK		0x01


struct sunxi_pin_bank {
	void __iomem *membase;
	u32  pin_base;
	u8   nr_pins;
	char *name;
};


static struct sunxi_pin_bank sunxi_pin_banks[] = {
	{SUNXI_PIO_VBASE,   SUNXI_PB_BASE, 11, "PB"},
	{SUNXI_PIO_VBASE,   SUNXI_PC_BASE, 19, "PC"},
	{SUNXI_PIO_VBASE,   SUNXI_PD_BASE, 24, "PD"},
	{SUNXI_PIO_VBASE,   SUNXI_PE_BASE, 20, "PE"},
	{SUNXI_PIO_VBASE,   SUNXI_PF_BASE,  7, "PF"},
	{SUNXI_PIO_VBASE,   SUNXI_PG_BASE, 14, "PG"},
	{SUNXI_PIO_VBASE,   SUNXI_PH_BASE, 12, "PH"},
	{SUNXI_R_PIO_VBASE, SUNXI_PL_BASE, 13, "PL"},
};


struct cpld_uxep
{
    u32 CS3_N;
    u32 CS4_N;
    u32 RE_N;
    u32 WE_N;

    u32 ADDR0;
    u32 ADDR1;    
    u32 ADDR2;       //4
    u32 ADDR3;
    
    u32 DATA0;
    u32 DATA1;    
    u32 DATA2;
    u32 DATA3;
    u32 DATA4;
    u32 DATA5;
    u32 DATA6;
    u32 DATA7;
};



struct cpld_io_param
{
    u16 cs;
    u8  addr;
    u8  data;
};



/* module name and version */
#define SUNXI_CPLD_DRV_NAME             "cpld"
#define SUNXI_CPLD_DEV_NAME             "cpld"
#define SUNXI_CPLD_LICENSE              "GPL"
#define SUNXI_CPLD_VERSION              "0.0.1"
#define SUNXI_CPLD_OWNER                 "sunxi-cpld"


#define SUNXI_CPLD_IOC_MAGIC            0xE2
#define SUNXI_CPLD_IOC_READ             _IO(SUNXI_CPLD_IOC_MAGIC, 0)
#define SUNXI_CPLD_IOC_WRITE            _IO(SUNXI_CPLD_IOC_MAGIC, 1)


#define SUNXI_CPLD_DATA_LEN		        2
#define SUNXI_CPLD_MAX_REG			    32


static struct class *sunxi_cpld_class;
static int  sunxi_cpld_major =  234;

static struct cpld_uxep sunxi_cpld_uxep;
static spinlock_t		  sunxi_cpld_lock;
static struct gpio sunxi_cpld_board_ver_gpios[] = {
	{ GPIOF(6),  GPIOF_DIR_IN, "BOARD_V1" },
	{ GPIOF(5),  GPIOF_DIR_IN, "BOARD_V2" },
	{ GPIOF(4),  GPIOF_DIR_IN, "BOARD_V3" },
	{ GPIOF(3),  GPIOF_DIR_IN, "BOARD_V4" }
};

static struct gpio sunxi_cpld_local_bus_gpios[] = {
    { GPIOL(4),  GPIOF_DIR_OUT, "UEXP_CS3_N" },
    { GPIOG(4),  GPIOF_DIR_OUT, "UEXP_CS4_N" },
    { GPIOL(2),  GPIOF_DIR_OUT, "UEXP_RE_N" },
    { GPIOL(3),  GPIOF_DIR_OUT, "UEXP_WE_N" },
    
	{ GPIOG(0),  GPIOF_DIR_OUT, "UEXP_A0" },
	{ GPIOG(1),  GPIOF_DIR_OUT, "UEXP_A1" },
	{ GPIOG(2),  GPIOF_DIR_OUT, "UEXP_A2" },
	{ GPIOG(3),  GPIOF_DIR_OUT, "UEXP_A3" },
    
    { GPIOL(12), GPIOF_DIR_OUT, "UEXP_D0" },
    { GPIOL(11), GPIOF_DIR_OUT, "UEXP_D1" },
    { GPIOL(10), GPIOF_DIR_OUT, "UEXP_D2" },
    { GPIOL(9),  GPIOF_DIR_OUT, "UEXP_D3" },
    
    { GPIOL(8),  GPIOF_DIR_OUT, "UEXP_D4" },
    { GPIOL(7),  GPIOF_DIR_OUT, "UEXP_D5" },
    { GPIOL(6),  GPIOF_DIR_OUT, "UEXP_D6" },
    { GPIOL(5),  GPIOF_DIR_OUT, "UEXP_D7" },
};

extern void (*write_cpld_cs_ctl)(u16 cs, u8 addr, u8 data);

static struct sunxi_pin_bank *sunxi_pin_to_bank(u32 pin)
{
	u32 pin_offset = pin % SUNXI_BANK_SIZE;
	u32 pin_base = pin - pin_offset;
	u32 i;
	
	/* search target bank within pinctrl->desc banks */
	for (i = 0; i < ARRAY_SIZE(sunxi_pin_banks); i++) {
		if (pin_base == sunxi_pin_banks[i].pin_base) {
			return &(sunxi_pin_banks[i]);
		}
	}
    
	/* invalid pin number to seach bank */
	printk(KERN_ERR "seach pin [%d] target bank failed\n", pin);
    
	return NULL;
}

#define SUNXI_PIN_RESET_BIAS(pin) ((pin < SUNXI_PL_BASE) ? (pin) : (pin - SUNXI_PL_BASE))

static inline u32 sunxi_mux_reg(u32 pin)
{
	u8  bank = pin / PINS_PER_BANK;
	u32 offset = bank * BANK_MEM_SIZE;
    
	offset += MUX_REGS_OFFSET;
	offset += pin % PINS_PER_BANK / MUX_PINS_PER_REG * 0x04;
    
	return round_down(offset, 4);
}

static inline u32 sunxi_mux_offset(u32 pin)
{
	u32 pin_num = pin % MUX_PINS_PER_REG;
    
	return pin_num * MUX_PINS_BITS;
}

static inline u32 sunxi_data_reg(u32 pin)
{
	u8 bank = pin / PINS_PER_BANK;
	u32 offset = bank * BANK_MEM_SIZE;
    
	offset += DATA_REGS_OFFSET;
	offset += pin % PINS_PER_BANK / DATA_PINS_PER_REG * 0x04;
    
	return round_down(offset, 4);
}

static inline u32 sunxi_data_offset(u16 pin)
{
	u32 pin_num = pin % DATA_PINS_PER_REG;
    
	return pin_num * DATA_PINS_BITS;
}

int sunxi_pin_set_dir(u32 pin, u8 dir)
{
    struct sunxi_pin_bank *bank;
    u32 reg_val;
    u32 pin_bias;

    
    if (dir > 1) {
		printk(KERN_ERR "sunxi_pin_set_dir parameter error!\n");
		return -EINVAL;
	}
    
    bank = sunxi_pin_to_bank(pin);
    if(!bank) {
		printk(KERN_ERR "sunxi_pin_set_dir bank error!\n");
		return -EINVAL;
	}
    
    pin_bias = SUNXI_PIN_RESET_BIAS(pin);
    reg_val = readl(bank->membase + sunxi_mux_reg(pin_bias));

    if(0 == dir) {
        reg_val &= ~(0x7 << sunxi_mux_offset(pin_bias));
    }
    else {
        reg_val &= ~(0x7 << sunxi_mux_offset(pin_bias));
        reg_val |=  (0x1 << sunxi_mux_offset(pin_bias));
    }

    writel(reg_val, bank->membase + sunxi_mux_reg(pin_bias));

    return 0;
}
EXPORT_SYMBOL(sunxi_pin_set_dir);

int sunxi_pin_get_data(u32 pin)
{
    struct sunxi_pin_bank *bank;
    u32 reg_val;
    u32 pin_bias;
    int data;


    bank = sunxi_pin_to_bank(pin);
    if(!bank) {
		printk(KERN_ERR "sunxi_pin_set_data bank error!\n");
		return -EINVAL;
	}
    
    pin_bias = SUNXI_PIN_RESET_BIAS(pin);
    reg_val = readl(bank->membase + sunxi_data_reg(pin_bias));
    data = (reg_val >> sunxi_data_offset(pin_bias)) & DATA_PINS_MASK;
    
    return data;
}
EXPORT_SYMBOL(sunxi_pin_get_data);

int sunxi_pin_set_data(u32 pin, u8 data)
{
    struct sunxi_pin_bank *bank;
    u32 reg_val;
    u32 pin_bias;


    if (data > 1) {
		printk(KERN_ERR "sunxi_pin_set_data parameter error!\n");
		return -EINVAL;
	}


    bank = sunxi_pin_to_bank(pin);
    if(!bank) {
		printk(KERN_ERR "sunxi_pin_set_data bank error!\n");
		return -EINVAL;
	}
    
    pin_bias = SUNXI_PIN_RESET_BIAS(pin);
    reg_val = readl(bank->membase + sunxi_data_reg(pin_bias));

    reg_val &= ~(0x1 << sunxi_data_offset(pin_bias));
    reg_val |=  (data << sunxi_data_offset(pin_bias));

    writel(reg_val, bank->membase + sunxi_data_reg(pin_bias));

    return 0;
}
EXPORT_SYMBOL(sunxi_pin_set_data);



static int sunxi_cpld_gpio_init(void)
{
    int ret;

    
    ret = gpio_request_array(sunxi_cpld_local_bus_gpios, ARRAY_SIZE(sunxi_cpld_local_bus_gpios));
    if (IS_ERR_VALUE(ret)) {
        printk(KERN_ERR "request gpios failed, errno %d\n", ret);
        return -EINVAL;
    }

    sunxi_cpld_uxep.CS3_N = GPIOL(4);
    sunxi_cpld_uxep.CS4_N = GPIOG(4);
    sunxi_cpld_uxep.RE_N  = GPIOL(2);
    sunxi_cpld_uxep.WE_N  = GPIOL(3);

    sunxi_cpld_uxep.ADDR0 = GPIOG(0);
    sunxi_cpld_uxep.ADDR1 = GPIOG(1);
    sunxi_cpld_uxep.ADDR2 = GPIOG(2);
    sunxi_cpld_uxep.ADDR3 = GPIOG(3);

    sunxi_cpld_uxep.DATA0 = GPIOL(12);
    sunxi_cpld_uxep.DATA1 = GPIOL(11);
    sunxi_cpld_uxep.DATA2 = GPIOL(10);
    sunxi_cpld_uxep.DATA3 = GPIOL(9);
    
    sunxi_cpld_uxep.DATA4 = GPIOL(8);
    sunxi_cpld_uxep.DATA5 = GPIOL(7);
    sunxi_cpld_uxep.DATA6 = GPIOL(6);
    sunxi_cpld_uxep.DATA7 = GPIOL(5);


    sunxi_pin_set_dir(sunxi_cpld_uxep.CS3_N, 1);
    sunxi_pin_set_data(sunxi_cpld_uxep.CS3_N, 1);

    sunxi_pin_set_dir(sunxi_cpld_uxep.CS4_N, 1);
    sunxi_pin_set_data(sunxi_cpld_uxep.CS4_N, 1);

    sunxi_pin_set_dir(sunxi_cpld_uxep.RE_N, 1);
    sunxi_pin_set_data(sunxi_cpld_uxep.RE_N, 1);

    sunxi_pin_set_dir(sunxi_cpld_uxep.WE_N, 1);
    sunxi_pin_set_data(sunxi_cpld_uxep.WE_N, 1);

    sunxi_pin_set_dir(sunxi_cpld_uxep.ADDR0, 1);
    sunxi_pin_set_data(sunxi_cpld_uxep.ADDR0, 0);

    sunxi_pin_set_dir(sunxi_cpld_uxep.ADDR1, 1);
    sunxi_pin_set_data(sunxi_cpld_uxep.ADDR1, 0);

    sunxi_pin_set_dir(sunxi_cpld_uxep.ADDR2, 1);
    sunxi_pin_set_data(sunxi_cpld_uxep.ADDR2, 0);

    sunxi_pin_set_dir(sunxi_cpld_uxep.ADDR3, 1);
    sunxi_pin_set_data(sunxi_cpld_uxep.ADDR3, 0);
    
    return 0;
}

static void sunxi_cpld_dnelay_100ns(int n)
{
    int i = 20, j = 0;

    for (j = 0; j < n; j++) {
        while (i--)
            ;
        i = 20;
    }
}


static void sunxi_cpld_data_pin_dir(int dir)
{
    u32 reg_val;

    
    if(GPIOF_DIR_IN == dir)
    {
        reg_val = readl(SUNXI_R_PIO_VBASE + 0x00);
        reg_val &= ~((0x7 << 20) | (0x7 << 24) | (0x7 << 28));
        writel(reg_val, SUNXI_R_PIO_VBASE + 0x00);

        reg_val = readl(SUNXI_R_PIO_VBASE + 0x04);
        reg_val &= ~((0x7 <<  0) | (0x7 << 4) | (0x7 << 8) | (0x7 << 12) | (0x7 << 16));
        writel(reg_val, SUNXI_R_PIO_VBASE + 0x04);
    }
    else
    {
        reg_val = readl(SUNXI_R_PIO_VBASE + 0x00);
        reg_val &= ~((0x7 << 20) | (0x7 << 24) | (0x7 << 28));
        reg_val |=  ((0x1 << 20) | (0x1 << 24) | (0x1 << 28));
        writel(reg_val, SUNXI_R_PIO_VBASE + 0x00);

        reg_val = readl(SUNXI_R_PIO_VBASE + 0x04);
        reg_val &= ~((0x7 <<  0) | (0x7 << 4) | (0x7 << 8) | (0x7 << 12) | (0x7 << 16));
        reg_val |=  ((0x1 <<  0) | (0x1 << 4) | (0x1 << 8) | (0x1 << 12) | (0x1 << 16));
        writel(reg_val, SUNXI_R_PIO_VBASE + 0x04);
    }
}

void sunxi_cpld_read_data(u16 cs, u8 addr, u8* data)
{
	u8 val = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&sunxi_cpld_lock, flags);

    /*
    ** Step 1: set data GPIO dir
    */
    sunxi_cpld_data_pin_dir(GPIOF_DIR_IN);
    
	/*
    ** Step 2: set addr
    */
	val = (addr >> 0) & 1;
    sunxi_pin_set_data(sunxi_cpld_uxep.ADDR0, val);
	val = (addr >> 1) & 1;
	sunxi_pin_set_data(sunxi_cpld_uxep.ADDR1, val);
	val = (addr >> 2) & 1;
	sunxi_pin_set_data(sunxi_cpld_uxep.ADDR2, val);
	val = (addr >> 3) & 1;
	sunxi_pin_set_data(sunxi_cpld_uxep.ADDR3, val);
    
    /*
    ** Step 3: enable CS and RE
    */
	sunxi_pin_set_data(cs, 0);
	sunxi_pin_set_data(sunxi_cpld_uxep.RE_N, 0);
    
    /*
    ** Step 4: delay
    */
	sunxi_cpld_dnelay_100ns(1);

	/*
    ** Step 5: get data
    */
    *data = 0;
    
	val = sunxi_pin_get_data(sunxi_cpld_uxep.DATA0) & 1;
	*data |= (val << 0);
	val = sunxi_pin_get_data(sunxi_cpld_uxep.DATA1) & 1;
	*data |= (val << 1);
    val = sunxi_pin_get_data(sunxi_cpld_uxep.DATA2) & 1;
	*data |= (val << 2);
    val = sunxi_pin_get_data(sunxi_cpld_uxep.DATA3) & 1;
	*data |= (val << 3);

    val = sunxi_pin_get_data(sunxi_cpld_uxep.DATA4) & 1;
	*data |= (val << 4);
    val = sunxi_pin_get_data(sunxi_cpld_uxep.DATA5) & 1;
	*data |= (val << 5);
    val = sunxi_pin_get_data(sunxi_cpld_uxep.DATA6) & 1;
	*data |= (val << 6);
    val = sunxi_pin_get_data(sunxi_cpld_uxep.DATA7) & 1;
	*data |= (val << 7);

    /*
    ** Step 6: disable CS and RE
    */
	sunxi_pin_set_data(sunxi_cpld_uxep.RE_N, 1);
    sunxi_pin_set_data(cs, 1);

	spin_unlock_irqrestore(&sunxi_cpld_lock, flags);

    //printk(KERN_ERR "sunxi_cpld_read_data cs:%d addr:%d data:%d\n", cs, addr, *data);
}
EXPORT_SYMBOL(sunxi_cpld_read_data);

void sunxi_cpld_write_data(u16 cs, u8 addr, u8 data)
{
	u8 val = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&sunxi_cpld_lock, flags);
    /*
    ** Step 1: set data GPIO dir
    */
    sunxi_cpld_data_pin_dir(GPIOF_DIR_OUT);

	/*
    ** Step 2: set addr
    */
	val = (addr >> 0) & 1;
	sunxi_pin_set_data(sunxi_cpld_uxep.ADDR0, val);
	val = (addr >> 1) & 1;
	sunxi_pin_set_data(sunxi_cpld_uxep.ADDR1, val);
	val = (addr >> 2) & 1;
	sunxi_pin_set_data(sunxi_cpld_uxep.ADDR2, val);
	val = (addr >> 3) & 1;
	sunxi_pin_set_data(sunxi_cpld_uxep.ADDR3, val);

	/*
    ** Step 3: set data
    */
    val = (data >> 0) & 1;
	sunxi_pin_set_data(sunxi_cpld_uxep.DATA0, val);
    val = (data >> 1) & 1;
	sunxi_pin_set_data(sunxi_cpld_uxep.DATA1, val);
    val = (data >> 2) & 1;
	sunxi_pin_set_data(sunxi_cpld_uxep.DATA2, val);
    val = (data >> 3) & 1;
	sunxi_pin_set_data(sunxi_cpld_uxep.DATA3, val);

    val = (data >> 4) & 1;
	sunxi_pin_set_data(sunxi_cpld_uxep.DATA4, val);
    val = (data >> 5) & 1;
	sunxi_pin_set_data(sunxi_cpld_uxep.DATA5, val);
    val = (data >> 6) & 1;
	sunxi_pin_set_data(sunxi_cpld_uxep.DATA6, val);
    val = (data >> 7) & 1;
	sunxi_pin_set_data(sunxi_cpld_uxep.DATA7, val);

    /*
    ** Step 4: enable CS and WE
    */
	sunxi_pin_set_data(cs, 0);
	sunxi_pin_set_data(sunxi_cpld_uxep.WE_N, 0);
    
    /*
    ** Step 5: delay
    */
	//sunxi_cpld_dnelay_100ns(1);

    /*
    ** Step 6: disable CS and RE
    */
	sunxi_pin_set_data(sunxi_cpld_uxep.WE_N, 1);
    sunxi_pin_set_data(cs, 1);

	spin_unlock_irqrestore(&sunxi_cpld_lock, flags);
    //printk(KERN_ERR "sunxi_cpld_write_data cs:%d addr:%d data:%d\n", cs, addr, data);
}
EXPORT_SYMBOL(sunxi_cpld_write_data);

static long sunxi_cpld_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct cpld_io_param sunxi_cpld_io_param;

    
    switch(cmd)
    {
        case SUNXI_CPLD_IOC_READ:
            if(copy_from_user(&sunxi_cpld_io_param, (struct cpld_io_param __user *)arg, sizeof(struct cpld_io_param)))
    		{
    		    printk(KERN_ERR "%s %d: copy param from user failed.\n", __FUNCTION__, __LINE__);
    			return -EFAULT;
    		}

            sunxi_cpld_read_data(sunxi_cpld_io_param.cs, sunxi_cpld_io_param.addr, &sunxi_cpld_io_param.data);

    		if(copy_to_user((struct cpld_io_param __user *)arg, &sunxi_cpld_io_param, sizeof(struct cpld_io_param)))
    		{
    		    printk(KERN_ERR "%s %d: copy to to user failed.\n", __FUNCTION__, __LINE__);
    			return -EFAULT;
    		}
            break;
        case SUNXI_CPLD_IOC_WRITE:
            if(copy_from_user(&sunxi_cpld_io_param, (struct cpld_io_param __user *)arg, sizeof(struct cpld_io_param)))
    		{
    		    printk(KERN_ERR "%s %d: copy param from user failed.\n", __FUNCTION__, __LINE__);
    			return -EFAULT;
    		}

            sunxi_cpld_write_data(sunxi_cpld_io_param.cs, sunxi_cpld_io_param.addr, sunxi_cpld_io_param.data);
            break;    
        default:
            printk(KERN_WARNING "%s Warning: ioctl cmd[%x] undefined\n", __FUNCTION__, cmd);
            break;
    }

	return 0;
}

static ssize_t sunxi_cpld_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{
	u8  data[SUNXI_CPLD_DATA_LEN];
	int ret = 0;

    
	if(count != SUNXI_CPLD_DATA_LEN){
		printk(KERN_ERR "cpld read len:%u error\n", count);
		return 0;
	}

	if(copy_from_user(data, buf, count)){
		ret =  - EFAULT;
		goto  read_fail;
	}

    //printk(KERN_ERR "sunxi_cpld_read len:%u data:[%d][%d]\n", count, data[0], data[1]);
    
    if(data[0] < SUNXI_CPLD_MAX_REG)
    {
	    sunxi_cpld_read_data(sunxi_cpld_uxep.CS3_N, data[0], (data+1));
    }
    else
    {
        sunxi_cpld_read_data(sunxi_cpld_uxep.CS4_N, data[0] - SUNXI_CPLD_MAX_REG, (data+1));
    }

    
	if (copy_to_user(buf, data, count)){
		ret =  - EFAULT;
	}
	else
		return  SUNXI_CPLD_DATA_LEN;

read_fail:
	return ret;
}

static ssize_t sunxi_cpld_write(struct file *filp, const char __user *buf, size_t count, loff_t *offset)
{
	u8  data[SUNXI_CPLD_DATA_LEN];
	int ret = 0;

    
	if(count != SUNXI_CPLD_DATA_LEN){
		printk(KERN_ERR "ag cpld write len:%u error\n",count);
		return 0;
	}

	if(copy_from_user(data,buf, count)){
		ret =  - EFAULT;
		goto write_fail;
	}

    //printk(KERN_ERR "sunxi_cpld_write len:%u data:[%d][%d]\n", count, data[0], data[1]);
    
    if(data[0] < SUNXI_CPLD_MAX_REG)
    {
	    sunxi_cpld_write_data(sunxi_cpld_uxep.CS3_N, data[0], data[1]);
    }
    else
    {
        sunxi_cpld_write_data(sunxi_cpld_uxep.CS4_N, data[0] - SUNXI_CPLD_MAX_REG, data[1]);
    }
	return SUNXI_CPLD_DATA_LEN;

write_fail:
	return ret;
}


struct file_operations sunxi_cpld_fops = {
    read          : sunxi_cpld_read,
	write         : sunxi_cpld_write,
	unlocked_ioctl: sunxi_cpld_ioctl,
};

int sunxi_cpld_board_ver(void)
{
    int i, gpio, val, ver = 0;
    int ret;

    ret = gpio_request_array(sunxi_cpld_board_ver_gpios, ARRAY_SIZE(sunxi_cpld_board_ver_gpios));
    if (IS_ERR_VALUE(ret)) {
        printk(KERN_ERR "request gpios failed, errno %d\n", ret);
        return -EINVAL;
    }

    for(i=0; i<ARRAY_SIZE(sunxi_cpld_board_ver_gpios); i++) {
        gpio = sunxi_cpld_board_ver_gpios[i].gpio;
        val = gpio_get_value(gpio);

        ver |= val << i;

        //printk(KERN_ERR "gpios %d val %d\n", gpio, val);
    }

    printk(KERN_ERR "board ver %d\n", ver);

    return ver;
}
EXPORT_SYMBOL(sunxi_cpld_board_ver);
static int __init sunxi_cpld_probe(struct platform_device *pdev)
{
    int result = 0;
    struct device *class_device;
	spin_lock_init(&sunxi_cpld_lock);
    
    result = register_chrdev(sunxi_cpld_major, SUNXI_CPLD_DEV_NAME, &sunxi_cpld_fops);
    if (result < 0) {
        printk(KERN_ERR "sunxi cpld: can't get major %d\n", sunxi_cpld_major);
        return result;
    }

    if (sunxi_cpld_major == 0) {
        sunxi_cpld_major = result; /* dynamic */
    }

    sunxi_cpld_class = class_create(THIS_MODULE, SUNXI_CPLD_DEV_NAME);
    if(IS_ERR(sunxi_cpld_class))
    {
        printk(KERN_ERR "sunxi_cpld: class_create %s ERROR!\n", SUNXI_CPLD_DEV_NAME);
        return -EFAULT;
    }
    
    class_device = device_create(sunxi_cpld_class, NULL, MKDEV(sunxi_cpld_major, 0), NULL, SUNXI_CPLD_DEV_NAME);
    if(IS_ERR(class_device))
    {
        printk(KERN_ERR "sunxi_cpld: device_create %s ERROR!\n", SUNXI_CPLD_DEV_NAME);
        return -EFAULT;
    }

    //sunxi_cpld_board_ver();
    sunxi_cpld_gpio_init();

    printk(KERN_ERR "%s\n", __FUNCTION__);

	return 0;
}

static int __exit sunxi_cpld_remove(struct platform_device *pdev)
{
    printk(KERN_ERR "%s\n", __FUNCTION__);


    gpio_free_array(sunxi_cpld_board_ver_gpios, ARRAY_SIZE(sunxi_cpld_board_ver_gpios));
    gpio_free_array(sunxi_cpld_local_bus_gpios, ARRAY_SIZE(sunxi_cpld_local_bus_gpios));

    device_destroy(sunxi_cpld_class, MKDEV(sunxi_cpld_major, 0));
    class_destroy(sunxi_cpld_class);
    unregister_chrdev(sunxi_cpld_major, SUNXI_CPLD_DEV_NAME);
    
	return 0;
}

static void sunxi_cpld_release(struct device * dev)
{
    printk(KERN_ERR "%s\n", __FUNCTION__);
    return;
}

static struct platform_device sunxi_cpld_device = {
	.name 	= SUNXI_CPLD_DEV_NAME,
	.id 	= PLATFORM_DEVID_NONE,
	.dev = {
        .release = sunxi_cpld_release,
    }
};

static struct platform_driver sunxi_cpld_driver = {
	.probe  = sunxi_cpld_probe,
	.remove = sunxi_cpld_remove,
	.driver = {
		.name  = SUNXI_CPLD_DRV_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init sunxi_cpld_init(void)
{
	int err = 0;

    if((err = platform_device_register(&sunxi_cpld_device)) < 0)
        return err;

    if((err = platform_driver_register(&sunxi_cpld_driver)) < 0)
        return err;
	write_cpld_cs_ctl = sunxi_cpld_write_data;
    printk(KERN_ERR "%s\n", __FUNCTION__);

    return 0;
}

static void __exit sunxi_cpld_exit(void)
{
    printk(KERN_ERR "%s\n", __FUNCTION__);
    
    platform_driver_unregister(&sunxi_cpld_driver);
    platform_device_unregister(&sunxi_cpld_device);
    write_cpld_cs_ctl = NULL;
}

module_init(sunxi_cpld_init);
module_exit(sunxi_cpld_exit);

MODULE_LICENSE(SUNXI_CPLD_LICENSE);
MODULE_VERSION(SUNXI_CPLD_VERSION);

