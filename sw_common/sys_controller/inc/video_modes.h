//
// Copyright (C) 2015-2020  Markus Hiienkari <mhiienka@niksula.hut.fi>
//
// This file is part of Open Source Scan Converter project.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef VIDEO_MODES_H_
#define VIDEO_MODES_H_

#include <stdint.h>
#include "si5351.h"
#include "adv7513.h"
#include "sysconfig.h"

#define H_TOTAL_MIN 300
#define H_TOTAL_MAX 2800
#define H_TOTAL_ADJ_MAX 19
#define H_SYNCLEN_MIN 10
#define H_SYNCLEN_MAX 255
#define H_BPORCH_MIN 1
#define H_BPORCH_MAX 255
#define H_ACTIVE_MIN 200
#define H_ACTIVE_MAX 1920
#define V_SYNCLEN_MIN 1
#define V_SYNCLEN_MAX 7
#define V_BPORCH_MIN 1
#define V_BPORCH_MAX 63
#define V_ACTIVE_MIN 160
#define V_ACTIVE_MAX 1200

#define DEFAULT_SAMPLER_PHASE 0

typedef enum {
    FORMAT_RGBS = 0,
    FORMAT_RGBHV = 1,
    FORMAT_RGsB = 2,
    FORMAT_YPbPr = 3
} video_format;

typedef enum {
    VIDEO_SDTV      = (1<<0),
    VIDEO_EDTV      = (1<<1),
    VIDEO_HDTV      = (1<<2),
    VIDEO_PC        = (1<<3),
} video_type;

typedef enum {
    GROUP_NONE      = 0,
    GROUP_240P      = 1,
    GROUP_288P      = 2,
    GROUP_384P      = 3,
    GROUP_480I      = 4,
    GROUP_576I      = 5,
    GROUP_480P      = 6,
    GROUP_576P      = 7,
    GROUP_1080I     = 8,
} video_group;

typedef enum {
    //deprecated
    MODE_INTERLACED     = (1<<0),
    MODE_PLLDIVBY2      = (1<<1),
    //at least one of the flags below must be set for each mode
    MODE_PT             = (1<<2),
    MODE_L2             = (1<<3),
    MODE_L2_512_COL     = (1<<4),
    MODE_L2_384_COL     = (1<<5),
    MODE_L2_320_COL     = (1<<6),
    MODE_L2_256_COL     = (1<<7),
    MODE_L2_240x360     = (1<<8),
    MODE_L3_GEN_16_9    = (1<<9),
    MODE_L3_GEN_4_3     = (1<<10),
    MODE_L3_512_COL     = (1<<11),
    MODE_L3_384_COL     = (1<<12),
    MODE_L3_320_COL     = (1<<13),
    MODE_L3_256_COL     = (1<<14),
    MODE_L3_240x360     = (1<<15),
    MODE_L4_GEN_4_3     = (1<<16),
    MODE_L4_512_COL     = (1<<17),
    MODE_L4_384_COL     = (1<<18),
    MODE_L4_320_COL     = (1<<19),
    MODE_L4_256_COL     = (1<<20),
    MODE_L5_GEN_4_3     = (1<<21),
    MODE_L5_512_COL     = (1<<22),
    MODE_L5_384_COL     = (1<<23),
    MODE_L5_320_COL     = (1<<24),
    MODE_L5_256_COL     = (1<<25),
} mode_flags;

typedef enum {
    STDMODE_240p         = 7,
    STDMODE_288p         = 15,
    STDMODE_480i         = 23,
    STDMODE_480p         = 24,
    STDMODE_576i         = 27,
    STDMODE_576p         = 28,
    STDMODE_720p_50      = 30,
    STDMODE_720p_60      = 31,
    STDMODE_1280x1024_60 = 34,
    STDMODE_1080i_60     = 36,
    STDMODE_1080p_50     = 37,
    STDMODE_1080p_60     = 38,
    STDMODE_1600x1200_60 = 39,
    STDMODE_1920x1200_60 = 40,
    STDMODE_1920x1440_60 = 41
} stdmode_t;

typedef enum {
    ADMODE_240p      = 0,
    ADMODE_288p,
    ADMODE_480p,
    ADMODE_576p,
    ADMODE_720p_60,
    ADMODE_1280x1024_60,
    ADMODE_1080p_50_CR,
    ADMODE_1080p_60_LB,
    ADMODE_1080p_60_CR,
    ADMODE_1600x1200_60,
    ADMODE_1920x1200_60,
    ADMODE_1920x1440_50,
    ADMODE_1920x1440_60,
} ad_mode_id_t;

typedef struct {
    uint16_t h_active:13;
    uint16_t v_active:11;
    uint8_t v_hz;
    uint16_t h_total;
    uint8_t  h_total_adj:5;
    uint16_t v_total:11;
    uint16_t h_backporch:9;
    uint16_t v_backporch:9;
    uint16_t h_synclen:9;
    uint8_t v_synclen:4;
    uint8_t interlaced:1;
} __attribute__((packed)) sync_timings_t;

typedef struct {
    char name[14];
    HDMI_Video_Type vic:8;
    sync_timings_t timings;
    uint8_t sampler_phase;
    video_type type:4;
    video_group group:4;
    mode_flags flags;
    HDMI_pixelrep_t tx_pixelrep:2;
    HDMI_pixelrep_t hdmitx_pixr_ifr:2;
    // for generation from 27MHz clock
    uint8_t si_pclk_mult:4;
    si5351_ms_config_t si_ms_conf;
} mode_data_t;

typedef struct {
    ad_mode_id_t id;
    sync_timings_t timings_i;
    uint8_t sampler_phase;
    video_type type:4;
    video_group group:4;
    uint8_t x_rpt;
    uint8_t y_rpt;
    int16_t x_offset_i;
    int16_t y_offset_i;
    si5351_ms_config_t si_ms_conf;
} ad_mode_data_t;

typedef struct {
    uint8_t x_rpt;
    uint8_t y_rpt;
    uint8_t x_skip;
    int16_t x_offset;
    int16_t y_offset;
    uint16_t x_size;
    uint16_t y_size;
    uint16_t framesync_line;
    uint8_t x_start_lb;
    int8_t y_start_lb;
} vm_mult_config_t;


void set_default_vm_table();

uint32_t estimate_dotclk(mode_data_t *vm_in, uint32_t h_hz);

int get_adaptive_mode(uint16_t totlines, uint8_t interlaced, uint16_t hz_x100, vm_mult_config_t *vm_conf, mode_data_t *vm_in, mode_data_t *vm_out);

int get_mode_id(uint16_t totlines, uint8_t interlaced, uint16_t hz_x100, video_type typemask, vm_mult_config_t *vm_conf, mode_data_t *vm_in, mode_data_t *vm_out);

int get_standard_mode(unsigned stdmode_idx_arr_idx, vm_mult_config_t *vm_conf, mode_data_t *vm_in, mode_data_t *vm_out);

#endif /* VIDEO_MODES_H_ */
