#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "i2c_opencores.h"
#include "altera_avalon_pio_regs.h"
#include "altera_avalon_timer.h"
#include <sys/alt_timestamp.h>
#include "system.h"
#include "isl51002.h"
#include "adv7513.h"
#include "si5351.h"
#include "us2066.h"
#include "sc_config_regs.h"
#include "menu.h"
#include "controls.h"
#include "avconfig.h"
#include "av_controller.h"
#include "video_modes.h"

//fix PD and cec
#define ADV7513_MAIN_BASE 0x72
#define ADV7513_EDID_BASE 0x7e
#define ADV7513_PKTMEM_BASE 0x70
#define ADV7513_CEC_BASE 0x78

#define ISL51002_BASE (0x98>>1)
#define SI5351_BASE (0xC0>>1)
#define US2066_BASE (0x7a>>1)

isl51002_dev isl_dev = {.i2cm_base = I2C_OPENCORES_0_BASE,
                        .i2c_addr = ISL51002_BASE,
                        .xtal_freq = 24576000LU};

si5351_dev si_dev = {.i2cm_base = I2C_OPENCORES_0_BASE,
                     .i2c_addr = SI5351_BASE,
                     .xtal_freq = 24576000LU};

adv7513_dev advtx_dev = {.i2cm_base = I2C_OPENCORES_1_BASE,
                         .main_base = ADV7513_MAIN_BASE,
                         .edid_base = ADV7513_EDID_BASE,
                         .pktmem_base = ADV7513_PKTMEM_BASE,
                         .cec_base = ADV7513_CEC_BASE};

us2066_dev chardisp_dev = {.i2cm_base = I2C_OPENCORES_0_BASE,
                           .i2c_addr = US2066_BASE};

#ifdef DE2_115
alt_up_character_lcd_dev charlcd_dev = {.base = CHARACTER_LCD_0_BASE};
#endif

volatile sc_regs *sc = (volatile sc_regs*)SC_CONFIG_0_BASE;

extern mode_data_t video_modes[], video_modes_default[];
extern avconfig_t tc;

uint16_t sys_ctrl;

avinput_t avinput = AV_TESTPAT, target_avinput;
unsigned tp_stdmode_idx=-1, target_tp_stdmode_idx=2; // STDMODE_480p

char row1[US2066_ROW_LEN+1];
char row2[US2066_ROW_LEN+1];

static const char *avinput_str[] = { "Test pattern", "AV1_RGBS", "AV1_RGsB", "AV1_YPbPr", "AV1_RGBHV", "AV1_RGBCS", "AV2_YPbPr", "AV2_RGsB", "AV3_RGBHV", "AV3_RGBCS", "AV3_RGBS", "AV3_RGsB", "AV3_YPbPr", "AV4", "Last used" };

void chardisp_write_status() {
    if (!is_menu_active()) {
        us2066_write(&chardisp_dev, row1, row2);

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

    // Set input params
    hv_in_config.h_total = vm_in->timings.h_total;
    hv_in_config.h_active = vm_in->timings.h_active;
    hv_in_config.h_backporch = vm_in->timings.h_backporch;
    hv_in_config2.h_synclen = vm_in->timings.h_synclen;
    hv_in_config2.v_active = vm_in->timings.v_active;
    hv_in_config3.v_backporch = vm_in->timings.v_backporch;
    hv_in_config3.v_synclen = vm_in->timings.v_synclen;
    hv_in_config2.interlaced = vm_in->timings.interlaced;

    // Set output params
    hv_out_config.h_total = vm_out->timings.h_total;
    hv_out_config.h_active = vm_out->timings.h_active;
    hv_out_config.h_backporch = vm_out->timings.h_backporch;
    hv_out_config2.h_synclen = vm_out->timings.h_synclen;
    hv_out_config2.v_total = vm_out->timings.v_total;
    hv_out_config2.v_active = vm_out->timings.v_active;
    hv_out_config3.v_backporch = vm_out->timings.v_backporch;
    hv_out_config3.v_synclen = vm_out->timings.v_synclen;
    hv_out_config2.interlaced = vm_out->timings.interlaced;
    hv_out_config3.v_startline = vm_conf->framesync_line;

    xy_out_config.x_size = vm_conf->x_size;
    xy_out_config.y_size = vm_conf->y_size;
    xy_out_config.x_offset = vm_conf->x_offset;
    xy_out_config2.y_offset = vm_conf->y_offset;
    xy_out_config2.x_start_lb = vm_conf->x_start_lb;
    xy_out_config2.y_start_lb = vm_conf->y_start_lb;
    xy_out_config2.x_rpt = vm_conf->x_rpt;
    xy_out_config2.y_rpt = vm_conf->y_rpt;
    xy_out_config2.x_skip = vm_conf->x_skip;

    misc_config.mask_br = avconfig->mask_br;
    misc_config.mask_color = avconfig->mask_color;
    misc_config.reverse_lpf = avconfig->reverse_lpf;
    misc_config.lm_deint_mode = avconfig->lm_deint_mode;
    misc_config.ypbpr_cs = avconfig->ypbpr_cs;

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
}

int init_hw() {
    int ret;

    // unreset hw
    usleep(400000);
    sys_ctrl = SCTRL_ISL_RESET_N|SCTRL_HDMIRX_RESET_N;
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);

    I2C_init(I2C_OPENCORES_0_BASE,ALT_CPU_FREQ,400000);
    I2C_init(I2C_OPENCORES_1_BASE,ALT_CPU_FREQ,400000);

    // init HDMI TX
    adv7513_init(&advtx_dev, 1);

    // Init character OLED
    us2066_init(&chardisp_dev);

    // Init ISL51002
    sniprintf(row1, US2066_ROW_LEN+1, "Init ISL51002");
    chardisp_write_status();
    ret = isl_init(&isl_dev);
    if (ret != 0) {
        printf("ISL51002 init fail\n");
        return ret;
    }
    // force reconfig
    memset(&isl_dev.cfg, 0xff, sizeof(isl51002_config));

    // Enable test pattern generation
    sys_ctrl |= SCTRL_VGTP_ENABLE;
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);

    // Init Si5351C
    sniprintf(row1, US2066_ROW_LEN+1, "Init Si5351C");
    chardisp_write_status();
    si5351_init(&si_dev);

    set_default_avconfig(1);
    set_default_keymap();
    tc.adv7513_cfg.i2s_fs = ADV_96KHZ;

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

void switch_tp_mode(rc_code_t code) {
    if (code == RC_LEFT)
        target_tp_stdmode_idx--;
    else if (code == RC_RIGHT)
        target_tp_stdmode_idx++;
}

int main() {
    int ret, i, man_input_change;
    int mode, amode_match;
    uint32_t pclk_hz, dotclk_hz, h_hz, v_hz_x100;
    uint32_t sys_status;
    uint16_t remote_code;
    uint8_t pixelrep;
    uint8_t btn_vec, btn_vec_prev=0;
    uint8_t remote_rpt, remote_rpt_prev=0;
    isl_input_t target_isl_input;
    video_sync target_isl_sync=0;
    video_format target_format;
    video_type target_typemask=0;
    mode_data_t vmode_in, vmode_out;
    vm_mult_config_t vm_conf;
    status_t status;
    avconfig_t *cur_avconfig;
    int enable_isl=0, enable_hdmirx=0, enable_tp=1;

    ret = init_hw();
    if (ret == 0) {
        printf("### DExx-vd INIT OK ###\n\n");
        sniprintf(row1, US2066_ROW_LEN+1, "DExx-vd");
        chardisp_write_status();
    } else {
        sniprintf(row2, US2066_ROW_LEN+1, "failed (%d)", ret);
        chardisp_write_status();
        while (1) {}
    }

    cur_avconfig = get_current_avconfig();

    while (1) {

        // Read remote control and PCB button status
        sys_status = IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE);
        remote_code = (sys_status & SSTAT_RC_MASK) >> SSTAT_RC_OFFS;
        remote_rpt = (sys_status & SSTAT_RRPT_MASK) >> SSTAT_RRPT_OFFS;
        btn_vec = (~sys_status & SSTAT_BTN_MASK) >> SSTAT_BTN_OFFS;

        if ((remote_rpt == 0) || ((remote_rpt > 1) && (remote_rpt < 6)) || (remote_rpt == remote_rpt_prev))
            remote_code = 0;

        remote_rpt_prev = remote_rpt;

        if (btn_vec_prev == 0) {
            btn_vec_prev = btn_vec;
        } else {
            btn_vec_prev = btn_vec;
            btn_vec = 0;
        }

        target_avinput = avinput;
        parse_control(remote_code, btn_vec);

        if (target_avinput != avinput) {

            // defaults
            enable_isl = 1;
            enable_hdmirx = 0;
            enable_tp = 0;
            target_typemask = VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
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
                target_typemask = VIDEO_PC; // OK?
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
                target_typemask = VIDEO_PC;
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
                enable_hdmirx = 1;
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
            chardisp_write_status();
        }

        status = update_avconfig();

        alt_timestamp_start();
        while (1) {
            if ((IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & SSTAT_VS_MASK) || (alt_timestamp() > 25*(ALT_CPU_FREQ/1000)))
                break;
        }

        if (enable_tp) {
            if (tp_stdmode_idx != target_tp_stdmode_idx) {
                get_standard_mode((unsigned)target_tp_stdmode_idx, &vm_conf, &vmode_in, &vmode_out);
                if (vmode_out.si_pclk_mult > 0)
                    si5351_set_integer_mult(&si_dev, SI_PLLA, SI_CLK4, SI_XTAL, si_dev.xtal_freq, vmode_out.si_pclk_mult, vmode_out.si_ms_conf.outdiv);
                else
                    si5351_set_frac_mult(&si_dev, SI_PLLA, SI_CLK4, SI_XTAL, &vmode_out.si_ms_conf);

                update_sc_config(&vmode_in, &vmode_out, &vm_conf, cur_avconfig);
                adv7513_set_pixelrep_vic(&advtx_dev, vmode_out.tx_pixelrep, vmode_out.hdmitx_pixr_ifr, vmode_out.vic);

                sniprintf(row2, US2066_ROW_LEN+1, "%ux%u @ %uHz", vmode_out.timings.h_active, vmode_out.timings.v_active, vmode_out.timings.v_hz);
                chardisp_write_status();

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
                    chardisp_write_status();
                    printf("ISL51002 sync lost\n");
                }
            }

            if (isl_dev.sync_active) {
                if (isl_get_sync_stats(&isl_dev, sc->fe_status.vtotal, sc->fe_status.interlace_flag, sc->fe_status2.pcnt_frame) || (status == MODE_CHANGE)) {

#ifdef ISL_SYNC_MEAS
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
                    h_hz = (100*27000000UL)/((100*isl_dev.ss.pcnt_frame)/isl_dev.ss.v_total);
#endif

                    if (isl_dev.ss.interlace_flag)
                        v_hz_x100 *= 2;

                    mode = get_adaptive_mode(isl_dev.ss.v_total, isl_dev.ss.interlace_flag, v_hz_x100, &vm_conf, &vmode_in, &vmode_out);

                    if (mode < 0) {
                        amode_match = 0;
                        mode = get_mode_id(isl_dev.ss.v_total, isl_dev.ss.interlace_flag, v_hz_x100, target_typemask, &vm_conf, &vmode_in, &vmode_out);
                    } else {
                        amode_match = 1;
                    }

                    if (mode >= 0) {
                        printf("\nMode %s selected (%s linemult)\n", vmode_in.name, amode_match ? "Adaptive" : "Pure");

                        //sniprintf(row1, US2066_ROW_LEN+1, "%-10s %4u%c x%u%c", avinput_str[avinput], isl_dev.ss.v_total, isl_dev.ss.interlace_flag ? 'i' : 'p', vm_conf.y_rpt+1, amode_match ? 'a' : ' ');
                        sniprintf(row1, US2066_ROW_LEN+1, "%s %4u-%c x%u%c", avinput_str[avinput], isl_dev.ss.v_total, isl_dev.ss.interlace_flag ? 'i' : 'p', vm_conf.y_rpt+1, amode_match ? 'a' : ' ');
                        sniprintf(row2, US2066_ROW_LEN+1, "%lu.%.2lukHz %lu.%.2luHz %c%c", (h_hz+5)/1000, ((h_hz+5)%1000)/10,
                                                                                            (v_hz_x100/100),
                                                                                            (v_hz_x100%100),
                                                                                            isl_dev.ss.h_polarity ? '-' : '+',
                                                                                            isl_dev.ss.v_polarity ? '-' : '+');
                        chardisp_write_status();

                        pclk_hz = h_hz * vmode_in.timings.h_total;
                        dotclk_hz = estimate_dotclk(&vmode_in, h_hz);
                        printf("H: %u.%.2ukHz V: %u.%.2uHz\n", (h_hz+5)/1000, ((h_hz+5)%1000)/10, (v_hz_x100/100), (v_hz_x100%100));
                        printf("Estimated source dot clock: %lu.%.2uMHz\n", (dotclk_hz+5000)/1000000, ((dotclk_hz+5000)%1000000)/10000);
                        printf("PCLK_IN: %luHz PCLK_OUT: %luHz\n", pclk_hz, vmode_out.si_pclk_mult*pclk_hz);

                        isl_source_setup(&isl_dev, vmode_in.timings.h_total);

                        isl_set_afe_bw(&isl_dev, dotclk_hz);
                        isl_set_sampler_phase(&isl_dev, vmode_in.sampler_phase);

                        // Setup Si5351
                        if (amode_match)
                            si5351_set_frac_mult(&si_dev, SI_PLLA, SI_CLK4, SI_CLKIN, &vmode_out.si_ms_conf);
                        else
                            si5351_set_integer_mult(&si_dev, SI_PLLA, SI_CLK4, SI_CLKIN, pclk_hz, vmode_out.si_pclk_mult, 0);

                        // Wait a couple frames so that next sync measurements from FPGA are stable
                        // TODO: don't use pclk for the sync meas
                        usleep(40000);

                        // TODO: dont read polarity from ISL51002
                        sys_ctrl &= ~(SCTRL_ISL_VS_POL);
                        if ((target_isl_sync == SYNC_HV) && isl_dev.ss.v_polarity)
                            sys_ctrl |= SCTRL_ISL_VS_POL;
                        IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);

                        update_sc_config(&vmode_in, &vmode_out, &vm_conf, cur_avconfig);

                        // Setup VIC and pixel repetition
                        adv7513_set_pixelrep_vic(&advtx_dev, vmode_out.tx_pixelrep, vmode_out.hdmitx_pixr_ifr, vmode_out.vic);
                    }
                } else if (status == SC_CONFIG_CHANGE) {
                    update_sc_config(&vmode_in, &vmode_out, &vm_conf, cur_avconfig);
                }
            } else {
    
            }

            isl_update_config(&isl_dev, &cur_avconfig->isl_cfg);
        }

        adv7513_check_hpd_power(&advtx_dev);
        adv7513_update_config(&advtx_dev, &cur_avconfig->adv7513_cfg);

        usleep(300);    // Avoid executing mainloop multiple times per vsync
    }

    return 0;
}
