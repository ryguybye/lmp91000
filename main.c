/***************************************************************************//**
 * @file main.c
 * @brief main() function.
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "em_cmu.h"
#include "em_gpio.h"
#include "em_usart.h"

#include "sl_hal_sysrtc.h"

// SYSRTC runs at 32.768 kHz
#define RTC_FREQ 32768

// UART buffer size
#define RX_BUFFER_SIZE 32

static uint32_t unix_epoch_offset = 0;

void uart_init(void)
{
  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(cmuClock_USART0, true);

  // Configure pins
  GPIO_PinModeSet(gpioPortA, 5, gpioModePushPull, 1); // TX
  GPIO_PinModeSet(gpioPortA, 6, gpioModeInput, 0);    // RX
  GPIO_PinModeSet(gpioPortA, 0, gpioModePushPull, 1);

  USART_InitAsync_TypeDef init = USART_INITASYNC_DEFAULT;
  init.baudrate = 115200;

  USART_InitAsync(USART0, &init);

  // Series 2 routing
  GPIO->USARTROUTE[0].TXROUTE =
      (gpioPortA << _GPIO_USART_TXROUTE_PORT_SHIFT)
    | (5 << _GPIO_USART_TXROUTE_PIN_SHIFT);

  GPIO->USARTROUTE[0].RXROUTE =
      (gpioPortA << _GPIO_USART_RXROUTE_PORT_SHIFT)
    | (6 << _GPIO_USART_RXROUTE_PIN_SHIFT);

  GPIO->USARTROUTE[0].ROUTEEN =
      GPIO_USART_ROUTEEN_TXPEN |
      GPIO_USART_ROUTEEN_RXPEN;
}

void uart_send_string(const char *str)
{
  while (*str) {
    USART_Tx(USART0, *str++);
  }
}

int uart_read_line(char *buffer, int max_len)
{
  int i = 0;

  while (i < max_len - 1) {
    char c = USART_Rx(USART0);

    if (c == '\r' || c == '\n') break;

    buffer[i++] = c;
  }

  buffer[i] = '\0';
  return i;
}

void sysrtc_init(void)
{
  CMU_ClockEnable(cmuClock_SYSRTC, true);
  // Minimal init: many HAL versions auto-configure via slcp
  // If your SDK requires a struct, tell me and I’ll match it exactly
  sl_hal_sysrtc_init_t init = SL_HAL_SYSRTC_INIT_DEFAULT;
    sl_hal_sysrtc_init(&init);

    // Start the counter
    sl_hal_sysrtc_enable();
}

void set_rtc_time(uint32_t pc_timestamp)
{
  uint32_t rtc_count = SYSRTC0->CNT;
  uint32_t rtc_seconds = rtc_count / RTC_FREQ;

  unix_epoch_offset = pc_timestamp - rtc_seconds;
}

void print_current_time(void)
{
  uint32_t rtc_count = SYSRTC0->CNT;
  uint32_t rtc_seconds = rtc_count / RTC_FREQ;

  time_t current_time = (time_t)(unix_epoch_offset + rtc_seconds);
  struct tm *time_info = gmtime(&current_time);

  char out[80];

  sprintf(out,
          "RTC Time: %04d-%02d-%02d %02d:%02d:%02d UTC\r\n",
          time_info->tm_year + 1900,
          time_info->tm_mon + 1,
          time_info->tm_mday,
          time_info->tm_hour,
          time_info->tm_min,
          time_info->tm_sec);

  uart_send_string(out);
}


int main(void)
{
  uart_init();
  sysrtc_init();

  char buffer[RX_BUFFER_SIZE];

  while (1)
  {
    uart_send_string("Enter Unix Timestamp:\r\n");

    int len = uart_read_line(buffer, RX_BUFFER_SIZE);

    if (len > 0)
    {
      uint32_t received = (uint32_t)strtoul(buffer, NULL, 10);

      if (received > 0)
      {
        set_rtc_time(received);
        uart_send_string("Time Synced!\r\n");
      }
    }

    print_current_time();

    // crude delay loop (replace with timer if needed)
    for (volatile uint32_t i = 0; i < 5000000; i++);
  }
}
