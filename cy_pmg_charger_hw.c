/***************************************************************************//**
* \file cy_pmg_charger_hw.c
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

#include "cy_tcpwm_pwm.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#if defined(DEBUG_PRINT_ENABLE) && (DEBUG_PRINT_ENABLE)
#include <stdio.h>
#endif /* defined(DEBUG_PRINT_ENABLE) && (DEBUG_PRINT_ENABLE) */
#include "cy_gpio.h"
#include "cy_pmg_charger.h"
#include "cy_pmg_charger_hw.h"
#include "cybsp.h"

#define PWM_VSET_PORT GPIO_PRT1
#define PWM_VSET_PIN 0U

#define IINMON_IN_PORT GPIO_PRT2
#define IINMON_IN_PIN 0U

#define VBUS_MON_PORT GPIO_PRT2
#define VBUS_MON_PIN 1U

#define EF1_IN_PORT GPIO_PRT2
#define EF1_IN_PIN 4U

#define EF2_IN_PORT GPIO_PRT2
#define EF2_IN_PIN 5U

#define PWMI_OUT_PORT GPIO_PRT2
#define PWMI_OUT_PIN 6U

#define VBAT_MON_PORT GPIO_PRT4
#define VBAT_MON_PIN 0U

#define IOUTMON_IN_PORT GPIO_PRT4
#define IOUTMON_IN_PIN 3U

#define ZERO_CURRENT_OFFSET_MV               (200u)
#define IOUT_SENSE_GAIN                      (8u)
#define IIN_SENSE_GAIN                       (20u)
#define MA_PER_CURRENT_STEP                  (1000u / CY_PD_CUR_PER_UNIT)
#define LIMITED_CHARGE_DIVISOR               (2u)

#define AMUX_SETTLE_DELAY_US                 (20u)
#define GPIO_RESTORE_DELAY_US                (10u)
#define BB_RESET_ASSERT_DELAY_US             (100u)
#define BB_RESET_RECOVERY_DELAY_US           (150u)
#define EF_DEBOUNCE_DELAY_US                 (100u)

#if defined(CY_APP_LED_CONTROL_ENABLE) && (CY_APP_LED_CONTROL_ENABLE)
#include "cy_app_led_ctrl.h"
static cy_stc_led_ctrl_context_t led_context;

void pmg_led_mngr(void)
{
    cy_stc_battery_status_t* batt_stat = &(get_battery_charging_context()->batteryStatus);

    if ((batt_stat->faults.timeout_expired != 0u) ||
        (batt_stat->faults.batt_ocp != 0u) ||
        (batt_stat->faults.batt_ovp != 0u) ||
        (batt_stat->faults.batt_uvp != 0u) ||
        (batt_stat->faults.bb_hw_ocp != 0u) ||
        (batt_stat->faults.bb_hw_ovp != 0u))
    {
        /* Latch the fault indication. The individual fault flags can be
         * re-evaluated and cleared by the monitor on subsequent cycles (e.g.
         * read_bb_error_flag() clears the BB flags once the error-flag pins
         * recover after BB shutdown). The latch keeps the LED solid ON until
         * the fault state is reset at the next APP_EVT_CONNECT/disconnect. */
        batt_stat->faults.fault_active = true;
    }

    if (batt_stat->faults.fault_active)
    {
        (void)Cy_App_LedCtrl_SwitchMode(CY_APP_LED_CTRL_LED_ON, &led_context);
    }
    else
    {
        switch (batt_stat->pmg_fsm_state)
        {
            case PMG_FSM_OFF:
            case PMG_FSM_CHARGING_FULL:
            case PMG_FSM_SHUTDOWN:
                (void)Cy_App_LedCtrl_SwitchMode(CY_APP_LED_CTRL_LED_OFF, &led_context);
                break;

            case PMG_FSM_CHARGING_PRECHRG:
            case PMG_FSM_CHARGING_NORMAL:
            case PMG_FSM_CHARGING_TOPOFF:
                (void)Cy_App_LedCtrl_SwitchMode(CY_APP_LED_CTRL_LED_BLINKING, &led_context);

                /* LED API uses 1000ms as a default blink_rate. Need to override. */
                led_context.blink_rate = LED_PD_SNK_TIMER_PERIOD;
                break;

            case PMG_FSM_COOLDOWN:
                (void)Cy_App_LedCtrl_SwitchMode(CY_APP_LED_CTRL_LED_BREATHING, &led_context);
                break;

            case PMG_FSM_SOURCE:
                /* In Source mode the LED is blinking with a different period. */
                (void)Cy_App_LedCtrl_SwitchMode(CY_APP_LED_CTRL_LED_BLINKING, &led_context);

                /* LED API uses 1000ms as a default blink_rate. Need to override. */
                led_context.blink_rate = LED_PD_SRC_TIMER_PERIOD;
                break;

            default:
                /* Do nothing */
                break;
        }
    }
}


#endif /* defined(CY_APP_LED_CONTROL_ENABLE) && (CY_APP_LED_CONTROL_ENABLE) */


static void Init_PWM_Output(uint32_t pwm_period, uint32_t pwm_compare)
{
    Cy_GPIO_Pin_FastInit(PWM_VSET_PORT, PWM_VSET_PIN, CY_GPIO_DM_STRONG_IN_OFF, 0UL,
                         P1_0_TCPWM_LINE0);

    /* Initialize PWM using the configuration structure generated using device configurator */
    if (Cy_TCPWM_PWM_Init(CYBSP_PWM_HW, CYBSP_PWM_NUM, &CYBSP_PWM_config) != CY_TCPWM_SUCCESS)
    {
        DEBUG_PRINT("\r\n API Cy_TCPWM_Init failed \n");
        CY_ASSERT(0);
    }
    /*
     * Set PWM Period count value
     *
     * Calling this API function overwrites the default period value stored in the TCPWM
     * configuration structure
     */
    Cy_TCPWM_PWM_SetPeriod0(CYBSP_PWM_HW, CYBSP_PWM_NUM, pwm_period);

    /*
     * Set PWM Compare value
     *
     * The Compare value is used to adjust the PWM duty cycle
     * Calling this API function overwrites the default compare value stored in the TCPWM
     * configuration structure
     */
    Cy_TCPWM_PWM_SetCompare0(CYBSP_PWM_HW, CYBSP_PWM_NUM, pwm_compare);

    /* Enable the TCPWM as PWM */
    Cy_TCPWM_PWM_Enable(CYBSP_PWM_HW, CYBSP_PWM_NUM);

    /* Start the PWM */
    Cy_TCPWM_TriggerReloadOrIndex(CYBSP_PWM_HW, CYBSP_PWM_MASK);
}


void Cy_PmgChg_HW_Init(void)
{
    /*
     * This is a temporary solution. In future has to be generated by Device configurator
     */
    HSIOM->AMUX_SPLIT_CTL[0] = HSIOM_AMUX_SPLIT_CTL_SWITCH_BB_SL_Msk |
                               HSIOM_AMUX_SPLIT_CTL_SWITCH_BB_SR_Msk;
    HSIOM->AMUX_SPLIT_CTL[1] = HSIOM_AMUX_SPLIT_CTL_SWITCH_AA_SL_Msk |
                               HSIOM_AMUX_SPLIT_CTL_SWITCH_AA_SR_Msk;

    #if defined(VBUS_EXTERNAL_DISCHARGE) && (VBUS_EXTERNAL_DISCHARGE)
    /* VBUS_Discharge pin configuration */
    Cy_USBPD_Vbus_ExtDischargeInit();
    #endif /* defined(VBUS_EXTERNAL_DISCHARGE) && (VBUS_EXTERNAL_DISCHARGE) */

    /*
     * To measure VBUS_MON the PDL mostly uses ID_0 and AMUXA.
     * VBAT_MON, IIN_MON, IOUT_MON shell use ID_1 and AMUXB.
     */
    Cy_GPIO_Pin_FastInit(IINMON_IN_PORT, IINMON_IN_PIN, CY_GPIO_DM_ANALOG, 1u, HSIOM_SEL_GPIO);
    Cy_GPIO_Pin_FastInit(VBUS_MON_PORT, VBUS_MON_PIN, CY_GPIO_DM_ANALOG, 1u, HSIOM_SEL_AMUXA);
    Cy_GPIO_Pin_FastInit(EF1_IN_PORT, EF1_IN_PIN, CY_GPIO_DM_HIGHZ, 1u, HSIOM_SEL_GPIO);
    Cy_GPIO_Pin_FastInit(EF2_IN_PORT, EF2_IN_PIN, CY_GPIO_DM_HIGHZ, 1u, HSIOM_SEL_GPIO);
    Cy_GPIO_Pin_FastInit(PWMI_OUT_PORT, PWMI_OUT_PIN, CY_GPIO_DM_STRONG_IN_OFF, 0u, HSIOM_SEL_GPIO);
    Cy_GPIO_Pin_FastInit(VBAT_MON_PORT, VBAT_MON_PIN, CY_GPIO_DM_ANALOG, 1u, HSIOM_SEL_GPIO);
    Cy_GPIO_Pin_FastInit(IOUTMON_IN_PORT, IOUTMON_IN_PIN, CY_GPIO_DM_ANALOG, 1u, HSIOM_SEL_GPIO);

    #if defined(CY_APP_LED_CONTROL_ENABLE) && (CY_APP_LED_CONTROL_ENABLE)
    led_context.port = UART_RX_PORT;
    led_context.pinNum = UART_RX_PIN;
    led_context.ledOrientation = false;
    led_context.timerId = LED_TIMER_ID;
    led_context.timerContext = get_battery_charging_context()->ptrPdStack->ptrTimerContext;
    (void)Cy_App_LedCtrl_Init(&led_context);
    #endif /* defined(CY_APP_LED_CONTROL_ENABLE) && (CY_APP_LED_CONTROL_ENABLE) */
}


static const cy_stc_sysint_t ef_intr_cfg =
{
    .intrSrc      = ioss_interrupts_gpio_2_IRQn,
    .intrPriority = 0U,
};


static void ef_handler(void)
{
    cy_stc_battery_charging_context_t*  ptrBatteryContext = get_battery_charging_context();
    cy_stc_battery_status_t* batt_stat = &(ptrBatteryContext->batteryStatus);
    cy_stc_pdstack_context_t* ptrPdStackContext = ptrBatteryContext->ptrPdStack;
    uint8_t ef1 = 0u;
    uint8_t ef2 = 0u;

    /* clear gpio interrupt */
    NVIC_ClearPendingIRQ(ef_intr_cfg.intrSrc);
    Cy_GPIO_ClearInterrupt(EF1_IN_PORT, EF1_IN_PIN);
    Cy_GPIO_ClearInterrupt(EF2_IN_PORT, EF2_IN_PIN);

    ef1 = (uint8_t)Cy_GPIO_Read(EF1_IN_PORT, EF1_IN_PIN);
    ef2 = (uint8_t)Cy_GPIO_Read(EF2_IN_PORT, EF2_IN_PIN);

    if (ef1 == 0u)
    {
        if (ef2 != 0u)
        {
            batt_stat->faults.bb_hw_ocp = 1u;
            NVIC_DisableIRQ(ef_intr_cfg.intrSrc);
            Cy_App_Pmg_Charging_Disable(ptrPdStackContext);
        }
        else
        {
            /* Just set a flag. Do not need to disable charging. The BB takes care about OTP. */
            batt_stat->faults.bb_hw_otp = 1u;
        }
    }
    else
    {
        if (ef2 == 0u)
        {
            batt_stat->faults.bb_hw_ovp = 1u;
            NVIC_DisableIRQ(ef_intr_cfg.intrSrc);
            Cy_App_Pmg_Charging_Disable(ptrPdStackContext);
        }
        else
        {
            /* Do nothing. Interrupts already cleared. */
        }
    }
    DEBUG_PRINT("\r\n EF interrupt \r\n");
}


static void pwm_init_timer_cb(cy_timer_id_t id,            /**< Timer ID for which callback is being generated. */
                              void* callbackContext)       /**< Timer module Context. */
{
    CY_UNUSED_PARAMETER(id);               /* Unused parameter */
    CY_UNUSED_PARAMETER(callbackContext);  /* Unused parameter */
    Cy_App_Pmg_FsmSetEvt(PMG_EVT_PWM_INIT_CMPT);
}


static uint16_t get_adc_result_mv(const GPIO_PRT_Type* port, const uint32_t pin)
{
    cy_stc_usbpd_context_t* ptrUsbPdContext = get_battery_charging_context()->ptrPdStack->ptrUsbPdContext;
    uint32_t volt;

    Cy_GPIO_SetHSIOM(port, pin, HSIOM_SEL_AMUXB);
    Cy_SysLib_DelayUs(AMUX_SETTLE_DELAY_US);

    /* Take ADC sample. */
    volt = Cy_USBPD_Adc_Sample(ptrUsbPdContext, CY_USBPD_ADC_ID_1, CY_USBPD_ADC_INPUT_AMUX_B);
    volt *= ptrUsbPdContext->adcVdddMv[CY_USBPD_ADC_ID_1];
    volt /= PD_ADC_NUM_LEVELS;

    Cy_GPIO_SetHSIOM(port, pin, HSIOM_SEL_GPIO);
    Cy_SysLib_DelayUs(GPIO_RESTORE_DELAY_US);

    return (uint16_t)(volt);
}


static void rect_timer_cb(cy_timer_id_t id,                /**< Timer ID for which callback is being generated. */
                          void* callbackContext)           /**< Timer module Context. */
{
    CY_UNUSED_PARAMETER(id);               /* Unused parameter */
    CY_UNUSED_PARAMETER(callbackContext);  /* Unused parameter */

    Cy_GPIO_Write(RECT_PORT, RECT_PIN, 1u);
    Cy_App_Pmg_FsmSetEvt(PMG_EVT_BB_INIT_CMPT);
}


static void pwmi_timer_cb(cy_timer_id_t id,                /**< Timer ID for which callback is being generated. */
                          void* callbackContext)           /**< Timer module Context. */
{
    CY_UNUSED_PARAMETER(id);               /* Unused parameter */
    CY_UNUSED_PARAMETER(callbackContext);  /* Unused parameter */

    Cy_GPIO_Write(PWMI_OUT_PORT, PWMI_OUT_PIN, 1u);
    DEBUG_PRINT("\r\n BB PWMI Switch ON ..\r\n");
}


static void fet_in_timer_cb(cy_timer_id_t id,              /**< Timer ID for which callback is being generated. */
                            void* callbackContext)         /**< Timer module Context. */
{
    CY_UNUSED_PARAMETER(id);               /* Unused parameter */
    CY_UNUSED_PARAMETER(callbackContext);  /* Unused parameter */
    Cy_App_Pmg_FsmSetEvt(PMG_EVT_IN_SS_CMPT);
}


static void fet_out_timer_cb(cy_timer_id_t id,             /**< Timer ID for which callback is being generated. */
                             void* callbackContext)        /**< Timer module Context. */
{
    CY_UNUSED_PARAMETER(id);               /* Unused parameter */
    CY_UNUSED_PARAMETER(callbackContext);  /* Unused parameter */
    Cy_App_Pmg_FsmSetEvt(PMG_EVT_OUT_SS_CMPT);
}


static void bb_discharge_timer_cb(cy_timer_id_t id,        /**< Timer ID for which callback is being generated. */
                                  void* callbackContext)   /**< Timer module Context. */
{
    CY_UNUSED_PARAMETER(id);               /* Unused parameter */
    CY_UNUSED_PARAMETER(callbackContext);  /* Unused parameter */
    Cy_GPIO_Write(PWMI_OUT_PORT, PWMI_OUT_PIN, 0u);
    Cy_PmgChg_HW_Cmd(BATT_CHGE_BB_RST);
}


static void pfet_turn_off_timer_cb(cy_timer_id_t id,       /**< Timer ID for which callback is being generated. */
                                   void* callbackContext)  /**< Timer module Context. */
{
    CY_UNUSED_PARAMETER(id);               /* Unused parameter */
    cy_stc_pdstack_context_t* ptrPdStackContext = (cy_stc_pdstack_context_t*)callbackContext;
    Cy_GPIO_Write(PWMI_OUT_PORT, PWMI_OUT_PIN, 1u);
    (void)Cy_PdUtils_SwTimer_Start(ptrPdStackContext->ptrTimerContext,
                                   (void*)ptrPdStackContext,
                                   (cy_timer_id_t)BB_DSCHRG_TIMER_ID,
                                   BB_DSCHRG_TIMER_PERIOD,
                                   &bb_discharge_timer_cb);
}


void Cy_PmgChg_HW_Cmd(const batt_chge_src_cmd_t src_cmd)
{
    cy_stc_battery_charging_context_t* ptrBatteryChargingContext = get_battery_charging_context();
    cy_stc_battery_status_t* batt_stat = &(ptrBatteryChargingContext->batteryStatus);
    cy_stc_pdstack_context_t* ptrPdStackContext = ptrBatteryChargingContext->ptrPdStack;
    const cy_stc_battery_charger_cfg_t* cfg = ptrBatteryChargingContext->ptrBattCghCfg;
    uint32_t pwm_compare = 0u;

    switch (src_cmd)
    {
        case BATT_CHGE_BB_DIS:
            Cy_PdUtils_SwTimer_Stop(ptrPdStackContext->ptrTimerContext,
                                    (cy_timer_id_t)RECT_TIMER_ID);
            Cy_PdUtils_SwTimer_Stop(ptrPdStackContext->ptrTimerContext,
                                    (cy_timer_id_t)PWMI_TIMER_ID);

            NVIC_DisableIRQ(ef_intr_cfg.intrSrc);
            Cy_GPIO_Write(PWMI_OUT_PORT, PWMI_OUT_PIN, 0u);
            DEBUG_PRINT("\r\n BB PWMI Switch OFF ..\r\n");

            /* Disable the TCPWM as PWM */
            Cy_TCPWM_PWM_Disable(CYBSP_PWM_HW, CYBSP_PWM_NUM);
            DEBUG_PRINT("\r\n BB VSET PWM Disabled\r\n");
            break;

        case BATT_CHGE_BB_RST:
        {
            /* BB reset causes VDDD (reference for ADC) drop. To avoid VBUS_OVP disallow and clear interrupt. */
            uint32_t state = Cy_SysLib_EnterCriticalSection();
            Cy_GPIO_Write(BUCK_BOOST_EN_PORT, BUCK_BOOST_EN_PIN, 1u);
            Cy_SysLib_DelayUs(BB_RESET_ASSERT_DELAY_US); /* Wait BB detect reset, to reactivate soft-start routine */
            Cy_GPIO_Write(BUCK_BOOST_EN_PORT, BUCK_BOOST_EN_PIN, 0u);
            Cy_SysLib_DelayUs(BB_RESET_RECOVERY_DELAY_US);
            ptrPdStackContext->ptrUsbPdContext->base->intr1 = PDSS_INTR1_CMP_OUT1_CHANGED |
                                                              PDSS_INTR1_CMP_OUT2_CHANGED;
            Cy_SysLib_ExitCriticalSection(state);
            break;
        }

        case BATT_CHGE_BB_EN:
            Cy_GPIO_Write(RECT_PORT, RECT_PIN, 0u);
            (void)Cy_PdUtils_SwTimer_Start(ptrPdStackContext->ptrTimerContext,
                                           (void*)ptrPdStackContext,
                                           (cy_timer_id_t)RECT_TIMER_ID,
                                           RECT_TIMER_PERIOD,
                                           &rect_timer_cb);
            (void)Cy_PdUtils_SwTimer_Start(ptrPdStackContext->ptrTimerContext,
                                           (void*)ptrPdStackContext,
                                           (cy_timer_id_t)PWMI_TIMER_ID,
                                           PWMI_TIMER_PERIOD,
                                           &pwmi_timer_cb);

            /* configure and enable pin interrupt */
            (void)Cy_SysInt_Init(&ef_intr_cfg, &ef_handler);
            Cy_GPIO_SetInterruptEdge(EF1_IN_PORT, EF1_IN_PIN, CY_GPIO_INTR_FALLING);
            Cy_GPIO_SetInterruptEdge(EF2_IN_PORT, EF2_IN_PIN, CY_GPIO_INTR_FALLING);
            NVIC_ClearPendingIRQ(ef_intr_cfg.intrSrc);
            Cy_GPIO_ClearInterrupt(EF1_IN_PORT, EF1_IN_PIN);
            Cy_GPIO_ClearInterrupt(EF2_IN_PORT, EF2_IN_PIN);
            NVIC_EnableIRQ(ef_intr_cfg.intrSrc);
            break;

        case BATT_CHGE_BB_OUT_EN:
            /* Turn on VBAT_FET to connect battery to Buck-Boost output */
            Cy_GPIO_Write(VBATT_PFET_CTRL_PORT, VBATT_PFET_CTRL_PIN, 1u);
            (void)Cy_PdUtils_SwTimer_Start(ptrPdStackContext->ptrTimerContext,
                                           (void*)ptrPdStackContext,
                                           (cy_timer_id_t)SOFTSTART_FET_OUT_TIMER_ID,
                                           SOFTSTART_FET_OUT_TIMER_PERIOD,
                                           &fet_out_timer_cb);
            DEBUG_PRINT("\r\n VBAT FET ON\r\n");
            break;

        case BATT_CHGE_BB_OUT_DIS:
            Cy_GPIO_Write(VBATT_PFET_CTRL_PORT, VBATT_PFET_CTRL_PIN, 0u);
            Cy_PdUtils_SwTimer_Stop(ptrPdStackContext->ptrTimerContext,
                                    (cy_timer_id_t)SOFTSTART_FET_OUT_TIMER_ID);
            DEBUG_PRINT("\r\n VBAT FET OFF\r\n");
            break;

        case BATT_CHGE_BB_IN_EN:
            Cy_GPIO_Write(VBUS_C_CTRL_PORT, VBUS_C_CTRL_PIN, 1u);
            batt_stat->bb_in_fet_status = true;
            (void)Cy_PdUtils_SwTimer_Start(ptrPdStackContext->ptrTimerContext,
                                           (void*)ptrPdStackContext,
                                           (cy_timer_id_t)SOFTSTART_FET_IN_TIMER_ID,
                                           SOFTSTART_FET_IN_TIMER_PERIOD,
                                           &fet_in_timer_cb);
            DEBUG_PRINT("\r\n VBUS FET ON\r\n");
            break;

        case BATT_CHGE_BB_IN_DIS:
            Cy_GPIO_Write(VBUS_C_CTRL_PORT, VBUS_C_CTRL_PIN, 0u);
            batt_stat->bb_in_fet_status = false;
            Cy_PdUtils_SwTimer_Stop(ptrPdStackContext->ptrTimerContext,
                                    (cy_timer_id_t)SOFTSTART_FET_IN_TIMER_ID);
            DEBUG_PRINT("\r\n VBUS Consumer FET OFF\r\n");
            break;

        case BATT_CHGE_BB_VSET_PWM:
            if (batt_stat->vbat_mon < cfg->battPackDisChrgVoltSnk)
            {
                batt_stat->chrg_i_set = cfg->battChrgMinCurr;
            }
            else
            {
                /* Take current available from contract but limit to max allowed charge current */
                batt_stat->chrg_i_set = CY_USBPD_GET_MIN(batt_stat->chrg_i_max, cfg->battChrgMaxCurr);
            }
            pwm_compare = batt_stat->chrg_i_set;
            pwm_compare *= RFB;
            pwm_compare *= IOUT_SENSE_GAIN;
            pwm_compare /= MA_PER_CURRENT_STEP;
            pwm_compare += ZERO_CURRENT_OFFSET_MV;
            pwm_compare *= PWM_PERIOD;
            pwm_compare /= batt_stat->vddd;
            Init_PWM_Output(PWM_PERIOD, pwm_compare);

            (void)Cy_PdUtils_SwTimer_Start(ptrPdStackContext->ptrTimerContext,
                                           (void*)ptrPdStackContext,
                                           (cy_timer_id_t)SOLN_BATT_PWM_INIT_TIMER_ID,
                                           SOLN_BATT_PWM_INIT_TIMER_PERIOD,
                                           &pwm_init_timer_cb);
            DEBUG_PRINT("\r\n Battery PWM output configured ");
            break;

        case BATT_CHGE_BB_DSCHRG:
            (void)Cy_PdUtils_SwTimer_Start(ptrPdStackContext->ptrTimerContext,
                                           (void*)ptrPdStackContext,
                                           (cy_timer_id_t)PFET_TURN_OFF_TIMER_ID,
                                           PFET_TURN_OFF_TIMER_PERIOD,
                                           &pfet_turn_off_timer_cb);
            break;

        default:
            /* Do nothing */
            break;
    }
}


void battery_measure_sar_adc_read(void)
{
    cy_stc_battery_charging_context_t* ptrBatteryChargingContext = get_battery_charging_context();
    cy_stc_battery_measure_t* adcResult = &(ptrBatteryChargingContext->batteryMeasure);
    cy_stc_usbpd_context_t* ptrUsbPdContext = ptrBatteryChargingContext->ptrPdStack->ptrUsbPdContext;

    uint8_t level = Cy_USBPD_Adc_Sample(ptrUsbPdContext, CY_USBPD_ADC_ID_1, CY_USBPD_ADC_INPUT_BANDGAP);

    /* Check for zero. If level came out as zero, then do not calculate. */
    if (level != 0u)
    {
        uint32_t vddd = PD_ADC_BAND_GAP_VOLT_MV * PD_ADC_NUM_LEVELS;
        vddd /= level;
        ptrUsbPdContext->adcVdddMv[CY_USBPD_ADC_ID_1] = vddd;
        adcResult->vddd_sum += vddd;

        uint32_t i_out_mon = get_adc_result_mv(IOUTMON_IN_PORT, IOUTMON_IN_PIN);
        if (i_out_mon > ZERO_CURRENT_OFFSET_MV)
        {
            i_out_mon -= ZERO_CURRENT_OFFSET_MV;
            i_out_mon *= MA_PER_CURRENT_STEP;
            i_out_mon /= (RFB * IOUT_SENSE_GAIN);
            adcResult->i_out_mon_sum += i_out_mon;
        }

        uint32_t i_in_mon = get_adc_result_mv(IINMON_IN_PORT, IINMON_IN_PIN);
        i_in_mon *= MA_PER_CURRENT_STEP;
        i_in_mon /= (RIN * IIN_SENSE_GAIN);
        adcResult->i_in_mon_sum += i_in_mon;

        uint32_t vbat = get_adc_result_mv(VBAT_MON_PORT, VBAT_MON_PIN);
        vbat *= (RVL2 + RVH2);
        vbat /= RVL2;
        adcResult->vbat_sum += vbat;

        uint32_t ntc1 = get_adc_result_mv(NTC1_PORT, NTC1_PIN);
        if (vddd > ntc1)
        {
            uint32_t ntc1_mv = (uint32_t)NTC1_RP;
            ntc1_mv *= ntc1;
            ntc1_mv /= (vddd - ntc1);
            adcResult->ntc1_sum += ntc1_mv;
        }

        adcResult->bjt_code_sum += Cy_USBPD_Adc_Sample(ptrUsbPdContext, CY_USBPD_ADC_ID_1, CY_USBPD_ADC_INPUT_BJT);
    }
}


static void battery_read_bb_error_flag(void)
{
    cy_stc_battery_status_t* batt_stat = &(get_battery_charging_context()->batteryStatus);

    /* Do not update EFx during BB discharge as faults expected */
    if (Cy_PdUtils_SwTimer_IsRunning(get_battery_charging_context()->ptrPdStack->ptrTimerContext,
                                     (cy_timer_id_t)BB_DSCHRG_TIMER_ID) == false)
    {
        uint8_t ef1 = (uint8_t)Cy_GPIO_Read(EF1_IN_PORT, EF1_IN_PIN);
        uint8_t ef2 = (uint8_t)Cy_GPIO_Read(EF2_IN_PORT, EF2_IN_PIN);

        if ((ef1 == 0u) || (ef2 == 0u))
        {
            Cy_SysLib_DelayUs(EF_DEBOUNCE_DELAY_US);
            ef1 = (uint8_t)Cy_GPIO_Read(EF1_IN_PORT, EF1_IN_PIN);
            ef2 = (uint8_t)Cy_GPIO_Read(EF2_IN_PORT, EF2_IN_PIN);
        }

        batt_stat->ef1 = ef1;
        batt_stat->ef2 = ef2;
    }
    #if 0
    DEBUG_PRINT_VAR("\r\n EF1:: %d ", batt_stat->ef1);
    DEBUG_PRINT_VAR(" EF2:: %d", batt_stat->ef2);
    #endif

    if ((batt_stat->ef1 == 0u) && (batt_stat->ef2 != 0u))
    {
        batt_stat->faults.bb_hw_ocp = 1u;
    }
    else if ((batt_stat->ef1 == 0u) && (batt_stat->ef2 == 0u))
    {
        batt_stat->faults.bb_hw_otp = 1u;
    }
    else if ((batt_stat->ef1 != 0u) && (batt_stat->ef2 == 0u))
    {
        batt_stat->faults.bb_hw_ovp = 1u;
    }
    else
    {
        batt_stat->faults.bb_hw_ovp = 0u;
        batt_stat->faults.bb_hw_ocp = 0u;
        batt_stat->faults.bb_hw_otp = 0u;
    }
}


static void run_battery_volt_monitor_task(void)
{
    cy_stc_battery_charging_context_t* ptrBatteryChargingContext = get_battery_charging_context();
    cy_stc_battery_status_t* batt_stat = &(ptrBatteryChargingContext->batteryStatus);
    const cy_stc_battery_charger_cfg_t* cfg = ptrBatteryChargingContext->ptrBattCghCfg;

    /* Print battery voltage only when VBAT_FET is on */
    if ((batt_stat->pmg_fsm_state == PMG_FSM_INIT_BB) ||
        (batt_stat->pmg_fsm_state == PMG_FSM_CHARGING_PRECHRG) ||
        (batt_stat->pmg_fsm_state == PMG_FSM_CHARGING_NORMAL) ||
        (batt_stat->pmg_fsm_state == PMG_FSM_CHARGING_TOPOFF) ||
        (batt_stat->pmg_fsm_state == PMG_FSM_SOURCE))
    {
        DEBUG_PRINT_VAR("\r\n Battery voltage: %d mV", batt_stat->vbat_mon);

        if (batt_stat->vbat_mon <= cfg->battPackUvpVolt)
        {
            batt_stat->faults.batt_uvp = 1u;
            DEBUG_PRINT("\r\n Battery is in UVP");
        }

        if (batt_stat->vbat_mon >= cfg->battPackOvpVolt)
        {
            batt_stat->faults.batt_ovp = 1u;
            DEBUG_PRINT("\r\n Battery is in OVP");
        }
    }
}


static void run_battery_curr_monitor_task(void)
{
    cy_stc_battery_charging_context_t* ptrBatteryChargingContext = get_battery_charging_context();
    cy_stc_battery_status_t* batt_stat = &(ptrBatteryChargingContext->batteryStatus);
    const cy_stc_battery_charger_cfg_t* cfg = ptrBatteryChargingContext->ptrBattCghCfg;
    uint32_t bb_input_cur_ocp_limit = ptrBatteryChargingContext->ptrPdStack->dpmStat.contract.curPwr;
    bb_input_cur_ocp_limit *= (PERCENT_DIVISOR + VBUS_OCP_CURR_PER);
    bb_input_cur_ocp_limit /= PERCENT_DIVISOR;

    DEBUG_PRINT_VAR("\r\n Input current: %d mA", batt_stat->vbus_i_in_mon * CY_PD_CUR_PER_UNIT);

    /* Limit value at 200mV to avoid negative current values. */
    DEBUG_PRINT_VAR("\r\n Output current: %d mA", batt_stat->chrg_i_out_mon * CY_PD_CUR_PER_UNIT);
    DEBUG_PRINT_VAR("\r\n Output Power: %lu mW",
                    (long unsigned int)((uint32_t)((batt_stat->vbat_mon *
                                                    batt_stat->chrg_i_out_mon) / MA_PER_CURRENT_STEP)));

    /* Verify VBUS current only when BB enabled */
    if ((batt_stat->chrg_i_out_mon > cfg->battChrgOcpCurr) ||
        ((batt_stat->vbus_i_in_mon > bb_input_cur_ocp_limit) &&
         ((batt_stat->pmg_fsm_state == PMG_FSM_CHARGING_PRECHRG) ||
          (batt_stat->pmg_fsm_state == PMG_FSM_CHARGING_NORMAL) ||
          (batt_stat->pmg_fsm_state == PMG_FSM_CHARGING_TOPOFF))))
    {
        batt_stat->faults.batt_ocp = 1u;
        DEBUG_PRINT("\r\n Battery or VBUS OCP is active");
    }
    else
    {
        batt_stat->faults.batt_ocp = 0u;
    }
}


static void measure_onboard_ntc1_sensor_data(void)
{
    cy_stc_battery_charging_context_t* ptrBatteryChargingContext = get_battery_charging_context();
    cy_stc_battery_status_t* batt_stat = &(ptrBatteryChargingContext->batteryStatus);
    const cy_stc_battery_charger_cfg_t* cfg = ptrBatteryChargingContext->ptrBattCghCfg;

    if (cfg->ntc1TempMonitorEn)
    {
        DEBUG_PRINT_VAR("\r\n ntc1:: %d C", batt_stat->ntc1_temp);

        switch (batt_stat->ntc1_state)
        {
            case STATE_TEMP_NORMAL_CHARGE:
                if (batt_stat->ntc1_temp < cfg->ntc1ColdThres)
                {
                    batt_stat->faults.ntc1_otp = 1u;
                    batt_stat->ntc1_state = STATE_TEMP_DISABLE_CHARGE_COLD;
                    DEBUG_PRINT("\r\n NTC1 cold, stop charging");
                }
                if (batt_stat->ntc1_temp >= cfg->ntc1HotThres)
                {
                    batt_stat->faults.ntc1_otp = 0u;
                    batt_stat->ntc1_state = STATE_TEMP_LIMITED_CHARGE;
                    DEBUG_PRINT("\r\n NTC1 limits current");
                }
                break;

            case STATE_TEMP_LIMITED_CHARGE:
                if (batt_stat->ntc1_temp >= cfg->ntc1OtpThres)
                {
                    batt_stat->faults.ntc1_otp = 1u;
                    batt_stat->ntc1_state = STATE_TEMP_OTP;
                    DEBUG_PRINT("\r\n NTC1 OTP");
                }
                if (batt_stat->ntc1_temp < (cfg->ntc1HotThres - cfg->ntc1TempHyst))
                {
                    batt_stat->faults.ntc1_otp = 0u;
                    batt_stat->ntc1_state = STATE_TEMP_NORMAL_CHARGE;
                    DEBUG_PRINT("\r\n NTC1 un-limits current");
                }
                break;

            case STATE_TEMP_OTP:
                if (batt_stat->ntc1_temp < (cfg->ntc1OtpThres - cfg->ntc1TempHyst))
                {
                    batt_stat->faults.ntc1_otp = 0u;
                    batt_stat->ntc1_state = STATE_TEMP_LIMITED_CHARGE;
                    DEBUG_PRINT("\r\n NTC1 limits current");
                }
                break;

            case STATE_TEMP_DISABLE_CHARGE_COLD:
                if (batt_stat->ntc1_temp >= (cfg->ntc1ColdThres + cfg->ntc1TempHyst))
                {
                    batt_stat->faults.ntc1_otp = 0u;
                    batt_stat->ntc1_state = STATE_TEMP_NORMAL_CHARGE;
                    DEBUG_PRINT("\r\n NTC1 un-limits current");
                }
                break;

            default:
                /* Do nothing */
                break;
        }
    }
}


static void measure_bjt_sensor_data(void)
{
    cy_stc_battery_status_t* batt_stat = &(get_battery_charging_context()->batteryStatus);

    switch (batt_stat->bjt_state)
    {
        case STATE_TEMP_NORMAL_CHARGE:
            if (batt_stat->bjt_code <= BJT_OTP_CODE)
            {
                batt_stat->faults.bjt_otp = 1u;
                batt_stat->bjt_state = STATE_TEMP_OTP;
                DEBUG_PRINT("\r\n BJT OTP, stop charging");
            }
            break;

        case STATE_TEMP_OTP:
            if (batt_stat->bjt_code > (BJT_OTP_CODE + BJT_HYST_CODE))
            {
                batt_stat->faults.bjt_otp = 0u;
                batt_stat->bjt_state = STATE_TEMP_NORMAL_CHARGE;
                DEBUG_PRINT("\r\n BJT back to normal");
            }
            break;

        default:
            /* Do nothing */
            break;
    }
}


void soln_battery_charger_task(void)
{
    cy_stc_battery_status_t* batt_stat = &(get_battery_charging_context()->batteryStatus);

    battery_read_bb_error_flag();
    measure_onboard_ntc1_sensor_data();
    measure_bjt_sensor_data();
    run_battery_volt_monitor_task();

    /* Monitor current and do compensation only in SNK/charging mode */
    if (((batt_stat->pmg_fsm_state == PMG_FSM_CHARGING_PRECHRG) ||
         (batt_stat->pmg_fsm_state == PMG_FSM_CHARGING_NORMAL) ||
         (batt_stat->pmg_fsm_state == PMG_FSM_CHARGING_FULL) ||
         (batt_stat->pmg_fsm_state == PMG_FSM_CHARGING_TOPOFF))
        &&
        (batt_stat->is_vbus_pwr_sufficient) &&
        (batt_stat->faults.batt_ocp == 0u))
    {
        run_battery_curr_monitor_task();
    }
    else
    {
        batt_stat->chrg_i_out_mon = 0u;
        batt_stat->vbus_i_in_mon = 0u;
    }
}


void compute_pwm_duty_cycle(uint16_t chrg_i_max)
{
    cy_stc_battery_charging_context_t* ptrBatteryChargingContext = get_battery_charging_context();
    cy_stc_battery_status_t* batt_stat = &(ptrBatteryChargingContext->batteryStatus);
    cy_stc_pdstack_context_t* ptrPdStackContext = ptrBatteryChargingContext->ptrPdStack;
    const cy_stc_battery_charger_cfg_t* cfg = ptrBatteryChargingContext->ptrBattCghCfg;
    static uint16_t offset = 0;
    static bool offset_sign_minus = false;
    static cal_state_t cal_state = STATE_CAL_INIT;
    uint16_t current = chrg_i_max;

    switch (cal_state)
    {
        case STATE_CAL_INIT:
            offset = 0;
            offset_sign_minus = false;
            (void)Cy_PdUtils_SwTimer_StartWocb(ptrPdStackContext->ptrTimerContext,
                                               (cy_timer_id_t)IOUT_OFFSET_CALIBR_ID,
                                               IOUT_OFFSET_CALIBR_PERIOD);
            cal_state = STATE_CAL_DELAY;
            break;

        case STATE_CAL_DELAY:
            if (Cy_PdUtils_SwTimer_IsRunning(ptrPdStackContext->ptrTimerContext,
                                             (cy_timer_id_t)IOUT_OFFSET_CALIBR_ID) == false)
            {
                /* Calculate offset between set and measured output current */
                if (batt_stat->chrg_i_set < batt_stat->chrg_i_out_mon)
                {
                    offset_sign_minus = true;
                    offset = batt_stat->chrg_i_out_mon - batt_stat->chrg_i_set;
                }
                else
                {
                    offset_sign_minus = false;
                    offset = batt_stat->chrg_i_set - batt_stat->chrg_i_out_mon;
                }
                cal_state = STATE_CAL_COMPLETE;
            }
            break;

        case STATE_CAL_COMPLETE:
            /* output current is already calibrated */
            break;

        default:
            /* Do nothing */
            break;
    }

    if (batt_stat->ntc1_state == STATE_TEMP_LIMITED_CHARGE)
    {
        current = CY_USBPD_GET_MIN(current / LIMITED_CHARGE_DIVISOR, CY_USBPD_I_2A);
        DEBUG_PRINT_VAR("\r\n NTC1 limits current to %d", current * CY_PD_CUR_PER_UNIT);
    }

    if (current > cfg->battChrgMaxCurr)
    {
        current = cfg->battChrgMaxCurr;
        DEBUG_PRINT_VAR("\r\n MAX_RATING limits current to %d", current * CY_PD_CUR_PER_UNIT);
    }

    batt_stat->chrg_i_set = current;
    DEBUG_PRINT_VAR("\r\n BB Out Curr:: %d mA\n", (current * CY_PD_CUR_PER_UNIT));

    if (offset_sign_minus == true)
    {
        current -= offset;
    }
    else
    {
        current += offset;
    }

    /* Calculate PWM compare value for the output current */
    uint32_t pwm_compare = current;
    pwm_compare *= (RFB * IOUT_SENSE_GAIN);
    pwm_compare /= MA_PER_CURRENT_STEP;
    pwm_compare += ZERO_CURRENT_OFFSET_MV;
    pwm_compare *= PWM_PERIOD;
    pwm_compare /= batt_stat->vddd;
    batt_stat->pwm_compare = (uint16_t)pwm_compare;

    Cy_TCPWM_PWM_SetCompare0(CYBSP_PWM_HW, CYBSP_PWM_NUM, batt_stat->pwm_compare);

    #if 0
    DEBUG_PRINT_VAR("\r\n BB Out PWM Compare:: %d\n", batt_stat->pwm_compare);
    #endif /* 0 */
}


/* Prototypes for the solution-layer source-path overrides. These provide a
 * visible declaration (Rule 8.4) for functions that strongly override the
 * pmg-app-common weak defaults registered with PDStack. */
void soln_set_volt_port1(uint16_t vol_in_mv);
void soln_vbus_fet_on(cy_stc_pdstack_context_t* context);
void soln_vbus_fet_off(cy_stc_pdstack_context_t* context);

/* Procedure to set VBUS voltage level for SRC-mode Buck controller */
void soln_set_volt_port1(uint16_t vol_in_mv)
{
    Cy_GPIO_Write(_9V_SRC_SEL_PORT, _9V_SRC_SEL_PIN, (vol_in_mv == CY_PD_VSAFE_9V) ? 1u : 0u);
    DEBUG_PRINT("\r\n >> SRC SET VOLT ");
}


void soln_vbus_fet_on(cy_stc_pdstack_context_t* context)
{
    (void)context; /* Unused parameter */

    Cy_GPIO_Write(BUCK_EN_PORT, BUCK_EN_PIN, 1u);
    Cy_GPIO_Write(LOADSWITCH_EN_PORT, LOADSWITCH_EN_PIN, 0u);
    DEBUG_PRINT("\r\n >> SRC ENA ");
}


void soln_vbus_fet_off(cy_stc_pdstack_context_t* context)
{
    (void)context; /* Unused parameter */

    Cy_GPIO_Write(LOADSWITCH_EN_PORT, LOADSWITCH_EN_PIN, 1u);
    Cy_GPIO_Write(BUCK_EN_PORT, BUCK_EN_PIN, 0u);
    DEBUG_PRINT("\r\n >> SRC DIS ");
}


#if VBUS_OVP_ENABLE
void soln_fault_vbus_ovp_enable(cy_stc_pdstack_context_t* context, uint16_t volt_mV, bool pfet,
                                cy_cb_vbus_fault_t ovp_cb)
{
    (void)pfet; /* Unused parameter */

    /* Calculate required VBUS for OVP. */
    uint8_t level =  Cy_USBPD_Adc_GetVbusLevel(context->ptrUsbPdContext, CY_USBPD_ADC_ID_0, volt_mV,
                                               GET_VBUS_OVP_TABLE(context->ptrUsbPdContext)->threshold);

    /* Use the ADC comparator to monitor VBus OVP voltage */
    Cy_USBPD_Adc_CompCtrl(context->ptrUsbPdContext, CY_USBPD_ADC_ID_0, CY_USBPD_ADC_INPUT_AMUX_A,
                          level, CY_USBPD_ADC_INT_FALLING, (cy_cb_adc_events_t)ovp_cb);
}


void soln_fault_vbus_ovp_disable(cy_stc_pdstack_context_t* context)
{
    /* Disable the ADC comparator for VBus OVP */
    Cy_USBPD_Adc_CompCtrl(context->ptrUsbPdContext, CY_USBPD_ADC_ID_0, CY_USBPD_ADC_INPUT_AMUX_A,
                          0u, CY_USBPD_ADC_INT_DISABLED, NULL);
}


#endif /* VBUS_OVP_ENABLE */


/* [] END OF FILE */
