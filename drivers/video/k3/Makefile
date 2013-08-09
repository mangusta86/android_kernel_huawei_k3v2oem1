obj-$(CONFIG_FB_K3_CLCD) := k3fb.o
#obj-m := k3fb.o

k3fb-objs := edc_reg.o \
	ldi_reg.o \
	mipi_reg.o \
	sbl_reg.o \
	k3_fb_panel.o \
	edc_overlay.o \
	mipi_dsi_host.o \
	k3_fb.o \
	ldi.o \
	mipi_dsi.o \
	backlight_pwm.o \
	mipi_toshiba_MDW70_V001.o \
	mipi_cmi_OTM1280A.o

EXTRA_CFLAGS += -Iarch/arm/mach-k3v2

KERNEL_MAKE := -C /home/z00176046/v200sdk/kernel

all:
	make $(KERNEL_MAKE) M=$(PWD) modules
clean:
	make $(KERNEL_MAKE) M=$(PWD) clean
#	rm *.o .*cmd
