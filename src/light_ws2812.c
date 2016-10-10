#include <avr/interrupt.h>

#include "enc28j60.h"
#include "enc28j60_defs.h"
#include "ws2812_config.h"
#include "ws2812.h"
#include "spi.h"

// Timing in ns
#define w_zeropulse   350
#define w_onepulse    900
#define w_totalperiod 1250

// Fixed cycles used by the inner loop
#define w_fixedlow    2
#define w_fixedhigh   4
#define w_fixedtotal  8   

// Insert NOPs to match the timing, if possible
#define w_zerocycles    (((F_CPU/1000)*w_zeropulse          )/1000000)
#define w_onecycles     (((F_CPU/1000)*w_onepulse    +500000)/1000000)
#define w_totalcycles   (((F_CPU/1000)*w_totalperiod +500000)/1000000)

// w1 - nops between rising edge and falling edge - low
#define w1 (w_zerocycles-w_fixedlow)
// w2   nops between fe low and fe high
#define w2 (w_onecycles-w_fixedhigh-w1)
// w3   nops to complete loop
#define w3 (w_totalcycles-w_fixedtotal-w1-w2)

#if w1>0
  #define w1_nops w1
#else
  #define w1_nops  0
#endif

// The only critical timing parameter is the minimum pulse length of the "0"
// Warn or throw error if this timing can not be met with current F_CPU settings.
#define w_lowtime ((w1_nops+w_fixedlow)*1000000)/(F_CPU/1000)
#if w_lowtime>550
   #error "Light_ws2812: Sorry, the clock speed is too low. Did you set F_CPU correctly?"
#elif w_lowtime>450
   #warning "Light_ws2812: The timing is critical and may only work on WS2812B, not on WS2812(S)."
   #warning "Please consider a higher clockspeed, if possible"
#endif   

#if w2>0
#define w2_nops w2
#else
#define w2_nops  0
#endif

#if w3>0
#define w3_nops w3
#else
#define w3_nops  0
#endif

#define w_nop1  "nop      \n\t"
#define w_nop2  "rjmp .+0 \n\t"
#define w_nop4  w_nop2 w_nop2
#define w_nop8  w_nop4 w_nop4
#define w_nop16 w_nop8 w_nop8

void ws2812_sync(void)
{
if (!ws2812_locked) {
  uint8_t ctr, masklo, maskhi;
  volatile uint8_t curbyte;
  uint8_t sreg_prev;
  uint16_t datalen = ws2812_LEDS * 3;
  ws2812_DDRREG |= _BV(ws2812_PIN); // Enable output
  
  enc28j60_writeReg16(ERDPTL, ENC28J60_HEAP_START);
  sreg_prev=SREG;
  cli();
  ENC28J60_enable;
  spi_wrrd(ENC28J60_READ_BUF_MEM);
		masklo = ~_BV(ws2812_PIN) & ws2812_PORTREG;
		maskhi = _BV(ws2812_PIN) | ws2812_PORTREG;

    do {
	//grb
	curbyte = spi_wrrd(0x00);
asm volatile(
    "       ldi   %0,8  \n\t"
    "loop%=:            \n\t"
    //"       st    X,%3 \n\t"    //  '1' [02] '0' [02] - re
"out %2, %3\n\t"
#if (w1_nops&1)
w_nop1
#endif
#if (w1_nops&2)
w_nop2
#endif
#if (w1_nops&4)
w_nop4
#endif
#if (w1_nops&8)
w_nop8
#endif
#if (w1_nops&16)
w_nop16
#endif
    "       sbrs  %1,7  \n\t"    //  '1' [04] '0' [03]
//    "       st    X,%4 \n\t"     //  '1' [--] '0' [05] - fe-low
"out %2, %4\n\t"
    "       lsl   %1    \n\t"    //  '1' [05] '0' [06]
#if (w2_nops&1)
  w_nop1
#endif
#if (w2_nops&2)
  w_nop2
#endif
#if (w2_nops&4)
  w_nop4
#endif
#if (w2_nops&8)
  w_nop8
#endif
#if (w2_nops&16)
  w_nop16 
#endif
    "       brcc skipone%= \n\t"    //  '1' [+1] '0' [+2] - 
//    "       st   X,%4      \n\t"    //  '1' [+3] '0' [--] - fe-high
"out %2, %4\n\t"
    "skipone%=:               "     //  '1' [+3] '0' [+2] - 

#if (w3_nops&1)
w_nop1
#endif
#if (w3_nops&2)
w_nop2
#endif
#if (w3_nops&4)
w_nop4
#endif
#if (w3_nops&8)
w_nop8
#endif
#if (w3_nops&16)
w_nop16
#endif
    "       dec   %0    \n\t"    //  '1' [+4] '0' [+3]
    "       brne  loop%=\n\t"    //  '1' [+5] '0' [+4]
    :	"=&d" (ctr)
    :	"r" (curbyte), "I" (_SFR_IO_ADDR(ws2812_PORTREG)), "r" (maskhi), "r" (masklo)
//    :	"r" (curbyte), "x" (port), "r" (maskhi), "r" (masklo)
    );
  } while (--datalen);
  ENC28J60_disable;
  SREG=sreg_prev;
}
}