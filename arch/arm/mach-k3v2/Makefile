#
# Makefile for the linux kernel.
#
obj-y				:= common.o lm.o k3v2_clocks_ops.o k3v2_clocks_cs.o k3v2_clocks_es.o k3v2_clocks_init_data_es.o k3v2_clocks_init_data_cs.o
obj-y				+= timer.o io.o irq.o clock.o pmem.o k3v2_irq_affinity.o
obj-y                           += k3v2_regulators_vcc.o regulators_vcc.o
obj-y                           += k3v2_iomux.o iomux.o k3v2_iomux_blocks_u9510.o
obj-$(CONFIG_PCI)		+= pcie.o 

obj-$(CONFIG_ARCH_PROVIDES_UDELAY) += delay.o
obj-$(CONFIG_MACH_TC45MSU3)	+= board-tc45msu3.o
obj-$(CONFIG_MACH_K3V2OEM1)	+= board-k3v2oem1.o
obj-$(CONFIG_MACH_K3V2OEM2)	+= board-k3v2oem2.o
obj-$(CONFIG_SMP)		+= platsmp.o headsmp.o
obj-$(CONFIG_HOTPLUG_CPU)	+= hotplug.o
obj-$(CONFIG_LOCAL_TIMERS)	+= localtimer.o
obj-$(CONFIG_K3V2_DMA)          += dma.o
obj-$(CONFIG_PM)		+= hilpm-cpugodp.o misc_assembly.o pm.o
obj-$(CONFIG_USB_SUPPORT)	+= usb_phy.o
obj-$(CONFIG_CPU_FREQ)		+= cpu-k3v2.o
obj-$(CONFIG_IPPS_SUPPORT)	+= ipps-core.o ipps2.o
obj-$(CONFIG_PM)		+= lowpmregs.o
obj-$(CONFIG_XMM_POWER)         += xmm_power.o
obj-$(CONFIG_WIFI_DEVICE)	+= k3v2_wifi_power.o
obj-$(CONFIG_XMM_RMNET)         += xmd_rmnet.o
