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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include "system.h"
#include "video_modes.h"
#include "avconfig.h"

#define LINECNT_MAX_TOLERANCE   30

#define VM_OUT_YMULT        (vm_conf->y_rpt+1)
#define VM_OUT_XMULT        (vm_conf->x_rpt+1)
#define VM_OUT_PCLKMULT     (((vm_conf->x_rpt+1)*(vm_conf->y_rpt+1))/(vm_conf->h_skip+1))

#include "video_modes_list.c"

const unsigned num_stdmodes = sizeof(stdmode_idx_arr)/sizeof(stdmode_t);

mode_data_t video_modes[sizeof(video_modes_default)/sizeof(mode_data_t)];
//ad_mode_data_t adaptive_modes[sizeof(adaptive_modes_default)/sizeof(ad_mode_data_t)];

void set_default_vm_table() {
    memcpy(video_modes, video_modes_default, sizeof(video_modes_default));
    //memcpy(adaptive_modes, adaptive_modes_default, sizeof(adaptive_modes_default));
}

void vmode_hv_mult(mode_data_t *vmode, uint8_t h_mult, uint8_t v_mult) {
    uint32_t val, bp_extra;

    val = vmode->timings.h_synclen * h_mult;
    if (val >= (1<<8)) {
        vmode->timings.h_synclen = (1<<8)-1;
        bp_extra = val - vmode->timings.h_synclen;
    } else {
        vmode->timings.h_synclen = val;
        bp_extra = 0;
    }

    val = (vmode->timings.h_backporch * h_mult) + bp_extra;
    if (val >= (1<<9))
        vmode->timings.h_backporch = (1<<9)-1;
    else
        vmode->timings.h_backporch = val;

    val = vmode->timings.h_active * h_mult;
    if (val >= (1<<11))
        vmode->timings.h_active = (1<<11)-1;
    else
        vmode->timings.h_active = val;

    vmode->timings.h_total = h_mult * vmode->timings.h_total + ((h_mult * vmode->timings.h_total_adj * 5 + 50) / 100);

    val = vmode->timings.v_synclen * v_mult;
    if (val >= (1<<4)) {
        vmode->timings.v_synclen = (1<<4)-1;
        bp_extra = val - vmode->timings.v_synclen;
    } else {
        vmode->timings.v_synclen = val;
        bp_extra = 0;
    }

    val = (vmode->timings.v_backporch * v_mult) + bp_extra;
    if (val >= (1<<9))
        vmode->timings.v_backporch = (1<<9)-1;
    else
        vmode->timings.v_backporch = val;

    val = vmode->timings.v_active * v_mult;
    if (val >= (1<<11))
        vmode->timings.v_active = (1<<11)-1;
    else
        vmode->timings.v_active = val;

    if (vmode->timings.interlaced && ((v_mult % 2) == 0)) {
        vmode->timings.interlaced = 0;
        vmode->timings.v_total *= (v_mult / 2);
    } else {
        vmode->timings.v_total *= v_mult;
    }
}

uint32_t estimate_dotclk(mode_data_t *vm_in, uint32_t h_hz) {
    if ((vm_in->type & VIDEO_SDTV) ||
        (vm_in->type & VIDEO_EDTV))
    {
        return h_hz * 858;
    } else {
        return vm_in->timings.h_total * h_hz;
    }
}

int get_framelock_config(mode_data_t *vm_in, ad_mode_id_t target_ad_id_list[][2], smp_mode_t target_sm_list[], mode_data_t *vm_out, ad_mode_data_t **ad_preset_o) {
    int i, j;
    ad_mode_data_t *ad_preset;
    smp_preset_t *smp_preset;
    mode_data_t *mode_preset;

    // Check adaptive LM presets first
    for (i=0; i<sizeof(adaptive_modes)/sizeof(ad_mode_data_t); i++) {
        ad_preset = (ad_mode_data_t*)&adaptive_modes[i];
        smp_preset = &smp_presets_default[ad_preset->smp_preset_id];

        if (smp_preset->timings_i.v_hz_max && (vm_in->timings.v_hz_max > smp_preset->timings_i.v_hz_max))
            continue;

        if (((ad_preset->v_total_override && (vm_in->timings.v_total == ad_preset->v_total_override)) || (!ad_preset->v_total_override && (vm_in->timings.v_total == smp_preset->timings_i.v_total))) &&
            (!vm_in->timings.h_total || (vm_in->timings.h_total == smp_preset->timings_i.h_total)) &&
            (vm_in->timings.interlaced == smp_preset->timings_i.interlaced) &&
            ((target_ad_id_list[smp_preset->group][0] == ad_preset->id) || (target_ad_id_list[smp_preset->group][1] == ad_preset->id)) &&
            (vm_in->timings.h_total || (target_sm_list[smp_preset->group] == smp_preset->sm)))
        {
            if (!vm_in->timings.h_active)
                vm_in->timings.h_active = smp_preset->timings_i.h_active;
            if (!vm_in->timings.v_active)
                vm_in->timings.v_active = smp_preset->timings_i.v_active;
            if ((!vm_in->timings.h_synclen) || (!vm_in->timings.h_backporch))
                vm_in->timings.h_synclen = smp_preset->timings_i.h_synclen;
            if (!vm_in->timings.v_synclen)
                vm_in->timings.v_synclen = smp_preset->timings_i.v_synclen;
            if (!vm_in->timings.h_backporch)
                vm_in->timings.h_backporch = smp_preset->timings_i.h_backporch;
            if (!vm_in->timings.v_backporch)
                vm_in->timings.v_backporch = smp_preset->timings_i.v_backporch;
            vm_in->timings.h_total = smp_preset->timings_i.h_total;
            vm_in->timings.h_total_adj = smp_preset->timings_i.h_total_adj;
            vm_in->sampler_phase = smp_preset->sampler_phase;
            vm_in->type = smp_preset->type;
            if (vm_in->name[0] == 0)
                strncpy(vm_in->name, smp_preset->name, 14);

            memcpy(vm_out, &video_modes_default[ad_mode_id_map[ad_preset->id]], sizeof(mode_data_t));

            vm_out->si_pclk_mult = 0;
            memcpy(&vm_out->si_ms_conf, &ad_preset->si_ms_conf, sizeof(si5351_ms_config_t));

            *ad_preset_o = ad_preset;
            return 0;
        }
    }

    // Go through sampling presets next to see if there is one compatible with input and video mode pointed by target_ad_id_list (passthru / 2x)
    for (i=0; i<sizeof(smp_presets_default)/sizeof(smp_preset_t); i++) {
        smp_preset = &smp_presets_default[i];

        if (smp_preset->timings_i.v_hz_max && (vm_in->timings.v_hz_max > smp_preset->timings_i.v_hz_max))
            continue;

        for (j=0; j<2; j++) {
            mode_preset = (mode_data_t*)&video_modes_default[ad_mode_id_map[target_ad_id_list[smp_preset->group][j]]];

            if (((vm_in->timings.v_total == smp_preset->timings_i.v_total) && (vm_in->timings.v_total == mode_preset->timings.v_total)) &&
                (!vm_in->timings.h_total || ((vm_in->timings.h_total == smp_preset->timings_i.h_total) && (vm_in->timings.h_total == mode_preset->timings.h_total))) &&
                (vm_in->timings.interlaced == smp_preset->timings_i.interlaced) &&
                (vm_in->timings.h_total || (target_sm_list[smp_preset->group] == smp_preset->sm)))
            {
                if (!vm_in->timings.h_active)
                    vm_in->timings.h_active = smp_preset->timings_i.h_active;
                if (!vm_in->timings.v_active)
                    vm_in->timings.v_active = smp_preset->timings_i.v_active;
                if ((!vm_in->timings.h_synclen) || (!vm_in->timings.h_backporch))
                    vm_in->timings.h_synclen = smp_preset->timings_i.h_synclen;
                if (!vm_in->timings.v_synclen)
                    vm_in->timings.v_synclen = smp_preset->timings_i.v_synclen;
                if (!vm_in->timings.h_backporch)
                    vm_in->timings.h_backporch = smp_preset->timings_i.h_backporch;
                if (!vm_in->timings.v_backporch)
                    vm_in->timings.v_backporch = smp_preset->timings_i.v_backporch;
                vm_in->timings.h_total = smp_preset->timings_i.h_total;
                vm_in->timings.h_total_adj = smp_preset->timings_i.h_total_adj;
                vm_in->sampler_phase = smp_preset->sampler_phase;
                vm_in->type = smp_preset->type;
                if (vm_in->name[0] == 0)
                    strncpy(vm_in->name, smp_preset->name, 14);

                memcpy(vm_out, mode_preset, sizeof(mode_data_t));
                memset(&vm_out->si_ms_conf, 0, sizeof(si5351_ms_config_t));

                if (vm_in->timings.interlaced && !mode_preset->timings.interlaced) {
                    vm_out->si_pclk_mult = 2;
                } else if (vm_in->timings.interlaced && !mode_preset->timings.interlaced) {
                    vm_out->si_pclk_mult = 1;
                    vm_out->si_ms_conf.outdiv = 1;
                } else {
                    vm_out->si_pclk_mult = 1;
                }

                return 1;
            }
        }
    }

    return -1;
}

int get_scaler_mode(mode_data_t *vm_in, mode_data_t *vm_out, vm_mult_config_t *vm_conf, int *framelock)
{
    int i, diff_lines, mindiff_id, mindiff_lines=1000;
    mode_data_t *freerun_preset = NULL;
    ad_mode_data_t *ad_preset;
    smp_preset_t *smp_preset;
    uint8_t h_skip = 0;
    avconfig_t* cc = get_current_avconfig();
    memset(vm_out, 0, sizeof(mode_data_t));

    const ad_mode_id_t pm_scl_map[][2] = {{-1, ADMODE_480p},
                                         {ADMODE_576p, -1},
                                         {-1, ADMODE_720p_60},
                                         {-1, ADMODE_1280x1024_60},
                                         {ADMODE_1080p_50_CR, ADMODE_1080p_60_LB},
                                         {-1, ADMODE_1600x1200_60},
                                         {ADMODE_1920x1200_50, ADMODE_1920x1200_60},
                                         {ADMODE_1920x1440_50, ADMODE_1920x1440_60},
                                         {ADMODE_2560x1440_50, ADMODE_2560x1440_60}};

    const smp_mode_t sm_240p_288p_map[] = {SM_GEN_4_3,
                                          SM_OPT_SNES_256COL, SM_OPT_SNES_512COL,
                                          SM_OPT_MD_256COL, SM_OPT_MD_320COL,
                                          SM_OPT_PSX_256COL, SM_OPT_PSX_320COL, SM_OPT_PSX_384COL, SM_OPT_PSX_512COL, SM_OPT_PSX_640COL,
                                          SM_OPT_SAT_320COL, SM_OPT_SAT_352COL, SM_OPT_SAT_640COL, SM_OPT_SAT_704COL,
                                          SM_OPT_N64_320COL, SM_OPT_N64_640COL};
    const smp_mode_t sm_480i_map[] = {SM_GEN_4_3, SM_OPT_DTV480I};
    const smp_mode_t sm_576i_map[] = {SM_GEN_4_3, SM_OPT_DTV576I};
    const smp_mode_t sm_480p_map[] = {SM_GEN_4_3, SM_OPT_DTV480P, SM_OPT_VGA480P60};
    const smp_mode_t sm_576p_map[] = {SM_GEN_4_3, SM_OPT_DTV576P};

    ad_mode_id_t target_ad_id_list[9][2];
    for (i=0; i<9; i++) {
        target_ad_id_list[i][0] = pm_scl_map[cc->scl_out_mode][0];
        target_ad_id_list[i][1] = pm_scl_map[cc->scl_out_mode][1];
    }


    smp_mode_t target_sm_list[] = { SM_OPT_PC_HDTV,                         // GROUP_NONE
                                    sm_240p_288p_map[cc->sm_scl_240p_288p], // GROUP_240P
                                    sm_240p_288p_map[cc->sm_scl_240p_288p], // GROUP_288P
                                    SM_OPT_PC_HDTV,                         // GROUP_384P
                                    sm_480i_map[cc->sm_scl_480i_576i],      // GROUP_480I
                                    sm_576i_map[cc->sm_scl_480i_576i],      // GROUP_576I
                                    sm_480p_map[cc->sm_scl_480p],           // GROUP_480P
                                    sm_576p_map[cc->sm_scl_576p],           // GROUP_576P
                                    SM_OPT_PC_HDTV};                        // GROUP_1080I

    if (cc->scl_framelock == SCL_FL_ON) {
        if (get_framelock_config(vm_in, target_ad_id_list, target_sm_list, vm_out, &ad_preset) < 0)
            freerun_preset = (target_ad_id_list[0][0] != (ad_mode_id_t)-1) ? (mode_data_t*)&video_modes_default[ad_mode_id_map[target_ad_id_list[0][0]]] : (mode_data_t*)&video_modes_default[ad_mode_id_map[target_ad_id_list[0][1]]];
        else
            h_skip = smp_presets_default[ad_preset->smp_preset_id].h_skip;
    } else if (cc->scl_framelock == SCL_FL_OFF_50HZ) {
        freerun_preset = (target_ad_id_list[0][0] != (ad_mode_id_t)-1) ? (mode_data_t*)&video_modes_default[ad_mode_id_map[target_ad_id_list[0][0]]] : (mode_data_t*)&video_modes_default[ad_mode_id_map[target_ad_id_list[0][1]]];
    } else if (cc->scl_framelock == SCL_FL_OFF_60HZ) {
        freerun_preset = (target_ad_id_list[0][1] != (ad_mode_id_t)-1) ? (mode_data_t*)&video_modes_default[ad_mode_id_map[target_ad_id_list[0][1]]] : (mode_data_t*)&video_modes_default[ad_mode_id_map[target_ad_id_list[0][0]]];
    }

    if (!freerun_preset) {
        *framelock = 1;
    } else {
        *framelock = 0;

        // Go through sampling presets and find closest one for analog sources
        if (!vm_in->timings.h_total) {
            for (i=0; i<sizeof(smp_presets_default)/sizeof(smp_preset_t); i++) {
                smp_preset = &smp_presets_default[i];

                if ((vm_in->timings.interlaced == smp_preset->timings_i.interlaced) &&
                    (!smp_preset->timings_i.v_hz_max || (vm_in->timings.v_hz_max <= smp_preset->timings_i.v_hz_max)) &&
                    (target_sm_list[smp_preset->group] == smp_preset->sm))
                {
                    diff_lines = abs(vm_in->timings.v_total - smp_preset->timings_i.v_total);

                    if (diff_lines < mindiff_lines) {
                        mindiff_id = i;
                        mindiff_lines = diff_lines;
                    }

                    if (mindiff_lines == 0)
                        break;
                }
            }

            if (mindiff_lines >= 1000)
                return -1;

            smp_preset = &smp_presets_default[mindiff_id];
            vm_in->timings.h_active = smp_preset->timings_i.h_active;
            vm_in->timings.v_active = smp_preset->timings_i.v_active;
            vm_in->timings.h_synclen = smp_preset->timings_i.h_synclen;
            vm_in->timings.v_synclen = smp_preset->timings_i.v_synclen;
            vm_in->timings.h_backporch = smp_preset->timings_i.h_backporch;
            vm_in->timings.v_backporch = smp_preset->timings_i.v_backporch;
            vm_in->timings.h_total = smp_preset->timings_i.h_total;
            vm_in->timings.h_total_adj = smp_preset->timings_i.h_total_adj;
            vm_in->sampler_phase = smp_preset->sampler_phase;
            vm_in->type = smp_preset->type;
            strncpy(vm_in->name, smp_preset->name, 14);

            h_skip = smp_preset->h_skip;
        }

        memcpy(vm_out, freerun_preset, sizeof(mode_data_t));
    }

    memset(vm_conf, 0, sizeof(vm_mult_config_t));
    vm_conf->h_skip = h_skip;

    const unsigned aspect_map[][2] = {{4, 3},
                                       {16, 9},
                                       {8, 7},
                                       {vm_in->timings.h_active, vm_in->timings.v_active}};

    // Calculate size and position based on aspect ratio
    if (cc->scl_aspect < 4) {
        if (vm_out->timings.v_active*aspect_map[cc->scl_aspect][0] <= vm_out->timings.h_active*aspect_map[cc->scl_aspect][1]) {
            // Pillarbox
            vm_conf->y_size = vm_out->timings.v_active;
            vm_conf->x_size = (aspect_map[cc->scl_aspect][0]*vm_out->timings.v_active)/aspect_map[cc->scl_aspect][1];
            vm_conf->x_offset = (vm_out->timings.h_active - vm_conf->x_size)/2;
        } else {
            // Letterbox
            vm_conf->x_size = vm_out->timings.h_active;
            vm_conf->y_size = (aspect_map[cc->scl_aspect][1]*vm_out->timings.h_active)/aspect_map[cc->scl_aspect][0];
            vm_conf->y_offset = (vm_out->timings.v_active - vm_conf->y_size)/2;
        }
    } else {
        vm_conf->x_size = vm_out->timings.h_active;
        vm_conf->y_size = vm_out->timings.v_active;
    }

    printf("x_offset: %d, x_size: %u\ny_offset: %d, y_size: %u\n", vm_conf->x_offset, vm_conf->x_size, vm_conf->y_offset, vm_conf->y_size);

    return 0;
}

int get_adaptive_lm_mode(mode_data_t *vm_in, mode_data_t *vm_out, vm_mult_config_t *vm_conf)
{
    ad_mode_data_t *ad_preset = NULL;
    smp_preset_t *smp_preset;
    int32_t v_linediff;
    uint32_t in_interlace_mult, out_interlace_mult, vtotal_ref;
    avconfig_t* cc = get_current_avconfig();
    memset(vm_out, 0, sizeof(mode_data_t));

    const ad_mode_id_t pm_ad_240p_map[] = {-1, ADMODE_480p, ADMODE_720p_60, ADMODE_1280x1024_60, ADMODE_1080i_60_LB, ADMODE_1080p_60_LB, ADMODE_1080p_60_CR, ADMODE_1600x1200_60, ADMODE_1920x1200_60, ADMODE_1920x1440_60, ADMODE_2560x1440_60};
    const ad_mode_id_t pm_ad_288p_map[] = {-1, ADMODE_576p, ADMODE_1080i_50_CR, ADMODE_1080p_50_CR, ADMODE_1920x1200_50, ADMODE_1920x1440_50, ADMODE_2560x1440_50};
    const ad_mode_id_t pm_ad_480i_map[] = {-1, ADMODE_240p, ADMODE_1280x1024_60, ADMODE_1080i_60_LB, ADMODE_1080p_60_LB, ADMODE_1920x1440_60, ADMODE_2560x1440_60};
    const ad_mode_id_t pm_ad_576i_map[] = {-1, ADMODE_288p, ADMODE_1080i_50_CR, ADMODE_1080p_50_CR};
    const ad_mode_id_t pm_ad_480p_map[] = {-1, ADMODE_240p, ADMODE_1280x1024_60, ADMODE_1080i_60_LB, ADMODE_1080p_60_LB, ADMODE_1920x1440_60, ADMODE_2560x1440_60};
    const ad_mode_id_t pm_ad_576p_map[] = {-1, ADMODE_288p, ADMODE_1920x1200_50};


    const smp_mode_t sm_240p_288p_map[] = {SM_GEN_4_3,
                                          SM_OPT_SNES_256COL, SM_OPT_SNES_512COL,
                                          SM_OPT_MD_256COL, SM_OPT_MD_320COL,
                                          SM_OPT_PSX_256COL, SM_OPT_PSX_320COL, SM_OPT_PSX_384COL, SM_OPT_PSX_512COL, SM_OPT_PSX_640COL,
                                          SM_OPT_SAT_320COL, SM_OPT_SAT_352COL, SM_OPT_SAT_640COL, SM_OPT_SAT_704COL,
                                          SM_OPT_N64_320COL, SM_OPT_N64_640COL};
    const smp_mode_t sm_480i_map[] = {SM_GEN_4_3, SM_GEN_16_9, SM_OPT_DTV480I, SM_OPT_DTV480I_WS};
    const smp_mode_t sm_576i_map[] = {SM_GEN_4_3, SM_GEN_16_9, SM_OPT_DTV576I, SM_OPT_DTV576I};
    const smp_mode_t sm_480p_map[] = {SM_GEN_4_3, SM_GEN_16_9, SM_OPT_DTV480P, SM_OPT_DTV480P_WS, SM_OPT_VGA480P60};
    const smp_mode_t sm_576p_map[] = {SM_GEN_4_3};

    ad_mode_id_t target_ad_id_list[][2] = { {-1, -1},
                                            {pm_ad_240p_map[cc->pm_ad_240p], -1},
                                            {pm_ad_288p_map[cc->pm_ad_288p], -1},
                                            {-1, -1},
                                            {pm_ad_480i_map[cc->pm_ad_480i], -1},
                                            {pm_ad_576i_map[cc->pm_ad_576i], -1},
                                            {pm_ad_480p_map[cc->pm_ad_480p], -1},
                                            {pm_ad_576p_map[cc->pm_ad_576p], -1},
                                            {-1, -1}};

    smp_mode_t target_sm_list[] = { -1,                                     // GROUP_NONE
                                    sm_240p_288p_map[cc->sm_ad_240p_288p],  // GROUP_240P
                                    sm_240p_288p_map[cc->sm_ad_240p_288p],  // GROUP_288P
                                    -1,                                     // GROUP_384P
                                    sm_480i_map[cc->sm_ad_480i_576i],       // GROUP_480I
                                    sm_576i_map[cc->sm_ad_480i_576i],       // GROUP_576I
                                    sm_480p_map[cc->sm_ad_480p],            // GROUP_480P
                                    sm_576p_map[cc->sm_ad_576p],            // GROUP_576P
                                    -1};                                    // GROUP_1080I


    if (get_framelock_config(vm_in, target_ad_id_list, target_sm_list, vm_out, &ad_preset) != 0)
        return -1;

    smp_preset = &smp_presets_default[ad_preset->smp_preset_id];

    in_interlace_mult = vm_in->timings.interlaced ? 2 : 1;
    out_interlace_mult = vm_out->timings.interlaced ? 2 : 1;

    vm_conf->x_rpt = ad_preset->x_rpt;
    vm_conf->y_rpt = ad_preset->y_rpt;
    vm_conf->h_skip = smp_preset->h_skip;
    vm_conf->x_offset = ((vm_out->timings.h_active-vm_in->timings.h_active*(vm_conf->x_rpt+1))/2) + ad_preset->x_offset_i;
    vm_conf->x_start_lb = (vm_conf->x_offset >= 0) ? 0 : (-vm_conf->x_offset / (vm_conf->x_rpt+1));
    vm_conf->x_size = vm_in->timings.h_active*(vm_conf->x_rpt+1);
    if (vm_conf->x_size >= 4096)
        vm_conf->x_size = 4095;
    if (vm_conf->y_rpt == (uint8_t)(-1)) {
        vm_conf->y_start_lb = ((vm_in->timings.v_active - (vm_out->timings.v_active*2))/2) + ad_preset->y_offset_i*2;
        vm_conf->y_offset = -vm_conf->y_start_lb/2;
        vm_conf->y_size = vm_in->timings.v_active/2;
    } else {
        vm_conf->y_start_lb = ((vm_in->timings.v_active - (vm_out->timings.v_active/(vm_conf->y_rpt+1)))/2) + ad_preset->y_offset_i;
        vm_conf->y_offset = -(vm_conf->y_rpt+1)*vm_conf->y_start_lb;
        vm_conf->y_size = vm_in->timings.v_active*(vm_conf->y_rpt+1);
    }

    // calculate the time (in output lines, rounded up) from source frame start to the point where first to-be-visible line has been written into linebuf
    v_linediff = (((vm_in->timings.v_synclen + vm_in->timings.v_backporch + ((vm_conf->y_start_lb < 0) ? 0 : vm_conf->y_start_lb) + 1) * vm_out->timings.v_total * in_interlace_mult) / (vm_in->timings.v_total * out_interlace_mult)) + 1;

    // subtract the previous value from the total number of output blanking/empty lines. Resulting value indicates how many lines output framestart must be offset
    v_linediff = (vm_out->timings.v_synclen + vm_out->timings.v_backporch + ((vm_conf->y_offset < 0) ? 0 : vm_conf->y_offset)) - v_linediff;

    // if linebuf is read faster than written, output framestart must be delayed accordingly to avoid read pointer catching write pointer
    vtotal_ref = (vm_conf->y_rpt == (uint8_t)(-1)) ? ((vm_in->timings.v_total*out_interlace_mult)/2) : (vm_in->timings.v_total*out_interlace_mult*(vm_conf->y_rpt+1));
    if (vm_out->timings.v_total * in_interlace_mult > vtotal_ref)
        v_linediff -= (((vm_in->timings.v_active * vm_out->timings.v_total * in_interlace_mult) / (vm_in->timings.v_total * out_interlace_mult)) - vm_conf->y_size);

    vm_conf->framesync_line = (v_linediff < 0) ? (vm_out->timings.v_total/out_interlace_mult)+v_linediff : v_linediff;

    printf("framesync_line = %u\nx_start_lb: %d, x_offset: %d, x_size: %u\ny_start_lb: %d, y_offset: %d, y_size: %u\n", vm_conf->framesync_line, vm_conf->x_start_lb, vm_conf->x_offset, vm_conf->x_size, vm_conf->y_start_lb, vm_conf->y_offset, vm_conf->y_size);

    return 0;
}

int get_pure_lm_mode(mode_data_t *vm_in, mode_data_t *vm_out, vm_mult_config_t *vm_conf)
{
    int i;
    unsigned num_modes = sizeof(video_modes)/sizeof(mode_data_t);
    avconfig_t* cc = get_current_avconfig();
    mode_flags valid_lm[] = { MODE_PT, (MODE_L2 | (MODE_L2<<cc->l2_mode)), (MODE_L3_GEN_16_9<<cc->l3_mode), (MODE_L4_GEN_4_3<<cc->l4_mode), (MODE_L5_GEN_4_3<<cc->l5_mode) };
    mode_flags target_lm;
    uint8_t pt_only = 0;
    uint8_t nonsampled_h_mult = 0, nonsampled_v_mult = 0;
    uint8_t upsample2x = vm_in->timings.h_total ? 0 : cc->upsample2x;

    // one for each video_group
    uint8_t* group_ptr[] = { &pt_only, &cc->pm_240p, &cc->pm_240p, &cc->pm_384p, &cc->pm_480i, &cc->pm_480i, &cc->pm_480p, &cc->pm_480p, &cc->pm_1080i };

    for (i=0; i<num_modes; i++) {
        switch (video_modes[i].group) {
            case GROUP_384P:
                //fixed Line2x/3x mode for 240x360p
                valid_lm[2] = MODE_L2_240x360;
                valid_lm[3] = MODE_L3_240x360;
                valid_lm[4] = MODE_L3_GEN_16_9;
                if ((!vm_in->timings.h_total) && (video_modes[i].timings.v_total == 449)) {
                    if (!strncmp(video_modes[i].name, "720x400_70", 10)) {
                        if (cc->s400p_mode == 0)
                            continue;
                    } else if (!strncmp(video_modes[i].name, "640x400_70", 10)) {
                        if (cc->s400p_mode == 1)
                            continue;
                    }
                }
                break;
            case GROUP_480I:
            case GROUP_576I:
                //fixed Line3x/4x mode for 480i
                valid_lm[2] = MODE_L3_GEN_16_9;
                valid_lm[3] = MODE_L4_GEN_4_3;
                break;
            case GROUP_480P:
                 if (video_modes[i].vic == HDMI_480p60) {
                    switch (cc->s480p_mode) {
                        case 0: // Auto
                            if (vm_in->timings.h_synclen > 82)
                                continue;
                            break;
                        case 1: // DTV 480p
                            break;
                        default:
                            continue;
                    }
                } else if (video_modes[i].vic == HDMI_640x480p60) {
                    switch (cc->s480p_mode) {
                        case 0: // Auto
                        case 2: // VESA 640x480@60
                            break;
                        default:
                            continue;
                    }
                }
                break;
            default:
                break;
        }

        if (video_modes[i].timings.v_hz_max && (vm_in->timings.v_hz_max > video_modes[i].timings.v_hz_max))
            continue;

        target_lm = valid_lm[*group_ptr[video_modes[i].group]];

        // HDMI input modes
        if (vm_in->timings.h_total) {
            if (target_lm >= MODE_L5_GEN_4_3)
                nonsampled_v_mult = 5;
            else if (target_lm >= MODE_L4_GEN_4_3)
                nonsampled_v_mult = 4;
            else if (target_lm >= MODE_L3_GEN_16_9)
                nonsampled_v_mult = 3;
            else if (target_lm >= MODE_L2)
                nonsampled_v_mult = 2;
            else
                nonsampled_v_mult = 1;

            target_lm = MODE_PT;
        }

        if ((target_lm & video_modes[i].flags) &&
            (vm_in->timings.interlaced == video_modes[i].timings.interlaced) &&
            (vm_in->timings.v_total <= (video_modes[i].timings.v_total+LINECNT_MAX_TOLERANCE)))
        {

            if (!vm_in->timings.h_active)
                vm_in->timings.h_active = video_modes[i].timings.h_active;
            if (!vm_in->timings.v_active)
                vm_in->timings.v_active = video_modes[i].timings.v_active;
            if ((!vm_in->timings.h_synclen) || (!vm_in->timings.h_backporch))
                vm_in->timings.h_synclen = video_modes[i].timings.h_synclen;
            if (!vm_in->timings.v_synclen)
                vm_in->timings.v_synclen = video_modes[i].timings.v_synclen;
            if (!vm_in->timings.h_backporch)
                vm_in->timings.h_backporch = video_modes[i].timings.h_backporch;
            if (!vm_in->timings.v_backporch)
                vm_in->timings.v_backporch = video_modes[i].timings.v_backporch;
            if (!vm_in->timings.h_total)
                vm_in->timings.h_total = video_modes[i].timings.h_total;
            vm_in->timings.h_total_adj = video_modes[i].timings.h_total_adj;
            vm_in->sampler_phase = video_modes[i].sampler_phase;
            vm_in->type = video_modes[i].type;
            if (!vm_in->vic)
                vm_in->vic = video_modes[i].vic;
            if (vm_in->name[0] == 0)
                strncpy(vm_in->name, video_modes[i].name, 14);

            memcpy(vm_out, vm_in, sizeof(mode_data_t));
            vm_out->vic = HDMI_Unknown;
            vm_out->si_pclk_mult = 1;
            vm_out->tx_pixelrep = TX_1X;
            vm_out->hdmitx_pixr_ifr = TX_1X;

            vm_conf->x_rpt = 0;
            vm_conf->y_rpt = 0;
            vm_conf->h_skip = 0;
            vm_conf->x_offset = 0;
            vm_conf->y_offset = 0;
            vm_conf->x_size = 0;
            vm_conf->y_size = 0;
            vm_conf->x_start_lb = 0;
            vm_conf->y_start_lb = 0;

            target_lm &= video_modes[i].flags;    //ensure L2 mode uniqueness

            if (nonsampled_v_mult) {
                if (nonsampled_v_mult > 1)
                    nonsampled_h_mult = (((((uint32_t)vm_in->timings.v_active*nonsampled_v_mult*40)/3)/vm_in->timings.h_active)+5)/10;

                vm_conf->x_rpt = (nonsampled_h_mult == 0) ? 0 : (nonsampled_h_mult-1);
                vm_conf->y_rpt = nonsampled_v_mult-1;

                vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);

                if ((vm_out->timings.v_hz_max*vm_out->timings.v_total*vm_out->timings.h_total)>>vm_out->timings.interlaced < 25000000UL) {
                    vm_out->tx_pixelrep = TX_2X;
                    vm_out->hdmitx_pixr_ifr = TX_2X;
                }

                vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
            } else {
                switch (target_lm) {
                    case MODE_PT:
                        vm_out->vic = vm_in->vic;
                        // Upsample / pixel-repeat horizontal resolution if necessary to fulfill min. 25MHz TMDS clock requirement
                        if ((vm_out->timings.v_hz_max*vm_out->timings.v_total*vm_out->timings.h_total)>>vm_out->timings.interlaced < 25000000UL) {
                            if (upsample2x) {
                                vmode_hv_mult(vm_in, 2, 1);
                                vmode_hv_mult(vm_out, 2, VM_OUT_YMULT);
                            } else {
                                vm_out->tx_pixelrep = TX_2X;
                            }
                            vm_out->hdmitx_pixr_ifr = TX_2X;
                        }
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        break;
                    case MODE_L2:
                        vm_conf->y_rpt = 1;

                        // Upsample / pixel-repeat horizontal resolution of 384p/480p/960i modes
                        if ((video_modes[i].group == GROUP_384P) || (video_modes[i].group == GROUP_480P) || (video_modes[i].group == GROUP_576P) || ((video_modes[i].group == GROUP_1080I) && (video_modes[i].timings.h_total < 1200))) {
                            if (upsample2x) {
                                vmode_hv_mult(vm_in, 2, 1);
                                vmode_hv_mult(vm_out, 2, VM_OUT_YMULT);
                            } else {
                                vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                                vm_out->tx_pixelrep = TX_2X;
                            }
                        } else {
                            vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        }
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        break;
                    case MODE_L2_512_COL:
                        vm_conf->y_rpt = 1;
                        vm_conf->x_rpt = 1;
                        vm_conf->h_skip = 1;
                        vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        break;
                    case MODE_L2_256_COL:
                        vm_conf->y_rpt = 1;
                        vm_conf->x_rpt = 2;
                        vm_conf->h_skip = 2;
                        vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        break;
                    case MODE_L2_320_COL:
                    case MODE_L2_384_COL:
                        vm_conf->y_rpt = 1;
                        vm_conf->x_rpt = 1;
                        vm_conf->h_skip = 1;
                        vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        break;
                    case MODE_L2_240x360:
                        vm_conf->y_rpt = 1;
                        vm_conf->x_rpt = 4;
                        vm_conf->h_skip = 4;
                        vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        break;
                    case MODE_L3_GEN_16_9:
                        vm_conf->y_rpt = 2;

                        // Upsample / pixel-repeat horizontal resolution of 480i mode
                        if ((video_modes[i].group == GROUP_480I) || (video_modes[i].group == GROUP_576I)) {
                            if (upsample2x) {
                                vmode_hv_mult(vm_in, 2, 1);
                                vmode_hv_mult(vm_out, 2, VM_OUT_YMULT);
                            } else {
                                vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                                vm_out->tx_pixelrep = TX_2X;
                            }
                        } else {
                            vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        }
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        break;
                    case MODE_L3_GEN_4_3:
                        vm_conf->y_rpt = 2;
                        vm_conf->x_size = vm_out->timings.h_active;
                        vm_out->timings.h_synclen /= 3;
                        vm_out->timings.h_backporch /= 3;
                        vm_out->timings.h_active /= 3;
                        vm_conf->x_offset = vm_out->timings.h_active/2;
                        vm_out->timings.h_total /= 3;
                        vm_out->timings.h_total_adj = 0;
                        vmode_hv_mult(vm_out, 4, VM_OUT_YMULT);
                        vm_out->si_pclk_mult = 4;
                        break;
                    case MODE_L3_512_COL:
                        vm_conf->y_rpt = 2;
                        vm_conf->x_rpt = 1;
                        vm_conf->h_skip = 1;
                        vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        break;
                    case MODE_L3_384_COL:
                        vm_conf->y_rpt = 2;
                        vm_conf->x_rpt = 2;
                        vm_conf->h_skip = 2;
                        vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        break;
                    case MODE_L3_320_COL:
                        vm_conf->y_rpt = 2;
                        vm_conf->x_rpt = 3;
                        vm_conf->h_skip = 3;
                        vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        break;
                    case MODE_L3_256_COL:
                        vm_conf->y_rpt = 2;
                        vm_conf->x_rpt = 4;
                        vm_conf->h_skip = 4;
                        vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        break;
                    case MODE_L3_240x360:
                        vm_conf->y_rpt = 2;
                        vm_conf->x_rpt = 6;
                        vm_conf->h_skip = 6;
                        vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        //cm.hsync_cut = 13;
                        break;
                    case MODE_L4_GEN_4_3:
                        vm_conf->y_rpt = 3;

                        // Upsample / pixel-repeat horizontal resolution of 480i mode
                        if ((video_modes[i].group == GROUP_480I) || (video_modes[i].group == GROUP_576I)) {
                            if (upsample2x) {
                                vmode_hv_mult(vm_in, 2, 1);
                                vmode_hv_mult(vm_out, 2, VM_OUT_YMULT);
                            } else {
                                vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                                vm_out->tx_pixelrep = TX_2X;
                            }
                        } else {
                            vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        }
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        break;
                    case MODE_L4_512_COL:
                        vm_conf->y_rpt = 3;
                        vm_conf->x_rpt = 1;
                        vm_conf->h_skip = 1;
                        vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        break;
                    case MODE_L4_384_COL:
                        vm_conf->y_rpt = 3;
                        vm_conf->x_rpt = 2;
                        vm_conf->h_skip = 2;
                        vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        break;
                    case MODE_L4_320_COL:
                        vm_conf->y_rpt = 3;
                        vm_conf->x_rpt = 3;
                        vm_conf->h_skip = 3;
                        vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        break;
                    case MODE_L4_256_COL:
                        vm_conf->y_rpt = 3;
                        vm_conf->x_rpt = 4;
                        vm_conf->h_skip = 4;
                        vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        break;
                    case MODE_L5_GEN_4_3:
                        vm_conf->y_rpt = 4;
                        vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        break;
                    case MODE_L5_512_COL:
                        vm_conf->y_rpt = 4;
                        vm_conf->x_rpt = 2;
                        vm_conf->h_skip = 2;
                        vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        //cm.hsync_cut = 40;
                        break;
                    case MODE_L5_384_COL:
                        vm_conf->y_rpt = 4;
                        vm_conf->x_rpt = 3;
                        vm_conf->h_skip = 3;
                        vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        //cm.hsync_cut = 30;
                        break;
                    case MODE_L5_320_COL:
                        vm_conf->y_rpt = 4;
                        vm_conf->x_rpt = 4;
                        vm_conf->h_skip = 4;
                        vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        //cm.hsync_cut = 24;
                        break;
                    case MODE_L5_256_COL:
                        vm_conf->y_rpt = 4;
                        vm_conf->x_rpt = 5;
                        vm_conf->h_skip = 5;
                        vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                        vm_out->si_pclk_mult = VM_OUT_PCLKMULT;
                        //cm.hsync_cut = 20;
                        break;
                    default:
                        printf("WARNING: invalid target_lm\n");
                        continue;
                        break;
                }
            }

            sniprintf(vm_out->name, 14, "%s x%u", vm_in->name, vm_conf->y_rpt+1);

            // Line5x format
            if (vm_conf->y_rpt == 4) {
                // adjust output width to 1920
                if ((cc->l5_fmt != 1) && (nonsampled_h_mult == 0)) {
                    vm_conf->x_size = vm_out->timings.h_active;
                    vm_conf->x_offset = (1920-vm_out->timings.h_active)/2;
                    vm_out->timings.h_synclen = (vm_out->timings.h_total - 1920)/4;
                    vm_out->timings.h_backporch = (vm_out->timings.h_total - 1920)/2;
                    vm_out->timings.h_active = 1920;
                }

                // adjust output height to 1080
                if (cc->l5_fmt == 0) {
                    vm_conf->y_start_lb = (vm_out->timings.v_active-1080)/10;
                    vm_out->timings.v_backporch += 5*vm_conf->y_start_lb;
                    vm_out->timings.v_active = 1080;
                }
            }

            vm_conf->framesync_line = vm_in->timings.interlaced ? ((vm_out->timings.v_total>>vm_out->timings.interlaced)-(vm_conf->y_rpt+1)) : 0;

            if (vm_conf->x_size == 0)
                vm_conf->x_size = vm_out->timings.h_active;
            if (vm_conf->y_size == 0)
                vm_conf->y_size = vm_out->timings.v_active;

            /*if (cm.hdmitx_vic == HDMI_Unknown)
                cm.hdmitx_vic = cm.cc.default_vic;*/

            return 0;
        }
    }

    return -1;
}

int get_standard_mode(unsigned stdmode_idx_arr_idx, vm_mult_config_t *vm_conf, mode_data_t *vm_in, mode_data_t *vm_out)
{
    stdmode_idx_arr_idx = stdmode_idx_arr_idx % num_stdmodes;

    memset(vm_conf, 0, sizeof(vm_mult_config_t));
    memset(vm_in, 0, sizeof(mode_data_t));
    memcpy(vm_out, &video_modes_default[stdmode_idx_arr[stdmode_idx_arr_idx]], sizeof(mode_data_t));

    return 0;
}
