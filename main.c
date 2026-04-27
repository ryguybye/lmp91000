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
#include <time.h>
#include <stdlib.h>
#include <string.h>

//#include "sl_system_init.h"
#include "sl_sleeptimer.h"
#include "sl_iostream.h"
#include "sl_iostream_handles.h"

// Global offset to store the Unix Epoch baseline
static uint32_t unix_epoch_offset = 0;

//Update the internal clock using a Unix timestamp received from the PC.
void set_rtc_time(uint32_t pc_timestamp) {
  uint32_t current_ticks = sl_sleeptimer_get_tick_count();
  uint32_t uptime_seconds;
  
  // Convert ticks to seconds (handling frequency scaling automatically)
  uptime_seconds = sl_sleeptimer_tick_to_ms(current_ticks) / 1000;

  // Calculate the offset between uptime and real-world time
  unix_epoch_offset = pc_timestamp - uptime_seconds;
}

//Gets current time and prints it to the serial console.
void print_current_time(void) {
  uint32_t current_ticks = sl_sleeptimer_get_tick_count();
  uint32_t uptime_seconds;
 uptime_seconds = sl_sleeptimer_tick_to_ms(current_ticks) / 1000;

  time_t current_time = (time_t)(unix_epoch_offset + uptime_seconds);
  struct tm *time_info = gmtime(&current_time);

  // Send formatted time back to the computer
  printf("RTC Time: %04d-%02d-%02d %02d:%02d:%02d UTC\r\n",
         time_info->tm_year + 1900, time_info->tm_mon + 1, time_info->tm_mday,
         time_info->tm_hour, time_info->tm_min, time_info->tm_sec);
}

int main(void) {
 // sl_system_init();

  char input_buffer[16];
  size_t bytes_read;

  while (1) {
    // Check for incoming timestamp from PC
    sl_iostream_read(sl_iostream_vcom_handle, input_buffer, sizeof(input_buffer) - 1, &bytes_read);
    
    if (bytes_read > 0) {
      input_buffer[bytes_read] = '\0';
      uint32_t received_val = (uint32_t)strtoul(input_buffer, NULL, 10);
      if (received_val > 0) {
        set_rtc_time(received_val);
        printf("Time Synced!\r\n");
      }
    }

    //Report time every few seconds
    print_current_time();
    sl_sleeptimer_delay_millisecond(2000);
  }
}
