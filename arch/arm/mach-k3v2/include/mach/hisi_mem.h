#ifndef _HI_MEM_INCLUDE_H_
#define _HI_MEM_INCLUDE_H_

#include <linux/mm.h>
void __init hisi_allocate_memory_regions(void);

extern unsigned long hisi_reserved_codec_phymem;
extern unsigned long hisi_reserved_gpu_phymem;
extern unsigned long hisi_reserved_dumplog_phymem;
extern unsigned long hisi_reserved_camera_phymem;

#ifdef CONFIG_MACH_TC45MSU3
#define HISI_BASE_MEMORY_SIZE	(SZ_512M)
#else
#define HISI_BASE_MEMORY_SIZE	(SZ_1G)
#endif

#if defined(CONFIG_MACH_K3V2OEM1)
#define HISI_MEM_GPU_SIZE	(192 * SZ_1M + HISI_FRAME_BUFFER_SIZE)
#elif defined(CONFIG_MACH_TC45MSU3)
#define HISI_MEM_GPU_SIZE	(80 * SZ_1M)
#endif
#define HISI_PMEM_CAMERA_SIZE	(4 * SZ_1K)
#define HISI_MEM_CODEC_SIZE	(27 * SZ_1M)
#define HISI_PMEM_GRALLOC_SIZE	(56 * SZ_1M)
#define HISI_PMEM_DUMPLOG_SIZE	(2 * SZ_1M)

/* temp */
#define CAMERA_PREVIEW_BUF_BASE (hisi_reserved_camera_phymem)
#define CAMERA_PREVIEW_BUF_SIZ	(hisi_reserved_media_phymem)

/* alloc from HISI_MEM_GPU_SIZE */
#define HIGPU_BUF_BASE	(hisi_reserved_gpu_phymem)
#define HIGPU_BUF_SIZE	(HISI_MEM_GPU_SIZE - HISI_FRAME_BUFFER_SIZE)

#define HISI_FRAME_BUFFER_BASE    (HIGPU_BUF_BASE + HIGPU_BUF_SIZE)
/* 64 bytes odd align */
#ifdef CONFIG_LCD_TOSHIBA_MDW70
#define HISI_FRAME_BUFFER_SIZE	PAGE_ALIGN(1280 * 720 * 4 * 4)
#elif defined(CONFIG_LCD_PANASONIC_VVX10F002A00)
#define HISI_FRAME_BUFFER_SIZE	PAGE_ALIGN(1920 * 1200 * 4 * 4)
#elif defined(CONFIG_LCD_CMI_OTM1280A)
#define HISI_FRAME_BUFFER_SIZE	PAGE_ALIGN(1280 * 720 * 4 * 4)
#endif


unsigned long hisi_get_reserve_mem_size(void);
#endif /* end _HI_MEM_INCLUDE_H_ */

