/***************************************************************************//**
* \file cy_pmg_charger_hw.h
* \version 1.0
*
* \brief
* Provides the public functions for the API for the PMG battery charger hardware.
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

#ifndef CY_PMG_CHARGER_HW_H
#define CY_PMG_CHARGER_HW_H

#include <stdbool.h>
#include <stdint.h>

/* Enables UART sending DEBUG data */
#ifndef DEBUG_PRINT_ENABLE
#define DEBUG_PRINT_ENABLE (0)
#endif /* DEBUG_PRINT_ENABLE */

/* Printed DEBUG data formatted into a table */
#ifndef DEBUG_PRINT_TABLE_VIEW
#define DEBUG_PRINT_TABLE_VIEW (0)
#endif /* DEBUG_PRINT_TABLE_VIEW */

#if DEBUG_PRINT_ENABLE && (DEBUG_PRINT_TABLE_VIEW == 0)
#define DEBUG_PRINT(string)                         \
{                                                   \
    Cy_SCB_UART_PutString(CYBSP_UART_HW, string);   \
}

#define DEBUG_PRINT_VAR(string, value)              \
{                                                   \
    char_t temp[80];                                \
    (void)sprintf(temp,string,value);               \
    Cy_SCB_UART_PutString(CYBSP_UART_HW, temp);     \
}
#else /* DEBUG_PRINT_ENABLE && (DEBUG_PRINT_TABLE_VIEW == 0) */
#define DEBUG_PRINT(string)
#define DEBUG_PRINT_VAR(string, value)
#endif /* DEBUG_PRINT_ENABLE && (DEBUG_PRINT_TABLE_VIEW == 0) */

/*
 * Set the PWM period in count value in the range: 0 - 65535. Maximum value 65535 corresponds to 1
 * full round of count.
 */
#ifndef PWM_PERIOD
#define PWM_PERIOD                                  (3990u)
#endif /* PWM_PERIOD */


/*******************************************************************************
 * Global Function Declaration
 ******************************************************************************/

/**
 * @brief Peripherals initialization.
 */
void Cy_PmgChg_HW_Init(void);

/**
 * @brief Procedure to control BB, input/output switches based on commands.
 * @param src_cmd Commands like en/dis BB, IN/OUT switches
 */
void Cy_PmgChg_HW_Cmd(const batt_chge_src_cmd_t src_cmd);

/**
 * @brief Regular task for voltages measurement and analysis.
 */
void soln_battery_charger_task(void);

/**
 * @brief Procedure to calculate PWM compare value based on charging current.
 * @param chrg_i_max Battery Charging current
 */
void compute_pwm_duty_cycle(uint16_t chrg_i_max);

/**
 * @brief Procedure to do all measurement.
 */
void battery_measure_sar_adc_read(void);

/**
 * @brief LED manager - decides the LED mode in according to Charging FSM state.
 */
void pmg_led_mngr(void);

#endif /* CY_PMG_CHARGER_HW_H */

/* [] END OF FILE */
