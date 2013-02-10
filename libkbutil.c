
#include "libkbutil.h"


 /*
  *   GENERAL
  */



 /*
  *   HIST
  */

volatile history_t h;

// GENERIC

void history_init(void)
{
	h.hp = 0;
    h.top = 0;
    h.valid = 0x00;
    for (uint8_t i = 0 ; i < HISTLEN; i++) {
        h.As[i] = 0xFF;
        h.Ds[i] = 0xFF;
	}
}

uint8_t history_full(void)
{	
	return (uint8_t) (h.valid == (1<<HISTLEN) - 1);
}

void history_add(int16_t a, int16_t b, int16_t d)
{	
    h.valid |= _BV(h.hp);
    h.As[h.hp] = (uint8_t) a;
    h.Ds[h.hp] = (uint8_t) d;
	h.hp = (h.hp == HISTLEN - 1) ? 0 : h.hp + 1;
    h.top = (h.top < b) ? (uint8_t) b : h.top;
}

// SPECIAL

uint8_t history_top(void)
{
	return h.top;
}

uint8_t history_heard(uint8_t target)
{
	for (uint8_t i = 0 ; i < HISTLEN; i++) {        
		if (h.valid & _BV(i)){ 
			if (h.As[i] == target) {
				return 1;
			}
		}
	}
	return 0;
}

uint8_t history_onbound(uint8_t bound_id, uint8_t bound_dist)
{
    uint16_t pos = 0;
    uint8_t j = 0;
    for (uint8_t i = 0 ; i < HISTLEN; i++) {
        if (bound_id == ANON || bound_id == h.As[i]) {
            pos += h.Ds[i];
            j++;
        }
	}
    if ((pos == 0) || (j < 2)) {
        return 0;
    } else {
        pos /= j;
    	return abs(bound_dist - pos) <= BOUND_ACCURACY;
    }
}

void history_debug(void)  // DEBUG
{
  	kprints("--HIST--");
    kprinti(h.hp);
    kprinti(h.top);
    kprinti(h.valid);
    NEWLINE;
    for (uint8_t i = 0 ; i < HISTLEN; i++) {
        kprinti(h.As[i]);
        kprinti(h.Ds[i]);
	}
    NEWLINE;
}

 /* 
  *   FORM
  */ 

void getRules(uint8_t target, uint8_t *r)
{
	switch (target) {
/*
// A!
	case 1: r[0]= 0; r[1]=33; r[2]= 0; r[3]=33; break;
	case 2: r[0]= 0; r[1]=55; r[2]= 1; r[3]=33; break;
	case 3: r[0]= 0; r[1]=66; r[2]= 2; r[3]=33; break;
	case 4: r[0]= 0; r[1]=66; r[2]= 3; r[3]=33; break;
	case 5: r[0]= 0; r[1]=35; r[2]= 4; r[3]=33; break;
	// TODO
*/
	case 1: r[0]= 0; r[1]=35; r[2]= 0; r[3]=35; break;
	case 2: r[0]= 1; r[1]=35; r[2]= 0; r[3]=70; break;
	case 3: r[0]= 2; r[1]=35; r[2]= 1; r[3]=70; break;
	case 4: r[0]= 3; r[1]=35; r[2]= 2; r[3]=70; break;
	case 5: r[0]= 4; r[1]=35; r[2]= 3; r[3]=70; break;
			
	default:
		r[0]=0; r[1]=0; r[2]=0; r[3]=0; break;
	}
}


 /* 
  *   MOVE
  */ 


static gear_t gear = GEAR_PARK;

void change_gear(gear_t g)
{
    if (gear == g) {
        return;
    }

    switch (g) {
        case   GEAR_CW: set_motor(REV,  0); break;
        case  GEAR_CCW: set_motor(0,  REV); break;
        case GEAR_FWRD: set_motor(REV,REV); break;
        default: break;
    }

    gear = g;
    BLINK;
}

void move_FWRD(void) {
    change_gear(GEAR_FWRD);
	set_motor(cw_in_straight, ccw_in_straight);
}

void move_CW(void)
{
    change_gear(GEAR_CW);
	set_motor(cw_in_place, 0);
}

void move_CCW(void) {
    change_gear(GEAR_CCW);
	set_motor(0,ccw_in_place);
}

void move_steer(uint8_t bound_id, uint8_t bound_dist) {
    volatile uint8_t *ids = history_ids;
    volatile uint8_t *dists = history_dists;
    uint8_t orbiting = (bound_id == ANON);

    uint8_t min = 0xFF;
    uint8_t j = 0;
    int16_t avg = 0;

    for (uint8_t i = 0; i < HISTLEN; i++) {
        if (dists[i] < min) {
            min = dists[i];
        }        
        if (ids[i] == bound_id) {
            avg += dists[i];
            j++;
        }
    }

    avg = (j==0) ? 0 : avg/j;

    int8_t diff = orbiting ? (int8_t) (min - bound_dist) : (int8_t) (avg - bound_dist);

    // Gradient rules

    // 1. Dodge the exisiting pattern - don't bump
    if (orbiting && min <= 40) {
        move_CW();
    } else if (orbiting && min < 45) {
        move_FWRD();

    // 2. Home in on the bound_dist - black magic
    //    We approach, turn and compensate in order to orient the bot tangentially
    } else if (diff <= -2) { // below, strikethrough (i.e., we only want to approach from above)
        move_FWRD(); 
    } else if (diff == -1) { // compensate
        move_CW();
    } else if (diff ==  0) { // compensate
        move_FWRD();
    } else if (diff ==  1) { // compensate the turn for tanget pos
        move_CW();
    } else if (diff <= 8) { // turn towards
        LED_GREEN;
        move_CCW();

    // 3. meander closer from above
    } else if (diff <= 18) {
        LED_RED;
        move_CW();         // penultimate turn
    } else if (diff <= 28) {
        LED_ORANGE;
        move_CCW();          // penpenultimate turn
    } else if (diff <= 38) {
        LED_OFF;
        move_CW();          // penpenultimate turn
    } else if (diff <= 48) {
        move_CW();          // penpenultimate turn
    }

}
