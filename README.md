# TC1 U4
Alternative firmware for U4 (STC15 8051 uC) on TransistorTester derivate TC1.
Use in Combination with https://github.com/svn2github/transistortester

## Compilation

This project is for Keil µVision IDE, which can be found here: http://www.keil.com/
You can either compile the fw yourself, use the latest precompiled fw in /Objects or download one of the released fw files.

## Connection

Just grab any kind of USB-to-UART(3V3) Adapter and hook up...
* TXD (Stick) to P1,
* RXD to P2 and
* GND to GND

Alternatively you can either desolder it and use some SOP8 ↔ DIP8 socket converter
or a SOIC (SOP8) test clip.

## Upload / Flashing

Download the STC-ISP Software here: http://www.stcmicro.com/rar/stc-isp6.86.rar  
Open it but leave everything as it is, except:
* Input IRC frequency
which is set to 12.000 MHz.

Press "Open Code File" and select the firmware (u4.hex).

Be prepared to perform an unpowered cold start of the STC15, i.e. to connect 3.3V VCC from the programmer to the chips VCC (pin2) but don't do that now.

Press "Download/Program" and now hook up 3.3V from the programmer to the chips VCC (power the IC).

Wait for the firmware being uploaded.

Alternatively it can be flashed in a similar manner (unpowered cold start) with stcgal (https://github.com/grigorig/stcgal) on Linux:
```
stcgal  -t 12000 -D Objects/u4.hex
```

## Troubleshooting
- some USB-to-UART adapters have some voltage floating on RX and TX which will power the STC15L right from the beginning so that it can't perform a true cold start - you may need to find a different adapter in this case. I had bad luck with Chinese fake FTDI232RL and good luck with genuine Prolific PL2303HX
- RX and TX may need to be swapped
