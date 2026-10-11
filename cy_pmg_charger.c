/***************************************************************************//**
* \file cy_pmg_charger.c
* \version 1.0
*
* \brief
* Provides the public functions for the API for the PMG battery charger.
*
********************************************************************************
* \copyright
* Copyright(c) 2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is owned
* by Infineon Technologies AG or one of its affiliates ("Infineon") and is
* protected by and subject to worldwide patent protection, worldwide copyright
* laws, and international treaty provisions. Therefore, you may use this
* Software only as provided in the license agreement accompanying the software
* package from which you obtained this Software. If no license agreement
* applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE IS
* PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING,
* BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF THIRD-PARTY RIGHTS
* AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A SPECIFIC
* USE/PURPOSE OR MERCHANTABILITY. Infineon reserves the right to make changes
* to the Software without notice. You are responsible for properly designing,
* programming, and testing the functionality and safety of your intended
* application of the Software, as well as complying with any legal requirements
* related to its use. Infineon does not guarantee that the Software will be
* free from intrusion, data theft or loss, or other breaches ("Security
* Breaches"), and Infineon shall have no liability arising out of any Security
* Breaches. Unless otherwise explicitly approved by Infineon, the Software may
* not be used in any application where a failure of the Product or any
* consequences of the use thereof can reasonably be expected to result in
* personal injury.
*******************************************************************************/

#include "cy_pdutils.h"
#include "cy_pmg_charger.h"
#include "cy_pmg_charger_hw.h"
#include "cybsp.h"
#include <math.h>

#if defined(DEBUG_PRINT_ENABLE) && (DEBUG_PRINT_ENABLE)
#include <stdio.h>
#endif /* defined(DEBUG_PRINT_ENABLE) && (DEBUG_PRINT_ENABLE) */

/* Kelvin to Celsius offset */
#define PMG_KELVIN_TO_CELSIUS_OFFSET    (273.0f)

/* Debug print buffer size */
#define PMG_DEBUG_PRINT_BUFFER_SIZE     (80u)

static bool cooldown = false;
static cy_stc_battery_charging_context_t glBatChrgCtx;

cy_stc_battery_charging_context_t*  get_battery_charging_context(void)
{
    return (&glBatChrgCtx);
}


static void timer_minute_tick_cb(cy_timer_id_t id, void*  callbackCtx);

/* PMG FSM prototypes. */
static void chg_fsm_off(pmg_fsm_evt_t evt);
static void chg_fsm_init(pmg_fsm_evt_t evt);
static void chg_fsm_init_fet_in(pmg_fsm_evt_t evt);
static void chg_fsm_init_fet_out(pmg_fsm_evt_t evt);
static void chg_fsm_init_bb(pmg_fsm_evt_t evt);
static void chg_fsm_charging_precharge(pmg_fsm_evt_t evt);
static void chg_fsm_charging_normal(pmg_fsm_evt_t evt);
static void chg_fsm_charging_full(pmg_fsm_evt_t evt);
static void chg_fsm_charging_topoff(pmg_fsm_evt_t evt);
static void chg_fsm_shutdown(pmg_fsm_evt_t evt);
static void chg_fsm_cooldown(pmg_fsm_evt_t evt);
static void chg_fsm_source_init(pmg_fsm_evt_t evt);
static void chg_fsm_source(pmg_fsm_evt_t evt);


static void chg_fsm_off(pmg_fsm_evt_t evt)
{
    /* Nothing to do in this state. */
    CY_UNUSED_PARAMETER(evt);
}


static void chg_fsm_init(pmg_fsm_evt_t evt)
{
    if (evt == PMG_FSM_EVT_ENTRY)
    {
        Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_INIT_FET_IN);
    }
}


static void chg_fsm_init_fet_in(pmg_fsm_evt_t evt)
{
    if (evt == PMG_FSM_EVT_ENTRY)
    {
        Cy_PmgChg_HW_Cmd(BATT_CHGE_BB_IN_EN);
    }
    if (evt == PMG_FSM_EVT_IN_SS_CMLT)
    {
        Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_INIT_FET_OUT);
    }
}


static void chg_fsm_init_fet_out(pmg_fsm_evt_t evt)
{
    if (evt == PMG_FSM_EVT_ENTRY)
    {
        Cy_PmgChg_HW_Cmd(BATT_CHGE_BB_OUT_EN);
    }
    if (evt == PMG_FSM_EVT_OUT_SS_CMLT)
    {
        Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_INIT_BB);
    }
}


static void chg_fsm_init_bb(pmg_fsm_evt_t evt)
{
    cy_stc_battery_status_t* batt_stat = &(glBatChrgCtx.batteryStatus);
    const cy_stc_battery_charger_cfg_t* cfg = glBatChrgCtx.ptrBattCghCfg;

    if (evt == PMG_FSM_EVT_ENTRY)
    {
        Cy_PmgChg_HW_Cmd(BATT_CHGE_BB_VSET_PWM);
    }
    if (evt == PMG_FSM_EVT_PWM_INIT_CMLT)
    {
        Cy_PmgChg_HW_Cmd(BATT_CHGE_BB_RST);
        Cy_PmgChg_HW_Cmd(BATT_CHGE_BB_EN);
    }
    if (evt == PMG_FSM_EVT_BB_INIT_CMLT)
    {
        /* Restore from Overheat to state it was before. Avoid ENTRY to continue timeout count. */
        if ((cooldown) &&
            ((batt_stat->pmg_fsm_state_prev == PMG_FSM_CHARGING_PRECHRG) ||
             (batt_stat->pmg_fsm_state_prev == PMG_FSM_CHARGING_NORMAL) ||
             (batt_stat->pmg_fsm_state_prev == PMG_FSM_CHARGING_TOPOFF)))
        {
            batt_stat->pmg_evt |= PMG_EVT_COOLDOWN;
            batt_stat->pmg_fsm_state = batt_stat->pmg_fsm_state_prev;
        }
        else
        {
            if (batt_stat->vbat_mon < cfg->battPackDisChrgVoltSnk)
            {
                Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_CHARGING_PRECHRG);
            }
            else
            {
                Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_CHARGING_NORMAL);
            }
        }
        cooldown = false;
    }
}


static void chg_fsm_charging_precharge(pmg_fsm_evt_t evt)
{
    cy_stc_battery_status_t* batt_stat = &(glBatChrgCtx.batteryStatus);
    const cy_stc_battery_charger_cfg_t* cfg = glBatChrgCtx.ptrBattCghCfg;

    if (evt == PMG_FSM_EVT_ENTRY)
    {
        batt_stat->chrg_timeout_counter = 0u;
        if (cfg->preChrgTmrEn)
        {
            (void)Cy_PdUtils_SwTimer_Start(glBatChrgCtx.ptrPdStack->ptrTimerContext,
                                           glBatChrgCtx.ptrPdStack,
                                           MINUTE_TIMER_ID,
                                           MINUTE_TIMER_PERIOD,
                                           &timer_minute_tick_cb);
        }
    }
    if (cfg->preChrgTmrEn)
    {
        if (batt_stat->chrg_timeout_counter >= cfg->preChrgTmrCnt)
        {
            batt_stat->faults.timeout_expired = 1u;
        }
    }
    if (batt_stat->vbat_mon > cfg->battPackDisChrgVoltSnk)
    {
        Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_CHARGING_NORMAL);
    }
    compute_pwm_duty_cycle(cfg->battChrgMinCurr);
}


/* Calculate reduced current from max current based on cycle number */
static uint16_t i_cycle(uint16_t chrg_i_max, uint8_t cycle_num)
{
    #if MULTIPLICATIVE_CURRENT_REDUCTION
    uint16_t chrg_i_cycle = chrg_i_max;
    uint8_t i = 0;
    while (i < cycle_num)
    {
        chrg_i_cycle *= CURRENT_REDUCTION_PER_CYCLE;
        chrg_i_cycle /= PERCENT_DIVISOR;
        i++;
    }
    return chrg_i_cycle;
    #else /* MULTIPLICATIVE_CURRENT_REDUCTION */
    uint16_t chrg_i_cycle = glBatChrgCtx.ptrBattCghCfg->battChrgMinCurr * cycle_num;
    return ((chrg_i_max > chrg_i_cycle) ? (chrg_i_max - chrg_i_cycle) : 0u);
    #endif /* MULTIPLICATIVE_CURRENT_REDUCTION */
}


static void chg_fsm_charging_normal(pmg_fsm_evt_t evt)
{
    cy_stc_battery_status_t* batt_stat = &(glBatChrgCtx.batteryStatus);
    const cy_stc_battery_charger_cfg_t* cfg = glBatChrgCtx.ptrBattCghCfg;
    uint16_t chrg_cur;

    if (evt == PMG_FSM_EVT_ENTRY)
    {
        batt_stat->chrg_timeout_counter = 0u;
        batt_stat->chrg_cycle_num = 0u;
        if (cfg->normalChrgTmrEn)
        {
            (void)Cy_PdUtils_SwTimer_Start(glBatChrgCtx.ptrPdStack->ptrTimerContext,
                                           glBatChrgCtx.ptrPdStack,
                                           MINUTE_TIMER_ID,
                                           MINUTE_TIMER_PERIOD,
                                           &timer_minute_tick_cb);
        }
    }
    if (cfg->normalChrgTmrEn)
    {
        if (batt_stat->chrg_timeout_counter >= cfg->normalChrgTmrCnt)
        {
            batt_stat->faults.timeout_expired = 1u;
        }
    }
    chrg_cur = i_cycle(batt_stat->chrg_i_max, batt_stat->chrg_cycle_num);
    if (batt_stat->vbat_mon > cfg->battPackMaxVolt)
    {
        /* need to wait some time to settle reduced current */
        static uint8_t skip_reduce_curr = PMG_CURRENT_REDUCTION_SETTLE_DELAY_MS / SOLN_BATT_MONITOR_TASK_TIMER_PERIOD;
        if (skip_reduce_curr != 0u)
        {
            skip_reduce_curr--;
        }
        else
        {
            skip_reduce_curr = PMG_CURRENT_REDUCTION_SETTLE_DELAY_MS / SOLN_BATT_MONITOR_TASK_TIMER_PERIOD;
            batt_stat->chrg_cycle_num++;
            chrg_cur = i_cycle(batt_stat->chrg_i_max, batt_stat->chrg_cycle_num);
            if (chrg_cur < cfg->battChrgMinCurr)
            {
                switch (cfg->postChargeAct)
                {
                    case (uint8_t)PMG_POST_CHARGE_ACTION_RECHARGE: /* recharge */
                        Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_CHARGING_FULL);
                        break;

                    case (uint8_t)PMG_POST_CHARGE_ACTION_SHUTDOWN: /* shutdown */
                        Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_SHUTDOWN);
                        break;

                    case (uint8_t)PMG_POST_CHARGE_ACTION_TOPOFF: /* top-off */
                        Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_CHARGING_TOPOFF);
                        break;

                    default:
                        /* Do nothing */
                        break;
                }
            }
        }
    }
    if (batt_stat->vbat_mon < cfg->battPackDisChrgVoltSnk)
    {
        chrg_cur = cfg->battChrgMinCurr;
        Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_CHARGING_PRECHRG);
    }
    compute_pwm_duty_cycle(chrg_cur);
}


static void chg_fsm_charging_topoff(pmg_fsm_evt_t evt)
{
    cy_stc_battery_status_t* batt_stat = &(glBatChrgCtx.batteryStatus);
    const cy_stc_battery_charger_cfg_t* cfg = glBatChrgCtx.ptrBattCghCfg;

    if (evt == PMG_FSM_EVT_ENTRY)
    {
        batt_stat->chrg_timeout_counter = 0u;
        if (cfg->topoffChrgTmrEn)
        {
            (void)Cy_PdUtils_SwTimer_Start(glBatChrgCtx.ptrPdStack->ptrTimerContext,
                                           glBatChrgCtx.ptrPdStack,
                                           MINUTE_TIMER_ID,
                                           MINUTE_TIMER_PERIOD,
                                           &timer_minute_tick_cb);
        }
    }
    if (cfg->topoffChrgTmrEn)
    {
        if (batt_stat->chrg_timeout_counter >= cfg->topoffChrgTmrCnt)
        {
            batt_stat->faults.timeout_expired = 1u;
        }
    }

    if (batt_stat->vbat_mon < cfg->battPackReChrgVolt)
    {
        Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_CHARGING_NORMAL);
    }
    compute_pwm_duty_cycle(cfg->battChrgMinCurr);
}


static void chg_fsm_charging_full(pmg_fsm_evt_t evt)
{
    const cy_stc_battery_status_t* batt_stat = &(glBatChrgCtx.batteryStatus);
    const cy_stc_battery_charger_cfg_t* cfg = glBatChrgCtx.ptrBattCghCfg;

    if (evt == PMG_FSM_EVT_ENTRY)
    {
        /* Disable BB and input FET. Output FET stays enabled to monitor battery voltage. */
        Cy_PmgChg_HW_Cmd(BATT_CHGE_BB_DIS);
        Cy_PmgChg_HW_Cmd(BATT_CHGE_BB_IN_DIS);
        Cy_PdUtils_SwTimer_Stop(glBatChrgCtx.ptrPdStack->ptrTimerContext, MINUTE_TIMER_ID);
    }
    if (batt_stat->vbat_mon < cfg->battPackReChrgVolt)
    {
        /* Disable Battery FET and init normal charging. */
        Cy_PmgChg_HW_Cmd(BATT_CHGE_BB_OUT_DIS);
        Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_INIT);
    }
}


static void chg_fsm_shutdown(pmg_fsm_evt_t evt)
{
    if (evt == PMG_FSM_EVT_ENTRY)
    {
        Cy_App_Pmg_Charging_Disable(glBatChrgCtx.ptrPdStack);
    }
}


static void chg_fsm_cooldown(pmg_fsm_evt_t evt)
{
    const cy_stc_battery_status_t* batt_stat = &(glBatChrgCtx.batteryStatus);

    if (evt == PMG_FSM_EVT_ENTRY)
    {
        /* Disable BB and input/output switches. */
        Cy_PmgChg_HW_Cmd(BATT_CHGE_BB_DIS);
        Cy_PmgChg_HW_Cmd(BATT_CHGE_BB_IN_DIS);
        Cy_PmgChg_HW_Cmd(BATT_CHGE_BB_OUT_DIS);
    }
    if ((batt_stat->faults.bjt_otp == 0u) && (batt_stat->faults.ntc1_otp == 0u))
    {
        cooldown = true;
        Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_INIT);
    }
}


static void chg_fsm_source_init(pmg_fsm_evt_t evt)
{
    if (evt == PMG_FSM_EVT_ENTRY)
    {
        Cy_PmgChg_HW_Cmd(BATT_CHGE_BB_OUT_EN);
    }
    if (evt == PMG_FSM_EVT_OUT_SS_CMLT)
    {
        Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_SOURCE);
    }
}


static void chg_fsm_source(pmg_fsm_evt_t evt)
{
    CY_UNUSED_PARAMETER(evt);
    #if (!(CY_PD_SINK_ONLY))
    const cy_stc_battery_status_t* batt_stat = &(glBatChrgCtx.batteryStatus);
    cy_stc_pdstack_context_t* ptrPdStackContext = glBatChrgCtx.ptrPdStack;
    const cy_stc_battery_charger_cfg_t* cfg = glBatChrgCtx.ptrBattCghCfg;

    /* Verify the battery is not discharged.
     * Check if LoadSwitch has not cut off VBUS at fault.
     * If so, disable VBUS to avoid LoadSwitch re-enable VBUS.
     */
    if ((batt_stat->vbat_mon < cfg->battPackDisChrgVoltSrc) ||
        (ptrPdStackContext->ptrAppCbk->vbus_is_present(ptrPdStackContext,
                                                       CY_PD_VSAFE_5V,
                                                       CY_PD_VSAFE_5V_SNK_TURN_ON_MARGIN) == false))
    {
        /* Battery empty. Just disable VBUS and go into shutdown state. */
        Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_SHUTDOWN);
        ptrPdStackContext->ptrAppCbk->psrc_disable(ptrPdStackContext, NULL);
    }
    #endif /* !(CY_PD_SINK_ONLY) */
}


void Cy_App_Pmg_Init(cy_stc_pdstack_context_t* ptrPdStackContext,
                     const cy_stc_battery_charger_cfg_t* ptrBatChgCfg)
{
    glBatChrgCtx.ptrPdStack = ptrPdStackContext;
    glBatChrgCtx.ptrBattCghCfg = ptrBatChgCfg;

    Cy_PmgChg_HW_Init();
}


static void faults_handler(void)
{
    cy_stc_battery_status_t* batt_stat = &(glBatChrgCtx.batteryStatus);

    if ((batt_stat->faults.timeout_expired != 0u) ||
        (batt_stat->faults.batt_ocp != 0u) ||
        (batt_stat->faults.batt_ovp != 0u) ||
        (batt_stat->faults.batt_uvp != 0u) ||
        (batt_stat->faults.bb_hw_ocp != 0u) ||
        (batt_stat->faults.bb_hw_ovp != 0u))
    {
        if (batt_stat->pmg_fsm_state != PMG_FSM_OFF)
        {
            #if (!(CY_PD_SINK_ONLY))
            /* if fault happens in SOURCE mode disable VBUS and go into shutdown state. */
            if (batt_stat->pmg_fsm_state == PMG_FSM_SOURCE)
            {
                cy_stc_pdstack_context_t* ptrPdStackContext = glBatChrgCtx.ptrPdStack;
                ptrPdStackContext->ptrAppCbk->psrc_disable(ptrPdStackContext, NULL);
            }
            #endif /* !(CY_PD_SINK_ONLY) */
            Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_SHUTDOWN);
        }
    }
    else if ((batt_stat->faults.bjt_otp != 0u) ||
             (batt_stat->faults.ntc1_otp != 0u))
    {
        if ((batt_stat->pmg_fsm_state == PMG_FSM_CHARGING_PRECHRG) ||
            (batt_stat->pmg_fsm_state == PMG_FSM_CHARGING_NORMAL) ||
            (batt_stat->pmg_fsm_state == PMG_FSM_CHARGING_TOPOFF))
        {
            /* If the device overheats in charging state, go to cool-down state. */
            batt_stat->pmg_fsm_state_prev = batt_stat->pmg_fsm_state;
            Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_COOLDOWN);
        }
    }
    else
    {
        /* Do nothing, no faults detected. */
    }
}


static void charging_process(void)
{
    static void(*const pmg_fsm_table[PMG_FSM_MAX_STATES]) (pmg_fsm_evt_t evt) =
    {
        &chg_fsm_off,                               /*  0: PMG_FSM_OFF */
        &chg_fsm_init,                              /*  1: PMG_FSM_INIT */
        &chg_fsm_init_fet_in,                       /*  2: PMG_FSM_INIT_FET_IN */
        &chg_fsm_init_fet_out,                      /*  3: PMG_FSM_INIT_FET_OUT */
        &chg_fsm_init_bb,                           /*  4: PMG_FSM_INIT_BB */
        &chg_fsm_charging_precharge,                /*  5: PMG_FSM_CHARGING_PRECHRG */
        &chg_fsm_charging_normal,                   /*  6: PMG_FSM_CHARGING_NORMAL */
        &chg_fsm_charging_full,                     /*  7: PMG_FSM_CHARGING_FULL */
        &chg_fsm_charging_topoff,                   /*  8: PMG_FSM_CHARGING_TOPOFF */
        &chg_fsm_shutdown,                          /*  9: PMG_FSM_SHUTDOWN */
        &chg_fsm_cooldown,                          /* 10: PMG_FSM_COOLDOWN */
        &chg_fsm_source_init,                       /* 11: PMG_FSM_SOURCE_INIT */
        &chg_fsm_source                             /* 12: PMG_FSM_SOURCE */
    };

    cy_stc_battery_status_t* batt_stat = &(glBatChrgCtx.batteryStatus);
    cy_stc_battery_measure_t* adcResult = &(glBatChrgCtx.batteryMeasure);
    uint8_t evt;

    batt_stat->vbat_mon = (uint16_t)(adcResult->vbat_sum / adcResult->index);
    batt_stat->chrg_i_out_mon = (uint16_t)(adcResult->i_out_mon_sum / adcResult->index);
    batt_stat->vbus_i_in_mon = (uint16_t)(adcResult->i_in_mon_sum / adcResult->index);

    float32_t temperature = adcResult->ntc1_sum;
    temperature /= adcResult->index;
    temperature /= NTC1_R;
    temperature = logf(temperature);
    temperature = NTC1_B / temperature;
    temperature -= PMG_KELVIN_TO_CELSIUS_OFFSET;
    batt_stat->ntc1_temp = (int16_t)temperature;

    batt_stat->bjt_code = (uint8_t)(adcResult->bjt_code_sum / adcResult->index);
    batt_stat->vddd = (uint16_t)(adcResult->vddd_sum / adcResult->index);

    (void)memset(adcResult, 0, sizeof(cy_stc_battery_measure_t));

    soln_battery_charger_task();

    faults_handler();

    #if defined(DEBUG_PRINT_ENABLE) && (DEBUG_PRINT_ENABLE)
    #if defined(DEBUG_PRINT_TABLE_VIEW) && (DEBUG_PRINT_TABLE_VIEW)
    {
        char_t temp[PMG_DEBUG_PRINT_BUFFER_SIZE];
        uint32_t fault_word;
        (void)memcpy(&fault_word, &batt_stat->faults, sizeof(fault_word));

        #define PRINT_VAR(format, var) \
                (void)sprintf(temp, format, var); \
                Cy_SCB_UART_PutString(CYBSP_UART_HW, temp); \

        PRINT_VAR("FSM %d ", batt_stat->pmg_fsm_state);
        PRINT_VAR("EVT 0x%lx ", batt_stat->pmg_evt);
        PRINT_VAR("FLT 0x%lx ", fault_word);
        PRINT_VAR("NTC %dC ", batt_stat->ntc1_temp);
        PRINT_VAR("BJT %d ", batt_stat->bjt_code);
        PRINT_VAR("VBAT %d ", batt_stat->vbat_mon);
        PRINT_VAR("IBAT %d ", batt_stat->chrg_i_out_mon * CY_PD_CUR_PER_UNIT);
        PRINT_VAR("PBAT %lu ", (uint32_t)(batt_stat->vbat_mon * batt_stat->chrg_i_out_mon / 100u));
        PRINT_VAR("ISET %d ", batt_stat->chrg_i_set * CY_PD_CUR_PER_UNIT);
        #if 0
        PRINT_VAR("PWM %d ", batt_stat->pwm_compare);
        #endif /* 0 */
        PRINT_VAR("IBUS %d ", batt_stat->vbus_i_in_mon * CY_PD_CUR_PER_UNIT);
        PRINT_VAR("TIME %d ", batt_stat->chrg_timeout_counter);
        PRINT_VAR("NTCST %d ", batt_stat->ntc1_state);
        PRINT_VAR("BJTST %d ", batt_stat->bjt_state);
        PRINT_VAR("EF1 %x ", batt_stat->ef1);
        PRINT_VAR("EF2 %x ", batt_stat->ef2);
        PRINT_VAR("VDDD %d ", batt_stat->vddd);
        Cy_SCB_UART_PutString(CYBSP_UART_HW, "\r\n");
    }
    #else /* defined(DEBUG_PRINT_TABLE_VIEW) && (DEBUG_PRINT_TABLE_VIEW) */
    {
        char_t temp[PMG_DEBUG_PRINT_BUFFER_SIZE];
        uint32_t fault_word;
        (void)memcpy(&fault_word, &batt_stat->faults, sizeof(fault_word));
        (void)sprintf(temp, "\r\n FSM: %d EVT: 0x%lx FAULT: 0x%lx\n",
                      batt_stat->pmg_fsm_state, batt_stat->pmg_evt, fault_word);
        Cy_SCB_UART_PutString(CYBSP_UART_HW, temp);
    }
    #endif /* defined(DEBUG_PRINT_TABLE_VIEW) && (DEBUG_PRINT_TABLE_VIEW) */
    #endif /* defined(DEBUG_PRINT_ENABLE) && (DEBUG_PRINT_ENABLE) */

    /* Gets the next event to be processed. */
    evt = Cy_PdUtils_EventGroup_GetEvent(&(batt_stat->pmg_evt), true);

    #if defined(CY_APP_LED_CONTROL_ENABLE) && (CY_APP_LED_CONTROL_ENABLE)
    /* LED mode according to Charging FSM state and EVT */
    pmg_led_mngr();
    #endif /* defined(CY_APP_LED_CONTROL_ENABLE) && (CY_APP_LED_CONTROL_ENABLE) */

    /* Calls the FSM handler function. */
    pmg_fsm_table[batt_stat->pmg_fsm_state](evt);
}


void Cy_App_Pmg_Task(const cy_stc_pdstack_context_t* ptrPdStackContext)
{
    cy_stc_battery_measure_t* adcResult = &(glBatChrgCtx.batteryMeasure);

    if (Cy_PdUtils_SwTimer_IsRunning(ptrPdStackContext->ptrTimerContext,
                                     SOLN_BATT_MONITOR_TASK_TIMER_ID))
    {
        if (Cy_PdUtils_SwTimer_IsRunning(ptrPdStackContext->ptrTimerContext,
                                         ADC_PERIOD_TIMER_ID) == false)
        {
            (void)Cy_PdUtils_SwTimer_StartWocb(ptrPdStackContext->ptrTimerContext,
                                               ADC_PERIOD_TIMER_ID,
                                               ADC_PERIOD_TIMER_PERIOD);
            battery_measure_sar_adc_read();
            adcResult->index++;
        }
    }
    else
    {
        (void)Cy_PdUtils_SwTimer_StartWocb(ptrPdStackContext->ptrTimerContext,
                                           SOLN_BATT_MONITOR_TASK_TIMER_ID,
                                           SOLN_BATT_MONITOR_TASK_TIMER_PERIOD);
        if (adcResult->index != 0u)
        {
            charging_process();
        }
    }
}


void Cy_App_Pmg_Charging_Disable(const cy_stc_pdstack_context_t* ptrPdStackContext)
{
    /* Input FET status has to be saved before BATT_CHGE_BB_IN_DIS, where it is cleared */
    bool fet_in_en = glBatChrgCtx.batteryStatus.bb_in_fet_status;

    /* Disable input and output FETs */
    Cy_PmgChg_HW_Cmd(BATT_CHGE_BB_OUT_DIS);
    Cy_PmgChg_HW_Cmd(BATT_CHGE_BB_IN_DIS);

    /* Input capacitors were charged only when the device was acting as a sink and the input FET was enabled */
    if ((ptrPdStackContext->dpmConfig.curPortRole == CY_PD_PRT_ROLE_SINK) && (fet_in_en))
    {
        /* Disable Buck-Boost has to be here to avoid multiple disabling discharge */
        Cy_PmgChg_HW_Cmd(BATT_CHGE_BB_DIS);

        /* Discharge C_PWR using BB in Sink role. Give 5ms PFET turn off time, PWMI on, discharge 10ms, PWMI off */
        Cy_PmgChg_HW_Cmd(BATT_CHGE_BB_DSCHRG);
    }

    Cy_PdUtils_SwTimer_Stop(ptrPdStackContext->ptrTimerContext, SOLN_BATT_PWM_INIT_TIMER_ID);
    Cy_PdUtils_SwTimer_Stop(ptrPdStackContext->ptrTimerContext, MINUTE_TIMER_ID);

    Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_OFF);

    #if defined(CY_APP_LED_CONTROL_ENABLE) && (CY_APP_LED_CONTROL_ENABLE)
    /* LED mode according to Charging FSM state and EVT */
    pmg_led_mngr();
    #endif /* defined(CY_APP_LED_CONTROL_ENABLE) && (CY_APP_LED_CONTROL_ENABLE) */
}


static void timer_minute_tick_cb(cy_timer_id_t id, void* callbackCtx)
{
    cy_stc_pdstack_context_t* ptrPdStackContext = (cy_stc_pdstack_context_t*)callbackCtx;

    glBatChrgCtx.batteryStatus.chrg_timeout_counter++;
    (void)Cy_PdUtils_SwTimer_Start(ptrPdStackContext->ptrTimerContext,
                                   ptrPdStackContext,
                                   id,
                                   MINUTE_TIMER_PERIOD,
                                   &timer_minute_tick_cb);
    DEBUG_PRINT_VAR("\r\n Time: %i min\n", glBatChrgCtx.batteryStatus.chrg_timeout_counter);
}


/*
 * Update the Buck boost output voltage and current based on the USB-C Input power and current
 * battery voltage
 */
static void calc_buck_boost_out_pwr_settings(const cy_stc_pdstack_context_t* ptrPdStackContext)
{
    cy_stc_battery_status_t* batt_stat = &(glBatChrgCtx.batteryStatus);
    const cy_stc_battery_charger_cfg_t* cfg = glBatChrgCtx.ptrBattCghCfg;
    uint32_t power = (uint32_t)ptrPdStackContext->dpmStat.contract.minVolt *
                     ptrPdStackContext->dpmStat.contract.curPwr;

    if ((power / PERCENT_DIVISOR) >= cfg->vbusMinPwr)
    {
        DEBUG_PRINT("\r\n Input Power sufficient");
        batt_stat->is_vbus_pwr_sufficient = true;
    }
    else
    {
        DEBUG_PRINT("\r\n Input Power NOT sufficient");
        batt_stat->is_vbus_pwr_sufficient = false;
    }
    power = (power * cfg->buckBoostEffcy) / PERCENT_DIVISOR;

    /* Source 5% voltage tolerance */
    power = (power * cfg->vbusTolMultiplier) / PERCENT_DIVISOR;

    /* Manufacture efficiency */
    power = (power * cfg->manuEffcyMultiplier) / PERCENT_DIVISOR;

    /* Current limited by contract power */
    batt_stat->chrg_i_max = (uint16_t)(power / cfg->battPackMaxVolt);

    batt_stat->chrg_bb_pwr = power / PERCENT_DIVISOR;

    DEBUG_PRINT("\r\n BAT VA UPDATE ");
    DEBUG_PRINT_VAR("\r\n Contract volt:: %d mV\n", ptrPdStackContext->dpmStat.contract.minVolt);
    DEBUG_PRINT_VAR("\r\n Contract Curr:: %d mA\n",
                    ptrPdStackContext->dpmStat.contract.curPwr * CY_PD_CUR_PER_UNIT);
    DEBUG_PRINT_VAR("\r\n BB MAX Output Pwr:: %lu mW\n", (long unsigned int)batt_stat->chrg_bb_pwr);
}


/* Battery Charger Solution PD event handler */
void Cy_App_Pmg_PdEventHandler(cy_stc_pdstack_context_t* ptrPdStackContext,
                               cy_en_pdstack_app_evt_t evt, const void* data)
{
    const cy_stc_pdstack_pd_contract_info_t* contract_status;
    cy_stc_battery_status_t* batt_stat = &(glBatChrgCtx.batteryStatus);

    #if 0
    DEBUG_PRINT_VAR(" Charger event is: %#x \r\n", (evt));
    #endif /* 0 */
    switch (evt)
    {
        case APP_EVT_TYPEC_STARTED:
            DEBUG_PRINT("\r\n TypeC Start");
            break;

        case APP_EVT_CONNECT:
            cooldown = false;
            (void)memset(&batt_stat->faults, 0, sizeof(batt_stat->faults));
            batt_stat->ntc1_state = STATE_TEMP_NORMAL_CHARGE;
            batt_stat->bjt_state = STATE_TEMP_NORMAL_CHARGE;
            Cy_PmgChg_HW_Cmd(BATT_CHGE_BB_RST);
            DEBUG_PRINT("\r\n Connect ");
            if (ptrPdStackContext->dpmConfig.curPortRole == CY_PD_PRT_ROLE_SINK)
            {
                DEBUG_PRINT("\r\n >>SNK ");
            }
            else
            {
                DEBUG_PRINT("\r\n >>SRC ");
                Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_SOURCE_INIT);
            }
            break;

        case APP_EVT_PE_DISABLED:
        /* Intentional fall-through. */
        case APP_EVT_HARD_RESET_SENT:                       /* Intentional fall through */
        case APP_EVT_HARD_RESET_RCVD:                       /* Intentional fall through */
        case APP_EVT_VBUS_PORT_DISABLE:                     /* Intentional fall through */
        case APP_EVT_DISCONNECT:                            /* Intentional fall through */
        case APP_EVT_TYPE_C_ERROR_RECOVERY:                 /* Intentional fall through */
        case APP_EVT_VBUS_UVP_FAULT:
        case APP_EVT_VBUS_OVP_FAULT:
        case APP_EVT_PR_SWAP_ACCEPTED:
        case APP_EVT_STANDBY_CURRENT:

            if (((evt == APP_EVT_HARD_RESET_SENT) || (evt == APP_EVT_HARD_RESET_RCVD)) &&
                (ptrPdStackContext->dpmConfig.curPortRole == CY_PD_PRT_ROLE_SINK) &&
                (ptrPdStackContext->dpmStat.deadBat))
            {
                /* In dead-battery mode, disable the sink port to disable VBUS OVP.
                 * After a hard reset, VBUS drops below 5 V while the ADC uses VDDD as its
                 * reference. The resulting ADC measurements may be inaccurate and trigger
                 * a false OVP event. */
                ptrPdStackContext->ptrAppCbk->psnk_disable(ptrPdStackContext, NULL);
            }

            DEBUG_PRINT_VAR("\r\n Charger event is: %#x \n", (evt));
            batt_stat->chrg_i_set = 0u;
            (void)memset(&batt_stat->faults, 0, sizeof(batt_stat->faults));
            cooldown = false;
            batt_stat->ntc1_state = STATE_TEMP_NORMAL_CHARGE;
            batt_stat->bjt_state = STATE_TEMP_NORMAL_CHARGE;
            Cy_App_Pmg_Charging_Disable(ptrPdStackContext);
            break;

        case APP_EVT_PD_CONTRACT_NEGOTIATION_COMPLETE:
            /* coverity[misra_c_2012_rule_11_5_violation : SUPPRESS] The PD event callback
             * delivers the contract info payload as a const void pointer; the documented
             * type for this event is cy_stc_pdstack_pd_contract_info_t. */
            contract_status = (const cy_stc_pdstack_pd_contract_info_t*)data;
            if ((contract_status->status == CY_PDSTACK_CONTRACT_NEGOTIATION_SUCCESSFUL) ||
                (contract_status->status == CY_PDSTACK_CONTRACT_CAP_MISMATCH_DETECTED))
            {
                /* Check PD power is sufficient */
                calc_buck_boost_out_pwr_settings(ptrPdStackContext);

                if (ptrPdStackContext->dpmConfig.curPortRole == CY_PD_PRT_ROLE_SINK)
                {
                    if (batt_stat->is_vbus_pwr_sufficient == true)
                    {
                        if (batt_stat->pmg_fsm_state == PMG_FSM_OFF)
                        {
                            Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_INIT);
                        }
                    }
                    else
                    {
                        Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_SHUTDOWN);
                    }
                }
                else
                {
                    Cy_App_Pmg_Next_Fsm_Entry(PMG_FSM_SOURCE_INIT);
                }
            }
            break;

        default:
            /* Do nothing */
            break;
    }
}


void Cy_App_Pmg_FsmSetEvt(uint32_t evt_mask)
{
    glBatChrgCtx.batteryStatus.pmg_evt |= evt_mask;
}


void Cy_App_Pmg_FsmClearEvt(uint32_t evt_mask)
{
    glBatChrgCtx.batteryStatus.pmg_evt &= ~evt_mask;
}


/* Creates an entry event into next FSM state. */
void Cy_App_Pmg_Next_Fsm_Entry(pmg_state_t next_state)
{
    glBatChrgCtx.batteryStatus.pmg_evt |= PMG_EVT_ENTRY;
    glBatChrgCtx.batteryStatus.pmg_fsm_state = next_state;
}


/* [] END OF FILE */
