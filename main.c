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


int state;
int button_changed;
int button_state;
int avr_online_state;


void timer0_ISR (void) interrupt 1
{
	static float avg_button = 10;
	static float avg_avr_online = 10;

	int new_button_state;
	int new_avr_online_state;

	// Increment Counter to shorten overflow time
	//TH0 = (65536 - 1000)/256;     // TODO: Calculation?! Old: 1ms = 1000MZ, davon das High-Byte in TH0
	//TL0 = (65536 - 1000)%256;     // das Low-Byte in TL0

	// Button entprellen

	avg_button     = (avg_button * 9+I_BUTTON * 10) / 10;
	avg_avr_online = (avg_avr_online * 9+I_AVR_ONLINE * 10) / 10;

	if (avg_button > 5) {
		new_button_state = HIGH;
	} else {
		new_button_state = LOW;
	}

	if (avg_avr_online > 5) {
		new_avr_online_state = HIGH;
	} else {
		new_avr_online_state = LOW;
	}

	if (new_button_state != button_state) {
		button_state = new_button_state;
		button_changed = 1;
	}

	if (new_avr_online_state != avr_online_state) {
		avr_online_state = new_avr_online_state;
	}
	
}


void setup()
{
	// P33 (I_AVR_ONLINE) to Input only (HiZ)
	P3M0 &= ~(0 << 3);
	P3M1 |= 1 << 3;

	// P34 (I_BUTTON) to Input only (HiZ)
	P3M0 &= ~(0 << 4);
	P3M1 |= 1 << 4;

	// P35 (O_BOOST) to PushPull Output
	P3M0 |= 1 << 5;
	P3M1 &= ~(0 << 5);

	// P32 (O_AVR_BUTTON) to PushPull Output
	P3M0 |= 1 << 2;
	P3M1 &= ~(0 << 2);

	// Init States
	state = SLEEP;
	button_changed = 0;
	button_state = HIGH;
	avr_online_state = LOW;
	
	// Init Outputs
	O_AVR_BUTTON = HIGH;
	O_BOOST = LOW;

	// Timers
	TMOD = (TMOD & 0xF0) | 0x01;  // Set T/C0 Mode 1, 16Bit, No Auto Reload
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
				button_changed = 0;
				O_AVR_BUTTON = LOW;
				O_BOOST = HIGH;
				state = BOOT;
			}
			break;

		case BOOT:
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
			break;
		}
	}

}


