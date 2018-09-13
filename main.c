#include <STC15F2K60S2.h>
#include <intrins.h>

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
#define BOOT    1
#define ONLINE  2

volatile int state;
volatile int button_changed;
volatile int button_state;
volatile int avr_online_state;


void extInt2_ISR (void) interrupt 10
{
	_nop_();
}

void timer0_ISR (void) interrupt 1
{
	static float avg_button = 10;
	static float avg_avr_online = 10;

	int new_button_state;
	int new_avr_online_state;

	// Button debouncing using exponential moving average
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


// TODO: timer 1 as system clock

void setup()
{
	// Keep P33 (I_AVR_ONLINE) and P34 (I_BUTTON) as Quasi-Bidirectional
	// since we need the internal PullUps

	// P35 (O_BOOST): PushPull Output
	SET_BIT(P3M0, 5);
	CLR_BIT(P3M1, 5);

	// P32 (O_AVR_BUTTON): PushPull Output
	SET_BIT(P3M1, 2);
	CLR_BIT(P3M1, 2);

	// Init States
	state = SLEEP;

	button_changed = FALSE;
	button_state = HIGH;
	avr_online_state = LOW;

	// Init I/O's
	O_AVR_BUTTON = LOW;  // we can keep this signal low since the ATmega is not yet up, even if low means "pressed"
	O_BOOST = LOW;
	I_BUTTON = HIGH;     // set to weak high => pullups!
	I_AVR_ONLINE = HIGH; // set to weak high => pullups!

	// Timers & Interrupts
	SET_BIT(INT_CLKO, 4);         // Interrupt on falling edge of INT2/P34 (Button)
	TMOD = (TMOD & 0xF0) | 0x00;  // Set T/C0 Mode 0, 16Bit, Auto Reload
	AUXR |= 0x80;                 // Timer 0 in 1T mode, not 12T mode.
	// setting reload values to shorten overflow time
	// any value written into TH0/TL0 is passed to the reload registers
	// RL_TH0/TL0 as long as TR0 = 0, if TR0=1 the values are written into
	// the hidden reload registers only
	// timer frequency = system freq / (65536 - [RL_TH0, RL_TL0]) / 2
	TH0 = (65536 - 100) / 256;    // TODO: Calculation?! Old: 1ms = 1000MZ, davon das High-Byte in TH0
	TL0 = (65536 - 100) % 256;    // das Low-Byte in TL0
	ET0 = 1;                      // Enable Timer 0 Interrupts
	TR0 = 1;                      // Start Timer 0 Running}

void delay(int ms)
{
	unsigned int j  =   0;
	unsigned int g  =   0;
	for(j=0;j<ms;j++)
	{
		for(g=0;g<600;g++)
		{
			_nop_();
			_nop_();
			_nop_();
			_nop_();
			_nop_();
		}
	}
}

void main()
{
	setup();
	EA = 1; // Global Interrupt Enable

	while(1)
	{
		switch (state) {
		case SLEEP:
			PCON = 0x02; // Stop/PowerDown Mode

			// Normally we would check here for (button_changed && button_state == LOW)
			// but debouncing would take too much time, we would enter powerDown mode again
			// before the button is debounced.

			O_AVR_BUTTON = LOW;
			delay(1); // otherwise the fw hangs up
			O_BOOST = HIGH; // TODO: maybe switch?
			state = BOOT;

			break;

		case BOOT:
			O_AVR_BUTTON = button_state;

			if (avr_online_state == HIGH)
				state = ONLINE;

			// TODO: Timeout
			break;

		case ONLINE:
			O_AVR_BUTTON = button_state;

			if (avr_online_state == LOW) {
				O_BOOST = LOW;
				state = SLEEP;
			}
			break;

		default:
			state = SLEEP;
			break;
		}

}

}
