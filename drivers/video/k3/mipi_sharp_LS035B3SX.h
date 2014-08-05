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

#ifndef MIPI_SHARP_LS035B3SX_H
#define MIPI_SHARP_LS035B3SX_H

#include "k3_fb_def.h"


/*----------------Power ON Sequence(power on to Normal mode)----------------------*/
/*
** Initial condition (RESET="L")
**
** VCI ON Logic Voltage
** WAIT (10ms) For Power Stable
** VDD5 ON Analog Voltage
** WAIT (10ms) For Power Stable
** Reset release (Reset=H)
** WAIT MIN 5ms
*/

/* Sleep out */
static char exit_sleep[] = {
	0x11,
};
/* WAIT MIN 120ms*/

/* Register bank select */
static char powerOnData1[] = {
	0xB0,
	0x00,
};

static char powerOnData2[] = {
	0xE6,
	0x12, 0x04, 0x00, 0x05,
	0x07, 0x03, 0xFF, 0x10,
	0xFF, 0xFF,
};

static char powerOnData3[] = {
	0xC6,
	0x05,
};

/* Power ON sequence setting B */
static char powerOnData4[] = {
	0x99,
	0x2B, 0x51,
};

/* Power on sequence setting A */
static char powerOnData5[] = {
	0x98,
	0x01, 0x05, 0x06, 0x0A,
	0x18, 0x0E, 0x22, 0x23,
	0x24,
};

/* Power off sequence setting A */
static char powerOnData6[] = {
	0x9B,
	0x02, 0x06, 0x08, 0x0A,
	0x0C, 0x01,
};

/* Red Gamma Sets in Positive */
static char powerOnData7[] = {
	0xA2,
	0x00, 0x2D, 0x0E, 0x05,
	0xF9, 0x87, 0x66, 0x05,
};

/* Red Gamma Sets in Negative */
static char powerOnData8[] = {
	0xA3,
	0x00, 0x2D, 0x0E, 0x05,
	0xF9, 0x87, 0x66, 0x05,
};

/* Green Gamma Sets in Positive */
static char powerOnData9[] = {
	0xA4,
	0x04, 0x2D, 0x0E, 0x05,
	0xF9, 0x87, 0x66, 0x05,
};

/* Green Gamma Sets in Negative */
static char powerOnData10[] = {
	0xA5,
	0x04, 0x2D, 0x0E, 0x05,
	0xF9, 0x87, 0x66, 0x05,
};

/* Blue Gamma Sets in Positive */
static char powerOnData11[] = {
	0xA6,
	0x02, 0x30, 0x13, 0x46,
	0x2C, 0xA9, 0x76, 0x06,
};

/* Blue Gamma Sets in Negative */
static char powerOnData12[] = {
	0xA7,
	0x02, 0x30, 0x13, 0x46,
	0x2C, 0xA9, 0x76, 0x06,
};

/* SETVGMPM: This command set the voltage of VRMP ,VRMM. */
static char powerOnData13[] = {
	0xB4,
	0x68,
};

/* RBIAS1: */
static char powerOnData14[] = {
	0xB5,
	0x33, 0x03,
};

/* SELMODE: */
static char powerOnData15[] = {
	0xB6,
	0x02,
};

/* SET_DDVDHP: */
static char powerOnData16[] = {
	0xB7,
	0x08, 0x44, 0x06, 0x2E,
	0x00, 0x00, 0x30, 0x33,
};

/* SET_DDVDHM: */
static char powerOnData17[] = {
	0xB8,
	0x1F, 0x44, 0x10, 0x2E,
	0x1F, 0x00, 0x30, 0x33,
};

/* SET_VGH: */
static char powerOnData18[] = {
	0xB9,
	0x48, 0x11, 0x01, 0x00,
	0x30,
};

/* SET_VGL */
static char powerOnData19[] = {
	0xBA,
	0x4F, 0x11, 0x00, 0x00,
	0x30,
};

/* SET_VCL */
static char powerOnData20[] = {
	0xBB,
	0x11, 0x01, 0x00, 0x30,
};

/* TVBP */
static char powerOnData21[] = {
	0xBC,
	0x06,
};

/* THDEHBP */
static char powerOnData22[] = {
	0xBF,
	0x80,
};

static char powerOnData23[] = {
	0xB0,
	0x01,
};

/* If BANK(B1h) = 1, then this registers are able to be written or read. */
static char powerOnData24[] = {
	0xC0,
	0xC8,
};

static char powerOnData25[] = {
	0xC2,
	0x00,
};

static char powerOnData26[] = {
	0xC3,
	0x00,
};

static char powerOnData27[] = {
	0xC4,
	0x12,
};

static char powerOnData28[] = {
	0xC5,
	0x24,
};

static char powerOnData29[] = {
	0xC8,
	0x00,
};

/* Adjust the start position of SSD. */
static char powerOnData30[] = {
	0xCA,
	0x12,
};

/* Adjust the interval of SSD. */
static char powerOnData31[] = {
	0xCC,
	0x12,
};

/* Blanking Period, Partial non-display area setting. */
static char powerOnData32[] = {
	0xD4,
	0x00,
};

static char powerOnData33[] = {
	0xDC,
	0x20,
};

/* VALGO */
static char powerOnData34[] = {
	0x96,
	0x01,
};

/* RDDCOLMODE
  * 0x55:16bits 0x66:18bits 0x77:24bits */
static char color_mode_16bits[] = {
	0x0C,
	0x55,
};

static char color_mode_18bits[] = {
	0x0C,
	0x66,
};

static char powerOnData35[] = {
	0x0C,
	0x77,
};

/* MADCTL */
static char powerOnData36[] = {
	0x36,
	0x03,
};

/* Display On */
static char display_on[] = {
	0x29,
};


/*-------------------Power OFF Sequence(Normal to power off)----------------------*/

/* Display Off */
static char display_off[] = {
	0x28,
};

/* Sleep In */
static char enter_sleep[] = {
	0x10,
};
/* WAIT MIN 120ms For Power Down */

/*
** Reset release (Reset=L)
** WAIT (10ms)
** VDD5OFF Analog Voltage
** WAIT (10ms) For Power Stable
** VCI OFF Logic Voltage
*/


#endif  /* MIPI_SHARP_LS035B3SX_H */
