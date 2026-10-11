/***************************************************************************//**
* \file cy_pmg_charger.h
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

#ifndef CY_PMG_CHARGER_H
#define CY_PMG_CHARGER_H

#include "cy_pdstack_common.h"
#include "cy_pdutils_sw_timer.h"
#include <stdbool.h>

/*******************************************************************************
 * Data types
 ******************************************************************************/

/* Percentage of current reduction between charging cycles. */
#ifndef CURRENT_REDUCTION_PER_CYCLE
#define CURRENT_REDUCTION_PER_CYCLE                 (50u)
#endif /* CURRENT_REDUCTION_PER_CYCLE */

/* Multiplicative or subtractive reduction of current based on cycle number */
#ifndef MULTIPLICATIVE_CURRENT_REDUCTION
#define MULTIPLICATIVE_CURRENT_REDUCTION            (1u)
#endif /* MULTIPLICATIVE_CURRENT_REDUCTION */

#ifndef RVL2
#define RVL2                                        (9100u) /* in Ohm */
#endif /* RVL2 */

#ifndef RIN
#define RIN                                         (9u) /* in mOhm */
#endif /* RIN */

#ifndef RVH2
#define RVH2                                        (30100u) /* in Ohm */
#endif /* RVH2 */

#ifndef RFB
#define RFB                                         (15u) /* in mOhm */
#endif /* RFB */

/* thermistor B coefficient */
#ifndef NTC1_B
#define NTC1_B                                      (4509.0f) /* in Kelvin */
#endif /* NTC1_B */

/* temperature for nominal thermistor resistance */
#ifndef NTC1_T25
#define NTC1_T25                                    (273.0f + 25.0f) /* in Kelvin */
#endif /* NTC1_T25 */

/* thermistor resistance at 25C */
#ifndef NTC1_R25
#define NTC1_R25                                    (100000.0f) /* in Ohm */
#endif /* NTC1_R25 */

/* Pull-up resistor in thermistor divider */
#ifndef NTC1_RP
#define NTC1_RP                                     (40000.0f) /* in Ohm */
#endif /* NTC1_RP */

/* Intermediate define helping to convert resistance into temperature */
#define NTC1_R                                      (NTC1_R25 * expf(-NTC1_B / NTC1_T25))

/*
 * BJT average ADC code - Temperature C
 * 41 - 0 C
 * 39 - 25 C
 * 37 - 60 C
 * 36 - 70 C
 * 35 - 85 C
 * 34 - 100 C
 * 33 - 110 C
 * 32 - 125 C
 */

/* 100; Variation: 85C to 115C */
#ifndef BJT_OTP_CODE
#define BJT_OTP_CODE                                (34u)
#endif /* BJT_OTP_CODE */

/* 1 ADC BJT code */
#ifndef BJT_HYST_CODE
#define BJT_HYST_CODE                               (1u)
#endif /* BJT_HYST_CODE */

/*
 * Activity indicator LED timer. The timer is used to indicate that the firmware
 * is functional. The feature is controlled by APP_FW_LED_ENABLE.
 */
#define LED_TIMER_ID                                (CY_PDUTILS_TIMER_USER_START_ID)

/* The LED toggle period (ms) to be used when a battery is charging. */
#ifndef LED_PD_SNK_TIMER_PERIOD
#define LED_PD_SNK_TIMER_PERIOD                     (100u)
#endif /* LED_PD_SNK_TIMER_PERIOD */

/* The LED toggle period (ms) to be used when a battery is discharging. */
#ifndef LED_PD_SRC_TIMER_PERIOD
#define LED_PD_SRC_TIMER_PERIOD                     (500u)
#endif /* LED_PD_SRC_TIMER_PERIOD */

#define SOLN_BATT_MONITOR_TASK_TIMER_ID             (CY_PDUTILS_TIMER_USER_START_ID + 1u)
/* Period of FSM cycling/processing in ms */
#ifndef SOLN_BATT_MONITOR_TASK_TIMER_PERIOD
#define SOLN_BATT_MONITOR_TASK_TIMER_PERIOD         (100u)
#endif /* SOLN_BATT_MONITOR_TASK_TIMER_PERIOD */

#define SOLN_BATT_PWM_INIT_TIMER_ID                 (CY_PDUTILS_TIMER_USER_START_ID + 2u)
/* Delay in ms between PWM set and BB enable */
#ifndef SOLN_BATT_PWM_INIT_TIMER_PERIOD
#define SOLN_BATT_PWM_INIT_TIMER_PERIOD             (150u)
#endif /* SOLN_BATT_PWM_INIT_TIMER_PERIOD */

#define MINUTE_TIMER_ID                             (CY_PDUTILS_TIMER_USER_START_ID + 3u)
/* One minute tick delay (ms) */
#define MINUTE_TIMER_PERIOD                         (60000u)

#define PWMI_TIMER_ID                               (CY_PDUTILS_TIMER_USER_START_ID + 4u)
#ifndef PWMI_TIMER_PERIOD
/* PWMI timer period (ms) */
#define PWMI_TIMER_PERIOD                           (5u)
#endif /* PWMI_TIMER_PERIOD */

#define RECT_TIMER_ID                               (CY_PDUTILS_TIMER_USER_START_ID + 5u)
#ifndef RECT_TIMER_PERIOD
/* Rectifier timer period (ms) */
#define RECT_TIMER_PERIOD                           (PWMI_TIMER_PERIOD + 10u)
#endif /* RECT_TIMER_PERIOD */

#define IOUT_OFFSET_CALIBR_ID                       (CY_PDUTILS_TIMER_USER_START_ID + 6u)
/* Output current offset calibration delay (ms) */
#ifndef IOUT_OFFSET_CALIBR_PERIOD
#define IOUT_OFFSET_CALIBR_PERIOD                   (500u)
#endif /* IOUT_OFFSET_CALIBR_PERIOD */

#define SOFTSTART_FET_IN_TIMER_ID                   (CY_PDUTILS_TIMER_USER_START_ID + 7u)
#ifndef SOFTSTART_FET_IN_TIMER_PERIOD
/* Input FET soft-start enable delay (ms) */
#define SOFTSTART_FET_IN_TIMER_PERIOD               (1200u)
#endif /* SOFTSTART_FET_IN_TIMER_PERIOD */

#define SOFTSTART_FET_OUT_TIMER_ID                  (CY_PDUTILS_TIMER_USER_START_ID + 8u)
#ifndef SOFTSTART_FET_OUT_TIMER_PERIOD
/* Output FET soft-start enable delay (ms) */
#define SOFTSTART_FET_OUT_TIMER_PERIOD              (1200u)
#endif /* SOFTSTART_FET_OUT_TIMER_PERIOD */

/* Timer controls how frequently ADC measures In/Out voltages/currents */
#define ADC_PERIOD_TIMER_ID                         (CY_PDUTILS_TIMER_USER_START_ID + 9u)
/* Period (in ms) of ADC measurements for averaging */
#define ADC_PERIOD_TIMER_PERIOD                     (2u)


/* Timer to turn off PFET before enable BB discharge */
#define PFET_TURN_OFF_TIMER_ID                       (CY_PDUTILS_TIMER_USER_START_ID + 10u)
/* Period (in ms) of turn off */
#define PFET_TURN_OFF_TIMER_PERIOD                   (5u)


/* Timer to discharge input BB capacitors */
#define BB_DSCHRG_TIMER_ID                           (CY_PDUTILS_TIMER_USER_START_ID + 11u)
/* Period (in ms) of discharge */
#define BB_DSCHRG_TIMER_PERIOD                       (10u)

/* VBUS OCP percentage for BB Input Current  */
#ifndef VBUS_OCP_CURR_PER
#define VBUS_OCP_CURR_PER                           (20u)
#endif /* VBUS_OCP_CURR_PER */

/* Settle delay before reducing current again */
#define PMG_CURRENT_REDUCTION_SETTLE_DELAY_MS       (1000u)

#define PERCENT_DIVISOR                             (100u)

/**
 * @brief Commands to control BB, input/output switches
 */
typedef enum
{
    BATT_CHGE_BB_DIS = 0u,
    BATT_CHGE_BB_EN,
    BATT_CHGE_BB_OUT_EN,
    BATT_CHGE_BB_OUT_DIS,
    BATT_CHGE_BB_IN_EN,
    BATT_CHGE_BB_IN_DIS,
    BATT_CHGE_BB_VSET_PWM,
    BATT_CHGE_BB_RST,
    BATT_CHGE_BB_DSCHRG
} batt_chge_src_cmd_t;

/**
 * @brief Enumeration for temperature states
 */
typedef enum
{
    STATE_TEMP_NORMAL_CHARGE = 0u,
    STATE_TEMP_LIMITED_CHARGE,
    STATE_TEMP_OTP,
    STATE_TEMP_DISABLE_CHARGE_COLD
} temp_state_t;

/**
 * @brief Enumeration for i_out calibration states
 */
typedef enum
{
    STATE_CAL_INIT = 0u,
    STATE_CAL_DELAY,
    STATE_CAL_COMPLETE
} cal_state_t;

/**
 * @brief Enumeration for post charge action
 */
enum
{
    PMG_POST_CHARGE_ACTION_RECHARGE = 0u,
    PMG_POST_CHARGE_ACTION_SHUTDOWN,
    PMG_POST_CHARGE_ACTION_TOPOFF
};

/**  No charging events pending. */
#define PMG_EVT_NONE                             (0x00000000u)

/** State change pending. */
#define PMG_EVT_ENTRY                            (0x00000001u)

/** BB input switch soft-start complete. */
#define PMG_EVT_IN_SS_CMPT                       (0x00000002u)

/** BB output switch soft-start complete. */
#define PMG_EVT_OUT_SS_CMPT                      (0x00000004u)

/** VSET PWM initialization complete. */
#define PMG_EVT_PWM_INIT_CMPT                    (0x00000008u)

/** BB initialization complete including RECT+PWMI delay */
#define PMG_EVT_BB_INIT_CMPT                     (0x00000010u)

/** Event related to stop/restore charging in cooldown state. */
#define PMG_EVT_COOLDOWN                         (0x00000020u)

/** Mask all charger states. */
#define PMG_EVT_ALL_MASK                         (0xFFFFFFFFu)

/**
 * @typedef pmg_fsm_evt_t
 * @brief List of PMG events.
 */
typedef enum
{
    PMG_FSM_EVT_ENTRY = 0,             /**< 0: PMG Event: State entry. */
    PMG_FSM_EVT_IN_SS_CMLT,            /**< 1: PMG Event: BB Input switch soft-start complete. */
    PMG_FSM_EVT_OUT_SS_CMLT,           /**< 2: PMG Event: BB Output switch soft-start complete. */
    PMG_FSM_EVT_PWM_INIT_CMLT,         /**< 3: PMG Event: VSET PWM initialization complete. */
    PMG_FSM_EVT_BB_INIT_CMLT,          /**< 4: PMG Event: BB init complete (RECT+PWMI delay) */
    PMG_FSM_EVT_COOLDOWN,              /**< 5: PMG Event: Enter/exit for cool-down state. */
    PMG_FSM_EVT_MAX_EVTS               /**< 6: Number of events. */
} pmg_fsm_evt_t;

/**
 * @typedef pmg_state_t
 * @brief List of PMG charging states in the battery charging state machine.
 */
typedef enum
{
    PMG_FSM_OFF = 0,                  /**< PMG state machine inactive. */
    PMG_FSM_INIT,
    PMG_FSM_INIT_FET_IN,
    PMG_FSM_INIT_FET_OUT,
    PMG_FSM_INIT_BB,
    PMG_FSM_CHARGING_PRECHRG,
    PMG_FSM_CHARGING_NORMAL,
    PMG_FSM_CHARGING_FULL,
    PMG_FSM_CHARGING_TOPOFF,          /**< PMG state for top-off charging. */
    PMG_FSM_SHUTDOWN,
    PMG_FSM_COOLDOWN,                 /**< PMG state to wait cool-down. */
    PMG_FSM_SOURCE_INIT,              /**< PMG state when device is initialized to SRC mode. */
    PMG_FSM_SOURCE,                   /**< PMG state when device is in SRC mode. */
    PMG_FSM_MAX_STATES                /**< Invalid state ID. */
} pmg_state_t;

/** Structure to hold battery charging faults as individual bit flags. */
typedef struct
{
    uint32_t batt_ovp         : 1; /**< Bit 0: ovp fault condition flag */
    uint32_t batt_ocp         : 1; /**< Bit 1: ocp fault condition flag */
    uint32_t batt_rcp         : 1; /**< Bit 2: rcp fault condition flag */
    uint32_t batt_uvp         : 1; /**< Bit 3: uvp fault condition flag */
    uint32_t batt_otp         : 1; /**< Bit 4: OTP fault condition flag */
    uint32_t ntc0_otp         : 1; /**< Bit 5: NTC0 OTP fault condition flag */
    uint32_t ntc1_otp         : 1; /**< Bit 6: NTC1 OTP fault condition flag */
    uint32_t timeout_expired  : 1; /**< Bit 7: top-off, pre-charge or normal charge
                                        timeout flag */
    uint32_t bb_hw_ovp        : 1; /**< Bit 8: BB HW OVP fault condition flag */
    uint32_t bb_hw_ocp        : 1; /**< Bit 9: BB HW OCP fault condition flag */
    uint32_t bb_hw_otp        : 1; /**< Bit 10: BB HW OTP fault condition flag */
    uint32_t bjt_otp          : 1; /**< Bit 11: BJT OTP fault condition flag */
    uint32_t fault_active     : 1; /**< Bit 12: Latched fault indication */
    uint32_t reserved         : 19;  /**< Bits 31:13 - Reserved for future use */
} pmg_fault_t;

/**
 * @brief Structure holds the Battery Status
 */
typedef struct
{
    volatile bool is_vbus_pwr_sufficient;   /**< VBUS Power is sufficient to charge the battery */
    uint8_t chrg_cycle_num;                 /**< Number of Full Charge cycles during current Type-C
                                                 Connections */
    pmg_fault_t faults;                     /**< Battery charging faults */
    uint16_t pwm_compare;                   /**< Current PWM compare value */
    volatile uint16_t chrg_i_max;           /**< BB MAX current allowed in normal conditions */
    volatile uint16_t chrg_i_set;           /**< BB live current for PWM calculation */
    volatile uint32_t chrg_bb_pwr;          /**< Current Input power available for Buck-Boost */
    volatile uint16_t vbat_mon;             /**< Current battery voltage measured by ADC */
    volatile uint16_t chrg_i_out_mon;       /**< Current Battery charging current measured using the
                                                 sense resistor in 10mA units */
    volatile uint16_t vbus_i_in_mon;        /**< Current BB input current measured in 10mA units */
    int16_t ntc1_temp;                      /**< Temperature in degree Celsius measured by NTC1 sensor */
    temp_state_t ntc1_state;                /**< NTC1 limit current state machine */
    uint8_t bjt_code;                       /**< BJT (silicon temperature sensor) ADC code */
    temp_state_t bjt_state;                 /**< BJT limit current state machine */
    pmg_state_t pmg_fsm_state;              /**< Current state of the PMG state machine */
    pmg_state_t pmg_fsm_state_prev;         /**< Charging state when Overheat happened */
    uint32_t pmg_evt;                       /**< Bitmap representing event notifications to the state
                                                 machine */
    uint16_t chrg_timeout_counter;          /**< Minutes counter */
    uint8_t ef1;                            /**< Error flag 1 read from BB */
    uint8_t ef2;                            /**< Error flag 2 read from BB */
    uint16_t vddd;                          /**< VDDD voltage in mV */
    bool bb_in_fet_status;                  /**< BB input FET status */
} cy_stc_battery_status_t;

/**
 * @brief Structure contains ADC measured values
 */
typedef struct
{
    uint32_t index;                    /**< Filter Index */
    uint32_t i_out_mon_sum;            /**< Measured output BB current. */
    uint32_t i_in_mon_sum;             /**< Measured input BB current */
    float32_t ntc1_sum;                /**< NTC1 sensor temperature */
    uint32_t vbat_sum;                 /**< Voltage at battery */
    uint32_t bjt_code_sum;             /**< BJT (Silicon temperature sensor) ADC code */
    uint32_t vddd_sum;                 /**< VDDD voltage in mV */
} cy_stc_battery_measure_t;

/**
 * @brief Structure to hold PMG1Bx middleware configuration information.
 */
typedef struct
{
    uint16_t battCellMaxVolt;           /**< Maximum allowed cell voltage for individual cells
                                             in mV */
    uint16_t battCellReChrgVolt;        /**< Battery recharge voltage per cell. This is the
                                             threshold used to resume charging if the battery
                                             voltage falls after a charge cycle is complete. */
    uint16_t battCellDisChrgVoltSrc;    /**< Completely discharged cell voltage when USB-C is
                                             in Source mode. Battery cannot be used to source
                                             power below this voltage. */
    uint16_t battCellDisChrgVoltSnk;    /**< Discharged cell voltage when USB-C is in Sink mode.
                                             Pre-charging is done below this voltage. */
    uint16_t battCellOvpVolt;           /**< Cell OVP threshold in mV to detect overvoltage
                                             condition */
    uint16_t battCellUvpVolt;           /**< Undervoltage threshold of cell below which battery
                                             will not be charged at all; battery is considered
                                             damaged */

    uint16_t battPackMaxVolt;           /**< Maximum battery voltage corresponding to full charge
                                             in mV */
    uint16_t battPackReChrgVolt;        /**< Battery voltage threshold to resume charging if the
                                             battery voltage falls after a charge cycle is complete
                                             in mV */
    uint16_t battPackDisChrgVoltSrc;    /**< Completely discharged battery voltage when USB-C is
                                             in Source mode. Battery cannot be used to source power
                                             below this voltage */
    uint16_t battPackDisChrgVoltSnk;    /**< Discharged battery voltage when USB-C is in Sink mode.
                                             Pre-charging is done below this voltage. */

    uint16_t battPackOvpVolt;           /**< Battery overvoltage protection threshold */
    uint16_t battPackUvpVolt;           /**< Undervoltage detection threshold of the battery below
                                             which battery will not be charged */

    uint16_t battRemoveThresVolt;       /**< Battery removal threshold below which battery is not
                                             detected as present */
    uint16_t battChrgMaxCurr;           /**< Maximum rated charging current limit for the battery.
                                             Set this as per battery specification. Step: 10 mA */

    uint16_t battChrgMinCurr;           /**< Low charging current limit used for pre-charge and
                                             top-off charge. Step: 10 mA */
    uint16_t battChrgOcpCurr;           /**< Overcurrent protection (OCP) threshold for the battery.
                                             Step: 10 mA */

    uint8_t cellCount;                  /**< Total number of battery cells connected in series */
    uint8_t buckBoostEffcy;             /**< Efficiency of buck-boost power stage */
    uint8_t vbusTolMultiplier;          /**< USB-C VBUS tolerance */
    uint8_t manuEffcyMultiplier;        /**< Manufacturing efficiency to cover tolerances and any
                                             additional power loss in the system */

    uint32_t extMsgValidBatStatSrc;     /**< Valid battery status response when operating in
                                             USB-C Source mode */
    uint32_t extMsgValidBatStatSnk;     /**< Valid battery status response when operating in
                                             USB-C Sink mode */
    uint32_t extMsgInvalidBatRef;       /**< Invalid battery_status response */
    uint32_t extMsgVidPid;              /**< Battery capability message VID and PID count */
    uint16_t extMsgBatDesCap;           /**< Battery capability message design capacity */
    uint16_t extMsgBatFullChrgCap;      /**< Battery capability message last full charge capacity */

    uint16_t vbusMinPwr;                /**< Minimum power from USB-C port required for charging the
                                             battery in mWatt */
    uint16_t preChrgTmrCnt;             /**< Pre-charge timeout duration in minutes. */

    uint16_t normalChrgTmrCnt;          /**< Normal charge timeout duration in minutes */
    uint16_t topoffChrgTmrCnt;          /**< Top-off charge timeout duration in minutes */

    bool preChrgTmrEn;                  /**< Enable the pre-charge timer */
    bool normalChrgTmrEn;               /**< Enable the normal charge timer */
    bool topoffChrgTmrEn;               /**< Enable the top-off charge timer */
    uint8_t postChargeAct;              /**< System action after a full charge:
                                             0 - recharge, 1 - shutdown, 2 - top-off */

    int16_t battOtpThres;               /**< Battery overtemperature protection (OTP) threshold
                                             above which charging is cut off */
    int16_t battRoomThres;              /**< Minimum battery temperature required for charging with
                                             maximum current. Charging current will drop to 1A below
                                             this temperature */

    int16_t battColdThres;              /**< Battery temperature threshold below which battery
                                             charging is cut-off and above which battery charging is
                                             limited to 1A or less. */
    int16_t battTempHyst;               /**< Battery temperature hysteresis value for various
                                             threshold detection */

    bool battTempMonitorEn;             /**< Enable battery temperature monitoring */
    bool ntc0TempMonitorEn;             /**< Enable board temperature monitoring by NTC0 */
    int16_t ntc0OtpThres;               /**< Board overtemperature (OTP) threshold above which
                                             charging is cut-off */

    int16_t ntc0HotThres;               /**< Board temperature threshold above which charging
                                             current is reduced by half and less than 2A */
    int16_t ntc0TempHyst;               /**< Temperature hysteresis value for NTC0-based threshold
                                             detection */

    int16_t ntc1OtpThres;               /**< Board overtemperature (OTP) threshold above which
                                             charging is cut-off */
    int16_t ntc1HotThres;               /**< Board temperature threshold above which charging
                                             current is reduced by half and less than 2A */

    int16_t ntc1ColdThres;              /**< Board temperature threshold below which battery
                                             charging is cut-off */
    int16_t ntc1TempHyst;               /**< Temperature hysteresis value for NTC1-based threshold
                                             detection */

    bool ntc1TempMonitorEn;             /**< Enable board temperature monitoring by NTC1 */
    uint8_t reserved[3];                /**< Reserved for future use. */
} cy_stc_battery_charger_cfg_t;

/**
 * @brief Structure to Battery Charging Middleware context information.
 */
typedef struct
{
    cy_stc_battery_status_t batteryStatus;              /**< Battery Status */
    cy_stc_battery_measure_t batteryMeasure;            /**< Battery Measure */
    cy_stc_pdstack_context_t* ptrPdStack;               /**< Pointer to PD stack */
    const cy_stc_battery_charger_cfg_t* ptrBattCghCfg;  /**< Pointer to Battery Charging settings
                                                             configuration */
} cy_stc_battery_charging_context_t;

/*******************************************************************************
 * Global Function Declaration
 ******************************************************************************/
/**
 * @brief This function initializes solution status structure.
 * @param ptrPdStackContext PD Stack context.
 * @param ptrBatChgCfg Pointer to Battery Charging configuration.
 */
void Cy_App_Pmg_Init(cy_stc_pdstack_context_t* ptrPdStackContext,
                     const cy_stc_battery_charger_cfg_t* ptrBatChgCfg);

/**
 * @brief Battery Charger Solution PD event handler .
 * @param ptrPdStackContext PD Stack context.
 * @param evt PD event
 * @param data Pointer to PD event data
 */
void Cy_App_Pmg_PdEventHandler(cy_stc_pdstack_context_t* ptrPdStackContext,
                               cy_en_pdstack_app_evt_t evt, const void* data);

/**
 * @brief This function processes Solution level tasks.
 * @param ptrPdStackContext PD Stack context.
 */
void Cy_App_Pmg_Task(const cy_stc_pdstack_context_t* ptrPdStackContext);

/**
 * @brief Procedure returns pointer to battery charger structure.
 *
 * @return cy_stc_battery_charging_context_t Pointer to battery charger structure
 */
cy_stc_battery_charging_context_t*  get_battery_charging_context(void);

/**
 * @brief This function disables input/output switches and BB charging module.
 *
 * @param ptrPdStackContext Pointer to PD stack context.
 */
void Cy_App_Pmg_Charging_Disable(const cy_stc_pdstack_context_t* ptrPdStackContext);

/**
 * @brief This function sets an event status for the battery charging state machine to process.
 *
 * @param evt_mask Mask specifying events to be set.
 */
void Cy_App_Pmg_FsmSetEvt(uint32_t evt_mask);

/**
 * @brief This function clears one or more events after the battery charging state machine has
 * dealt with them.
 *
 * @param evt_mask Event Mask to be cleared.
 */
void Cy_App_Pmg_FsmClearEvt(uint32_t evt_mask);

/**
 * @brief Creates an entry event into next FSM state.
 *
 * @param next_state Next FSM state.
 */
void Cy_App_Pmg_Next_Fsm_Entry(pmg_state_t next_state);

#endif /* CY_PMG_CHARGER_H */

/* [] END OF FILE */
