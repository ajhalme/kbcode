
/**
 **    Proximity sensing
 **
 **    Antti Halme, Aalto University, 2012
 **/

#include "libkb.h"

/*
 *   UTILS
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

#define NEWLINE kprints("          ")

#define randy(void) ((uint8_t)(rand() & 0xFF))

#define BLINK _delay_ms(25)
#define PAUSE _delay_ms(250)

// special

#define SEEKER 42
#define CW_MOVE set_motor(cw_in_place,0)
#define CW_IGNITE set_motor(0xAA,0)
#define HALT set_motor(0,0)

/*
 *   FSM
 */

typedef enum {
    INIT, SEEK, SHOW
} STATE;

STATE state = INIT;

void state_init(void)
{
    LED_WHITE;
    set_motor(0,0);
    enable_tx = 1; 
    message_out(SEEKER, SEEKER, SEEKER);
    state = SEEK;
}

void state_seek(void)
{
    static uint8_t seeking = 0;

    get_message();
    if(message_rx[5] == 1) {
        state = SHOW;
        HALT;
        seeking = 0;
    } else {
        LED_WHITE;
        if (!seeking) {
            CW_IGNITE;
            seeking = 1;
        }
        BLINK;     
        CW_MOVE;
        LED_OFF;
    }

    PAUSE;
}

void state_show(void)
{
    set_motor(0,0);

    uint8_t dist = message_rx[3];
    uint8_t isclose = (dist == 33);

    if (isclose) {
        LED_WHITE;
    } else {      
        dist = dist / 10;      
        switch (dist) {
            case 3: LED_RED; break;
            case 4: LED_ORANGE; break;
            case 5: LED_GREEN; break;
            case 6: LED_TURQOISE; break;
            case 7: LED_BLUE; break;
            case 8: LED_VIOLET; break;
           default: LED_ON; break;
        }
    }

    kprints("ambient:  ");
    kprinti(get_ambient_light());
    NEWLINE;

    kprints("msgs      ");
    kprinti(message_rx[0]);
    kprinti(message_rx[1]);
    kprinti(message_rx[2]);
    kprinti(message_rx[3]);
    NEWLINE;	

    PAUSE;
    state = SEEK;
}



void user_program(void)
{
    switch(state) {
        case INIT: state_init(); break;
        case SEEK: state_seek(); break;
        case SHOW: state_show(); break;

        default: break;
    }
}

int main(void)
{
  init_robot(); 
  main_program_loop(user_program);
  return 0;
}

