# giant_led2
Giant led pwm controller

This program applies to a PIC 16F688 microcontroller. The purpose of the program is to read serial commands and adjust three pwm outputs accordingly (for red,green,blue segments of transistor drivers to rgb led lines). 

Compiles with XC8 2.35

Since the 16F688 does not have a pwm peripheral it uses a normal timer interrupt to make pseudo-pwm outputs
