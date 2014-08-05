/* Copyright (c) 2008-2011, Hisilicon Tech. Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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

#ifndef MIPI_SAMSUNG_S6E39A_H
#define MIPI_SAMSUNG_S6E39A_H

#include "k3_fb_def.h"


/*----------------Power ON Sequence(power on to Normal mode)----------------------*/

static char powerOnData1[] = {
    0xF0,
    0x5A, 0x5A,
};

static char powerOnData2[] = {
    0xF1,
    0x5A, 0x5A,
};

static char powerOnData3[] = {
    0xFC,
    0x5A, 0x5A,
};

static char powerOnData4[] = {
    0xB1,
    0x01, 0x00, 0x16,
};

static char powerOnData5[] = {
    0xB2,
    0x06, 0x06, 0x06,
};

/* 300 */
static char back_light[] = {
    0xfa, 0x02, 0x10, 0x10, 0x10, 0xec, 0xb7, 0xef, 0xd1, 0xca,
    0xd1, 0xd8, 0xda, 0xd8, 0xb5, 0xb8, 0xb0, 0xc5, 0xc8, 0xbf,
    0x00, 0xb9, 0x00, 0x93, 0x00, 0xd9
};

#if 0
static char back_light_700[] = {
    0xfa, 0x02, 0x10, 0x10, 0x10, 0xd1, 0x34, 0xd0, 0xd6, 0xba,
    0xdc, 0xe0, 0xd9, 0xe2, 0xc2, 0xc0, 0xbf, 0xd4, 0xd5, 0xd0,
    0x00, 0x73, 0x00, 0x59, 0x00, 0x82
};
#endif

static char gamma_setting_update[] = {
    0xfa,
    0x03
};

static char powerOnData6[] = {
    0xF8,
    0x28, 0x28, 0x08, 0x08,
    0x40, 0xb0, 0x50, 0x90,
    0x10, 0x30, 0x10, 0x00,
    0x00,
};

static char powerOnData7[] = {
    0xF6,
    0x00, 0x84, 0x09,
};

static char powerOnData8[] = {
    0xb0,
    0x01,
};

static char powerOnData9[] = {
    0xc0,
    0x00,
};

static char powerOnData10[] = {
    0xb0,
    0x09,
};

static char powerOnData11[] = {
    0xd5,
    0x64,
};

static char powerOnData12[] = {
    0xb0,
    0x0b,
};

static char powerOnData13[] = {
    0xd5,
    0xa4,
};

static char powerOnData14[] = {
    0xb0,
    0x0c,
};

static char powerOnData15[] = {
    0xd5,
    0x7e,
};

static char powerOnData16[] = {
    0xb0,
    0x0d,
};

static char powerOnData17[] = {
    0xd5,
    0x20,
};

static char powerOnData18[] = {
    0xb0,
    0x08,
};
	
static char powerOnData19[] = {
    0xfd,
    0xf8,
};

static char powerOnData20[] = {
    0xb0,
    0x04,
};
	
static char powerOnData21[] = {
    0xf2,
    0x4d,
};

static char exit_sleep[] = {
    0x11,
};

static char te_off[] = {
    0x34,
};

static char memory_window_setting1[] = {
    0x2a,
    0x00, 0x1e, 0x02, 0x39,
};

static char memory_window_setting2[] = {
    0x2b,
    0x00, 0x00, 0x03, 0xbf,
};

static char memory_window_setting3[] = {
    0xc0,
    0x01,
};

static char display_on[] = {
    0x29,
};

/*-------------------Power OFF Sequence(Normalto power off)----------------------*/

/* Display Off */
static char display_off[] = {
    0x28,
};

/* Sleep In */
static char enter_sleep[] = {
    0x10,
};
/* WAIT MIN 120ms For Power Down */


#endif  /* MIPI_SAMSUNG_S6E39A_H */
