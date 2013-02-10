
/**
 **    Shouter leader election
 **
 **    Antti Halme, Aalto University, 2012
 **/

#include "libkb.h"

/*
 *   UTILS
 */

#define MODE_SHOUT  6 // we dodge the LSB
#define MODE_EXEC  12 
#define LEADMAX    15 // iterations before leader becomes apparent

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

#define BLINK _delay_ms(25);
#define PAUSE _delay_ms(250);


/*
 *   GLOBALS
 */


static uint16_t le = 0;  // leader election variable
static uint8_t isBoss = 1;


/*
 *   FSM
 */

typedef enum {
	INIT,
	LISTEN,
	LEAD,
	FOLLOW
} FSM ;


static FSM state = INIT;


void state_init(void)
{
    uint8_t i;
    int16_t seed = 0;

    LED_WHITE;
    enable_tx = 1;  

    for (i = 0; i < 25; i++){
        message_out(randy(), randy(), randy());
        get_message();
        seed += message_rx[0] + message_rx[1] + message_rx[2] + message_rx[3];
        seed += get_ambient_light() + measure_voltage() + ADCW;
        srand(seed);
        BLINK;
    }

    srand(seed);

    le = (randy() << 8) + randy();
    message_out((uint8_t) (le>>8), (uint8_t) le, MODE_SHOUT);
    set_motor(0,0);

    LED_BLUE;
    state = LISTEN;
}


void state_listen(void) {
  static uint16_t round = 0;
  round++;

  get_message();
  if(message_rx[5] == 1) {
	uint8_t mode = message_rx[2];

	if (mode == MODE_SHOUT) {
		uint16_t heard = (message_rx[0] << 8) +  message_rx[1];

		if (le > heard){
			LED_RED;
			le = heard;
			message_out((uint8_t) (le>>8), (uint8_t) le, MODE_SHOUT);
			isBoss = 0;
		}
	}
  }

  if (isBoss) {
	LED_OFF;
    BLINK;
	LED_BLUE;
  } else {
    BLINK;
  }

  if (round > LEADMAX) {
    if (isBoss == 1) {
		state = LEAD;
		LED_GREEN;
		message_out(0, 0, MODE_EXEC);
	} else {
		isBoss = 0;
		LED_OFF;
		enable_tx = 0;
		state = FOLLOW;		
	}
  }
}



void user_program(void)
{
    switch (state) {
    case   INIT: state_init(); PAUSE; break;
    case LISTEN: state_listen(); PAUSE; break;

    case   LEAD:
    case FOLLOW:
        default: break;
    }

    kprints("LE");
    kprinti(le);
    NEWLINE;
}

int main(void)
{
  init_robot();
  main_program_loop(user_program);
  return 0;
}

