#include "board.h"
#include "display.h"
#include "ringbuffer.h"
#include "serial.h"
#include "led.h"
#include "delay.h"
#include "ttydata.h"            // callfn
#include "clock.h"

uint8_t log_enabled = 0;
uint8_t display_channel = 0;

//////////////////////////////////////////////////
// Display routines
void
display_char(char data)
{
# define buffer_free 1
# define buffer_used()


#ifdef HAS_UART
  if(display_channel & DISPLAY_USB) {
    if((TTY_Tx_Buffer.nbytes  < TTY_BUFSIZE-2) ||
       (TTY_Tx_Buffer.nbytes  < TTY_BUFSIZE && (data == '\r' || data == '\n')))
    rb_put(&TTY_Tx_Buffer, data);
  }
#endif
}

void
display_string(char *s)
{
  while(*s)
    display_char(*s++);
}

void
display_string_P(const char *s)
{
  uint8_t c;
  while((c = __LPM(s))) {
    display_char(c);
    s++;
  }
}

void
display_nl()
{
  display_char('\r');
  display_char('\n');
}

void
display_udec(uint16_t d, int8_t pad, uint8_t padc)
{
  char buf[6];
  uint8_t i=6;

  buf[--i] = 0;
  do {
    buf[--i] = d%10 + '0';
    d /= 10;
    pad--;
  } while(d && i);

  while(--pad >= 0 && i > 0)
    buf[--i] = padc;
  DS(buf+i);
}

void
display_hex(uint16_t h, int8_t pad, uint8_t padc)
{
  char buf[5];
  uint8_t i=5;

  buf[--i] = 0;
  do {
    uint8_t m = h%16;
    buf[--i] = (m < 10 ? '0'+m : 'A'+m-10);
    h /= 16;
    pad--;
  } while(h);

  while(--pad >= 0 && i > 0)
    buf[--i] = padc;
  DS(buf+i);
}

void
display_hex2(uint8_t h)
{
  display_hex(h, 2, '0');
}
