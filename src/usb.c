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

#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#include "usb.h"

#ifdef LED_GPIO
#define LED_MASK (1 << LED_GPIO)
#define TOGGLE_LED() do { P0 ^= LED_MASK; } while (0)
#else
#define TOGGLE_LED()
#endif

#define JUMP_TO(x) ((void (*)())(x))()

#define DFU_ADDR_FLASH      FLASH_START
#define DFU_ADDR_BOOT       BOOT_START
#define DFU_ADDR_TRAMPOLINE (BOOT_START - 3)

#define FLASH_PAGE_MASK 0x1ff

#define FLASH_READ(addr) (*((__code uint8_t *) addr))

#define x2icpy_asm(dst, src, len) do { \
  __asm__("mov r0, #" #dst); \
  __asm__("mov dptr, #" #src); \
  __asm__("mov r1, #" #len); \
  __asm__("001$:"); \
  __asm__("movx a, @dptr"); \
  __asm__("mov @r0, a"); \
  __asm__("inc dptr"); \
  __asm__("inc r0"); \
  __asm__("djnz r1, 001$"); \
} while (0)

// copy of USB request setup buffer
const union setup_request __data request;

enum flash_state {
  FLASH_IDLE = 0,
  FLASH_UNLOCKED,
  FLASH_ERASE_PAGE,
  FLASH_PROGRAM,
};

static __data enum flash_state flash_state;

static __data enum dfu_state_t dfu_state;
static __data enum dfu_status_t dfu_status;
static __data uint16_t dfu_addr;
static __data uint16_t dfu_addr_end;
static __data uint8_t  dfu_ljmp_target[2];
static __data uint8_t  dfu_data_len;
static __data uint8_t  dfu_data_written;
static __data uint8_t  dfu_altsetting;

// USB configured state
static __data uint8_t configured;
static __bit wait_for_timeout;
static __data uint8_t timeout;

#define CONFIG_REG(reg, val) ((uint16_t)&reg - USB_REG_BASE), val

__code uint8_t usb_reg_data[] = {
	// Setup interrupts
	CONFIG_REG(usbien, 0x11),  // USB reset and setup data valid
	CONFIG_REG(in_ien, 0x00),  // Disable EP IN interrupts
	CONFIG_REG(out_ien, 0x01), // Enable EP0 OUT interrupts
	CONFIG_REG(in_irq, 0x1F),  // Clear IN IRQ flags
	CONFIG_REG(out_irq, 0x1F), // Clear OUT IRQ flags
	// Disable bulk EPs, disable ISO EPs
	CONFIG_REG(inbulkval, 0x01),
	CONFIG_REG(outbulkval, 0x01),
	CONFIG_REG(inisoval, 0x00),
	CONFIG_REG(outisoval, 0x00),
	// Setup EP buffers
	CONFIG_REG(bout1addr, 32),
	CONFIG_REG(bout2addr, 64),
	CONFIG_REG(binstaddr, 16),
	CONFIG_REG(bin1addr, 32),
	CONFIG_REG(bin2addr, 64),
};

// Reset the USB configuration
void usb_reset_config()
{
  __code uint8_t *config = usb_reg_data;
  while (*config) {
    __xdata uint8_t *reg = (__xdata uint8_t*)(USB_REG_BASE + *(config++));
    *reg = *(config++);
  }

  ien1 = 0x10;    // Enable USB interrupt

  flash_state = FLASH_IDLE;
  dfu_state = DFU_STATE_IDLE;
  dfu_status = DFU_OK;
  dfu_altsetting = 0;
}

void usb_tick()
{
  if (configured && wait_for_timeout) {
    TOGGLE_LED();
    if (timeout > 0 && --timeout == 0) {
      /* Disable T2 */
      T2CON = 0x00;
      /* Force USB disconnect */
      usbcs |= 0x08;
      delay_us(50000);
      /* Disable USB clock */
      USBSLP = 1;
      /* Clear DPS flag */
      __asm__("clr 0x92");
      /* Clear any pending interrupts */
      IRCON = 0x00;
      JUMP_TO(DFU_ADDR_TRAMPOLINE);
    }
  }
}

// Initialize the USB configuraiton
void usb_init()
{
  configured = false;
  wait_for_timeout = true;
  timeout = 50;

  // Wakeup USB
  usbcon = 0x40;

  // Reset the USB bus
  usbcs |= 0x08;
  delay_us(50000);
  usbcs &= ~0x08;

  // Set the default configuration
  usb_reset_config();
}

// Convert a device string to unicode, and write it to EP0
static void write_device_string(const __code char * string)
{
  uint8_t length = 2;
  uint8_t __xdata *buf = &in0buf[2];
  for(; *string;) {
    *(buf++) = *(string++);
    *(buf++) = 0;
    length += 2;
  }
  in0buf[0] = length;
  in0buf[1] = STRING_DESCRIPTOR;
  in0bc = length;
}


static void cmemcpy(__xdata void *dst, __code const void *src, uint8_t len)
{
  __xdata uint8_t *d = dst;
  __code const uint8_t *s = src;
  while (len--) {
    *d = *s;
    d++;
    s++;
  }
}

// Write a descriptor (as specified in the current request) to EP0
bool write_descriptor()
{
  uint16_t desc_len = request.desc.wLength;

  switch(request.desc.bType)
  {
    // Device descriptor request
    case DEVICE_DESCRIPTOR:
      if(desc_len > device_descriptor.bLength) desc_len = device_descriptor.bLength;
      cmemcpy(in0buf, &device_descriptor, desc_len);
      in0bc = desc_len;
      return true;

    // Configuration descriptor request
    case CONFIGURATION_DESCRIPTOR:
      if(desc_len > configuration_descriptor.wTotalLength) desc_len = configuration_descriptor.wTotalLength;
      cmemcpy(in0buf, &configuration_descriptor, desc_len);
      in0bc = desc_len;
      return true;

    // String descriptor request
    // - Language, Manufacturer, or Product
    case STRING_DESCRIPTOR: {
      if (request.desc.bIndex == 0) {
        // Language special case.
        in0buf[0] = 4;  // Length
        in0buf[1] = STRING_DESCRIPTOR;
        in0buf[2] = 9;
        in0buf[3] = 4;
        in0bc = in0buf[0];
      } else {
        write_device_string(device_strings[request.desc.bIndex - 1]);
      }
      return true;
    }
  }

  // Not handled
  return false;
}

void handle_ep0_data()
{
  if (dfu_state == DFU_STATE_DNLOAD_IDLE) {
    dfu_state = DFU_STATE_DNLOAD_BUSY;
    dfu_data_len = out0bc;
    dfu_data_written = 0;
    if (dfu_addr == DFU_ADDR_FLASH) {
      /* Check that first opcode is ljmp */
      if (dfu_data_len < 3
          || out0buf[0] != 0x02) {
        dfu_state = DFU_STATE_ERROR;
        dfu_status = DFU_FILE;
      } else {
        /* Patch ljmp address to jump to bootloader */
        dfu_ljmp_target[0]= out0buf[1];
        dfu_ljmp_target[1]= out0buf[2];
        out0buf[1] = DFU_ADDR_BOOT >> 8;
        out0buf[2] = DFU_ADDR_BOOT & 0xff;
      }
    }
  } else {
    dfu_state = DFU_STATE_ERROR;
  }
}

#define flash_unlock() do {FCR = 0xAA; FCR = 0x55; WEN = 1;} while (0)
#define flash_lock() (WEN = 0)
#define flash_erase_page(page) (FCR = page)
#define flash_write_byte(addr, val) (*((__xdata uint8_t *)addr) = val)

void usb_flash_ready()
{
  if (dfu_state == DFU_STATE_DNLOAD_BUSY) {
    switch (flash_state) {
    case FLASH_IDLE:
      flash_unlock();
      flash_state = FLASH_UNLOCKED;
      break;
    case FLASH_UNLOCKED:
      if (dfu_data_len <= dfu_data_written) {
        flash_lock();
        flash_state = FLASH_IDLE;
        dfu_state = DFU_STATE_DNLOAD_IDLE;
        return;
      }
      if ((dfu_addr & FLASH_PAGE_MASK) == 0) {
        TOGGLE_LED();
        flash_state = FLASH_ERASE_PAGE;
        flash_erase_page(dfu_addr >> 9);
        while (RDYN == 1);
        return;
      }
      // Fall through
    case FLASH_ERASE_PAGE: {
        uint8_t data = out0buf[dfu_data_written];
        if (data != 0xff) {
          flash_write_byte(dfu_addr, out0buf[dfu_data_written]);
          while (RDYN == 1);
        }
        flash_state = FLASH_PROGRAM;
        return;
      }
    case FLASH_PROGRAM:
      flash_state = FLASH_UNLOCKED;
      dfu_addr++;
      dfu_data_written++;
      break;
    }
  } else if (dfu_state == DFU_STATE_MANIFEST) {
    flash_unlock();
    if (FLASH_READ(DFU_ADDR_TRAMPOLINE) != 0xff) {
      flash_erase_page(DFU_ADDR_TRAMPOLINE >> 9);
      while (RDYN == 1);
    }
    flash_write_byte(DFU_ADDR_TRAMPOLINE, 0x02);
    while (RDYN == 1);
    flash_write_byte(DFU_ADDR_TRAMPOLINE + 1, dfu_ljmp_target[0]);
    while (RDYN == 1);
    flash_write_byte(DFU_ADDR_TRAMPOLINE + 2, dfu_ljmp_target[1]);
    while (RDYN == 1);
    flash_lock();
    dfu_state = DFU_STATE_IDLE;
  }
}

// Handle a DFU setup request
bool handle_dfu_request()
{
  bool handled = true;
  wait_for_timeout = false;

  if (dfu_state == DFU_STATE_IDLE) {
    dfu_addr = flash_regions[dfu_altsetting].wStart;
    dfu_addr_end = flash_regions[dfu_altsetting].wEnd;
  }

  switch (request.setup.bRequest) {
  case DFU_GETSTATUS:
    in0buf[0] = dfu_status;
    // Poll timeout in ms.
    in0buf[1] = 30;
    in0buf[2] = 0;
    in0buf[3] = 0;
    in0buf[4] = dfu_state;
    in0buf[5] = 0;
    in0bc = 6;
    break;
  case DFU_CLRSTATUS:
  case DFU_ABORT:
    dfu_status = DFU_OK;
    dfu_state = DFU_STATE_IDLE;
    break;
  case DFU_UPLOAD:
    if (dfu_state == DFU_STATE_IDLE ||
        dfu_state == DFU_STATE_UPLOAD_IDLE) {
      int cnt = request.setup.wLength < 64 ? request.setup.wLength : 64;
      int i;
      TOGGLE_LED();
      dfu_state = DFU_STATE_UPLOAD_IDLE;
      for (i = 0; i < cnt && dfu_addr < dfu_addr_end; i++) {
        in0buf[i] = FLASH_READ(dfu_addr++);
      }
      in0bc = i;
      if (i == 0) {
        dfu_state = DFU_STATE_IDLE;
      }
    } else {
      handled = false;
    }
    break;
  case DFU_DNLOAD:
    if (dfu_state == DFU_STATE_IDLE ||
         dfu_state == DFU_STATE_DNLOAD_IDLE) {
      if (dfu_state == DFU_STATE_DNLOAD_IDLE && request.setup.wLength == 0) {
        if (dfu_altsetting == 0) {
          /* Still have to write the trampoline jump to flash */
          dfu_state = DFU_STATE_MANIFEST;
        } else {
          dfu_state = DFU_STATE_IDLE;
        }
      } else if (dfu_addr < dfu_addr_end) {
        dfu_state = DFU_STATE_DNLOAD_IDLE;
        out0bc = 0xff;  // Expect OUT data
      } else {
        handled = false;
      }
    } else {
      handled = false;
    }
    break;
  default:
    handled = false;
    break;
  }
  if (!handled) {
    dfu_status = DFU_STALLED_PKT;
    dfu_state = DFU_STATE_ERROR;
  }
  return handled;
}

// Handle a USB setup request
void handle_setup_request()
{
  bool handled = false;
  if ((request.setup.bmRequestType & 0x7f)== 0x21) {
    handled = handle_dfu_request();
  } else {
    switch(request.setup.bRequest)
    {
      // Return a descriptor
      case GET_DESCRIPTOR:
        if(write_descriptor()) handled = true;
        break;

      // The host has assigned an address, but we don't have to do anything
      case SET_ADDRESS:
        handled = true;
        break;

      // Set the configuration state
      case SET_CONFIGURATION:
        if (request.setup.wValue == 0) configured = false; // Not configured, drop back to powered state
        else configured = true;                       // Configured
        handled = true;
        break;

      // Get the configuration state
      case GET_CONFIGURATION:
        in0buf[0] = configured;
        in0bc = 1;
        handled = true;
        break;

      case SET_INTERFACE:
        if (request.setup.wValue < ARRAY_SIZE(flash_regions)) {
          dfu_altsetting = request.setup.wValue;
          handled = true;
        }
        break;

      // Get endpoint or interface status
      case GET_STATUS:

        // Device / Interface status, always two 0 bytes because
        // we're bus powered and don't support remote wakeup
        in0buf[0] = 0;
        in0buf[1] = 0;
        in0bc = 2;
        handled = true;
        break;
    }
  }

  // Stall if the request wasn't handled
  if(handled) ep0cs = 0x02; // hsnak
  else ep0cs = 0x11; // ep0stall
}

// USB IRQ handler
void usb_irq()
{
  // Which IRQ?
  // ref: nRF24LU1+ Product Spec, Section 7.8.3, Table 34
  switch (ivec)
  {
    // Setup data available
    case 0x00:
      x2icpy_asm(_request, 0xc7e8, 8);
      handle_setup_request();
      usbirq = 0x01;
      break;

    // Reset to initial state
    case 0x10:
      usb_reset_config();
      usbirq = 0x10;
      break;

    // EP0 OUT
    case 0x1c:
      handle_ep0_data();
      out_irq = 0x01;
      out0bc = 0xff;
      ep0cs = 0x02; // hsnak
      break;

    // Ack any spurious interrupt
    default:
      usbirq = 0x0e;
      break;
  }
}
