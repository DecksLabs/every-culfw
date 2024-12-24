#include <avr/io.h>
#include "spi.h"
#include "board.h"

void
spi_init(void)
{
    PORTC.DIR |= PIN0_bm; /* Set MOSI pin direction to output */
    PORTC.DIR &= ~PIN1_bm; /* Set MISO pin direction to input */
    PORTC.DIR |= PIN2_bm; /* Set SCK pin direction to output */
    PORTC.DIR |= PIN3_bm; /* Set CS (using PC3 for this) pin direction to output */

    PORTMUX.TWISPIROUTEA = PORTMUX_SPI0_ALT1_gc;

    SPI0.CTRLB |= SPI_SSD_bm; // we're driving CS manually

    SPI0.CTRLA = SPI_CLK2X_bm           /* Enable double-speed */
               | SPI_ENABLE_bm          /* Enable module */
               | SPI_MASTER_bm          /* SPI module in Master mode */
               | SPI_PRESC_DIV4_gc;    /* System Clock divided by 4 */

}

uint8_t
spi_send(uint8_t data)
{
    SPI0.DATA = data;

    while (!(SPI0.INTFLAGS & SPI_IF_bm))  /* waits until data is exchanged*/
    {
        ;
    }

    return SPI0.DATA;
}
