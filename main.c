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
#include <stdint.h>

#include "em_cmu.h"
#include "em_gpio.h"
#include "em_usart.h"
#include "em_device.h"

#define RTC_FREQ 32768
#define RX_BUFFER_SIZE 64

static uint32_t unix_epoch_offset = 0;

/* ================= UART ================= */

void uart_init(void)
{
  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(cmuClock_USART0, true);

  GPIO_PinModeSet(gpioPortA, 5, gpioModePushPull, 1);
  GPIO_PinModeSet(gpioPortA, 6, gpioModeInput, 0);

  USART_InitAsync_TypeDef init = USART_INITASYNC_DEFAULT;
  init.baudrate = 115200;

  USART_InitAsync(USART0, &init);

  GPIO->USARTROUTE[0].TXROUTE =
      (gpioPortA << _GPIO_USART_TXROUTE_PORT_SHIFT) |
      (5 << _GPIO_USART_TXROUTE_PIN_SHIFT);

  GPIO->USARTROUTE[0].RXROUTE =
      (gpioPortA << _GPIO_USART_RXROUTE_PORT_SHIFT) |
      (6 << _GPIO_USART_RXROUTE_PIN_SHIFT);

  GPIO->USARTROUTE[0].ROUTEEN =
      GPIO_USART_ROUTEEN_TXPEN |
      GPIO_USART_ROUTEEN_RXPEN;
}

void uart_send_string(const char *str)
{
  while (*str) USART_Tx(USART0, *str++);
}

int uart_read_line(char *buffer, int max_len)
{
  int i = 0;
  char c;

  while (i < max_len - 1)
  {
    c = USART_Rx(USART0);

    if (c == '\r' || c == '\n') break;

    buffer[i++] = c;
  }

  buffer[i] = '\0';
  return i;
}

/* ================= RTC (NO time.h) ================= */

/*
 * Simple leap-year aware conversion (Gregorian calendar)
 * Converts date to Unix epoch seconds
 */
static int is_leap(int y)
{
  return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int days_in_month(int m, int y)
{
  static const int days[] =
    {31,28,31,30,31,30,31,31,30,31,30,31};

  if (m == 2) return days[m-1] + is_leap(y);
  return days[m-1];
}

uint32_t datetime_to_epoch(int year, int month, int day,
                           int hour, int min, int sec)
{
  uint32_t days = 0;

  // years
  for (int y = 1970; y < year; y++)
    days += is_leap(y) ? 366 : 365;

  // months
  for (int m = 1; m < month; m++)
    days += days_in_month(m, year);

  // days
  days += (day - 1);

  return days * 86400 + hour * 3600 + min * 60 + sec;
}

/* ================= RTC CONTROL ================= */

void rtc_init(void)
{
  CMU_ClockEnable(cmuClock_SYSRTC, true);

   // Reset counter to known value
   SYSRTC0->CNT = 0;
}

void rtc_set_time(uint32_t epoch)
{
  unix_epoch_offset = epoch;

  SYSRTC0->CNT = 0;
}

uint32_t rtc_get_time(void)
{
  // Current Time = (The date you entered) + (seconds elapsed since then)
  return unix_epoch_offset + (SYSRTC0->CNT / RTC_FREQ);
}

/* ================= UPDATED PRINT ================= */

void print_time(void)
{
  uint32_t total_seconds = rtc_get_time();

  if (unix_epoch_offset == 0) {
    uart_send_string("Clock not set. Please enter date/time.\r\n");
    return;
  }

  int year = 1970;
  int month = 1;

  // 1. Calculate Year
  while (1) {
    uint32_t seconds_in_year = (is_leap(year) ? 366 : 365) * 86400UL;
    if (total_seconds < seconds_in_year) break;
    total_seconds -= seconds_in_year;
    year++;
  }

  // 2. Calculate Month
  for (month = 1; month <= 12; month++) {
    uint32_t seconds_in_month = days_in_month(month, year) * 86400UL;
    if (total_seconds < seconds_in_month) break;
    total_seconds -= seconds_in_month;
  }

  // 3. Calculate Days, Hours, Mins, Secs
  int day = (total_seconds / 86400UL) + 1;
  uint32_t remaining = total_seconds % 86400UL;

  uint32_t hour = remaining / 3600UL;
  uint32_t min  = (remaining % 3600UL) / 60UL;
  uint32_t sec  = remaining % 60UL;

  char buffer[100];
  sprintf(buffer, "Current Date: %04d-%02d-%02d | Time: %02lu:%02lu:%02lu UTC\r\n",
          year, month, day, hour, min, sec);

  uart_send_string(buffer);
}

/* ================= MAIN ================= */

int main(void)
{
  uart_init();
  rtc_init();

  char input[RX_BUFFER_SIZE];

  uart_send_string("Enter time: YYYY-MM-DD HH:MM:SS\r\n");

  while (1)
  {
    int len = uart_read_line(input, RX_BUFFER_SIZE);

    if (len > 0)
    {
      int y, m, d, h, mi, s;

      if (sscanf(input, "%d-%d-%d %d:%d:%d",
                 &y, &m, &d, &h, &mi, &s) == 6)
      {
        uint32_t epoch = datetime_to_epoch(y, m, d, h, mi, s);

        rtc_set_time(epoch);

        uart_send_string("RTC Updated!\r\n");
      }
      else
      {
        uart_send_string("Invalid format\r\n");
      }
    }

    print_time();

    for (volatile int i = 0; i < 5000000; i++);
  }
}
