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


#include "nRF24LU1P.h"
#include "usb_desc.h"

extern void usb_init();
extern void usb_irq();
extern void usb_flash_ready();
extern void usb_tick();

// USB request
struct usb_request_t
{
  uint8_t bmRequestType;
  uint8_t bRequest;
  uint16_t wValue;
  uint16_t wIndex;
  uint16_t wLength;
};

struct descriptor_request_t
{
  uint8_t bmRequestType;
  uint8_t bRequest;
  uint8_t bIndex;
  uint8_t bType;
  uint16_t wLangId;
  uint16_t wLength;
};

union setup_request {
  struct usb_request_t setup;
  struct descriptor_request_t desc;
  char raw[8];
};

// USB request types
enum usb_request_type_t
{
  GET_STATUS = 0,
  SET_ADDRESS = 5,
  GET_DESCRIPTOR = 6,
  GET_CONFIGURATION = 8,
  SET_CONFIGURATION = 9,
  SET_INTERFACE = 11,
};

enum dfu_request_type_t
{
  DFU_DETACH = 0,
  DFU_DNLOAD = 1,
  DFU_UPLOAD = 2,
  DFU_GETSTATUS = 3,
  DFU_CLRSTATUS = 4,
  DFU_GETSTATE = 5,
  DFU_ABORT = 6,
};

enum dfu_state_t
{
  DFU_STATE_APPIDLE = 0,
  DFU_STATE_APPDETACH = 1,
  DFU_STATE_IDLE = 2,
  DFU_STATE_DNLOAD_SYNC = 3,
  DFU_STATE_DNLOAD_BUSY = 4,
  DFU_STATE_DNLOAD_IDLE = 5,
  DFU_STATE_MANIFEST_SYNC = 6,
  DFU_STATE_MANIFEST = 7,
  DFU_STATE_MANIFEST_WAITRESET = 8,
  DFU_STATE_UPLOAD_IDLE = 9,
  DFU_STATE_ERROR = 10,
};

enum dfu_status_t
{
  DFU_OK = 0,
  DFU_FILE = 2,          // File failed checks
  DFU_CHECK_ERASED = 5,  // Failed to erase
  DFU_VERIFY = 7,        // Failed to verify
  DFU_ADDRESS = 8,       // Address out of range
  DFU_STALLED_PKT = 15,  // Stalled an unexpected request
};

//Vendor control messages and commands
#define TRANSMIT_PAYLOAD               0x04
#define ENTER_SNIFFER_MODE             0x05
#define ENTER_PROMISCUOUS_MODE         0x06
#define ENTER_TONE_TEST_MODE           0x07
#define TRANSMIT_ACK_PAYLOAD           0x08
#define SET_CHANNEL                    0x09
#define GET_CHANNEL                    0x0A
#define ENABLE_LNA                     0x0B
#define TRANSMIT_PAYLOAD_GENERIC       0x0C
#define ENTER_PROMISCUOUS_MODE_GENERIC 0x0D
#define RECEIVE_PACKET                 0x12
#define LAUNCH_LOGITECH_BOOTLOADER     0xFE
#define LAUNCH_NORDIC_BOOTLOADER       0xFF

