#ifndef _HI_MEM_INCLUDE_H_
#define _HI_MEM_INCLUDE_H_

#include <linux/mm.h>
void __init hisi_allocate_memory_regions(void);

extern unsigned long hisi_reserved_codec_phymem;
extern unsigned long hisi_reserved_gpu_phymem;
extern unsigned long hisi_reserved_frame_buffer_phymem;
extern unsigned long hisi_reserved_dumplog_phymem;
extern unsigned long hisi_reserved_camera_phymem;

#ifdef CONFIG_MACH_TC45MSU3
#define HISI_BASE_MEMORY_SIZE	(SZ_512M)
#else
#define HISI_BASE_MEMORY_SIZE	(SZ_1G)
#endif

#define HISI_MEM_GPU_SIZE	(HIGPU_BUF_SIZE)
#define HISI_MEM_FRAME_BUFFER_SIZE (HISI_FRAME_BUFFER_SIZE)
#define HISI_PMEM_CAMERA_SIZE	(4 * SZ_1K)
#define HISI_MEM_CODEC_SIZE	(27 * SZ_1M)
#define HISI_PMEM_GRALLOC_SIZE	(56 * SZ_1M)
#define HISI_PMEM_DUMPLOG_SIZE	(2 * SZ_1M)
#if defined(CONFIG_LCD_1080P)
    #define HISI_MEM_COPYBIT_SIZE   (18 * SZ_1M)
#else
    #define HISI_MEM_COPYBIT_SIZE   (11 * SZ_1M)
#endif
/* OverlayCompose */
#if defined(CONFIG_OVERLAY_COMPOSE)
#  if defined(CONFIG_LCD_1080P)
#  define HISI_PMEM_OVERLAY_SIZE	(144 * SZ_1M)
#  else
#  define HISI_PMEM_OVERLAY_SIZE	(64 * SZ_1M)
#  endif
#else
#define HISI_PMEM_OVERLAY_SIZE	(0)
#endif

/* temp */
#define CAMERA_PREVIEW_BUF_BASE (hisi_reserved_camera_phymem)
#define CAMERA_PREVIEW_BUF_SIZ	(hisi_reserved_media_phymem)

/* alloc from HISI_MEM_GPU_SIZE */
#define HIGPU_BUF_BASE	(hisi_reserved_gpu_phymem)
#define HISI_FRAME_BUFFER_BASE (hisi_reserved_frame_buffer_phymem)
#if defined(CONFIG_OVERLAY_COMPOSE)
#  if defined(CONFIG_LCD_1080P)
#  define HIGPU_BUF_SIZE	(36 * SZ_1M + HISI_MEM_COPYBIT_SIZE)
#  else
#  define HIGPU_BUF_SIZE	(128 * SZ_1M + HISI_MEM_COPYBIT_SIZE)
#  endif
#else
#  if defined(CONFIG_LCD_1080P)
#  define HIGPU_BUF_SIZE	(100 * SZ_1M + HISI_MEM_COPYBIT_SIZE)
#  else
#  define HIGPU_BUF_SIZE	(192 * SZ_1M + HISI_MEM_COPYBIT_SIZE)
#  endif
#endif

#ifndef ALIGN_UP
#define ALIGN_UP(x, align) (((x) + ((align)-1)) & ~((align)-1))
#endif
#ifndef ALIGN_DOWN
#define ALIGN_DOWN(x, align)  ((x) & ~((align)-1))
#endif

/* FB width must be 16 pixels align */
#define NUM_FRAME_BUFFERS  4
#ifdef CONFIG_LCD_TOSHIBA_MDW70
#define HISI_FRAME_BUFFER_SIZE	PAGE_ALIGN(1280 * ALIGN_UP(720, 16) * 4 * NUM_FRAME_BUFFERS)
#elif defined(CONFIG_LCD_PANASONIC_VVX10F002A00)
#define HISI_FRAME_BUFFER_SIZE	PAGE_ALIGN(1200 * ALIGN_UP(1920, 16) * 4 * NUM_FRAME_BUFFERS)
#elif defined(CONFIG_LCD_CMI_OTM1280A)
#define HISI_FRAME_BUFFER_SIZE	PAGE_ALIGN(1280 * ALIGN_UP(720, 16) * 4 * NUM_FRAME_BUFFERS)
#elif defined(CONFIG_LCD_SAMSUNG_S6E39A)
#define HISI_FRAME_BUFFER_SIZE	PAGE_ALIGN(960 * ALIGN_UP(540, 16) * 4 * NUM_FRAME_BUFFERS)
#elif defined(CONFIG_LCD_SAMSUNG_LMS350DF04)
#define HISI_FRAME_BUFFER_SIZE	PAGE_ALIGN(480 * ALIGN_UP(320, 16) * 4 * NUM_FRAME_BUFFERS)
#elif defined(CONFIG_LCD_SHARP_LS035B3SX)
#define HISI_FRAME_BUFFER_SIZE	PAGE_ALIGN(960 * ALIGN_UP(640, 16) * 4 * NUM_FRAME_BUFFERS)
#elif defined(CONFIG_LCD_JDI_OTM1282B)
#define HISI_FRAME_BUFFER_SIZE	PAGE_ALIGN(1280 * ALIGN_UP(720, 16) * 4 * NUM_FRAME_BUFFERS)
#elif defined(CONFIG_LCD_CMI_PT045TN07)
#define HISI_FRAME_BUFFER_SIZE	PAGE_ALIGN(960 * ALIGN_UP(540, 16) * 4 * NUM_FRAME_BUFFERS)
#else
#error "HISI_FRAME_BUFFER_SIZE not defined"
#endif

unsigned long hisi_get_reserve_mem_size(void);
unsigned long hisi_get_reserve_gpu_mem_size(void);
#endif /* end _HI_MEM_INCLUDE_H_ */

