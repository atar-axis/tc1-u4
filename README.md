# U4
Alternative FW for U4 (STC15 8051 uC) on TransistorTester TC1.
Use in Combination with https://github.com/svn2github/transistortester

## Compilation

This project is for Keil's µVision IDE, which can be found here: http://www.keil.com/

## Connection

Just grab any kind of USB-to-UART(3V3) Adapter and hook up...
* TXD (Stick) to P1,
* RXD to P2 and
* GND to GND

Alternatively you can either desolder it and use any kind of SOP8 ↔ DIP8 socket converter
or a SOIC (SOP8) test clip.

## Upload / Flashing

Download the STC-ISP Software here: http://www.stcmicro.com/rar/stc-isp6.86.rar  
Open it, leave everything except:
* Input IRC frequency, set to 12.000 MHz

Press "Open Code File" and select the firmware (u4.hex).
Press "Download/Program" and now hook up 3V3 to VCC (power the IC).

Wait for the firmware being uploaded.
