#include <STC15F2K60S2.h>
#include <intrins.h>

#define LOW  0
#define HIGH 1

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


void timer0_ISR (void) interrupt 1
{
	static float avg_button = 10;
	static float avg_avr_online = 10;

	int new_button_state;
	int new_avr_online_state;

	// Increment Counter to shorten overflow time
	TH0 = (65536 - 1000) / 256; // TODO: Calculation?! Old: 1ms = 1000MZ, davon das High-Byte in TH0
	TL0 = (65536 - 1000) % 256; // das Low-Byte in TL0

	// Button debouncing using exponential moving average
	// ? avg = alpha * avg + (1 - alpha) * input, alpha < 1
	avg_button = 0.9 * avg_button + 0.1 * (I_BUTTON * 10);
	// avg_button     = (avg_button * 9     + I_BUTTON * 100)     / 10;
	avg_avr_online = (avg_avr_online * 9 + I_AVR_ONLINE * 100) / 10;

	if (avg_button > 5) {
		new_button_state = HIGH;
	} else {
		new_button_state = LOW;
	}

	if (avg_avr_online > 50) {
		new_avr_online_state = HIGH;
	} else {
		new_avr_online_state = LOW;
	}

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
	P3M0 |= 1 << 5;
	P3M1 &= ~(0 << 5);

	// P32 (O_AVR_BUTTON): PushPull Output
	P3M0 |= 1 << 2;
	P3M1 &= ~(0 << 2);

	// Init States
	state = SLEEP;
	button_changed = 0;
	button_state = HIGH;
	avr_online_state = LOW;

	// Init I/O's
	O_AVR_BUTTON = HIGH;
	O_BOOST = LOW;
	I_BUTTON = 1;     // set to weak high => pullups!
	I_AVR_ONLINE = 1; // set to weak high => pullups!

	// Timers
	TMOD = (TMOD & 0xF0) | 0x01;  // Set T/C0 Mode 1, 16Bit, Manual Reload
	ET0 = 1;                      // Enable Timer 0 Interrupts
	TR0 = 1;                      // Start Timer 0 Running
	EA = 1;                       // Global Interrupt Enable
}

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

	while(1)
	{
		switch (state) {
		case SLEEP:
			// TODO: POWER SAVING OPTIONS
			// deactivate timer, enable interrupt2, ...

			if (button_changed && button_state == LOW) {
				//button_changed = 0;
				O_AVR_BUTTON = LOW;
				delay(1); // otherwise the fw hangs up
				O_BOOST = HIGH;
				state = BOOT;
			}


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
				//button_changed = 0; // clear the button event, otherwise you cannot shut down
			}
			break;

		default:
			break;
		}

}

}
