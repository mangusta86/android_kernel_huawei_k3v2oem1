#Android makefile to build kernel as a part of Android Build

#ifeq ($(TARGET_PREBUILT_KERNEL),)

KERNEL_OUT := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ
KERNEL_CONFIG := $(KERNEL_OUT)/.config
TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/arch/arm/boot/zImage

$(KERNEL_OUT):
	mkdir -p $(KERNEL_OUT)

$(KERNEL_CONFIG): $(KERNEL_OUT)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-linux-androideabi- $(KERNEL_DEFCONFIG)

$(TARGET_PREBUILT_KERNEL): $(KERNEL_CONFIG)
	$(hide) $(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-linux-androideabi- -j 18
	$(hide) $(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-linux-androideabi- -j 18 zImage

kernelconfig: $(KERNEL_OUT)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-linux-androideabi- $(KERNEL_DEFCONFIG) menuconfig
