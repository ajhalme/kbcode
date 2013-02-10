
/**
 **    PID orbiting
 **
 **    Antti Halme, Aalto University, 2012
 **/

#include "libkb2.h"

#include "pid.h"

// #include <inavr.h> #include <ioavr.h> #include "stdint.h" #include "pid.h"


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
#define BREATH _delay_ms(125);
#define PAUSE _delay_ms(250);

// special

#define ee_CW_IN_PLACE 0x004
#define ee_CCW_IN_PLACE 0x008
#define ee_CW_IN_STRAIGHT 0x00B
#define ee_CCW_IN_STRAIGHT 0x013
#define ee_SENSOR_LOW 0x20
#define ee_SENSOR_HIGH 0x50

#define MODE_ELECT  6 // we dodge the LSB
#define MODE_GO  12 // we dodge the LSB
#define LEADMAX    25 // iterations before leader becomes apparent, depends on N / patience

#define TIMEOUT 50000
#define RRANGE 90
#define FRINGE 70
#define TURN 25

#define HEARD (message_rx[5] == 1 && message_rx[3] <= RRANGE)


/*
 *   MOVE
 */


#define REV 0x99
#define HALT    set_motor(0,0)

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
    BLINK; // TODO: RT?
}

void   move_CW(void) { change_gear(GEAR_CW);  set_motor(cw_in_place, 0); }
void  move_CCW(void) { change_gear(GEAR_CCW); set_motor(0,ccw_in_place); }
void move_FWRD(void) { change_gear(GEAR_FWRD);set_motor(cw_in_straight, ccw_in_straight); }

void fast_FWRD(void) { change_gear(GEAR_FWRD); set_motor(cw_in_straight + 5, ccw_in_straight + 5);}

// PID

#define K_P     1.00
#define K_I     0.00
#define K_D     0.00

struct PID_DATA pidData;
uint8_t control_values[4] = {0};


/*
 *   FSM
 */

typedef enum {
	// leader election
	INIT,
	ELECT,

	// orbit
	PID,

	// end
	HOLD
} FSM;

static FSM state = INIT;


/*
 *  LEADER ELECTION
 */


static uint16_t le = 0; // leader election token

void state_init(void)
{
    int16_t seed = 0;

    LED_WHITE;
	for(uint8_t i=0;i<14;i++) { // we use calibration data to seed random
		seed ^= (eeprom_read_byte((uint8_t *) (ee_SENSOR_LOW+i*2))<<8);
		seed ^= eeprom_read_byte((uint8_t *) (ee_SENSOR_LOW+i*2+1));
		seed ^= (eeprom_read_byte((uint8_t *) (ee_SENSOR_HIGH+i*2))<<8);
		seed ^= eeprom_read_byte((uint8_t *) (ee_SENSOR_HIGH+i*2+1));
		seed ^= measure_voltage() ^ (rand() & 0xFFFF);
        srand(seed);
	}
  
    le = (randy() << 8) + randy();
    enable_tx = 1;
    message_out((uint8_t) (le>>8), (uint8_t) le, MODE_ELECT);
    set_motor(0,0);

	control_values[0] = eeprom_read_byte((uint8_t *) ee_CW_IN_PLACE);
	control_values[1] = eeprom_read_byte((uint8_t *) ee_CCW_IN_PLACE);
	control_values[2] = eeprom_read_byte((uint8_t *) ee_CW_IN_STRAIGHT);
	control_values[3] = eeprom_read_byte((uint8_t *) ee_CCW_IN_STRAIGHT);

    LED_BLUE;
    state = ELECT;
    PAUSE;
}

void state_elect(void) {
    static uint8_t  isBoss = 1;
    static uint16_t  round = 0;

    round++;

    get_message();
    if(HEARD) {
        uint8_t mode = message_rx[2];

        if (mode == MODE_ELECT) {
            uint16_t heard_id = (message_rx[0] << 8) + message_rx[1];

            if (heard_id < le){
                le = heard_id;
                message_out((le>>8), le, MODE_ELECT);
                isBoss = 0;
            }
        }
    }

	LED_OFF;
	BLINK;
    if (isBoss) LED_GREEN; else LED_RED;
    BLINK; BLINK;

    if (round > LEADMAX) {
        if (isBoss == 1) { // ACTIVATEL
            message_out(0, 0, MODE_GO);
            enable_tx = 1;
            state = HOLD;
        } else {
            LED_OFF;	
            enable_tx = 0;
            state = PID;
            pid_Init(K_P * SCALING_FACTOR, K_I * SCALING_FACTOR , K_D * SCALING_FACTOR , &pidData);
            move_FWRD();
        }
    }
}


/*
 *  PID ORBITING
 */


uint8_t get_reference(void) { return FRINGE; }

uint8_t get_measure(void)
{ 
    for (;;) { 
        get_message(); 
        if (HEARD) {
            return message_rx[3];
        }
    }
}

void state_pid(void)
{ 
    static int16_t memory = 0;
    static int8_t direction = 0;
//    static void (* op)(void) = move_CW;

    uint8_t ref = get_reference();         
    uint8_t measure = get_measure();
    int16_t input = pid_Controller(get_reference(), get_measure(), &pidData);

    direction = input - memory;
    memory = input;

#define WANT_DOWN  (input  < 0)
#define WANT_HERE  (input == 0)
#define WANT_UP    (input  > 0)

#define DIR_DOWN    (direction  > 0) 
#define DIR_TANGENT (direction == 0) 
#define DIR_UP      (direction  < 0)
    
    if      (DIR_TANGENT || (DIR_DOWN && WANT_DOWN) || (DIR_UP && WANT_UP))  { LED_VIOLET; fast_FWRD(); }
    else if (DIR_DOWN && WANT_UP) { LED_BLUE; move_CW(); }
    else if (DIR_UP && WANT_DOWN) { LED_RED; move_CCW(); }

    kprints("STATEPID");
    kprinti(ref); kprinti(measure); kprinti(input); kprinti(direction);
    NEWLINE;
    // CW, CCW, FWRD_CW, FWRD_CCW
    kprinti(control_values[0]); kprinti(control_values[1]); 
    kprinti(control_values[2]); kprinti(control_values[3]);
    NEWLINE;

    BREATH; LED_OFF; BREATH;
}

void user_program(void)
{  
  switch (state) {
	case  INIT: state_init(); break;
	case ELECT: state_elect(); break;

	case   PID: state_pid(); break;

    case  HOLD:
	   default: break;
  }
}


int main(void)
{
  init_robot(); // HW/SW setup
  main_program_loop(user_program);
  return 0;
}
