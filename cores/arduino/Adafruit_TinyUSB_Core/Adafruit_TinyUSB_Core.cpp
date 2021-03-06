/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach for Adafruit
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifdef USE_TINYUSB

#include "Arduino.h"
#include "Adafruit_TinyUSB_Core.h"
#include <Reset.h> // Needed for auto-reset with 1200bps port touch

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

// Init usb hardware when starting up. Softdevice is not enabled yet
static void usb_hardware_init(void)
{
#ifdef PIN_LED_TXL
//	txLEDPulse = 0;
	pinMode(PIN_LED_TXL, OUTPUT);
	digitalWrite(PIN_LED_TXL, HIGH);
#endif

#ifdef PIN_LED_RXL
//	rxLEDPulse = 0;
	pinMode(PIN_LED_RXL, OUTPUT);
	digitalWrite(PIN_LED_RXL, HIGH);
#endif

	/* Enable USB clock */
#if defined(__SAMD51__)
	MCLK->APBBMASK.reg |= MCLK_APBBMASK_USB;
	MCLK->AHBMASK.reg |= MCLK_AHBMASK_USB;

	// Set up the USB DP/DN pins
	PORT->Group[0].PINCFG[PIN_PA24H_USB_DM].bit.PMUXEN = 1;
	PORT->Group[0].PMUX[PIN_PA24H_USB_DM/2].reg &= ~(0xF << (4 * (PIN_PA24H_USB_DM & 0x01u)));
	PORT->Group[0].PMUX[PIN_PA24H_USB_DM/2].reg |= MUX_PA24H_USB_DM << (4 * (PIN_PA24H_USB_DM & 0x01u));
	PORT->Group[0].PINCFG[PIN_PA25H_USB_DP].bit.PMUXEN = 1;
	PORT->Group[0].PMUX[PIN_PA25H_USB_DP/2].reg &= ~(0xF << (4 * (PIN_PA25H_USB_DP & 0x01u)));
	PORT->Group[0].PMUX[PIN_PA25H_USB_DP/2].reg |= MUX_PA25H_USB_DP << (4 * (PIN_PA25H_USB_DP & 0x01u));


	GCLK->PCHCTRL[USB_GCLK_ID].reg = GCLK_PCHCTRL_GEN_GCLK1_Val | (1 << GCLK_PCHCTRL_CHEN_Pos);
#else
	PM->APBBMASK.reg |= PM_APBBMASK_USB;

	// Set up the USB DP/DN pins
	PORT->Group[0].PINCFG[PIN_PA24G_USB_DM].bit.PMUXEN = 1;
	PORT->Group[0].PMUX[PIN_PA24G_USB_DM/2].reg &= ~(0xF << (4 * (PIN_PA24G_USB_DM & 0x01u)));
	PORT->Group[0].PMUX[PIN_PA24G_USB_DM/2].reg |= MUX_PA24G_USB_DM << (4 * (PIN_PA24G_USB_DM & 0x01u));
	PORT->Group[0].PINCFG[PIN_PA25G_USB_DP].bit.PMUXEN = 1;
	PORT->Group[0].PMUX[PIN_PA25G_USB_DP/2].reg &= ~(0xF << (4 * (PIN_PA25G_USB_DP & 0x01u)));
	PORT->Group[0].PMUX[PIN_PA25G_USB_DP/2].reg |= MUX_PA25G_USB_DP << (4 * (PIN_PA25G_USB_DP & 0x01u));

	// Put Generic Clock Generator 0 as source for Generic Clock Multiplexer 6 (USB reference)
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(6)     | // Generic Clock Multiplexer 6
	GCLK_CLKCTRL_GEN_GCLK0 | // Generic Clock Generator 0 is source
	GCLK_CLKCTRL_CLKEN;
	while (GCLK->STATUS.bit.SYNCBUSY)
	;
#endif
}

uint8_t load_serial_number(uint16_t* serial_str)
{
  enum { SERIAL_BYTE_LEN = 16 };

#ifdef __SAMD51__
  uint32_t* id_addresses[4] = {(uint32_t *) 0x008061FC, (uint32_t *) 0x00806010,
                               (uint32_t *) 0x00806014, (uint32_t *) 0x00806018};
#else // samd21
  uint32_t* id_addresses[4] = {(uint32_t *) 0x0080A00C, (uint32_t *) 0x0080A040,
                               (uint32_t *) 0x0080A044, (uint32_t *) 0x0080A048};

#endif

  uint8_t raw_id[SERIAL_BYTE_LEN];

  for (int i=0; i<4; i++) {
      for (int k=0; k<4; k++) {
          raw_id[4 * i + k] = (*(id_addresses[i]) >> k * 8) & 0xff;
      }
  }

  static const char nibble_to_hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

  for (int i = 0; i < sizeof(raw_id); i++) {
    for (int j = 0; j < 2; j++) {
      uint8_t nibble = (raw_id[i] >> (j * 4)) & 0xf;
      // Strings are UTF-16-LE encoded.
      serial_str[i * 2 + j] = nibble_to_hex[nibble];
    }
  }

  return sizeof(raw_id)*2;
}

void Adafruit_TinyUSB_Core_init(void)
{
  USBDevice.addInterface( (Adafruit_USBD_Interface&) Serial);
  USBDevice.setID(USB_VID, USB_PID);
  USBDevice.begin();

  usb_hardware_init();

  // Init tinyusb stack
  tusb_init();
}

void Adafruit_TinyUSB_Core_touch1200(void)
{
  initiateReset(250);
}


extern  "C"
{

void yield(void)
{
  tud_task();
  tud_cdc_write_flush();
}

}

#endif // USE_TINYUSB
