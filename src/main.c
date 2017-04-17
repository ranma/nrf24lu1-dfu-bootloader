/*
  Copyright (C) 2016 Bastille Networks
  Copyright (C) 2017 Tobias Diedrich

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "usb.h"

// Program entry point
void setup()
{
  // Disable interrupts
  ien0 = 0x00;
  // CKCON = 0x02;  // USB errata, see nRF24LU1p AX PAN
  // T2 free-running with cpuclk/24
  T2CON = 0x81;
  // Set tick divider for watchdog
  TICKDV = 0xFF;

#ifdef LED_GPIO
#define LED_MASK (1 << LED_GPIO)
  P0DIR = (uint8_t) ~LED_MASK;
  P0 = LED_MASK;
#endif

  // Initialize and connect the USB controller
  usb_init();
}

void loop()
{
  // Tickle the watchdog, should be good for ~500s with above TICKDV.
  REGXH = 0xFF;
  REGXL = 0xFF;
  REGXC = 0x08;

  // Poll the usb interrupt
  if (USBIRQ) {
    USBIRQ = 0;
    usb_irq();
  }
  // Poll timer2
  if (TF2) {
    TF2 = 0;
    usb_tick();
  }
  if (RDYN == 0) {
    usb_flash_ready();
  }
}
