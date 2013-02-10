
/**
 **    Simple orbiting
 **
 **    Antti Halme, Aalto University, 2012
 **/

#include "libkb2.h"
#include "libkbutil.h"

/*
 *   UTILS
 */

// transmission modes
#define MODE_ELECT   (0x06) // we dodge the LSB
#define MODE_GO      (0x0C)
#define MODE_SCATTER (0x18)
#define MODE_DONE    (0x30)

// magic #
#define LEADMAX 25    // iterations before leader becomes apparent, depends on N / patience
#define TIMEOUT 50000 // timeout counter (function calls)
#define TURN 50       // spiral magic

// the State Machine
// leader: INIT - ELECT - HOLD
// follow: INIT - ELECT - SCATTER - GET/ORBIT/SPIRAL/FINISH/FORFEIT - HOLD
enum FSM {
	// leader election
	INIT,
	ELECT,

	// scatter
	SCATTER,

	// orbit
	GET,
	ORBIT,
	SPIRAL,

	// build form
	FINISH,
	FORFEIT,

	// end
	HOLD
};


// Globals
static enum FSM state = INIT;

static uint8_t id = ANON; // id for form
static uint8_t top = 0; // form max id
static uint8_t mark = 0; // form construction trigger
static uint8_t target = 0; // form target
static uint8_t rules[4] = {0, 0, 0, 0}; // the rules for target position



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
	}
    srand(seed);
  
    le = (randy() << 8) + randy();
    enable_tx = 1;
    message_out((uint8_t) (le>>8), (uint8_t) le, MODE_ELECT);
    set_motor(0,0);

    history_init();
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
            id = 0;
            message_out(0, 0, MODE_GO);
            enable_tx = 1;
            state = HOLD;
        } else {
            state = SCATTER;		
        }
    }

}


/*
 *  SCATTER
 */

void state_scatter(void) {
	static void (*op)(void) = move_CW;

	get_message();

	if (HEARD) { // scatter
		message_out(42,42,MODE_SCATTER);
		enable_tx = 1;
		if ((randy() & 0x11) == 0) {
			op = (op == move_CW) ? move_CCW : move_CW;
		}
		op(); BLINK; move_FWRD(); BLINK; HALT;
	} else if ((rand() & 0x1FF) == 0) { // start
		enable_tx = 0;
		target = 1;
		getRules(target, rules);
		state = GET;
	}
}


/*
 *  ORBIT
 */


void state_get(void) {
    LED_OFF;

	PAUSE; // too fast otherwise

	uint8_t isConnected = 0;
	for (uint8_t i = 0; i < RXBUFSIZE; i++) {
		get_message();
		if (HEARD) {
            isConnected = 1;
            if (message_rx[2] == MODE_GO) {
    			history_add(message_rx[0], message_rx[1], message_rx[3]);
            }
        }
    }

	if (isConnected) {
        if (id == ANON) {
		    state = ORBIT;
	    } else {
		    state = FINISH;
        }
	} else {
        state = SPIRAL;
    }
}



void state_spiral(void) {
	static uint16_t scount = 0;
	static uint16_t spiral = 0;

	LED_TURQOISE;
	scount++;

	if (scount < TURN) {
		move_CCW();
	} else if (scount < (TURN + spiral)) {
		move_FWRD();
	} else {
		scount = 0;
		spiral += 25;
	}

	get_message();
	if(HEARD && message_rx[2] == MODE_GO) {
		history_add(message_rx[0], message_rx[1], message_rx[3]);
		scount = 0;
		spiral = 0;
		state = GET;
  	}
	
    BLINK;
}


void state_orbit(void) { // follow the form gradient
	// static uint16_t to;

	LED_ORANGE;

    history_debug();

	// ensure we have enough history
	if (!history_full()) {
        goto out;
	}

	uint8_t htop = history_top();

	// adjust target, if necessary
	if (htop >= target) {
		target = (htop + 1);
		getRules(target, rules);
        goto out;
	}

	// decide: either follow the gradient or start closing in
	if (history_onbound(rules[0], FRINGE)) {
		id = target;
		state = FINISH;        
		return;
	}

out:
	move_steer(ANON, FRINGE);
    state = GET;
	BLINK;
}


/*
 *  BUILD FORM
 */


void state_finish(void) {	

    LED_VIOLET;

    // adjust traget, if necessary
	if (history_top() >= id) {
		state = FORFEIT;
		return;
	};	

    uint8_t heardR1 = history_heard(rules[0]);
    uint8_t heardR2 = history_heard(rules[2]);
    uint8_t heardR1bound = 0;
    uint8_t heardR2bound = 0;
    if (heardR1) { heardR1bound = history_onbound(rules[0], rules[1]); }
    if (heardR2) { heardR2bound = history_onbound(rules[2], rules[3]); }

    // done when second is reached (or when we want to be the first violin)
    if (((mark == 1) && heardR2bound) || (heardR1bound && (id == 1))) {
        HALT;
        top = id;
        if (id == N - 1) {
            message_out(id, id, MODE_DONE);
            LED_GREEN;
        } else {
            message_out(id, id, MODE_GO);
            LED_BLUE;
        }
        enable_tx = 1;
        state = HOLD;
        return;
    } else if (heardR1) { // otherwise follow first
        if (heardR1bound) {
            mark = 1;
        }
        move_steer(rules[0], rules[1]);
    } 
    
    /*
    else if (randy() & 0x7F) {
        state = FORFEIT;
        return;
    }
    */
    
    state = GET;
}

void state_forfeit(void) {
    mark = 0;
    target = 0;
    id = ANON;
    enable_tx = 0;
    history_reset();

    state = GET;
	return;
}


/*
 *  END
 */


void state_hold(void) {
	get_message();
	if (HEARD) {
		// uint8_t mheard = message_rx[0];
		uint8_t mtop   = message_rx[1];
		uint8_t mmode  = message_rx[2];
		// uint8_t mdist  = message_rx[3];
       
        if (mmode == MODE_GO || mmode == MODE_DONE) {
            if (mtop > top) { top = mtop; }

            message_out(id, top, mmode);
        }
	}
}


/*
 *  MAIN
 */


void user_program(void)
{
  
    switch (state) {
    case INIT:
        state_init(); break;
    case ELECT:
        state_elect(); break;

    case SCATTER:
        state_scatter(); break;

    case GET:
        state_get(); break;
    case ORBIT:
        state_orbit(); break;
    case SPIRAL:
        state_spiral(); break;

    case FINISH:
        state_finish(); break;
    case FORFEIT:
        state_forfeit(); break;

    case HOLD:
        state_hold(); break;

    default:
        HALT; break;
    }

  	kprints("sIDtTGET");
  	kprinti(state); kprinti(id); kprinti(id); kprinti(target);
	NEWLINE;
}

int main(void)
{
  init_robot(); // HW/SW setup
  main_program_loop(user_program);
  return 0;
}

