#include <avr/io.h>
#include "util/delay_basic.h"
#include "delay.h"

void
my_delay_us( uint16_t d )
{
  d<<=1;                // 4 cycles/loop, we are running 8MHz
  _delay_loop_2(d);
}

void
my_delay_ms( uint8_t d )
{
  while(d--) {
    my_delay_us( 1000 );
  }
}
