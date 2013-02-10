
#ifndef LIBKBUTIL
#define LIBKBUTIL

#include "libkb2.h"

/*
 *   GENERAL STUFF
 */


#define LED_OFF      set_color(0,0,0)
#define LED_ON       set_color(2,2,2)
#define LED_WHITE    set_color(3,3,3)
#define LED_RED      set_color(3,0,0)
#define LED_ORANGE   set_color(3,3,0)
#define LED_GREEN    set_color(0,3,0)
#define LED_TURQOISE set_color(0,3,3)
#define LED_BLUE     set_color(0,0,3)
#define LED_VIOLET   set_color(3,0,3)

#define NEWLINE kprints("        ")

#define randy(void) ((uint8_t)(rand() & 0xFF))

#define SPASM _delay_ms(15);
#define BLINK _delay_ms(25);
#define PAUSE _delay_ms(250);

#define ee_SENSOR_LOW 0x20
#define ee_SENSOR_HIGH 0x50

#define HEARD (message_rx[5] == 1 && message_rx[3] <= RRANGE)
#define RXBUFSIZE 4 // size of the RX-buffer, # msgs to hear at t = t_i


 /*
  *   HIST
  */ 


 // GENERIC

#define HISTLEN 8 // consider the history struct

typedef struct {
  uint8_t hp    : HISTLEN;
  uint8_t valid : HISTLEN; 
  uint8_t top;
  uint8_t As[HISTLEN];
  uint8_t Ds[HISTLEN];
} history_t;

extern volatile history_t h;

#define history_reset history_init

void history_init(void);
void history_reset(void);
void history_add(int16_t a, int16_t b, int16_t d);
uint8_t history_full(void);

 // SPECIAL

#define history_ids h.As
#define history_dists h.Ds

uint8_t history_top(void);
uint8_t history_heard(uint8_t);
uint8_t history_onbound(uint8_t bound_id, uint8_t bound_dist);

void history_debug(void);

 /* 
  *   FORM
  */ 

#define N 3              // secret N
#define ANON (N+1)       // anonymity marker, note: must be outside range(0,N)
#define FRINGE 75        // mm, the desired gradient
#define RRANGE 90        // mm, the RX threshold
#define BOUND_ACCURACY 1 // +/- mm

void getRules(uint8_t target, uint8_t *rules);


 /* 
  *   MOVE
  */ 

#define REV 0xAA

typedef enum {
    GEAR_PARK,
    GEAR_CW,
    GEAR_CCW,
    GEAR_FWRD
} gear_t;

#define HALT    set_motor(0,0)
#define GO      set_motor(REV,REV);


extern void move_FWRD(void);
extern void move_CW(void);
extern void move_CCW(void);

extern void move_steer(uint8_t bound_dist, uint8_t bound_id);

#endif // LIBKBUTIL
