/* Copyright Rudolf Koenig, 2008.
   Released under the GPL Licence, Version 2
   Inpired by the MyUSB USBtoSerial demo, Copyright (C) Dean Camera, 2008.
*/

//#include <avr/boot.h>
#include <avr/power.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>

#include <string.h>

#include "board.h"

#include "spi.h"
#include "cc1100.h"
#include "clock.h"
#include "delay.h"
#include "display.h"
#include "serial.h"
#include "fncollection.h"
#include "led.h"
#include "ringbuffer.h"
#include "rf_receive.h"
#include "rf_send.h"
#include "ttydata.h"

#include "rf_mbus.h"

const uint8_t mark433_pin = 0xff;

const PROGMEM t_fntab fntab[] = {
  { 'B', prepare_boot },
#ifdef HAS_MBUS
  { 'b', rf_mbus_func },
#endif
  { 'C', ccreg },
  { 'e', eeprom_factory_reset },
  { 'F', fs20send },
#ifdef HAS_RAWSEND
  { 'G', rawsend },
#endif
#ifdef HAS_RAWSEND
  { 'K', ks_send },
#endif
  { 'l', ledfunc },
#ifdef HAS_RAWSEND
  { 'M', em_send },
#endif
  { 'R', read_eeprom },
  { 't', gettime },
  { 'V', version },
  { 'W', write_eeprom },
  { 'X', set_txreport },
  { 'x', ccsetpa },
  { 0, 0 },
  { 0, 0 },
};

FUSES = {
    .WDTCFG     = FUSE_WDTCFG_DEFAULT,
    .BODCFG     = FUSE_BODCFG_DEFAULT,
    .OSCCFG     = FUSE_FREQSEL0,  // FREQSEL to 16MHz
    .SYSCFG0    = (FUSE_CRCSRC0 | FUSE_CRCSRC1 | FUSE_RSTPINCFG | FUSE_EESAVE),
    .SYSCFG1    = FUSE_SYSCFG1_DEFAULT,
    .APPEND     = FUSE_APPEND_DEFAULT,
    .BOOTEND    = FUSE_BOOTEND_DEFAULT,
};

int
main(void)
{
  wdt_reset();
  wdt_disable();

  _PROTECTED_WRITE(CLKCTRL.MCLKCTRLB, CLKCTRL_PDIV_2X_gc | CLKCTRL_PEN_bm);

  led_init();
  LED_ON();

  spi_init();

  //eeprom_factory_reset("xx");
  eeprom_init();

  // Setup the timers. Are needed for watchdog-reset
  //125Hz timer (0,008s)
  TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
  TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_NORMAL_gc;
  TCA0.SINGLE.EVCTRL &= ~(TCA_SINGLE_CNTEI_bm);
  TCA0.SINGLE.PER = 249;
  TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV256_gc | TCA_SINGLE_ENABLE_bm;

  //TCB0.CCMP = 4-1; // 8000000 / 2 (prescaler) / 4 (ccmp) = 0,000001s //when porting timer multiply it by 4 comparing to original project
  TCB0.CTRLB = TCB_CNTMODE_INT_gc; //periodic interrupt
  TCB0.INTCTRL = TCB_CAPT_bm; //enable interrupt
  TCB0.CTRLA = TCB_CLKSEL_CLKDIV2_gc;// | TCB_ENABLE_bm; //wasn't enabled here

  wdt_reset();
  wdt_enable(WDTO_2S);

  uart_init(UART_BAUD_RATE);

  tx_init();
  input_handle_func = analyze_ttydata;
  display_channel = DISPLAY_USB;
  LED_OFF();

  sei();

  for(;;) {
    uart_task();
    RfAnalyze_Task();
    Minute_Task();
    rf_mbus_task();
  }

}
