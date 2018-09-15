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

int state;
unsigned long int state_start_time;

volatile unsigned long int system_tick;

volatile int button_changed;
volatile int button_state;
volatile int avr_online_state;


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
	 * tick frequenycy =
	 * system frequency / 12 / (65536 - [RL_TH0, RL_TL0])
	 *
	 * The '12' is because we set the AUXR register to increment the
	 * timer register only every 12th clock cycle.
	 *
	 * Supposing that the system clock is set to 12.000 MHz, and
	 * [RL_TH0, RL_TL0] is "65536 - 1000", then a system-tick would be
	 * 12 MHz / 12 / 1000 = 1 kHz.
	 *
	 * Therefore a system tick would take 1ms
	 */
}

void timer1_ISR(void) interrupt 3
{
	static float avg_button = 10;
	static float avg_avr_online = 10;

	int new_button_state;
	int new_avr_online_state;

	// Button are debounced using exponential moving average
	// avg = alpha * avg + (1 - alpha) * input, alpha < 1
	// TODO: inefficient, replace
	avg_button     = 0.9 * avg_button + 0.1 * (I_BUTTON * 10);
	avg_avr_online = 0.9 * avg_avr_online + 0.1 * (I_AVR_ONLINE * 10);

	(avg_button > 5) ? (new_button_state = HIGH) : (new_button_state = LOW);
	(avg_avr_online > 5) ? (new_avr_online_state = HIGH) : (new_avr_online_state = LOW);

	if (new_button_state != button_state) {
		button_state = new_button_state;
		button_changed = 1;
	} else {
		button_changed = 0;
	}

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
	button_state = HIGH;
	avr_online_state = LOW;

	// Init I/O's
	O_AVR_BUTTON = LOW;  // we can keep this signal low since the ATmega is not yet up, even if low means "pressed"
	O_BOOST = LOW;
	I_BUTTON = HIGH;     // set to weak high => pullups!
	I_AVR_ONLINE = HIGH; // set to weak high => pullups!

	// Timers & Interrupts
	SET_BIT(INT_CLKO, 4); // Interrupt on falling edge of INT2/P34 (Button)

	// Timer 0
	TMOD &= 0xF0;     // Mode 0, 16Bit, Auto Reload
	CLR_BIT(AUXR, 7); // 12T Mode
	TH0 = (65536 - 1000) / 256; // Reload values after overflow, High value
	TL0 = (65536 - 1000) % 256; // Low value

	// Timer 1
	TMOD &= 0x0F;      // Set T/C1 Mode 0, 16Bit, Auto Reload
	CLR_BIT(AUXR, 6);  // Timer 1 in 12T mode, not 1T mode
	TH1 = (65536 - 500) / 256; // High byte reload value
	TL1 = (65536 - 500) % 256; // Low byte reload value

	/* NOTE on Reload Values for STC15 uC:
	 * Any value written into TH0/TL0 are passed to the reload registers
	 * RL_TH0/TL0 as long as TR0=0.
	 * If TR0=1, then the values are written into the hidden reload registers only.
	 */

	ET0 = 1; // Enable Timer 0 interrupts
	ET1 = 1; // Enable Timer 1 interrupts

	TR0 = 1; // Start Timer 0
	TR1 = 1; // Start Timer 1
}

unsigned long int ticks_since(unsigned long since)
{
	unsigned long int now = system_tick;

	if (now >= since)
		return (now - since);

	return (now + (1 + ULONG_MAX - since));
}

void set_state(int new_state)
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
			PCON = 0x02; // Stop/PowerDown Mode
			_nop_();     // We need at least one NOP after waking up
			set_state(IDLE);

			break;

		case IDLE:
			if(button_changed && button_state == LOW){
				O_BOOST = HIGH;     // powering the booster, avr, 5v rail
				O_AVR_BUTTON = LOW; // notify avr about the pressed button
				set_state(BOOT);
			}

			break;

		case BOOT:
			O_AVR_BUTTON = button_state;

			if (avr_online_state == HIGH){
				set_state(ONLINE);
			}

			if (ticks_since(state_start_time) > 1000){
				set_state(SLEEP);
			}

			break;

		case ONLINE:
			O_AVR_BUTTON = button_state;

			if (avr_online_state == LOW) {
				O_BOOST = LOW;
				set_state(SLEEP);
			}

			break;

		default:
			set_state(SLEEP);
			break;
		}

	}

}
