#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>

#include "board.h"
#include "led.h"
#include "fncollection.h"
#include "clock.h"
#include "display.h"
#include "rf_send.h"                   // credit_10ms

volatile uint32_t ticks;
volatile uint8_t  clock_hsec;

// count & compute in the interrupt, else long runnning tasks would block
// a "minute" task too long
ISR(TCA0_OVF_vect, ISR_BLOCK)
{
  TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
  
  // 125Hz
  ticks++; 
  clock_hsec++;
}

void
get_timestamp(uint32_t *ts)
{
  uint8_t l = SREG;
  cli(); 
  *ts = ticks;
  SREG = l;
}

void
gettime(char *unused)
{
  uint32_t actticks;
  get_timestamp(&actticks);
  uint8_t *p = (uint8_t *)&actticks;
  DH2(p[3]);
  DH2(p[2]);
  DH2(p[1]);
  DH2(p[0]);
  DNL();
}

void
Minute_Task(void)
{
  static uint8_t last_tick;
  if((uint8_t)ticks == last_tick)
    return;
  last_tick = (uint8_t)ticks;
  wdt_reset();


  if(clock_hsec<125)
    return;
  clock_hsec = 0;       // once per second from here on.

  if (credit_10ms < MAX_CREDIT) // 10ms/1s == 1% -> allowed talk-time without CD
    credit_10ms += 1;

  static uint8_t clock_sec;
  clock_sec++;
if(clock_sec != 60)       // once per minute from here on
    return;
  clock_sec = 0;
}
