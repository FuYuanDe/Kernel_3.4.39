#########################################################################
# $Id$
#########################################################################

#obj-$(CONFIG_SUN4I_NANDFLASH_TEST) +=nand_test.o
obj-m += nand_test.o

LICHEE_KDIR=/home/samba/r58-tina/tina/build_dir/target-arm_cortex-a7+neon_uClibc-0.9.33.2_eabi/linux-sunxi/linux-3.4.39
LICHEE_MOD_DIR=$(shell pwd)

all:
	$(MAKE) -C $(LICHEE_KDIR) M=$(PWD)

