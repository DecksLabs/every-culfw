#include <avr/io.h>

void led_init(void)
{
	PORTC.DIR |= PIN6_bm;
}

void LED_ON(void)
{
	PORTC.OUT |= PIN6_bm;
}

void LED_OFF(void)
{
	PORTC.OUT &= ~PIN6_bm;
}

void LED_TOGGLE(void)
{
	PORTC.OUT ^= PIN6_bm;
}
