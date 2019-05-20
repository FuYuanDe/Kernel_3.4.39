
################################################################################
#
# Common Variables that already set:
#     LICHEE_KDIR
#     LICHEE_MOD_DIR
#     CROSS_COMPILE
#     ARCH
#
#################################################################################
LICHEE_KDIR=/home/samba/r58-tina/tina/build_dir/target-arm_cortex-a7+neon_uClibc-0.9.33.2_eabi/linux-sunxi/linux-3.4.39
LICHEE_MOD_DIR=$(shell pwd)
ARCH=arm
CROSS_COMPILE=/home/samba/r58-tina/tina/staging_dir/toolchain-arm_cortex-a7+neon_gcc-4.8-linaro_uClibc-0.9.33.2_eabi/bin/arm-openwrt-linux-gcc
PWD=$(shell pwd)
			 
ifneq ($(strip $(CONFIG_ARCH_SUN8IW1P1)),)
obj-m += sun8iw1p1/
endif

ifneq ($(strip $(CONFIG_ARCH_SUN8IW3P1)),)
obj-m += sun8iw3p1/
endif

ifneq ($(strip $(CONFIG_ARCH_SUN9IW1P1)),)
obj-m += sun9iw1p1/
endif

ifneq ($(strip $(CONFIG_ARCH_SUN8IW5P1)),)
obj-m += sun8iw5p1/
endif

ifneq ($(strip $(CONFIG_ARCH_SUN8IW6P1)),)
obj-m += sun8iw6p1/
endif

ifneq ($(strip $(CONFIG_ARCH_SUN8IW7P1)),)
obj-m += sun8iw7p1/
endif

ifneq ($(strip $(CONFIG_ARCH_SUN8IW9P1)),)
obj-m += sun8iw9p1/
endif

install: build	
	@if cat ../../.config|grep "CONFIG_ARCH_SUN8IW1P1=y" >/dev/null 2>&1;then \
	cp sun8iw1p1/nand.ko $(LICHEE_MOD_DIR)/;\
	fi
	@if cat ../../.config|grep "CONFIG_ARCH_SUN8IW3P1=y" >/dev/null 2>&1;then \
	cp sun8iw3p1/nand.ko $(LICHEE_MOD_DIR)/;\
	fi
	@if cat ../../.config|grep "CONFIG_ARCH_SUN9IW1P1=y" >/dev/null 2>&1;then \
	cp sun9iw1p1/nand.ko $(LICHEE_MOD_DIR)/;\
	fi 
	@if cat ../../.config|grep "CONFIG_ARCH_SUN8IW5P1=y" >/dev/null 2>&1;then \
	cp sun8iw5p1/nand.ko $(LICHEE_MOD_DIR)/;\
	fi
	@if cat ../../.config|grep "CONFIG_ARCH_SUN8IW6P1=y" >/dev/null 2>&1;then \
	cp sun8iw6p1/nand.ko $(LICHEE_MOD_DIR)/;\
	fi	 
	@if cat ../../.config|grep "CONFIG_ARCH_SUN8IW7P1=y" >/dev/null 2>&1;then \
	cp sun8iw7p1/nand.ko $(LICHEE_MOD_DIR)/;\
	fi
	@if cat ../../.config|grep "CONFIG_ARCH_SUN8IW9P1=y" >/dev/null 2>&1;then \
	cp sun8iw9p1/nand.ko $(LICHEE_MOD_DIR)/;\
	fi
	
build:
	@if cat ../../.config|grep "CONFIG_ARCH_SUN8IW1P1=y" >/dev/null 2>&1;then \
	cp sun8iw1p1/libnand_sun8iw1p1 sun8iw1p1/libnand_sun8iw1p1.a;\
	fi
	@if cat ../../.config|grep "CONFIG_ARCH_SUN8IW3P1=y" >/dev/null 2>&1;then \
	cp sun8iw3p1/libnand_sun8iw3p1 sun8iw3p1/libnand_sun8iw3p1.a;\
	fi
	@if cat ../../.config|grep "CONFIG_ARCH_SUN9IW1P1=y" >/dev/null 2>&1;then \
	cp sun9iw1p1/libnand_sun9iw1p1 sun9iw1p1/libnand_sun9iw1p1.a;\
	fi	
	@if cat ../../.config|grep "CONFIG_ARCH_SUN8IW5P1=y" >/dev/null 2>&1;then \
	cp sun8iw5p1/libnand_sun8iw5p1 sun8iw5p1/libnand_sun8iw5p1.a;\
	fi
	@if cat ../../.config|grep "CONFIG_ARCH_SUN8IW6P1=y" >/dev/null 2>&1;then \
	cp sun8iw6p1/libnand_sun8iw6p1 sun8iw6p1/libnand_sun8iw6p1.a;\
	fi
	@if cat ../../.config|grep "CONFIG_ARCH_SUN8IW7P1=y" >/dev/null 2>&1;then \
	cp sun8iw7p1/libnand_sun8iw7p1 sun8iw7p1/libnand_sun8iw7p1.a;\
	fi	
	@if cat ../../.config|grep "CONFIG_ARCH_SUN8IW9P1=y" >/dev/null 2>&1;then \
	cp sun8iw9p1/libnand_sun8iw9p1 sun8iw9p1/libnand_sun8iw9p1.a;\
	fi
	$(MAKE) -C $(LICHEE_KDIR) M=$(PWD)

clean:
	@rm -rf *.o *.ko .*.cmd *.mod.c *.order *.symvers .tmp_versions *~
