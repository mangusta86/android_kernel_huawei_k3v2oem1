/* Copyright (c) 2008-2011, Hisilicon Tech. Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	 * Redistributions in binary form must reproduce the above
 *	   copyright notice, this list of conditions and the following
 *	   disclaimer in the documentation and/or other materials provided
 *	   with the distribution.
 *	 * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *	   contributors may be used to endorse or promote products derived
 *	   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/pwm.h>
#include <mach/platform.h>
#include <mach/gpio.h>
#include <mach/boardid.h>

#include "k3_fb.h"
#include "k3_fb_def.h"
#include "mipi_dsi.h"
#include "mipi_reg.h"

#include <linux/lcd_tuning.h>
/* END:   Added by huohua, 2012/01/07 */

#define PWM_LEVEL 100

#define TRUE_MIPI  1
#define FALSE_MIPI 0

static int isCabcVidMode = FALSE_MIPI;
/* END:   Added by huohua, 2012/02/14 */

/*----------------Power ON Sequence(sleep mode to Normal mode)---------------------*/
static char regShiftData1[] = {
	0x00, 0x80,
};

static char regShiftData2[] = {
	0x00, 0x90,
};

static char regShiftData3[] = {
	0x00, 0xa0,
};

static char regShiftData4[] = {
	0x00, 0xb0,
};

static char regShiftData5[] = {
	0x00, 0xc0,
};

static char regShiftData6[] = {
	0x00, 0xd0,
};

static char regShiftData7[] = {
	0x00, 0xe0,
};

static char regShiftData8[] = {
	0x00, 0xf0,
};

static char regShiftData9[] = {
	0x00, 0xb3,
};

static char regShiftData10[] = {
	0x00, 0xa2,
};

static char regShiftData11[] = {
	0x00, 0xb4,
};

static char regShiftData12[] = {
	0x00, 0x00,
};

static char regShiftData13[] = {
	0x00, 0xb6,
};

static char regShiftData14[] = {
	0x00, 0xb8,
};

static char regShiftData15[] = {
	0x00, 0x94,
};

static char regShiftData16[] = {
	0x00, 0x83,
};

static char poweronData1[] = {
	0xff,
	0x12, 0x80, 0x01,
};

static char poweronData2[] = {
	0xff,
	0x12, 0x80,
};

static char poweronData3[] = {
	0xb3,
	0x38, 0x38,
};

static char poweronData4[] = {
	0xcb,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
};

static char poweronData5[] = {
	0xcb,
	0x00, 0xc0, 0xff, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00,
};

static char poweronData6[] = {
	0xcb,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00,
};

static char poweronData7[] = {
	0xcb,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

static char poweronData8[] = {
	0xcb,
	0x04, 0x00, 0x0f, 0x00, 0x00,
	0x00, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0xf4,
};

static char poweronData9[] = {
	0xcb,
	0xf4, 0xf4, 0x00, 0xf4, 0x08,
	0x04, 0x04, 0x04, 0x00, 0x00,
	0x00, 0x00, 0x00,
};

static char poweronData10[] = {
	0xcb,
	0x55, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x08, 0x00, 0x00, 0x00,
};

static char poweronData11[] = {
	0xcb,
	0x00, 0x70, 0x01, 0x00, 0x00,
};

static char poweronData12[] = {
	0xcc,
	0x41, 0x42, 0x47, 0x48, 0x49,
	0x4a, 0x4b, 0x4c, 0x52, 0x55,
	0x43, 0x53, 0x65, 0x51, 0x4d,
	0x4e, 0x4f, 0x91, 0x8d, 0x8e,
	0x8f, 0x40, 0x40, 0x40, 0x40,
};

static char poweronData13[] = {
	0xcc,
	0x41, 0x42, 0x47, 0x48, 0x4c,
	0x4b, 0x4a, 0x49, 0x52, 0x55,
	0x43, 0x53, 0x65, 0x51, 0x4d,
	0x4e, 0x4f, 0x91, 0x8d, 0x8e,
	0x8f, 0x40, 0x40, 0x40, 0x40,
	0xff, 0xff, 0xff, 0x01,
};

static char poweronData14[] = {
	0xcc,
	0x41, 0x42, 0x47, 0x48, 0x49,
	0x4a, 0x4b, 0x4c, 0x52, 0x55,
	0x43, 0x53, 0x54, 0x51, 0x4d,
	0x4e, 0x4f, 0x91, 0x8d, 0x8e,
	0x8f, 0x40, 0x40, 0x40, 0x40,
};

static char poweronData15[] = {
	0xcc,
	0x41, 0x42, 0x47, 0x48, 0x4c,
	0x4b, 0x4a, 0x49, 0x52, 0x55,
	0x43, 0x53, 0x54, 0x51, 0x4d,
	0x4e, 0x4f, 0x91, 0x8d, 0x8e,
	0x8f, 0x40, 0x40, 0x40, 0x40,
	0xff, 0xff, 0xff, 0x01,
};

static char poweronData16[] = {
	0xc1,
	0x22, 0x00, 0x00, 0x00, 0x00,
};

static char poweronData17[] = {
	0xc0,
	0x00, 0x87, 0x00, 0x06, 0x0a,
	0x00, 0x87, 0x06, 0x0a, 0x00,
	0x00, 0x00,
};

static char poweronData18[] = {
	0xc0,
	0x00, 0x0a, 0x00, 0x14, 0x00,
	0x2a,
};

static char poweronData19[] = {
	0xc0,
	0x00, 0x03, 0x01, 0x01, 0x01,
	0x01, 0x1a, 0x03, 0x00, 0x02,
};

static char poweronData20[] = {
	0xc2,
	0x03, 0x02, 0x00, 0x00, 0x00,
	0x02, 0x00, 0x22,
};

static char poweronData21[] = {
	0xc2,
	0x03, 0x00, 0xff, 0xff, 0x00,
	0x00, 0x00, 0x00, 0x22,
};

static char poweronData22[] = {
	0xc2,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,
};

static char poweronData23[] = {
	0xc2,
	0xff, 0x00, 0xff, 0x00, 0x00,
	0x0a, 0x00, 0x0a,
};

static char poweronData24[] = {
	0xc2,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00,
};

static char poweronData25[] = {
	0xc2,
	0x84, 0x00, 0x10, 0x0d,
};

static char poweronData26[] = {
	0xc0,
	0x0f,
};

static char poweronData27[] = {
	0xc1,
	0xff,
};

static char poweronData28[] = {
	0xc0,
	0x54, 0x00,
};

static char poweronData29[] = {
	0xc5,
	0x20, 0x07, 0x00, 0xb0, 0xb0,
	0x00, 0x00, 0x00,
};

static char poweronData30[] = {
	0xc5,
	0x30, 0x85, 0x02, 0x88, 0x96,
	0x15, 0x00, 0x0c,0x44,0x44,0x44,
};

static char poweronData31[] = {
	0xd8,
	0x52, 0x00, 0x52, 0x00,
};

static char poweronData32[] = {
	0xd9,
	0x8f, 0x73, 0x80,
};

static char poweronData33[] = {
	0xc0,
	0x95,
};

static char poweronData34[] = {
	0xc0,
	0x05,
};

static char poweronData35[] = {
	0xf5,
	0x00, 0x00,
};

static char poweronData36[] = {
	0xb3,
	0x11,
};

static char poweronData37[] = {
	0xf5,
	0x00, 0x20,
};

static char poweronData38[] = {
	0xf5,
	0x0c, 0x12,
};

static char poweronData39[] = {
	0xf5,
	0x0a, 0x14, 0x06, 0x17,
};

static char poweronData40[] = {
	0xf5,
	0x0a, 0x14, 0x07, 0x14,
};

static char poweronData41[] = {
	0xf5,
	0x07, 0x16, 0x07, 0x14,
};

static char poweronData42[] = {
	0xf5,
	0x02, 0x12, 0x0a, 0x12, 0x07,
	0x12, 0x06, 0x12, 0x0b, 0x12,
	0x08, 0x12,
};

static char poweronData43[] = {
	0x35,
	0x00,
};

static char poweronData44[] = {
	0xE1,
	0x2C, 0x2F, 0x36, 0x41, 0x0B,//3E  41
	0x05, 0x17, 0x09, 0x07, 0x08,//14 15
	0x09, 0x1F, 0x05, 0x0F, 0x1E,//11 13 0x0B,
	0x22, 0x1F, 0x1F, //0x0E, 0x0B, 0x0B,
};
static char poweronData45[] = {
	0xE2,
	0x2C, 0x2F, 0x36, 0x41, 0x0B,//3E 41
	0x05, 0x17, 0x09, 0x07, 0x08,//14 15
	0x09, 0x1F, 0x05, 0x0F, 0x1E,//11 13
	0x22, 0x1F, 0x1F, //0x0E, 0x0B, 0x0B,
};
static char poweronData46[] = {
	0xE3,
	0x2C, 0x2E, 0x35, 0x3F, 0x0D,//3C  3f
	0x06, 0x19, 0x09, 0x07, 0x08,//16 17
	0x0A, 0x1D, 0x05, 0x0F, 0x1F,//12 14  0x0B,
	0x22, 0x1F, 0x1F, //0x0E, 0x0B, 0x0B,
};
static char poweronData47[] = {
	0xE4,
	0x2C, 0x2E, 0x35, 0x3F, 0x0D,//3C 3f
	0x06, 0x19, 0x09, 0x07, 0x08,//16 17
	0x0A, 0x1D, 0x05, 0x0F, 0x1F,//12 14 0x0B,
	0x22, 0x1F, 0x1F, //0x0E, 0x0B, 0x0B,
};
static char poweronData48[] = {
	0xE5,
	0x0E, 0x16, 0x23, 0x31, 0x0E,//2E  31
	0x07, 0x1F, 0x0A, 0x08, 0x07,//1C 1D
	0x09, 0x1C, 0x05, 0x10, 0x1B,//11 13 0x0C,
	0x21, 0x1F, 0x1F, //0x0D, 0x0B, 0x0B, 
};
static char poweronData49[] = {
	0xE6,
	0x0E, 0x16, 0x23, 0x31, 0x0E,//2E  31
	0x07, 0x1F, 0x0A, 0x08, 0x07,//1C 1D
	0x09, 0x1C, 0x05, 0x10, 0x1B,//11 13 0x0C,
	0x21, 0x1F, 0x1F, //0x0D, 0x0B, 0x0B,
};

static char CABCData1[] = {
	0xCA,
	0x80, 0xB2, 0xB6, 0xBC, 0xC0,
	0xC5, 0xCA, 0xCF, 0xD4, 0xD9,
	0xDE, 0xE3, 0xE8, 0xED, 0xF2,
	0xF7, 0xBE, 0xFF, 0xBE, 0xFF,
	0xBE, 0xFF, 0x05, 0x03, 0x05,
	0x03, 0x05, 0x03,
};

static char CABCData2[] = {
	0xF4,
	0x00,
};

static char CABCData3[] = {
	0xC7,
	0x10,
};

static char CABCData4[] = {
	0xC8,
	0x90, 0x98, 0x98, 0x89, 0x98,
	0x99, 0x88, 0x88, 0x88, 0x9A,
	0x88, 0x99, 0x98, 0x88, 0x78,
	0x77, 0x66, 0x55,
};

static char CABCData5[] = {
	0xC7,
	0x11,
};

static char CABCData6[] = {
	0xC8,
	0x90, 0x98, 0x98, 0x89, 0x98,
	0x99, 0x88, 0x88, 0x88, 0x99,
	0x88, 0x99, 0x88, 0x88, 0x88,
	0x77, 0x66, 0x56,
};

static char CABCData7[] = {
	0xC7,
	0x12,
};

static char CABCData8[] = {
	0xC8,
	0x90, 0x98, 0x98, 0x89, 0x98,
	0x99, 0x88, 0x88, 0x88, 0x98,
	0x98, 0x98, 0x89, 0x88, 0x88,
	0x77, 0x66, 0x56,
};

static char CABCData9[] = {
	0xC7,
	0x13,
};

static char CABCData10[] = {
	0xC8,
	0x90, 0x98, 0x98, 0x89, 0x98,
	0x89, 0x89, 0x88, 0x88, 0x88,
	0x98, 0x99, 0x88, 0x88, 0x88,
	0x77, 0x77, 0x56,
};

static char CABCData11[] = {
	0xC7,
	0x14,
};

static char CABCData12[] = {
	0xC8,
	0x90, 0x98, 0x98, 0x89, 0x88,
	0x89, 0x88, 0x88, 0x88, 0x98,
	0x98, 0x99, 0x88, 0x88, 0x88,
	0x77, 0x77, 0x56,
};

static char CABCData13[] = {
	0xC7,
	0x15,
};

static char CABCData14[] = {
	0xC8,
	0x90, 0x98, 0x98, 0x89, 0x88,
	0x89, 0x88, 0x88, 0x88, 0x88,
	0xA8, 0x89, 0x88, 0x88, 0x88,
	0x78, 0x77, 0x66,
};

static char CABCData15[] = {
	0xC7,
	0x16,
};

static char CABCData16[] = {
	0xC8,
	0x90, 0x98, 0x98, 0x89, 0x88,
	0x89, 0x88, 0x88, 0x88, 0x88,
	0xA8, 0x98, 0x88, 0x88, 0x88,
	0x78, 0x77, 0x66,
};

static char CABCData17[] = {
	0xC7,
	0x17,
};

static char CABCData18[] = {
	0xC8,
	0x90, 0x98, 0x98, 0x88, 0x88,
	0x89, 0x88, 0x88, 0x88, 0x88,
	0xA8, 0x98, 0x88, 0x88, 0x88,
	0x78, 0x77, 0x67,
};

static char CABCData19[] = {
	0xC7,
	0x18,
};

static char CABCData20[] = {
	0xC8,
	0x90, 0x98, 0x98, 0x88, 0x88,
	0x89, 0x88, 0x88, 0x88, 0x88,
	0xA8, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x77, 0x67,
};

static char CABCData21[] = {
	0xC7,
	0x19,
};

static char CABCData22[] = {
	0xC8,
	0x90, 0x98, 0x98, 0x88, 0x88,
	0x89, 0x88, 0x88, 0x88, 0x88,
	0x98, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x77, 0x67,
};

static char CABCData23[] = {
	0xC7,
	0x1A,
};

static char CABCData24[] = {
	0xC8,
	0x90, 0x98, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x98,
	0x98, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x78, 0x77,
};

static char CABCData25[] = {
	0xC7,
	0x1B,
};

static char CABCData26[] = {
	0xC8,
	0x90, 0x99, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x98, 0x88,
	0x88, 0x78, 0x77,
};

static char CABCData27[] = {
	0xC7,
	0x1C,
};

static char CABCData28[] = {
	0xC8,
	0x90, 0x89, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x98, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x78,
};

static char CABCData29[] = {
	0xC7,
	0x1D,
};

static char CABCData30[] = {
	0xC8,
	0x90, 0x89, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x78,
};

static char CABCData31[] = {
	0xC7,
	0x1E,
};

static char CABCData32[] = {
	0xC8,
	0x90, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88,
};

static char CABCData33[] = {
	0xC7,
	0x1F,
};

static char CABCData34[] = {
	0xC8,
	0x80, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88,
};

static char CABCData35[] = {
	0xC7,
	0x00,
};

static char CEData1[] = {
	0xD4,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
	0x00, 0x40, 0x00, 0x40, 0x00,
	0x40, 0x00, 0x40, 0x00, 0x40,
};

static char CEData2[] = {
	0xD5,
	0x00, 0x5a, 0x00, 0x59, 0x00,
	0x59, 0x00, 0x58, 0x00, 0x58,
	0x00, 0x57, 0x00, 0x57, 0x00,
	0x57, 0x00, 0x56, 0x00, 0x56,
	0x00, 0x55, 0x00, 0x55, 0x00,
	0x54, 0x00, 0x54, 0x00, 0x54,
	0x00, 0x54, 0x00, 0x54, 0x00,
	0x55, 0x00, 0x56, 0x00, 0x57,
	0x00, 0x58, 0x00, 0x59, 0x00,
	0x5a, 0x00, 0x5a, 0x00, 0x5b,
	0x00, 0x5c, 0x00, 0x5d, 0x00,
	0x5e, 0x00, 0x5f, 0x00, 0x60,
	0x00, 0x60, 0x00, 0x61, 0x00,
	0x62, 0x00, 0x62, 0x00, 0x63,
	0x00, 0x64, 0x00, 0x64, 0x00,
	0x65, 0x00, 0x65, 0x00, 0x66,
	0x00, 0x67, 0x00, 0x67, 0x00,
	0x68, 0x00, 0x69, 0x00, 0x69,
	0x00, 0x6a, 0x00, 0x6b, 0x00,
	0x6b, 0x00, 0x6c, 0x00, 0x6c,
	0x00, 0x6d, 0x00, 0x6e, 0x00,
	0x6e, 0x00, 0x6f, 0x00, 0x70,
	0x00, 0x70, 0x00, 0x71, 0x00,
	0x72, 0x00, 0x72, 0x00, 0x73,
	0x00, 0x73, 0x00, 0x73, 0x00,
	0x73, 0x00, 0x73, 0x00, 0x73,
	0x00, 0x73, 0x00, 0x73, 0x00,
	0x73, 0x00, 0x73, 0x00, 0x73,
	0x00, 0x73, 0x00, 0x73, 0x00,
	0x73, 0x00, 0x73, 0x00, 0x73,
	0x00, 0x73, 0x00, 0x73, 0x00,
	0x73, 0x00, 0x73, 0x00, 0x73,
	0x00, 0x73, 0x00, 0x73, 0x00,
	0x73, 0x00, 0x73, 0x00, 0x73,
	0x00, 0x73, 0x00, 0x73, 0x00,
	0x73, 0x00, 0x73, 0x00, 0x73,
	0x00, 0x73, 0x00, 0x73, 0x00,
	0x73, 0x00, 0x73, 0x00, 0x73,
	0x00, 0x73, 0x00, 0x73, 0x00,
	0x73, 0x00, 0x73, 0x00, 0x73,
	0x00, 0x73, 0x00, 0x73, 0x00,
	0x73, 0x00, 0x73, 0x00, 0x73,
	0x00, 0x73, 0x00, 0x73, 0x00,
	0x73, 0x00, 0x73, 0x00, 0x73,
	0x00, 0x73, 0x00, 0x73, 0x00,
	0x73, 0x00, 0x73, 0x00, 0x73,
	0x00, 0x73, 0x00, 0x73, 0x00,
	0x73, 0x00, 0x73, 0x00, 0x73,
	0x00, 0x73, 0x00, 0x73, 0x00,
	0x73, 0x00, 0x73, 0x00, 0x73,
	0x00, 0x73, 0x00, 0x73, 0x00,
	0x73, 0x00, 0x73, 0x00, 0x73,
	0x00, 0x73, 0x00, 0x73, 0x00,
	0x73, 0x00, 0x73, 0x00, 0x73,
	0x00, 0x73, 0x00, 0x73, 0x00,
	0x72, 0x00, 0x71, 0x00, 0x71,
	0x00, 0x70, 0x00, 0x6f, 0x00,
	0x6f, 0x00, 0x6e, 0x00, 0x6d,
	0x00, 0x6d, 0x00, 0x6c, 0x00,
	0x6c, 0x00, 0x6b, 0x00, 0x6a,
	0x00, 0x6a, 0x00, 0x69, 0x00,
	0x68, 0x00, 0x68, 0x00, 0x67,
	0x00, 0x66, 0x00, 0x66, 0x00,
	0x65, 0x00, 0x64, 0x00, 0x64,
	0x00, 0x63, 0x00, 0x63, 0x00,
	0x62, 0x00, 0x61, 0x00, 0x61,
	0x00, 0x60, 0x00, 0x60, 0x00,
	0x5f, 0x00, 0x5f, 0x00, 0x5e,
	0x00, 0x5e, 0x00, 0x5d, 0x00,
	0x5d, 0x00, 0x5d, 0x00, 0x5c,
	0x00, 0x5c, 0x00, 0x5b, 0x00,
	0x5b, 0x00, 0x5a, 0x00, 0x5a,
};

static char set_address[] = {
	0x36,
	0xd0,
};

static char bl_level[] = {
	0x51,
	0x00,
};

static char bl_enable[] = {
	0x53,
	0x24,
};

static char cabc_ui_on[] = {
	0x55,
	0x01,
};

static char soft_reset[] = {
	0x01,
};

/* exit sleep mode */
static char exit_sleep[] = {
	0x11,
};

/* set pixel off */
static char pixel_off[] = {
	0x22,
};

/* set pixel on */
static char pixel_on[] = {
	0x23,
};

/* set display off */
static char display_off[] = {
	0x28,
};

/* set display on */
static char display_on[] = {
	0x29,
};

/*-------------------Power OFF Sequence(Normal to power off)----------------------*/
static char enter_sleep[] = {
	0x10,
};

static struct dsi_cmd_desc cmi_video_on_cmds[] = {

	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData1), poweronData1},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData1), regShiftData1},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData2), poweronData2},


	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData3), regShiftData3},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData3), poweronData3},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData1), regShiftData1},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData4), poweronData4},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData2), regShiftData2},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData5), poweronData5},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData3), regShiftData3},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData6), poweronData6},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData4), regShiftData4},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData7), poweronData7},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData5), regShiftData5},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData8), poweronData8},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData6), regShiftData6},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData9), poweronData9},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData7), regShiftData7},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData10), poweronData10},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData8), regShiftData8},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData11), poweronData11},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData1), regShiftData1},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData12), poweronData12},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData3), regShiftData3},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData13), poweronData13},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData5), regShiftData5},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData14), poweronData14},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData7), regShiftData7},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData15), poweronData15},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData2), regShiftData2},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData16), poweronData16},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData1), regShiftData1},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData17), poweronData17},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData2), regShiftData2},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData18), poweronData18},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData3), regShiftData3},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData19), poweronData19},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData1), regShiftData1},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData20), poweronData20},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData2), regShiftData2},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData21), poweronData21},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData4), regShiftData4},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData22), poweronData22},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData3), regShiftData3},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData23), poweronData23},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData5), regShiftData5},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData24), poweronData24},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData7), regShiftData7},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData25), poweronData25},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData9), regShiftData9},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData26), poweronData26},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData10), regShiftData10},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData27), poweronData27},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData11), regShiftData11},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData28), poweronData28},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData1), regShiftData1},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData29), poweronData29},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData2), regShiftData2},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData30), poweronData30},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData31), poweronData31},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData32), poweronData32},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData5), regShiftData5},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData33), poweronData33},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData6), regShiftData6},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData34), poweronData34},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData13), regShiftData13},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData35), poweronData35},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData4), regShiftData4},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData36), poweronData36},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData4), regShiftData4},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData37), poweronData37},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData14), regShiftData14},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData38), poweronData38},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData15), regShiftData15},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData39), poweronData39},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData10), regShiftData10},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData40), poweronData40},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData2), regShiftData2},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData41), poweronData41},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData3), regShiftData3},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData42), poweronData42},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData44), poweronData44},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData45), poweronData45},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData46), poweronData46},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData47), poweronData47},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData48), poweronData48},
	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData49), poweronData49},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData1), regShiftData1},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData1), CABCData1},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData16), regShiftData16},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData2), CABCData2},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData3), CABCData3},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData4), CABCData4},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData5), CABCData5},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData6), CABCData6},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData7), CABCData7},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData8), CABCData8},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData9), CABCData9},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData10), CABCData10},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData11), CABCData11},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData12), CABCData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData13), CABCData13},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData14), CABCData14},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData15), CABCData15},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData16), CABCData16},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData17), CABCData17},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData18), CABCData18},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData19), CABCData19},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData20), CABCData20},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData21), CABCData21},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData22), CABCData22},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData23), CABCData23},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData24), CABCData24},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData25), CABCData25},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData26), CABCData26},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData27), CABCData27},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData28), CABCData28},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData29), CABCData29},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData30), CABCData30},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData31), CABCData31},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData32), CABCData32},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData33), CABCData33},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData34), CABCData34},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(regShiftData12), regShiftData12},
	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
		sizeof(CABCData35), CABCData35},
//	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
//		sizeof(CEData1), CEData1},
//	{DTYPE_GEN_LWRITE, 0, 30, WAIT_TYPE_US,
//		sizeof(CEData2), CEData2},

	{DTYPE_GEN_WRITE2, 0, 30, WAIT_TYPE_US,
		sizeof(poweronData43), poweronData43},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(set_address), set_address},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(bl_level), bl_level},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(bl_enable), bl_enable},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(cabc_ui_on), cabc_ui_on},
	{DTYPE_DCS_WRITE, 0, 50, WAIT_TYPE_MS,
		sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE, 0, 5, WAIT_TYPE_MS,
		sizeof(display_on), display_on},
};

static struct dsi_cmd_desc cmi_display_on_cmds2[] = {
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(bl_level), bl_level},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(bl_enable), bl_enable},
};


static struct dsi_cmd_desc cmi_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 0, 30, WAIT_TYPE_US,
		sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 0, 120, WAIT_TYPE_MS,
		sizeof(enter_sleep), enter_sleep}
};

static struct k3_fb_panel_data cmi_panel_data;

static ssize_t cmi_lcd_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct k3_panel_info *pinfo = NULL;

	pinfo = cmi_panel_data.panel_info;
	sprintf(buf, "CMI_OTM1280A 4.5'TFT %d x %d\n",
		pinfo->xres, pinfo->yres);
	ret = strlen(buf) + 1;

	return ret;
}
static struct lcd_tuning_dev *p_tuning_dev = NULL;
static int cabc_mode = 1;	//allow application to set cabc mode to ui mode
static ssize_t show_cabc_mode(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	return sprintf(buf, "%d\n", cabc_mode);
}
static int cmi_set_cabc(struct lcd_tuning_dev *ltd, enum tft_cabc cabc);
static ssize_t store_cabc_mode(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	return 0;
}

static DEVICE_ATTR(lcd_info, S_IRUGO, cmi_lcd_info_show, NULL);
static DEVICE_ATTR(cabc_mode, 0644, show_cabc_mode, store_cabc_mode);

static struct attribute *cmi_attrs[] = {
	&dev_attr_lcd_info,
	&dev_attr_cabc_mode,
	NULL,
};

static struct attribute_group cmi_attr_group = {
	.attrs = cmi_attrs,
};

static int cmi_sysfs_init(struct platform_device *pdev)
{
	int ret;
	ret = sysfs_create_group(&pdev->dev.kobj, &cmi_attr_group);
	if (ret) {
		k3fb_loge("create sysfs file failed!\n");
		return ret;
	}
	return 0;
}

static void cmi_sysfs_deinit(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &cmi_attr_group);
}

/* BEGIN: Added by wugao 00190753*/

static int toshiba_set_color_temperature(struct lcd_tuning_dev *ltd, unsigned int csc_value[])
{
		//int top_index, i;
		u32 edc_base = 0;
        int i;
        u32 tmp;
	BUG_ON(ltd == NULL);
	struct platform_device *pdev = (struct platform_device *)(ltd->data);
	struct k3_fb_data_type *k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	edc_base = k3fd->edc_base;

    pr_err("===========MDY90 [%s] \n", __func__);
    pr_err(" %d, %d, %d:\n", csc_value[0], csc_value[1], csc_value[2]);
    pr_err("%d, %d, %d\n",  csc_value[3], csc_value[4], csc_value[5]);
    pr_err("%d, %d, %d\n",  csc_value[6], csc_value[7], csc_value[8]);

    #if CLK_SWITCH
    /* enable edc clk */
    clk_enable(k3fd->edc_clk);
    /* enable ldi clk */
    clk_enable(k3fd->ldi_clk);
    #endif

    set_reg(edc_base + 0x400, 0x1, 1, 27);

    set_reg(edc_base + 0x408, csc_value[0], 13, 0);
    set_reg(edc_base + 0x408, csc_value[1], 13, 16);
    set_reg(edc_base + 0x40C, csc_value[2], 13, 0);
    set_reg(edc_base + 0x40C, csc_value[3], 13, 16);
    set_reg(edc_base + 0x410, csc_value[4], 13, 0);
    set_reg(edc_base + 0x410, csc_value[5], 13, 16);
    set_reg(edc_base + 0x414, csc_value[6], 13, 0);
    set_reg(edc_base + 0x414, csc_value[7], 13, 16);
    set_reg(edc_base + 0x418, csc_value[8], 13, 0);

    #if CLK_SWITCH
    /*Disable ldi clk*/
    clk_disable(k3fd->ldi_clk);
    /*Disable edc0 clk*/
    clk_disable(k3fd->edc_clk);
    #endif

	return 0;
}
/* END:   add by wugao 00190753*/

static int cmi_set_gamma(struct lcd_tuning_dev *ltd, enum lcd_gamma gamma)
{

	return 0;
}

static int cmi_set_cabc(struct lcd_tuning_dev *ltd, enum  tft_cabc cabc)
{
	return 0;
}


/******************************************************************************/
static int cmi_pwm_on(struct k3_fb_data_type *k3fd)
{
	BUG_ON(k3fd == NULL);

	/* backlight on */
	PWM_IOMUX_SET(&(k3fd->panel_info), NORMAL);
	PWM_GPIO_REQUEST(&(k3fd->panel_info));
	gpio_direction_input(k3fd->panel_info.gpio_pwm1);
	mdelay(1);
	pwm_set_backlight(k3fd->bl_level, &(k3fd->panel_info));

	return 0;
}

static int cmi_pwm_off(struct k3_fb_data_type *k3fd)
{
	BUG_ON(k3fd == NULL);

	/* backlight off */
	pwm_set_backlight(0, &(k3fd->panel_info));
	gpio_direction_output(k3fd->panel_info.gpio_pwm0, 0);
	mdelay(1);
	gpio_direction_input(k3fd->panel_info.gpio_pwm1);
	mdelay(1);
	PWM_GPIO_FREE(&(k3fd->panel_info));
	PWM_IOMUX_SET(&(k3fd->panel_info), LOWPOWER);

	return 0;
}

static void cmi_disp_on(struct k3_fb_data_type *k3fd)
{
	u32 edc_base = 0;
	struct k3_panel_info *pinfo = NULL;

	BUG_ON(k3fd == NULL);
	edc_base = k3fd->edc_base;
	pinfo = &(k3fd->panel_info);
	/*LCD_VCC_ENABLE(pinfo);*/
	LCD_IOMUX_SET(pinfo, NORMAL);
	LCD_GPIO_REQUEST(pinfo);
	gpio_direction_input(pinfo->gpio_lcd_id0);
	mdelay(1);
	gpio_direction_input(pinfo->gpio_lcd_id1);
	mdelay(1);
	gpio_direction_output(pinfo->gpio_reset, 0);
	mdelay(15);
	gpio_direction_output(pinfo->gpio_reset, 1);
	mdelay(50);

	mipi_dsi_cmds_tx(cmi_video_on_cmds, \
		ARRAY_SIZE(cmi_video_on_cmds), edc_base);

	printk("----display on-----\n");

}

static void cmi_disp_off(struct k3_fb_data_type *k3fd)
{
	u32 edc_base = 0;
	struct k3_panel_info *pinfo = NULL;

	BUG_ON(k3fd == NULL);
	edc_base = k3fd->edc_base;
	pinfo = &(k3fd->panel_info);

	mipi_dsi_cmds_tx(cmi_display_off_cmds,
		ARRAY_SIZE(cmi_display_off_cmds), edc_base);

	gpio_direction_input(pinfo->gpio_lcd_id0);
	mdelay(1);
	gpio_direction_input(pinfo->gpio_lcd_id1);
	mdelay(1);
	gpio_direction_output(pinfo->gpio_reset, 0);
	mdelay(1);
	LCD_GPIO_FREE(pinfo);
	LCD_IOMUX_SET(pinfo, LOWPOWER);
	LCD_VCC_DISABLE(pinfo);
	printk("-----display off-----\n");
}

static int mipi_cmi_panel_on(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;
	struct k3_panel_info *pinfo = NULL;

	BUG_ON(pdev == NULL);

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	pinfo = &(k3fd->panel_info);
	if (pinfo->lcd_init_step == LCD_INIT_POWER_ON) {
		LCD_VCC_ENABLE(pinfo);
		pinfo->lcd_init_step = LCD_INIT_SEND_SEQUENCE;
		return 0;
	}

	if (!k3fd->panel_info.display_on) {
		/* lcd display on */
		cmi_disp_on(k3fd);
		k3fd->panel_info.display_on = true;
		if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM) {
			/* backlight on */
			cmi_pwm_on(k3fd);
		}
	}

	return 0;
}

static int mipi_cmi_panel_off(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	if (k3fd->panel_info.display_on) {
		k3fd->panel_info.display_on = false;
		if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM) {
			/* backlight off */
			cmi_pwm_off(k3fd);
		}
		/* lcd display off */
		cmi_disp_off(k3fd);
	}

	return 0;
}

static int mipi_cmi_panel_remove(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	/*BUG_ON(k3fd == NULL);*/
	if (!k3fd) {
		return 0;
	}

	if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM) {
		PWM_CLK_PUT(&(k3fd->panel_info));
	}
	LCD_VCC_PUT(&(k3fd->panel_info));

	cmi_sysfs_deinit(pdev);

	return 0;
}

static int mipi_cmi_panel_set_backlight(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;
	u32 edc_base = 0;
	u32 level = 0;

	BUG_ON(pdev == NULL);
	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);
	edc_base = k3fd->edc_base;

	/*Our eyes are more sensitive to small brightness.
	So we adjust the brightness of lcd following iphone4 */
	//level = (k3fd->bl_level * square_point_six(k3fd->bl_level) * 100) / 2779;  //Y=(X/255)^1.6*255
	level = k3fd->bl_level;
	if (level > 255)
		level = 255;

	
	if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM) {
		return pwm_set_backlight(level, &(k3fd->panel_info));
	} else {
		if (!k3fd->cmd_mode_refresh) {
			outp32(edc_base + MIPIDSI_GEN_HDR_OFFSET, (level << 16) | (0x51 << 8) | 0x15);

		}
		return 0;
	}
}

static int mipi_cmi_panel_set_fastboot(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	LCD_VCC_ENABLE(&(k3fd->panel_info));
	LCD_IOMUX_SET(&(k3fd->panel_info), NORMAL);
	LCD_GPIO_REQUEST(&(k3fd->panel_info));

	if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM) {
		PWM_IOMUX_SET(&(k3fd->panel_info), NORMAL);
		PWM_GPIO_REQUEST(&(k3fd->panel_info));
	}

	k3fd->panel_info.display_on = true;

	return 0;
}

static int mipi_cmi_panel_set_cabc(struct platform_device *pdev, int value)
{
	return 0;
}


static int mipi_cmi_panel_check_esd(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	outp32(k3fd->edc_base + MIPIDSI_GEN_HDR_OFFSET, 0x0A << 8 | 0x06);
	/* return  inp32(k3fd->edc_base + MIPIDSI_GEN_PLD_DATA_OFFSET); */

	return 0;
}

static struct k3_panel_info cmi_panel_info = {0};
static struct k3_fb_panel_data cmi_panel_data = {
	.panel_info = &cmi_panel_info,
	.on = mipi_cmi_panel_on,
	.off = mipi_cmi_panel_off,
	.remove = mipi_cmi_panel_remove,
	.set_backlight = mipi_cmi_panel_set_backlight,
	.set_fastboot = mipi_cmi_panel_set_fastboot,
	.check_esd = mipi_cmi_panel_check_esd,
	.set_cabc = mipi_cmi_panel_set_cabc,
};

static struct lcd_tuning_ops sp_tuning_ops = {
	.set_gamma = NULL,
	.set_cabc = NULL,//cmi_set_cabc
       /* BEGIN: Added by wugao 00190753*/
        .set_color_temperature = toshiba_set_color_temperature
        /* END:   add by wugao 00190753*/
};

static int __devinit cmi_probe(struct platform_device *pdev)
{
	struct k3_panel_info *pinfo = NULL;
	struct resource *res = NULL;
	struct platform_device *reg_pdev;
	struct lcd_tuning_dev *ltd;
	struct lcd_properities lcd_props;

	pinfo = cmi_panel_data.panel_info;
	/* init lcd panel info */
	pinfo->display_on = false;
	pinfo->xres = 720;
	pinfo->yres = 1280;
	pinfo->width = 55;
	pinfo->height = 98;
	pinfo->type = PANEL_MIPI_CMD;
	pinfo->orientation = LCD_PORTRAIT;
	pinfo->bpp = EDC_OUT_RGB_888;
	pinfo->s3d_frm = EDC_FRM_FMT_2D;
	pinfo->bgr_fmt = EDC_RGB;
	pinfo->bl_set_type = BL_SET_BY_MIPI;
	pinfo->bl_max = PWM_LEVEL;
	pinfo->bl_min = 1;

	pinfo->frc_enable = 1;
	pinfo->esd_enable = 1;
	pinfo->sbl_enable = 0;

	pinfo->sbl.bl_max = 0xff;
	pinfo->sbl.cal_a = 0x0f;
	pinfo->sbl.str_limit = 0x40;

	pinfo->ldi.h_back_porch = 43;
	pinfo->ldi.h_front_porch = 80;
	pinfo->ldi.h_pulse_width = 57;
	pinfo->ldi.v_back_porch = 4;
	pinfo->ldi.v_front_porch = 15;
	pinfo->ldi.v_pulse_width = 2;

	pinfo->ldi.hsync_plr = 1;
	pinfo->ldi.vsync_plr = 0;
	pinfo->ldi.pixelclk_plr = 1;
	pinfo->ldi.data_en_plr = 0;

	pinfo->ldi.disp_mode = LDI_DISP_MODE_NOT_3D_FBF;

	/* Note: must init here */
	pinfo->frame_rate = 60;
	/*pinfo->clk_rate = LCD_GET_CLK_RATE(pinfo);*/
	pinfo->clk_rate = 76000000;

	pinfo->mipi.lane_nums = DSI_4_LANES;
	pinfo->mipi.color_mode = DSI_24BITS_1;
	pinfo->mipi.vc = 0;
	pinfo->mipi.dsi_bit_clk = 246; /*482; clock lane(p/n) */

	/* lcd vcc */
	LCD_VCC_GET(pdev, pinfo);
	LCDIO_SET_VOLTAGE(pinfo, 1800000, 1800000);
	/* lcd iomux */
	LCD_IOMUX_GET(pinfo);
	/* lcd resource */
	LCD_RESOURCE(pdev, pinfo, res);

	if (pinfo->bl_set_type & BL_SET_BY_PWM) {
		/* pwm clock*/
		PWM_CLK_GET(pinfo);
		/* pwm iomux */
		PWM_IOMUX_GET(pinfo);
		/* pwm resource */
		PWM_RESOUTCE(pdev, pinfo, res);
	}

	/* alloc panel device data */
	if (platform_device_add_data(pdev, &cmi_panel_data,
		sizeof(struct k3_fb_panel_data))) {
		k3fb_loge("platform_device_add_data failed!\n");
		platform_device_put(pdev);
		return -ENOMEM;
	}

	reg_pdev = k3_fb_add_device(pdev);
	lcd_props.type = TFT;
	lcd_props.default_gamma = GAMMA25;

	ltd = lcd_tuning_dev_register(&lcd_props, &sp_tuning_ops, (void *)reg_pdev);
	p_tuning_dev=ltd;
	if (IS_ERR(ltd)) {
		k3fb_loge("lcd_tuning_dev_register failed!\n");
		return -1;
	}

	cmi_sysfs_init(pdev);

	printk("-----%s complete\n\n", __func__);

	return 0;
}

static struct platform_driver this_driver = {
	.probe = cmi_probe,
	.remove = NULL,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.driver = {
		.name = "mipi_cmi_OTM1280A",
	},
};

static int __init mipi_cmi_panel_init(void)
{
	int ret = 0;

	printk("-----%s start\n", __func__);
	ret = platform_driver_register(&this_driver);
	if (ret) {
		k3fb_loge("not able to register the driver\n");
		return ret;
	}

	return ret;
}

module_init(mipi_cmi_panel_init);
