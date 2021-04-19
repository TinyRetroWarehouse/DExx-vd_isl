//
// Copyright (C) 2019-2020  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "i2c_opencores.h"
#include "altera_avalon_pio_regs.h"
#include "altera_avalon_timer.h"
#include <sys/alt_timestamp.h>
#include "system.h"
#include "isl51002.h"
#ifdef INC_ADV7513
#include "adv7513.h"
#endif
#ifdef INC_SII1136
#include "sii1136.h"
#endif
#include "si5351.h"
#include "us2066.h"
#include "sc_config_regs.h"
#include "menu.h"
#include "controls.h"
#include "avconfig.h"
#include "av_controller.h"
#include "video_modes.h"

#define FW_VER_MAJOR 0
#define FW_VER_MINOR 43

//fix PD and cec
#define ADV7513_MAIN_BASE 0x72
#define ADV7513_EDID_BASE 0x7e
#define ADV7513_PKTMEM_BASE 0x70
#define ADV7513_CEC_BASE 0x78

#define SII1136_BASE (0x72>>1)

#define ISL51002_BASE (0x98>>1)
#define SI5351_BASE (0xC0>>1)
#define US2066_BASE (0x7a>>1)

isl51002_dev isl_dev = {.i2cm_base = I2C_OPENCORES_0_BASE,
                        .i2c_addr = ISL51002_BASE,
                        .xclk_out_en = 1,
                        .xtal_freq = 27000000LU};

si5351_dev si_dev = {.i2cm_base = I2C_OPENCORES_0_BASE,
                     .i2c_addr = SI5351_BASE,
                     .xtal_freq = 27000000LU};

#ifdef INC_ADV7513
adv7513_dev advtx_dev = {.i2cm_base = I2C_OPENCORES_1_BASE,
                         .main_base = ADV7513_MAIN_BASE,
                         .edid_base = ADV7513_EDID_BASE,
                         .pktmem_base = ADV7513_PKTMEM_BASE,
                         .cec_base = ADV7513_CEC_BASE};
#endif

#ifdef INC_SII1136
sii1136_dev siitx_dev = {
#ifdef C5G
                         .i2cm_base = I2C_OPENCORES_2_BASE,
#else
                         .i2cm_base = I2C_OPENCORES_1_BASE,
#endif
                         .i2c_addr = SII1136_BASE};
#endif

us2066_dev chardisp_dev = {.i2cm_base = I2C_OPENCORES_0_BASE,
                           .i2c_addr = US2066_BASE};

#ifdef DE2_115
alt_up_character_lcd_dev charlcd_dev = {.base = CHARACTER_LCD_0_BASE};
#endif

volatile sc_regs *sc = (volatile sc_regs*)SC_CONFIG_0_BASE;
volatile osd_regs *osd = (volatile osd_regs*)OSD_GENERATOR_0_BASE;

extern avconfig_t tc;

uint16_t sys_ctrl;
uint32_t sys_status;
uint8_t sys_powered_on;

int enable_isl, enable_tp;

extern uint8_t osd_enable;

avinput_t avinput = AV_TESTPAT, target_avinput;
unsigned tp_stdmode_idx, target_tp_stdmode_idx;

mode_data_t vmode_in, vmode_out;

char row1[US2066_ROW_LEN+1], row2[US2066_ROW_LEN+1];
extern char menu_row1[US2066_ROW_LEN+1], menu_row2[US2066_ROW_LEN+1];

static const char *avinput_str[] = { "Test pattern", "AV1_RGBS", "AV1_RGsB", "AV1_YPbPr", "AV1_RGBHV", "AV1_RGBCS", "AV2_YPbPr", "AV2_RGsB", "AV3_RGBHV", "AV3_RGBCS", "AV3_RGBS", "AV3_RGsB", "AV3_YPbPr", "AV4", "Last used" };

#ifdef VIP
typedef struct {
    uint32_t ctrl;
    uint32_t status;
    uint32_t irq;
    uint32_t words;
    uint32_t h_active;
    uint32_t v_active_f0;
    uint32_t v_active_f1;
    uint32_t h_total;
    uint32_t v_total_f0;
    uint32_t v_total_f1;
} vip_cvi_ii_regs;

typedef struct {
    uint32_t ctrl;
    uint32_t status;
    uint32_t irq;
    uint32_t vmode_match;
    uint32_t banksel;
    uint32_t mode_ctrl;
    uint32_t h_active;
    uint32_t v_active;
    uint32_t v_active_f1;
    uint32_t h_frontporch;
    uint32_t h_synclen;
    uint32_t h_blank;
    uint32_t v_frontporch;
    uint32_t v_synclen;
    uint32_t v_blank;
    uint32_t v_frontporch_f0;
    uint32_t v_synclen_f0;
    uint32_t v_blank_f0;
    uint32_t active_start;
    uint32_t v_blank_start;
    uint32_t fid_r;
    uint32_t fid_f;
    uint32_t vid_std;
    uint32_t sof_sample;
    uint32_t sof_line;
    uint32_t vco_div;
    uint32_t anc_line;
    uint32_t anc_line_f0;
    uint32_t h_polarity;
    uint32_t v_polarity;
    uint32_t valid;
} vip_cvo_ii_regs;

typedef struct {
    uint32_t ctrl;
    uint32_t status;
    uint32_t irq;
    uint32_t width;
    uint32_t height;
} vip_scl_ii_regs;

typedef struct {
    uint32_t ctrl;
    uint32_t status;
    uint32_t rsv;
    uint32_t unused[11];
    uint32_t motion_shift;
    uint32_t unused2;
    uint32_t mode;
} vip_dli_ii_regs;

typedef struct {
    uint32_t ctrl;
    uint32_t status;
    uint32_t irq;
} vip_vfb_ii_regs;

volatile vip_cvi_ii_regs *vip_cvi = (volatile vip_cvi_ii_regs*)ALT_VIP_CL_CVI_0_BASE;
volatile vip_cvo_ii_regs *vip_cvo = (volatile vip_cvo_ii_regs*)ALT_VIP_CL_CVO_0_BASE;
volatile vip_dli_ii_regs *vip_dli = (volatile vip_dli_ii_regs*)ALT_VIP_CL_DIL_0_BASE;
volatile vip_scl_ii_regs *vip_scl_nn = (volatile vip_scl_ii_regs*)ALT_VIP_CL_SCL_0_BASE;
volatile vip_scl_ii_regs *vip_scl_pp = (volatile vip_scl_ii_regs*)ALT_VIP_CL_SCL_1_BASE;
volatile vip_vfb_ii_regs *vip_fb = (volatile vip_vfb_ii_regs*)ALT_VIP_CL_VFB_0_BASE;
#endif

si5351_ms_config_t si_audio_mclk_48k_conf = {3740, 628, 1125, 8832, 0, 1, 0, 0, 0};
si5351_ms_config_t si_audio_mclk_96k_conf = {3740, 628, 1125, 4160, 0, 2, 0, 0, 0};

void ui_disp_menu(uint8_t osd_mode)
{
    uint8_t menu_page;

    if ((osd_mode == 1) || (osd_enable == 2)) {
        strncpy((char*)osd->osd_array.data[0][0], menu_row1, OSD_CHAR_COLS);
        strncpy((char*)osd->osd_array.data[1][0], menu_row2, OSD_CHAR_COLS);
        osd->osd_row_color.mask = 0;
        osd->osd_sec_enable[0].mask = 3;
        osd->osd_sec_enable[1].mask = 0;
    } else if (osd_mode == 2) {
        menu_page = get_current_menunavi()->mp;
        strncpy((char*)osd->osd_array.data[menu_page][1], menu_row2, OSD_CHAR_COLS);
        osd->osd_sec_enable[1].mask |= (1<<menu_page);
    }

    us2066_write(&chardisp_dev, (char*)&menu_row1, (char*)&menu_row2);

#ifdef DE2_115
    alt_up_character_lcd_init(&charlcd_dev);
    alt_up_character_lcd_string(&charlcd_dev, menu_row1);
    alt_up_character_lcd_set_cursor_pos(&charlcd_dev, 0, 1);
    alt_up_character_lcd_string(&charlcd_dev, menu_row2);
#endif
}

void ui_disp_status(uint8_t refresh_osd_timer) {
    if (!is_menu_active()) {
        if (refresh_osd_timer)
            osd->osd_config.status_refresh = 1;

        strncpy((char*)osd->osd_array.data[0][0], row1, OSD_CHAR_COLS);
        strncpy((char*)osd->osd_array.data[1][0], row2, OSD_CHAR_COLS);
        osd->osd_row_color.mask = 0;
        osd->osd_sec_enable[0].mask = 3;
        osd->osd_sec_enable[1].mask = 0;

        us2066_write(&chardisp_dev, (char*)&row1, (char*)&row2);

#ifdef DE2_115
        alt_up_character_lcd_init(&charlcd_dev);
        alt_up_character_lcd_string(&charlcd_dev, row1);
        alt_up_character_lcd_set_cursor_pos(&charlcd_dev, 0, 1);
        alt_up_character_lcd_string(&charlcd_dev, row2);
#endif
    }
}

void update_sc_config(mode_data_t *vm_in, mode_data_t *vm_out, vm_mult_config_t *vm_conf, avconfig_t *avconfig)
{
    hv_config_reg hv_in_config = {.data=0x00000000};
    hv_config2_reg hv_in_config2 = {.data=0x00000000};
    hv_config3_reg hv_in_config3 = {.data=0x00000000};
    hv_config_reg hv_out_config = {.data=0x00000000};
    hv_config2_reg hv_out_config2 = {.data=0x00000000};
    hv_config3_reg hv_out_config3 = {.data=0x00000000};
    xy_config_reg xy_out_config = {.data=0x00000000};
    xy_config2_reg xy_out_config2 = {.data=0x00000000};
    misc_config_reg misc_config = {.data=0x00000000};
    sl_config_reg sl_config = {.data=0x00000000};
    sl_config2_reg sl_config2 = {.data=0x00000000};

    int vip_enable = (avconfig->oper_mode == OPERMODE_SCALER);
    uint32_t h_blank, v_blank, h_frontporch, v_frontporch;

    // Set input params
    hv_in_config.h_total = vm_in->timings.h_total;
    hv_in_config.h_active = vm_in->timings.h_active;
    hv_in_config.h_synclen = vm_in->timings.h_synclen;
    hv_in_config2.h_backporch = vm_in->timings.h_backporch;
    hv_in_config2.v_active = vm_in->timings.v_active;
    hv_in_config3.v_synclen = vm_in->timings.v_synclen;
    hv_in_config3.v_backporch = vm_in->timings.v_backporch;
    hv_in_config2.interlaced = vm_in->timings.interlaced;
    hv_in_config3.h_skip = vm_conf->h_skip;
    hv_in_config3.h_sample_sel = vm_conf->h_skip / 2; // TODO: fix

    // Set output params
    hv_out_config.h_total = vm_out->timings.h_total;
    hv_out_config.h_active = vm_out->timings.h_active;
    hv_out_config.h_synclen = vm_out->timings.h_synclen;
    hv_out_config2.h_backporch = vm_out->timings.h_backporch;
    hv_out_config2.v_total = vm_out->timings.v_total;
    hv_out_config2.v_active = vm_out->timings.v_active;
    hv_out_config3.v_synclen = vm_out->timings.v_synclen;
    hv_out_config3.v_backporch = vm_out->timings.v_backporch;
    hv_out_config2.interlaced = vm_out->timings.interlaced;
    hv_out_config3.v_startline = vm_conf->framesync_line;

    xy_out_config.x_size = vm_conf->x_size;
    xy_out_config.y_size = vm_conf->y_size;
    xy_out_config.y_offset = vm_conf->y_offset;
    xy_out_config2.x_offset = vm_conf->x_offset;
    xy_out_config2.x_start_lb = vm_conf->x_start_lb;
    xy_out_config2.y_start_lb = vm_conf->y_start_lb;
    xy_out_config2.x_rpt = vm_conf->x_rpt;
    xy_out_config2.y_rpt = vm_conf->y_rpt;

    misc_config.mask_br = avconfig->mask_br;
    misc_config.mask_color = avconfig->mask_color;
    misc_config.reverse_lpf = avconfig->reverse_lpf;
    misc_config.lm_deint_mode = avconfig->lm_deint_mode;
    misc_config.nir_even_offset = avconfig->nir_even_offset;
    misc_config.ypbpr_cs = avconfig->ypbpr_cs;
    misc_config.vip_enable = vip_enable;

    sc->hv_in_config = hv_in_config;
    sc->hv_in_config2 = hv_in_config2;
    sc->hv_in_config3 = hv_in_config3;
    sc->hv_out_config = hv_out_config;
    sc->hv_out_config2 = hv_out_config2;
    sc->hv_out_config3 = hv_out_config3;
    sc->xy_out_config = xy_out_config;
    sc->xy_out_config2 = xy_out_config2;
    sc->misc_config = misc_config;
    sc->sl_config = sl_config;
    sc->sl_config2 = sl_config2;

#ifdef VIP
    vip_cvi->ctrl = vip_enable;
    vip_dli->ctrl = vip_enable;
    vip_scl_nn->ctrl = vip_enable;
    vip_scl_pp->ctrl = vip_enable;
    vip_fb->ctrl = vip_enable;
    vip_cvo->ctrl = vip_enable;

    if (avconfig->scl_dil_alg == 0) {
        vip_dli->mode = (1<<1);
    } else if (avconfig->scl_dil_alg == 1) {
        vip_dli->mode = (1<<2);
    } else if (avconfig->scl_dil_alg == 3) {
        vip_dli->mode = (1<<0);
    } else {
        vip_dli->mode = 0;
    }

#ifdef DEBUG
    vip_dli->motion_shift = avconfig->scl_dil_motion_shift;
#endif

    if (avconfig->scl_alg == 0) {
        vip_scl_nn->width = vm_conf->x_size;
        vip_scl_nn->height = vm_conf->y_size;
    } else {
        vip_scl_nn->width = vm_in->timings.h_active;

        // Pre-scale height by 2 for 240p/288p when using Lanczos as main algorithm
        if (!vm_in->timings.interlaced && (vm_in->timings.v_active < 300))
            vip_scl_nn->height = vm_in->timings.v_active*2;
        else
            vip_scl_nn->height = vm_in->timings.v_active*(vm_in->timings.interlaced+1);
    }
    vip_scl_pp->width = vm_conf->x_size;
    vip_scl_pp->height = vm_conf->y_size;

    h_blank = vm_out->timings.h_total-vm_conf->x_size;
    v_blank = vm_out->timings.v_total-vm_conf->y_size;
    h_frontporch = h_blank-vm_conf->x_offset-vm_out->timings.h_backporch-vm_out->timings.h_synclen;
    v_frontporch = v_blank-vm_conf->y_offset-vm_out->timings.v_backporch-vm_out->timings.v_synclen;

    if ((vip_cvo->h_active != vm_conf->x_size) ||
        (vip_cvo->v_active != vm_conf->y_size) ||
        (vip_cvo->h_synclen != vm_out->timings.h_synclen) ||
        (vip_cvo->v_synclen != vm_out->timings.v_synclen) ||
        (vip_cvo->h_frontporch != h_frontporch) ||
        (vip_cvo->v_frontporch != v_frontporch) ||
        (vip_cvo->h_blank != h_blank) ||
        (vip_cvo->v_blank != v_blank))
    {
        vip_cvo->banksel = 0;
        vip_cvo->valid = 0;
        vip_cvo->mode_ctrl = 0;
        vip_cvo->h_active = vm_conf->x_size;
        vip_cvo->v_active = vm_conf->y_size;
        vip_cvo->h_synclen = vm_out->timings.h_synclen;
        vip_cvo->v_synclen = vm_out->timings.v_synclen;
        vip_cvo->h_frontporch = h_frontporch;
        vip_cvo->v_frontporch = v_frontporch;
        vip_cvo->h_blank = h_blank;
        vip_cvo->v_blank = v_blank;
        vip_cvo->h_polarity = 0;
        vip_cvo->v_polarity = 0;
        vip_cvo->valid = 1;
    }
#endif
}

int init_hw() {
    int ret;

    // reset hw
    sys_ctrl = 0x00;
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
    usleep(400000);

    I2C_init(I2C_OPENCORES_0_BASE,ALT_CPU_FREQ,400000);
    I2C_init(I2C_OPENCORES_1_BASE,ALT_CPU_FREQ,400000);
#ifdef C5G
    I2C_init(I2C_OPENCORES_2_BASE,ALT_CPU_FREQ,400000);
#endif

    // Init character OLED
    us2066_init(&chardisp_dev);

    // init HDMI TX
#ifdef INC_ADV7513
    ret = adv7513_init(&advtx_dev);
    if (ret != 0) {
        sniprintf(row1, US2066_ROW_LEN+1, "ADV7513 init fail");
        return ret;
    }
#endif
#ifdef INC_SII1136
    sys_ctrl |= SCTRL_HDMI_RESET_N;
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
    usleep(20000);
    ret = sii1136_init(&siitx_dev);
    if (ret != 0) {
        sniprintf(row1, US2066_ROW_LEN+1, "SII1136 init fail");
        return ret;
    }
#endif

    // Init ISL51002
    sniprintf(row1, US2066_ROW_LEN+1, "Init ISL51002");
    ui_disp_status(1);
    ret = isl_init(&isl_dev);
    if (ret != 0) {
        sniprintf(row1, US2066_ROW_LEN+1, "ISL51002 init fail");
        return ret;
    }
    // force reconfig (needed anymore?)
    memset(&isl_dev.cfg, 0xff, sizeof(isl51002_config));

    // Enable test pattern generation
    sys_ctrl |= SCTRL_VGTP_ENABLE;
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);

    // Init Si5351C
    sniprintf(row1, US2066_ROW_LEN+1, "Init Si5351C");
    ui_disp_status(1);
    si5351_init(&si_dev);

    si5351_set_frac_mult(&si_dev, SI_PLLB, SI_CLK2, SI_XTAL, &si_audio_mclk_96k_conf);

    set_default_avconfig(1);
    set_default_keymap();
    init_menu();

#ifdef VIP
    vip_cvi->ctrl = 0;
    vip_dli->ctrl = 0;
    vip_scl_nn->ctrl = 0;
    vip_scl_pp->ctrl = 0;
    vip_fb->ctrl = 0;
    vip_cvo->ctrl = 0;
#endif

    //memcpy(video_modes, video_modes_default, VIDEO_MODES_SIZE);

    return 0;
}

void switch_input(rc_code_t code, btn_vec_t pb_vec) {
    avinput_t prev_input = (avinput <= AV1_RGBS) ? AV4 : (avinput-1);
    avinput_t next_input = (avinput == AV4) ? AV1_RGBS : (avinput+1);

    switch (code) {
        case RC_BTN1: target_avinput = AV1_RGBS; break;
        case RC_BTN4: target_avinput = (avinput == AV1_RGsB) ? AV1_YPbPr : AV1_RGsB; break;
        case RC_BTN7: target_avinput = (avinput == AV1_RGBHV) ? AV1_RGBCS : AV1_RGBHV; break;
        case RC_BTN2: target_avinput = (avinput == AV2_YPbPr) ? AV2_RGsB : AV2_YPbPr; break;
        case RC_BTN3: target_avinput = (avinput == AV3_RGBHV) ? AV3_RGBCS : AV3_RGBHV; break;
        case RC_BTN6: target_avinput = AV3_RGBS; break;
        case RC_BTN9: target_avinput = (avinput == AV3_RGsB) ? AV3_YPbPr : AV3_RGsB; break;
        case RC_BTN5: target_avinput = AV4; break;
        case RC_BTN0: target_avinput = AV_TESTPAT; break;
        case RC_UP: target_avinput = prev_input; break;
        case RC_DOWN: target_avinput = next_input; break;
        default: break;
    }

    if (pb_vec & PB_BTN0)
        avinput = next_input;
}

void switch_audsrc(audinput_t *audsrc_map, HDMI_audio_fmt_t *aud_tx_fmt) {
    uint8_t audsrc = audsrc_map[0];

    *aud_tx_fmt = (audsrc == AUD_SPDIF) ? AUDIO_SPDIF : AUDIO_I2S;
}

void switch_tp_mode(rc_code_t code) {
    if (code == RC_LEFT)
        target_tp_stdmode_idx--;
    else if (code == RC_RIGHT)
        target_tp_stdmode_idx++;
}

int sys_is_powered_on() {
    return 1;
}

void sys_toggle_power() {
    return;
}

void print_vm_stats() {
    int row = 0;
    memset((void*)osd->osd_array.data, 0, sizeof(osd_char_array));

    if (enable_tp || (enable_isl && isl_dev.sync_active)) {
        if (!enable_tp) {
            sniprintf((char*)osd->osd_array.data[row][0], OSD_CHAR_COLS, "Input preset:");
            sniprintf((char*)osd->osd_array.data[row][1], OSD_CHAR_COLS, "%s", vmode_in.name);
            sniprintf((char*)osd->osd_array.data[++row][0], OSD_CHAR_COLS, "H/V synclen:");
            sniprintf((char*)osd->osd_array.data[row][1], OSD_CHAR_COLS, "%.5u %.5u", vmode_in.timings.h_synclen, vmode_in.timings.v_synclen);
            sniprintf((char*)osd->osd_array.data[++row][0], OSD_CHAR_COLS, "H/V backporch:");
            sniprintf((char*)osd->osd_array.data[row][1], OSD_CHAR_COLS, "%.5u %.5u", vmode_in.timings.h_backporch, vmode_in.timings.v_backporch);
            sniprintf((char*)osd->osd_array.data[++row][0], OSD_CHAR_COLS, "H/V active:");
            sniprintf((char*)osd->osd_array.data[row][1], OSD_CHAR_COLS, "%.5u %.5u", vmode_in.timings.h_active, vmode_in.timings.v_active);
            sniprintf((char*)osd->osd_array.data[++row][0], OSD_CHAR_COLS, "H/V total:");
            sniprintf((char*)osd->osd_array.data[row][1], OSD_CHAR_COLS, "%.5u %.5u", vmode_in.timings.h_total, vmode_in.timings.v_total);
            row++;
            row++;
        }

        sniprintf((char*)osd->osd_array.data[row][0], OSD_CHAR_COLS, "Output mode:");
        sniprintf((char*)osd->osd_array.data[row][1], OSD_CHAR_COLS, "%s", vmode_out.name);
        sniprintf((char*)osd->osd_array.data[++row][0], OSD_CHAR_COLS, "H/V synclen:");
        sniprintf((char*)osd->osd_array.data[row][1], OSD_CHAR_COLS, "%.5u %.5u", vmode_out.timings.h_synclen, vmode_out.timings.v_synclen);
        sniprintf((char*)osd->osd_array.data[++row][0], OSD_CHAR_COLS, "H/V backporch:");
        sniprintf((char*)osd->osd_array.data[row][1], OSD_CHAR_COLS, "%.5u %.5u", vmode_out.timings.h_backporch, vmode_out.timings.v_backporch);
        sniprintf((char*)osd->osd_array.data[++row][0], OSD_CHAR_COLS, "H/V active:");
        sniprintf((char*)osd->osd_array.data[row][1], OSD_CHAR_COLS, "%.5u %.5u", vmode_out.timings.h_active, vmode_out.timings.v_active);
        sniprintf((char*)osd->osd_array.data[++row][0], OSD_CHAR_COLS, "H/V total:");
        sniprintf((char*)osd->osd_array.data[row][1], OSD_CHAR_COLS, "%.5u %.5u", vmode_out.timings.h_total, vmode_out.timings.v_total);
        row++;
    }
    sniprintf((char*)osd->osd_array.data[++row][0], OSD_CHAR_COLS, "Firmware:");
    sniprintf((char*)osd->osd_array.data[row][1], OSD_CHAR_COLS, "v%u.%.2u @ " __DATE__, FW_VER_MAJOR, FW_VER_MINOR);
    osd->osd_config.status_refresh = 1;
    osd->osd_row_color.mask = 0;
    osd->osd_sec_enable[0].mask = (1<<(row+1))-1;
    osd->osd_sec_enable[1].mask = (1<<(row+1))-1;
}

void mainloop()
{
    int i, man_input_change, framelock;
    char op_status[4];
    oper_mode_t oper_mode;
    uint32_t pclk_i_hz, pclk_o_hz, dotclk_hz, h_hz, v_hz_x100, pll_h_total, pll_h_total_prev=0;
    isl_input_t target_isl_input=0;
    video_sync target_isl_sync=0;
    video_format target_format=0;
    vm_mult_config_t vm_conf;
    status_t status;
    avconfig_t *cur_avconfig;
    si5351_clk_src si_clk_src;

    enable_isl=0;
    enable_tp=1;

    cur_avconfig = get_current_avconfig();

    while (1) {
        target_avinput = avinput;
        read_controls();
        parse_control();

        if (!sys_powered_on)
            break;

        if (target_avinput != avinput) {

            // defaults
            enable_isl = 1;
            enable_tp = 0;
            target_isl_sync = SYNC_SOG;

            switch (target_avinput) {
            case AV_TESTPAT:
                enable_isl = 0;
                enable_tp = 1;
                tp_stdmode_idx = -1;
                break;
            case AV1_RGBS:
                target_isl_input = ISL_CH0;
                target_format = FORMAT_RGBS;
                break;
            case AV1_RGsB:
                target_isl_input = ISL_CH0;
                target_format = FORMAT_RGsB;
                break;
            case AV1_YPbPr:
                target_isl_input = ISL_CH0;
                target_format = FORMAT_YPbPr;
                break;
            case AV1_RGBHV:
                target_isl_input = ISL_CH0;
                target_isl_sync = SYNC_HV;
                target_format = FORMAT_RGBHV;
                break;
            case AV1_RGBCS:
                target_isl_input = ISL_CH0;
                target_isl_sync = SYNC_CS;
                target_format = FORMAT_RGBS;
                break;
            case AV2_YPbPr:
                target_isl_input = ISL_CH1;
                target_format = FORMAT_YPbPr;
                break;
            case AV2_RGsB:
                target_isl_input = ISL_CH1;
                target_format = FORMAT_RGsB;
                break;
            case AV3_RGBHV:
                target_isl_input = ISL_CH2;
                target_format = FORMAT_RGBHV;
                target_isl_sync = SYNC_HV;
                break;
            case AV3_RGBCS:
                target_isl_input = ISL_CH2;
                target_isl_sync = SYNC_CS;
                target_format = FORMAT_RGBS;
                break;
            case AV3_RGBS:
                target_isl_input = ISL_CH2;
                target_format = FORMAT_RGBS;
                break;
            case AV3_RGsB:
                target_isl_input = ISL_CH2;
                target_format = FORMAT_RGsB;
                break;
            case AV3_YPbPr:
                target_isl_input = ISL_CH2;
                target_format = FORMAT_YPbPr;
                break;
            case AV4:
                enable_isl = 0;
                target_format = FORMAT_YPbPr;
                break;
            default:
                enable_isl = 0;
                break;
            }

            printf("### SWITCH MODE TO %s ###\n", avinput_str[target_avinput]);

            avinput = target_avinput;
            isl_enable_power(&isl_dev, 0);
            isl_enable_outputs(&isl_dev, 0);

            sys_ctrl &= ~(SCTRL_CAPTURE_SEL|SCTRL_ISL_VS_POL|SCTRL_ISL_VS_TYPE|SCTRL_VGTP_ENABLE|SCTRL_CSC_ENABLE);

            if (enable_isl) {
                isl_source_sel(&isl_dev, target_isl_input, target_isl_sync, target_format);
                isl_dev.sync_active = 0;
                pll_h_total_prev = 0;

                // send current PLL h_total to isl_frontend for mode detection
                sc->hv_in_config.h_total = isl_get_pll_htotal(&isl_dev);

                // set some defaults
                if (target_isl_sync == SYNC_HV)
                    sys_ctrl |= SCTRL_ISL_VS_TYPE;
                if (target_format == FORMAT_YPbPr)
                    sys_ctrl |= SCTRL_CSC_ENABLE;
            } else if (enable_tp) {
                sys_ctrl |= SCTRL_VGTP_ENABLE;
            }

            IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);

            strncpy(row1, avinput_str[avinput], US2066_ROW_LEN+1);
            strncpy(row2, "    NO SYNC", US2066_ROW_LEN+1);
            ui_disp_status(1);
        }

        update_settings();
        status = update_avconfig();

        if (enable_tp) {
            if (tp_stdmode_idx != target_tp_stdmode_idx) {
                get_standard_mode((unsigned)target_tp_stdmode_idx, &vm_conf, &vmode_in, &vmode_out);
                if (vmode_out.si_pclk_mult > 0) {
                    si5351_set_integer_mult(&si_dev, SI_PLLA, SI_CLK4, SI_XTAL, 0, vmode_out.si_pclk_mult, vmode_out.si_ms_conf.outdiv);
                    pclk_o_hz = vmode_out.si_pclk_mult*si_dev.xtal_freq;
                } else {
                    si5351_set_frac_mult(&si_dev, SI_PLLA, SI_CLK4, SI_XTAL, &vmode_out.si_ms_conf);
                    pclk_o_hz = (vmode_out.timings.h_total*vmode_out.timings.v_total*(vmode_out.timings.v_hz_max ? vmode_out.timings.v_hz_max : 60))/(1+vmode_out.timings.interlaced);
                }

                printf("PCLK_OUT: %luHz\n", pclk_o_hz);

                update_osd_size(&vmode_out);
                update_sc_config(&vmode_in, &vmode_out, &vm_conf, cur_avconfig);
#ifdef INC_ADV7513
                adv7513_set_pixelrep_vic(&advtx_dev, vmode_out.tx_pixelrep, vmode_out.hdmitx_pixr_ifr, vmode_out.vic);
#endif
#ifdef INC_SII1136
                sii1136_init_mode(&siitx_dev, vmode_out.tx_pixelrep, vmode_out.hdmitx_pixr_ifr, vmode_out.vic, pclk_o_hz);
#endif

                //sniprintf(row2, US2066_ROW_LEN+1, "%ux%u%c @ %uHz", vmode_out.timings.h_active, vmode_out.timings.v_active<<vmode_out.timings.interlaced, vmode_out.timings.interlaced ? 'i' : '\0', vmode_out.timings.v_hz_max);
                sniprintf(row2, US2066_ROW_LEN+1, "Test: %s", vmode_out.name);
                ui_disp_status(1);

                tp_stdmode_idx = target_tp_stdmode_idx;
            }
        } else if (enable_isl) {
            if (isl_check_activity(&isl_dev, target_isl_input, target_isl_sync)) {
                if (isl_dev.sync_active) {
                    isl_enable_power(&isl_dev, 1);
                    isl_enable_outputs(&isl_dev, 1);
                    printf("ISL51002 sync up\n");
                } else {
                    isl_enable_power(&isl_dev, 0);
                    isl_enable_outputs(&isl_dev, 0);
                    strncpy(row1, avinput_str[avinput], US2066_ROW_LEN+1);
                    strncpy(row2, "    NO SYNC", US2066_ROW_LEN+1);
                    ui_disp_status(1);
                    printf("ISL51002 sync lost\n");
                }
            }

            if (isl_dev.sync_active) {
                if (isl_get_sync_stats(&isl_dev, sc->fe_status.vtotal, sc->fe_status.interlace_flag, sc->fe_status2.pcnt_frame) || (status == MODE_CHANGE)) {

#ifdef ISL_MEAS_HZ
                    if (isl_dev.sm.h_period_x16 > 0)
                        h_hz = (16*isl_dev.xtal_freq)/isl_dev.sm.h_period_x16;
                    else
                        h_hz = 0;
                    if ((isl_dev.sm.h_period_x16 > 0) && (isl_dev.ss.v_total > 0))
                        v_hz_x100 = (16*5*isl_dev.xtal_freq)/((isl_dev.sm.h_period_x16 * isl_dev.ss.v_total) / (100/5));
                    else
                        v_hz_x100 = 0;
#else
                    v_hz_x100 = (100*27000000UL)/isl_dev.ss.pcnt_frame;
                    h_hz = (100*27000000UL)/((100*isl_dev.ss.pcnt_frame*(1+isl_dev.ss.interlace_flag))/isl_dev.ss.v_total);
#endif

                    memset(&vmode_in, 0, sizeof(mode_data_t));
                    vmode_in.timings.h_synclen = isl_dev.sm.h_synclen_x16 / 16;
                    vmode_in.timings.v_hz_max = (v_hz_x100+50)/100;
                    vmode_in.timings.v_total = isl_dev.ss.v_total;
                    vmode_in.timings.interlaced = isl_dev.ss.interlace_flag;

                    if (cur_avconfig->oper_mode == OPERMODE_PURE_LM) {
                        oper_mode = (get_pure_lm_mode(&vmode_in, &vmode_out, &vm_conf) == 0) ? OPERMODE_PURE_LM : OPERMODE_INVALID;
                    } else if (cur_avconfig->oper_mode == OPERMODE_ADAPT_LM) {
                        oper_mode = (get_adaptive_lm_mode(&vmode_in, &vmode_out, &vm_conf) == 0) ? OPERMODE_ADAPT_LM : OPERMODE_INVALID;
                        if (oper_mode == OPERMODE_INVALID)
                            oper_mode = (get_pure_lm_mode(&vmode_in, &vmode_out, &vm_conf) == 0) ? OPERMODE_PURE_LM : OPERMODE_INVALID;
                    } else {
                        oper_mode = (get_scaler_mode(&vmode_in, &vmode_out, &vm_conf, &framelock) == 0) ? OPERMODE_SCALER : OPERMODE_INVALID;
                    }

                    if (oper_mode == OPERMODE_PURE_LM) {
                        sniprintf(op_status, 4, "x%u", (int8_t)vm_conf.y_rpt+1);
                        framelock = 1;
                    } else if (oper_mode == OPERMODE_ADAPT_LM) {
                        sniprintf(op_status, 4, "x%ua", (int8_t)vm_conf.y_rpt+1);
                        framelock = 1;
                    } else if (oper_mode == OPERMODE_SCALER) {
                        sniprintf(op_status, 4, "SCL");
                    }

                    if (oper_mode != OPERMODE_INVALID) {
                        printf("\nInput: %s -> Output: %s (opermode %d)\n", vmode_in.name, vmode_out.name, oper_mode);

                        sniprintf(row1, US2066_ROW_LEN+1, "%s %4u-%c %s", avinput_str[avinput], isl_dev.ss.v_total, isl_dev.ss.interlace_flag ? 'i' : 'p', op_status);
                        sniprintf(row2, US2066_ROW_LEN+1, "%lu.%.2lukHz %lu.%.2luHz %c%c", (h_hz+5)/1000, ((h_hz+5)%1000)/10,
                                                                                            (v_hz_x100/100),
                                                                                            (v_hz_x100%100),
                                                                                            isl_dev.ss.h_polarity ? '-' : '+',
                                                                                            (target_isl_sync == SYNC_HV) ? (isl_dev.ss.v_polarity ? '-' : '+') : (isl_dev.ss.sog_trilevel ? '3' : ' '));
                        ui_disp_status(1);

                        pll_h_total = (vm_conf.h_skip+1) * vmode_in.timings.h_total + (((vm_conf.h_skip+1) * vmode_in.timings.h_total_adj * 5 + 50) / 100);

                        si_clk_src = framelock ? SI_CLKIN : SI_XTAL;
                        pclk_i_hz = h_hz * pll_h_total;
                        dotclk_hz = estimate_dotclk(&vmode_in, h_hz);
                        pclk_o_hz = vmode_out.si_pclk_mult ? vmode_out.si_pclk_mult*((si_clk_src == SI_CLKIN) ? pclk_i_hz : si_dev.xtal_freq) : (vmode_out.timings.h_total*vmode_out.timings.v_total*(vmode_out.timings.v_hz_max ? vmode_out.timings.v_hz_max : 60))/(1+vmode_out.timings.interlaced);
                        printf("H: %lu.%.2lukHz V: %lu.%.2luHz\n", (h_hz+5)/1000, ((h_hz+5)%1000)/10, (v_hz_x100/100), (v_hz_x100%100));
                        printf("Estimated source dot clock: %lu.%.2luMHz\n", (dotclk_hz+5000)/1000000, ((dotclk_hz+5000)%1000000)/10000);
                        printf("PCLK_IN: %luHz PCLK_OUT: %luHz\n", pclk_i_hz, pclk_o_hz);

                        // CEA-770.3 HDTV modes use tri-level syncs which have twice the width of bi-level syncs of corresponding CEA-861 modes
                        if ((vmode_in.type & VIDEO_HDTV) && (target_isl_sync == SYNC_SOG)) {
                            vmode_in.timings.h_synclen *= 2;
                            isl_dev.sync_trilevel = 1;
                        } else {
                            isl_dev.sync_trilevel = 0;
                        }

                        isl_source_setup(&isl_dev, pll_h_total);

                        isl_set_afe_bw(&isl_dev, dotclk_hz);

                        if (pll_h_total != pll_h_total_prev)
                            isl_set_sampler_phase(&isl_dev, vmode_in.sampler_phase);

                        pll_h_total_prev = pll_h_total;

                        // Setup Si5351
                        if (vmode_out.si_pclk_mult == 0)
                            si5351_set_frac_mult(&si_dev, SI_PLLA, SI_CLK4, si_clk_src, &vmode_out.si_ms_conf);
                        else
                            si5351_set_integer_mult(&si_dev, SI_PLLA, SI_CLK4, si_clk_src, pclk_i_hz, vmode_out.si_pclk_mult, vmode_out.si_ms_conf.outdiv);

                        if (framelock)
                            sys_ctrl |= SCTRL_FRAMELOCK;
                        else
                            sys_ctrl &= ~SCTRL_FRAMELOCK;

                        // TODO: dont read polarity from ISL51002
                        sys_ctrl &= ~(SCTRL_ISL_HS_POL|SCTRL_ISL_VS_POL);
                        if ((target_isl_sync != SYNC_HV) || isl_dev.ss.h_polarity)
                            sys_ctrl |= SCTRL_ISL_HS_POL;
                        if ((target_isl_sync == SYNC_HV) && isl_dev.ss.v_polarity)
                            sys_ctrl |= SCTRL_ISL_VS_POL;
                        IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);

                        update_osd_size(&vmode_out);
                        update_sc_config(&vmode_in, &vmode_out, &vm_conf, cur_avconfig);

                        // Setup VIC and pixel repetition
#ifdef INC_ADV7513
                        adv7513_set_pixelrep_vic(&advtx_dev, vmode_out.tx_pixelrep, vmode_out.hdmitx_pixr_ifr, vmode_out.vic);
#endif
#ifdef INC_SII1136
                        sii1136_init_mode(&siitx_dev, vmode_out.tx_pixelrep, vmode_out.hdmitx_pixr_ifr, vmode_out.vic, pclk_o_hz);
#endif
                    }
                } else if (status == SC_CONFIG_CHANGE) {
                    update_sc_config(&vmode_in, &vmode_out, &vm_conf, cur_avconfig);
                }
            } else {

            }

            isl_update_config(&isl_dev, &cur_avconfig->isl_cfg, 0);
        }

#ifdef INC_ADV7513
        adv7513_check_hpd_power(&advtx_dev);
        if (advtx_dev.powered_on && (cur_avconfig->hdmitx_cfg.i2s_fs != advtx_dev.cfg.i2s_fs))
            si5351_set_frac_mult(&si_dev, SI_PLLB, SI_CLK2, SI_XTAL, (cur_avconfig->hdmitx_cfg.i2s_fs == IEC60958_FS_96KHZ) ? &si_audio_mclk_96k_conf : &si_audio_mclk_48k_conf);
        adv7513_update_config(&advtx_dev, &cur_avconfig->hdmitx_cfg);
#endif
#ifdef INC_SII1136
        sii1136_update_config(&siitx_dev, &cur_avconfig->hdmitx_cfg);
#endif

        usleep(20000);
    }
}

int main()
{
    int ret;

    while (1) {
        ret = init_hw();
        if (ret != 0) {
            sniprintf(row2, US2066_ROW_LEN+1, "Error code: %d", ret);
            printf("%s\n%s\n", row1, row2);
            us2066_display_on(&chardisp_dev);
            ui_disp_status(1);
            while (1) {}
        }

        printf("### DExx-vd INIT OK ###\n\n");

        sys_powered_on = 1;

        sniprintf(row1, US2066_ROW_LEN+1, "DExx-vd fw. %u.%.2u", FW_VER_MAJOR, FW_VER_MINOR);
        ui_disp_status(1);

        // ADVRX powerup
        // pcm powerup
        sys_ctrl |= SCTRL_POWER_ON;
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);

        // Set testpattern mode
        avinput = AV_TESTPAT;
        tp_stdmode_idx=-1;
        target_tp_stdmode_idx=3; // STDMODE_480p

        us2066_display_on(&chardisp_dev);

#ifdef INC_SII1136
        sii1136_enable_power(&siitx_dev, 1);
#endif

        mainloop();
    }
}
