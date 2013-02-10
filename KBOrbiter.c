
/**
 **    Simple orbiting
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

#define BLINK _delay_ms(25);
#define PAUSE _delay_ms(250);

// special

#define ee_SENSOR_LOW 0x20
#define ee_SENSOR_HIGH 0x50

#define MODE_SHOUT  6 // we dodge the LSB
#define MODE_EXEC  12 // we dodge the LSB
#define LEADMAX    15 // magic#, iterations before leader becomes apparent

#define TIMEOUT 50000
#define BOUND 75
#define TURN 25

/*
 *   GLOBALS
 */


static uint16_t le = 0;  // leader election variable
static uint8_t isBoss = 1;
static uint8_t dist;


/*
 *   MOVE
 */

#define REV 0xAA
#define HALT    set_motor(0,0)
#define GO      set_motor(REV,REV)

typedef enum {
    GEAR_PARK,
    GEAR_CW,
    GEAR_CCW,
    GEAR_FWRD
} gear_t;

static gear_t gear = GEAR_PARK;

void change_gear(gear_t g)
{
    if (gear == g) { return; }

    switch (g) {
        case   GEAR_CW: set_motor(REV,  0); break;
        case  GEAR_CCW: set_motor(0,  REV); break;
        case GEAR_FWRD: set_motor(REV,REV); break;
        default: break;
    }

    gear = g;
    BLINK;
}

void   move_CW(void) { change_gear(GEAR_CW);  set_motor(cw_in_place, 0); }
void  move_CCW(void) { change_gear(GEAR_CCW); set_motor(0,ccw_in_place); }
void move_FWRD(void) { change_gear(GEAR_FWRD);set_motor(cw_in_straight, ccw_in_straight); }


/*
 *   FSM
 */

typedef enum {
	// leader election
	INIT,
	LISTEN,
	FOLLOW,

	// orbit
	ORBIT,
	SPIRAL,
	REACT,

	// end
	HOLD
} FSM;

static FSM state = INIT;

// LEADER ELECTION

void state_init(void)
{
    int16_t seed = 0;

    LED_WHITE;
	for(uint8_t i=0;i<14;i++)
	{
		seed ^= (eeprom_read_byte((uint8_t *) (ee_SENSOR_LOW+i*2))<<8);
		seed ^= eeprom_read_byte((uint8_t *) (ee_SENSOR_LOW+i*2+1));
		seed ^= (eeprom_read_byte((uint8_t *) (ee_SENSOR_HIGH+i*2))<<8);
		seed ^= eeprom_read_byte((uint8_t *) (ee_SENSOR_HIGH+i*2+1));
	}
    srand(seed);

    le = (randy() << 8) + randy();
    enable_tx = 1;
    message_out((uint8_t) (le>>8), (uint8_t) le, MODE_SHOUT);
    set_motor(0,0);

    PAUSE; PAUSE;
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
		state = HOLD;
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

void state_follow(void) {
	LED_OFF;	
	enable_tx = 0;
	state = ORBIT;
	move_FWRD();
}


//  ORBITER


void state_orbit(void) {
	static uint16_t to;
	get_message();
	if(message_rx[5] == 1) {
		to = 0;
		uint8_t mode = message_rx[2];
    	dist = message_rx[3];

		if (mode == MODE_EXEC) {
			state = REACT;
		}
  	} else {
		to++;
		LED_OFF;
	}
	
	if (to >= TIMEOUT) {
		to = 0;
		LED_WHITE;
		state = SPIRAL;
	}
}

void state_spiral(void) {
	static uint16_t scount = 0;
	static uint16_t spiral = 0;

	scount++;

	if (scount < TURN) {
		set_motor(cw_in_place, 0);
	} else if (scount < (TURN + spiral)) {
		move_FWRD();
	} else {
		scount = 0;
		spiral += 5;
	}

	get_message();
	if(message_rx[5] == 1) {
		if (message_rx[3] < 90) {
			state = ORBIT;
			scount = 0;
			spiral = 0;
		}
  	}
	
	BLINK;
}

void state_react(void) { 
    int8_t diff = dist - BOUND;

    switch (diff) {
      case  4: LED_RED; break;
      case  3: LED_ORANGE; break;
	  case  2: LED_GREEN; break;
      case  1: LED_TURQOISE; break;

      case  0: LED_BLUE; break;

      case -1: LED_TURQOISE; break;
	  case -2: LED_GREEN; break;
      case -3: LED_ORANGE; break;
      case -4: LED_RED; break;

	  default:
	  	LED_OFF; break;
    }
	
    if (diff > 0) {
        move_CCW();
    } else if (diff < -7) {
        move_CW();
    } else {
        move_FWRD();
    }

	state = ORBIT;
}



void user_program(void)
{  
  switch (state) {
	case   INIT: state_init(); break;
	case LISTEN: state_listen(); PAUSE; break;
	case FOLLOW: state_follow(); PAUSE; break;

	case  ORBIT: state_orbit(); break;
	case SPIRAL: state_spiral(); break;
	case  REACT: state_react(); break;

    case   HOLD:
	    default: break;
  }

    kprints("LE");
    kprinti(le);
    NEWLINE;
}

int main(void)
{
  init_robot(); // HW/SW setup
  main_program_loop(user_program);
  return 0;
}

