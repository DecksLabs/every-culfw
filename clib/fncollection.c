#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>

#include "board.h"
#include "display.h"
#include "delay.h"
#include "fncollection.h"
#include "cc1100.h"
#include "../version.h"
#include "clock.h"
#include <avr/wdt.h>

uint8_t led_mode = 2;   // Start blinking

//////////////////////////////////////////////////
// EEprom

// eeprom_write_byte is inlined and it is too big
__attribute__((__noinline__)) 
void
ewb(uint8_t *p, uint8_t v)
{
  eeprom_write_byte(p, v);
  eeprom_busy_wait();
}

// eeprom_read_byte is inlined and it is too big
__attribute__((__noinline__)) 
uint8_t
erb(uint8_t *p)
{
  return eeprom_read_byte(p);
}

static void
display_ee_bytes(uint8_t *a, uint8_t cnt)
{
  while(cnt--) {
    DH2(erb(a++));
    if(cnt)
      DC(':');
  }

}

static void
display_ee_mac(uint8_t *a)
{
  display_ee_bytes( a, 6 );
}

void
read_eeprom(char *in)
{
  uint8_t hb[2], d;
  uint16_t addr;

  if(in[1] == 'M') { display_ee_mac(EE_DUDETTE_MAC); }  
  else if(in[1] == 'P') { display_ee_bytes(EE_DUDETTE_PUBL, 16); }  
  else {
    hb[0] = hb[1] = 0;
    d = fromhex(in+1, hb, 2);
    if(d == 2)
      addr = (hb[0] << 8) | hb[1];
    else
      addr = hb[0];

    d = erb((uint8_t *)addr);
    DC('R');                    // prefix
    DH(addr,4);                 // register number
    DS_P( PSTR(" = ") );
    DH2(d);                    // result, hex
    DS_P( PSTR(" / ") );
    DU(d,2);                    // result, decimal
  }
  DNL();
}

void
write_eeprom(char *in)
{
  uint8_t hb[6], d = 0;

	uint16_t addr;
	d = fromhex(in+1, hb, 3);
	if(d < 2)
	  return;
	if(d == 2)
	  addr = hb[0];
	else
	  addr = (hb[0] << 8) | hb[1];
	  
	ewb((uint8_t*)addr, hb[d-1]);

	// If there are still bytes left, then write them too
	in += (2*d+1);
	while(in[0]) {
	  if(!fromhex(in, hb, 1))
		return;
	  ewb((uint8_t*)++addr, hb[0]);
	  in += 2;
	}
}

void
eeprom_init(void)
{
  if(erb(EE_MAGIC_OFFSET)   != VERSION_1 ||
     erb(EE_MAGIC_OFFSET+1) != VERSION_2)
       eeprom_factory_reset(0);

  led_mode = erb(EE_LED);
}

void
eeprom_factory_reset(char *in)
{

  ewb(EE_MAGIC_OFFSET  , VERSION_1);
  ewb(EE_MAGIC_OFFSET+1, VERSION_2);

  cc_factory_reset();

  ewb(EE_RF_ROUTER_ID, 0);
  ewb(EE_RF_ROUTER_ROUTER, 0);
  ewb(EE_REQBL, 0);
  ewb(EE_LED, 2);

  if(in[1] != 'x')
    prepare_boot(0);
}

// LED
void
ledfunc(char *in)
{
  fromhex(in+1, &led_mode, 1);
  if(led_mode & 1)
    LED_ON();
  else
    LED_OFF();

  ewb(EE_LED, led_mode);
}

//////////////////////////////////////////////////
// boot
void
prepare_boot(char *in)
{
  uint8_t bl = 0;
  if(in)
    fromhex(in+1, &bl, 1);

  if(bl == 0xff)             // Allow testing
    while(1);
    
  if(bl)                     // Next reboot we'd like to jump to the bootloader.
    ewb( EE_REQBL, 1 );      // Simply jumping to the bootloader from here
                             // wont't work. Neither helps to shutdown USB
                             // first.


  TCA0.SINGLE.CTRLA &= ~TCA_SINGLE_ENABLE_bm; //TIMSK0 = 0;                // Disable the clock which resets the watchdog
  cli();
  
  wdt_enable(WDTO_15MS);       // Make sure the watchdog is running 
  while (1);                 // go to bed, the wathchdog will take us to reset
}

void
version(char *in)
{
  DS_P( PSTR("V " VERSION " " BOARD_ID_STR) );
  DNL();
}

void
dumpmem(uint8_t *addr, uint16_t len)
{
  for(uint16_t i = 0; i < len; i += 16) {
    uint8_t l = len;
    if(l > 16)
      l = 16;
    DH(i,4);
    DC(':'); DC(' ');
    for(uint8_t j = 0; j < l; j++) {
      DH2(addr[j]);
      if(j&1)
        DC(' ');
    }
    DC(' ');
    for(uint8_t j = 0; j < l; j++) {
      if(addr[j] >= ' ' && addr[j] <= '~')
        DC(addr[j]);
      else
        DC('.');
    }
    addr += 16;
    DNL();
  }
  DNL();
}
