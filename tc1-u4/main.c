#include <STC15F2K60S2.h>
#include <intrins.h> // _nop_();
#include <limits.h>  // <DATATYPE>_MAX, ...

#define LOW   0
#define HIGH  1
#define FALSE 0
#define TRUE  1

#define CLR_BIT(p,n) ((p) &= ~(1 << (n)))
#define SET_BIT(p,n) ((p) |= (1 << (n)))

// I/O Ports
#define I_BUTTON      P34
#define I_AVR_ONLINE  P33 // PD2
#define O_BOOST       P35
#define O_AVR_BUTTON  P32 // PD1

// States
#define SLEEP   0
#define IDLE    1
#define BOOT    2
#define ONLINE  3

char state;
// UINT_MAX = 0xFFFF; system_tick with rollover correction supports up to 2 * UINT_MAX * 5ms = 655 seconds
// ULONG_MAX = 0xFFFFFFFF; system_tick with rollover correction supports up to 2 * ULONG_MAX * 5ms = 248.5 days
unsigned long int state_start_time, wait_begin_time;
volatile unsigned long int system_tick;

volatile bit button_changed;
volatile bit button_state;
volatile bit avr_online_state;


void extInt2_ISR(void) interrupt 10
{
	// do nothing, INT2's only purpose is to wake up the system
}

void timer0_ISR(void) interrupt 1
{
	system_tick++;

	/* NOTE: How long does a system tick take?
	 *
	 * The calculation goes like:
	 * tick frequency = system frequency / 12 / (65536 - [RL_TH0, RL_TL0])
	 *
	 * The '12' is because we set the AUXR register to increment the
	 * timer register only every 12th clock cycle.
	 *
	 * Supposing that the system clock is set to 12.000 MHz, and
	 * [RL_TH0, RL_TL0] is "65536 - 1000", then a system-tick would be
	 * 12 MHz / 12 / 1000 = 1 kHz.
	 *
	 * Therefore, a system tick would take 1ms.
	 *
	 * For a 5ms system tick, [RL_TH0, RL_TL0] is "65536 - 5000". Then 
	 * a system-tick would be
	 * 12 MHz / 12 / 5000 = 200 Hz.
	 */
}

void timer2_ISR(void) interrupt 12 using 1
{
	// Improved ISR for debouncing BUTTON and syncing AVR_ONLINE inputs...
	// called every 5ms

	static char button_history = 0xFF; // initialized to history of unpressed button
	
	bit new_button_state = HIGH; // default unpressed button state
	bit new_avr_online_state = LOW; // default avr_online state

	// Button is debounced using shift register history (8 * 5ms = 40ms) to accomodate most switches.
	// avr_online input is simply sampled.
	
	button_history = (button_history << 1) | I_BUTTON;
	
	if (button_history == 0xFF) 
		new_button_state = HIGH;
	else if (button_history == 0x00)
		new_button_state = LOW;
	
	if (new_button_state != button_state) { // debounced BUTTON has changed state
		button_state = new_button_state;
		button_changed = TRUE;
	} 
	else {
		button_changed = FALSE;
	}

	
	new_avr_online_state = I_AVR_ONLINE;
	
	if (new_avr_online_state != avr_online_state) {
		avr_online_state = new_avr_online_state;
	}

}

void setup()
{
	// Keep P33 (I_AVR_ONLINE) and P34 (I_BUTTON) as Quasi-Bidirectional
	// since we need the internal PullUps

	// P35 (O_BOOST): PushPull Output
	SET_BIT(P3M0, 5);
	CLR_BIT(P3M1, 5);

	// P32 (O_AVR_BUTTON): PushPull Output
	SET_BIT(P3M0, 2);
	CLR_BIT(P3M1, 2);

	// Init States
	system_tick = 0;

	state = SLEEP;
	state_start_time = system_tick;

	button_changed = FALSE;
	button_state = HIGH; // button unpressed
	avr_online_state = LOW;

	// Init I/O's
	O_AVR_BUTTON = LOW;  // we can keep this signal low (to save power) since the ATmega is not yet up, even if low means "pressed"
	O_BOOST = LOW;
	I_BUTTON = HIGH;     // set to weak high => pullups!
	I_AVR_ONLINE = HIGH; // set to weak high => pullups!

	// Timers & Interrupts
	SET_BIT(INT_CLKO, 4); // Interrupt on falling edge of INT2/P34 (Button)

	// Timer 0
	TMOD &= 0xF0;     // T/C10 Mode 0, 16Bit, Auto Reload
	CLR_BIT(AUXR, 7); // Timer 0 in 12T Mode
	TH0 = (65536 - 5000) / 256; // Reload values after overflow, High value
	TL0 = (65536 - 5000) % 256; // Low value

	// Timer 2
	AUXR |= 0x04; //Timer clock is 1T mode
	T2L   = 0xA0; //Initial timer value
	T2H   = 0x15; //Initial timer value


	/* NOTE: Reload Values for STC15 uC:
	 * Any value written into TH0/TL0 are passed to the reload registers
	 * RL_TH0/TL0 as long as TR0=0.
	 * If TR0=1, then the values are written into the hidden reload registers only.
	 */

	ET0 = 1;         // Enable Timer 0 interrupts
	SET_BIT(IE2, 2); // Enable Timer 2 interrupts (IE2, ET2)

	TR0 = 1;      // Start Timer 0
	AUXR |= 0x10; // Start Timer 2

}

unsigned long int ticks_since(unsigned long since)
{
	unsigned long int now = system_tick;

	if (now >= since)
		return (now - since);

	return (now + (1 + ULONG_MAX - since));
}

void transition_to_state(char new_state)
{
	state_start_time = system_tick;
	state = new_state;
}

void main()
{
	setup();
	EA = 1; // Global Interrupt Enable

	while (1)
	{
		switch (state) {
		case SLEEP:
			O_BOOST = LOW;
			O_AVR_BUTTON = LOW; // to save power (prevent back-feeding power to AVR thru input protection diode)

			PCON = 0x02; // Stop/PowerDown Mode
			_nop_();     // We need at least one NOP after waking up

			transition_to_state(IDLE);
			break;

		case IDLE: // wait for button press
			O_AVR_BUTTON = button_state;

			if(button_state == LOW){ // button pressed
				transition_to_state(BOOT);
			}
			break;

		case BOOT:
			O_BOOST = HIGH; // powering the booster, AVR, 5V rail
			O_AVR_BUTTON = button_state; // safe to set button here since it's debounced

			if (avr_online_state == HIGH){ // received ACK from AVR
				transition_to_state(ONLINE);
			}

			if (ticks_since(state_start_time) > 200){ // 1s timeout for AVR response
				transition_to_state(SLEEP);
			}

			break;

		case ONLINE:
			O_AVR_BUTTON = button_state;

			if (avr_online_state == LOW) { // AVR gone offline
				wait_begin_time = system_tick;
				while (ticks_since(wait_begin_time) < 100) {} // wait 0.5s to display "Bye!" message before turning off AVR 5V rail.
				
				transition_to_state(SLEEP);
			}

			break;

		default:
			transition_to_state(SLEEP);
			break;
		}

	}

}
