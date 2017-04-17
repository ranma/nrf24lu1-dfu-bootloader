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


#include "usb_desc.h"

// Device descriptor
__code const device_descriptor_t device_descriptor =
{
  .bLength            = 18,     // Size of this struct
  .bDescriptorType    = DEVICE_DESCRIPTOR,
  .bcdUSB             = 0x0100, // USB 1.0
  .bDeviceClass       = 0x00,
  .bDeviceSubClass    = 0x00,
  .bDeviceProtocol    = 0x00,
  .bMaxPacketSize0    = 64,     // EP0 max packet size
  .idVendor           = 0x1915, // Nordic Semiconductor
  .idProduct          = 0x0102, // Nordic bootloader product ID incremebted by 1
  .bcdDevice          = 0x0001, // Device version number
  .iManufacturer      = STRING_DESCRIPTOR_MANUFACTURER,
  .iProduct           = STRING_DESCRIPTOR_PRODUCT,
  .iSerialNumber      = 0,
  .bNumConfigurations = 1,      // Configuration count
};

// Configuration descriptor
__code const configuration_descriptor_t configuration_descriptor =
{
  .bLength                = 9,     // Size of the configuration descriptor
  .bDescriptorType        = CONFIGURATION_DESCRIPTOR,
  .wTotalLength           = sizeof(configuration_descriptor),
  .bNumInterfaces         = 1,     // Interface count
  .bConfigurationValue    = 1,     // Configuration identifer
  .iConfiguration         = 0,
  .bmAttributes           = 0x80,  // Bus powered
  .bMaxPower              = 50,   // Max power of 50*2mA = 100mA
  .interface_flash =
    {
      .bLength            = 9,    // Size of the interface descriptor
      .bDescriptorType    = INTERFACE_DESCRIPTOR,
      .bInterfaceNumber   = 0,    // Interface index
      .bAlternateSetting  = 0,
      .bNumEndpoints      = 0,    // Only control EP0 is used.
      .bInterfaceClass    = 0xFE, // Application specific
      .bInterfaceSubClass = 0x01, // Device firmware upgrade
      .bInterfaceProtocol = 0x02, // DFU mode protocol
      .iInterface         = STRING_DESCRIPTOR_FLASH,
    },
  .interface_boot =
    {
      .bLength            = 9,    // Size of the interface descriptor
      .bDescriptorType    = INTERFACE_DESCRIPTOR,
      .bInterfaceNumber   = 0,    // Interface index
      .bAlternateSetting  = 1,
      .bNumEndpoints      = 0,    // Only control EP0 is used.
      .bInterfaceClass    = 0xFE, // Application specific
      .bInterfaceSubClass = 0x01, // Device firmware upgrade
      .bInterfaceProtocol = 0x02, // DFU mode protocol
      .iInterface         = STRING_DESCRIPTOR_BOOT,
    },
  .dfu_descriptor =
    {
      .bLength            = 9,
      .bDescriptorType    = DFU_FUNCTIONAL_DESCRIPTOR,
      .bmAttributes       = 0x7,
      .wDetachTimeOut     = 10000,
      .wTransferSize      = 64,
      .bcdDFUVersion      = 0x100,
    },
};

// String descriptor values
__code const char* const device_strings[4] =
{
  "ranma",          // Manufacturer
  "DFU boot",       // Product
  "flash",          // DFU interface
  "boot",           // DFU interface
};

__code const flash_region_t flash_regions[2] =
{
  {  // flash
    .wStart = FLASH_START,
    .wEnd   = FLASH_END,
  },
  {  // boot
    .wStart = BOOT_START,
    .wEnd   = BOOT_END,
  },
};
