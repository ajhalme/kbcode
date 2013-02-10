
/**
 **    EEPROM reader (and writer)
 **    Simple walker (CCW, CW and FWRD)
 **
 **    Antti Halme, Aalto University, 2012
 **/

#include "libkb.h"

/*
 *   UTILS
 */

// from libkb
#define ee_OSCCAL 0x001  // rc calibration value in eeprom, to be loaded to OSCCAL at startup
#define ee_CW_IN_PLACE 0x004  //motor calibration data in epromm
#define ee_CCW_IN_PLACE 0x008  //motor calibration data in epromm
#define ee_CW_IN_STRAIGHT 0x00B  //motor calibration data in epromm
#define ee_CCW_IN_STRAIGHT 0x013  ///motor calibration data in epromm
#define ee_SENSOR_LOW 0x20  //low gain sensor calibration data in epromm
#define ee_SENSOR_HIGH 0x50  //high-gain calibration data in epromm
#define ee_TX_MASK 0x90  //Role asignment

// from libkbutil
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
#define RUSH (0xAA)
#define BLINK _delay_ms(25);
#define PAUSE _delay_ms(2500);

/*
 *   FUNCS
 */

void motor_value_printer(void)
{
    uint8_t cw_in_place = eeprom_read_byte((uint8_t *)ee_CW_IN_PLACE);
    uint8_t ccw_in_place = eeprom_read_byte((uint8_t *)ee_CCW_IN_PLACE);
    uint8_t cw_in_straight = eeprom_read_byte((uint8_t *)ee_CW_IN_STRAIGHT);
    uint8_t ccw_in_straight = eeprom_read_byte((uint8_t *)ee_CCW_IN_STRAIGHT);

    kprints("cw_inplace");
    kprinti(cw_in_place);
    NEWLINE;  

    kprints("ccwinplace");
    kprinti(ccw_in_place);
    NEWLINE;  

    kprints("cw_in fwrd");
    kprinti(cw_in_straight);
    NEWLINE;  

    kprints("ccwin_fwrd");
    kprinti(ccw_in_straight);
    NEWLINE;  

    uint8_t osccal = eeprom_read_byte((uint8_t *)ee_OSCCAL);
    uint8_t sensor_lo = eeprom_read_byte((uint8_t *)ee_SENSOR_LOW);
    uint8_t sensor_hi = eeprom_read_byte((uint8_t *)ee_SENSOR_HIGH);
    uint8_t tx_mask = eeprom_read_byte((uint8_t *)ee_TX_MASK);

    kprints("osccal");
    kprinti(osccal);
    NEWLINE;  

    kprints("sensorLO");
    kprinti(sensor_lo);
    NEWLINE;  

    kprints("sensorHI");
    kprinti(sensor_hi);
    NEWLINE;  

    kprints("tx_mask");
    kprinti(tx_mask);
    NEWLINE;  
    }

/*
void motor_value_writer()
{
    eeprom_write_byte((uint8_t *)ee_CW_IN_PLACE, 66);
    eeprom_write_byte((uint8_t *)ee_CCW_IN_PLACE, 66);
    eeprom_write_byte((uint8_t *)ee_CW_IN_STRAIGHT, 66);
    eeprom_write_byte((uint8_t *)ee_CCW_IN_STRAIGHT, 66);
}
*/

/*
 *   FSM
 */


typedef enum {
    INIT,
    EEPROM,
    WALK

} STATE;

STATE state = INIT;

void state_init(void)
{
    LED_GREEN;
    set_motor(0,0);
    state = EEPROM;
    BLINK;
}

void state_eeprom(void) {

    // motor_value_writer();
    motor_value_printer();
    
    PAUSE;
    state = WALK;
}

void state_walk(void) {
    static uint8_t round = 0;

    switch(round) {
        case 0: 
            LED_TURQOISE; set_motor(0,RUSH);                         BLINK; 
            LED_BLUE;     set_motor(0,ccw_in_place);                 PAUSE; break;
        case 1: 
            LED_ORANGE;   set_motor(RUSH,   0);                      BLINK;
            LED_RED;      set_motor(cw_in_place,0);                  PAUSE; break;
        case 2: 
            LED_WHITE;    set_motor(RUSH,RUSH);                      BLINK;
            LED_VIOLET;   set_motor(cw_in_straight,ccw_in_straight); PAUSE; break;
        case 3: 
            LED_OFF;      set_motor(0,0);                            BLINK; break;
    }

    round = (round == 3) ? 0 : round + 1;
    state = EEPROM;
}


void user_program(void)
{
    switch(state) {
        case   INIT: state_init(); break;
        case EEPROM: state_eeprom(); break;
        case   WALK: state_walk(); break;

        default: break;
    }
}


int main(void)
{
  init_robot();     
  main_program_loop(user_program);
  return 0;
}

