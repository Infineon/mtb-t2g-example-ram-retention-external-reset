/**********************************************************************************************************************
 * \file main.c
 * \copyright Copyright (C) Infineon Technologies AG 2024
 *
 * Use of this file is subject to the terms of use agreed between (i) you or the company in which ordinary course of
 * business you are acting and (ii) Infineon Technologies AG or its licensees. If and as long as no such terms of use
 * are agreed, use of this file is subject to following:
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization obtaining a copy of the software and
 * accompanying documentation covered by this license (the "Software") to use, reproduce, display, distribute, execute,
 * and transmit the Software, and to prepare derivative works of the Software, and to permit third-parties to whom the
 * Software is furnished to do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including the above license grant, this restriction
 * and the following disclaimer, must be included in all copies of the Software, in whole or in part, and all
 * derivative works of the Software, unless such copies or derivative works are solely in the form of
 * machine-executable object code generated by a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *********************************************************************************************************************/
/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "cy_pdl.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
/* GPIO interrupt priority */
#define GPIO_INTERRUPT_PRIORITY     (3u)

/* GPIO interrupt configuration structure */
cy_stc_sysint_t gpio_irq_cfg =
{
    .intrSrc = ((NvicMux3_IRQn << CY_SYSINT_INTRSRC_MUXIRQ_SHIFT) | ioss_interrupts_gpio_0_IRQn),
    .intrPriority = GPIO_INTERRUPT_PRIORITY
};

/* SRAM Controller1 info */
#define SRAM_CONTROLLER1_START      (0x28040000)
#define SRAM_CONTROLLER1_END        (0x2805FFFF)

/* Delays */
#define LONG_DELAY_MS               100u      /* in ms */

/* PWR_MODE Register definitions */
#define PWR_MODE_RETAINED           0x05FA0002
#define PWR_MODE_ENABLED            0x05FA0003

/* SRAM1 valid value */
#define VALID_VAL                   0xA5A5A5A5

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
void Handle_GPIO_Interrupt(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
/**********************************************************************************************************************
 * Function Name: main
 * Summary:
 *  This is the main function.
 * Parameters:
 *  none
 * Return:
 *  int
 **********************************************************************************************************************
 */
int main(void)
{
    cy_rslt_t result;
    uint32_t data       = 0;
    uint32_t errCount   = 0;

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    Cy_SCB_UART_Init(UART_HW, &UART_config, NULL);
    Cy_SCB_UART_Enable(UART_HW);
    cy_retarget_io_init(UART_HW);

    /* Checking Cause for Reset */
    uint32_t cause = Cy_SysLib_GetResetReason();

    /* Not after the software reset */
    if (cause != CY_SYSLIB_RESET_SOFT)
    {
        /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
        printf("\x1b[2J\x1b[;H");
        printf("********************************************************************************\r\n");
        printf("RAM Retention after External Reset example\r\n");
        printf("********************************************************************************\r\n");

        /* CLearing the Reset Cause status register */
        Cy_SysLib_ClearResetReason();

        /* Writing data into SRAM Controller 1 Address space */
        for (uint32_t addr = (uint32_t)SRAM_CONTROLLER1_START; addr < (uint32_t)SRAM_CONTROLLER1_END; addr += 4)
        {
            *(uint32_t*)addr = VALID_VAL;
        }
    }
    /* After the software reset */
    else
    {
        /* Setting SRAM in enabled mode */
        CPUSS->RAM1_PWR_CTL = PWR_MODE_ENABLED;

        /* Checking if the data written before reset is retained */
        for (uint32_t addr = (uint32_t)SRAM_CONTROLLER1_START; addr < (uint32_t)SRAM_CONTROLLER1_END; addr += 4)
        {
            data = *(uint32_t*)addr;
            if (data != VALID_VAL)
            {
                errCount++;
            }
        }

        /* If data is retained then errCount value will be 0 */
        if (errCount == 0)
        {
            printf("Value Retained after reset\r\n");
        }
        else
        {
            printf("Value after reset changed.\r\n");
        }
    }

    /* Initialize the user button */
    result = Cy_GPIO_Pin_Init(CYBSP_USER_BTN_PORT, CYBSP_USER_BTN_PIN, &CYBSP_USER_BTN_config);

    /* User button init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    Cy_GPIO_SetInterruptMask(CYBSP_USER_BTN_PORT, CYBSP_USER_BTN_PIN, 1);

    /* Configure GPIO interrupt */
    Cy_SysInt_Init(&gpio_irq_cfg, &Handle_GPIO_Interrupt);
    NVIC_ClearPendingIRQ(NvicMux3_IRQn);
    NVIC_EnableIRQ((IRQn_Type) NvicMux3_IRQn);

    for (;;)
    {
    }
}

/**********************************************************************************************************************
 * Function Name: Handle_GPIO_Interrupt
 * Summary:
 *  GPIO interrupt handler.
 * Parameters:
 *  void *handlerArg (unused)
 *  cyhal_gpio_event_t event (unused)
 * Return:
 *  void
 **********************************************************************************************************************
 */
void Handle_GPIO_Interrupt(void)
{
    /* Checking SRAM Controller 1 status before setting it to Retention Mode */
    while ((CPUSS->RAM1_STATUS & CPUSS_RAM1_STATUS_WB_EMPTY_Msk) == 0)
    {
        __NOP();
    }

    /* Enabling SRAM Retention mode */
    CPUSS->RAM1_PWR_CTL = PWR_MODE_RETAINED;

    printf("Triggering Software Reset \r\n");
    Cy_SysLib_Delay(LONG_DELAY_MS);

    /* Triggering SoftReset */
    __NVIC_SystemReset();
}

/* [] END OF FILE */
