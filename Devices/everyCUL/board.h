#ifndef _BOARD_H
#define _BOARD_H

#include <stdint.h>

/* if you have an Arduino with only 8MHz disable the next line */
#define HAS_16MHZ_CLOCK

//SPI from nano maps to SPI1 on every (pins PE0:3, altmode enabled)
//#define SPI_PORT		PORTB
//#define SPI_DDR			DDRB
//#define SPI_SS			2
//#define SPI_MISO		4
//#define SPI_MOSI		3
//#define SPI_SCLK		5


//#define CC1100_CS_DDR     SPI_DDR
//#define CC1100_CS_PORT    SPI_PORT
//#define CC1100_CS_PIN     SPI_SS

/* CC1101 GDO0 Tx / Temperature Sensor */
//PF5 on every
//#define CC1100_OUT_DDR		      DDRD
//#define CC1100_OUT_PORT         PORTD
//#define CC1100_OUT_PIN          PD3
//#define CC1100_OUT_IN           PIND
//#define CCTEMP_MUX              CC1100_OUT_PIN

//PA0 on every
/* CC1101 GDO2 Rx Interrupt */
//#define CC1100_IN_DDR		        DDRD
//#define CC1100_IN_PORT          PIND
//#define CC1100_IN_PIN           PD2
//#define CC1100_IN_IN            PIND


//interrupt is on PA0 pin on every
//#define CC1100_INT		          INT0
//#define CC1100_INTVECT          INT0_vect
//#define CC1100_ISC		          ISC00
//#define CC1100_EICR             EICRA


#define BOARD_ID_STR            "nanoCUL868_r568"

#define HAS_UART
#define UART_BAUD_RATE          38400

#define TTY_BUFSIZE             1500

#define RCV_BUCKETS            2      //                 RAM: 25b * bucket
#define FULL_CC1100_PA                // PROGMEM:  108b
#define HAS_RAWSEND                   //

/* HAS_MBUS requires about 1kB RAM, if you want to use it you
   should consider disabling other unneeded features
   to avoid stack overflows
*/
#define HAS_MBUS



#endif // board.h
