/*
 * File:   main.c
 * Author: tdp301
 *
 * Created on March 28, 2022, 8:12 PM
 * 
 * Compiled with XC8 v2.35
 * 
 * Compiled for PIC16F688
 * 
 * Pins used:
 *	2 CLKIN
 *  4 MCLR (program only)
 *  5 RX (serial)
 *  6 TX (serial)
 *  7 C3 Heartbeat led at 26 hz
 *  8 C2 FET for turning on LED
 *  10 C0 blue pwm
 *  11 A2 green pwm
 *  12 A1 red pwm, (and ICSPCLK program only)
 *  13 A0 power led aka uart activity, (and ICSPDAT program only)
 * 
 */

#include "ledCtrl.h"


int green_duty = 0; //0=off, 255 = full on
int red_duty = 0;
int blue_duty = 0;

unsigned int led_on = 1;
int hb_state = 0;
int pled_state = 0;

void process_buffer(char *buffer, int len)
{
	if (len < 4)
	{
		// printf("Len bad\r\n");
		return;
	}
	if ((buffer[len - 1] == '%') &&
		(buffer[len - 2] == '%') &&
		(buffer[len - 3] == '%'))
	{
		send_string("led off");
		led_on = 0;
		return;
	}
	else if ((buffer[len - 1] == '$') &&
		(buffer[len - 2] == '$') &&
		(buffer[len - 3] == '$'))
	{
		led_on = 1;
		send_string("led on");
		return;
	}

	if (buffer[0] != '$')
	{
		return;
	}
	
	if (len != 7)
	{
		return;
	}
	
	//Copy two byte ascii pieces to a temp string then convert hex to actual number
	char tempStr[3] = {0,0,0};
	
	strncpy(tempStr, buffer+1, 2);
	red_duty = (int)strtol(tempStr, NULL,16);
	
	strncpy(tempStr, buffer+3, 2);
	green_duty = (int)strtol(tempStr, NULL,16);

	strncpy(tempStr, buffer+5, 2);
	blue_duty = (int)strtol(tempStr, NULL,16);
	
	//debug
//	char temp[30];
//	sprintf((char *)temp,"red duty is [%d]\r\n", red_duty);
//	send_string(temp);
	
}




void main()
{
	char input_buffer[30];
	int index = 0;

	init();
	init_serial();

	send_string("Startup complete\r\n");

	//Packet:
	//$,RRR,GGG,BBB,#
	//$rrggbb# (rrggbb are HEX)
	//%%%%# led off
	//$$$$# led on

	while (1)
	{
		if (RCIF)
		{
			//Serial input available. Add to buffer
			input_buffer[index] = RCREG;
			RCIF = 0;
			_putc(input_buffer[index]);	//echo char

			pled_state = !pled_state;
			LED_POWER = pled_state != 0;

			if (input_buffer[index] == '#')
			{
				//process
				process_buffer(input_buffer, index);
				index = 0;

			}
			else
			{
				index++;
				if (index >= 30)
					index = 0;
			}

			
		}
		
		if (RCSTAbits.OERR)
		{
			//Receive overflow, restart port
			RCSTAbits.CREN = 0;
			;
			RCSTAbits.CREN = 1;
		}

		;
	} //end while(1)
}

unsigned long  static_timer2_duty = 0;

void __interrupt() generalInt(void)
{
	if ((TMR1IF != 0) || (INTCONbits.T0IF != 0))
	{

		static_timer2_duty++;

		if (static_timer2_duty >= 256)
		{
			static_timer2_duty = 0;
			hb_state= ! hb_state;
			HB_LED = hb_state != 0;
		}

		if (led_on)
		{
			FET_PIN = 1;
		}
		else
		{
			FET_PIN = 0;
		}


		if (static_timer2_duty < red_duty)
		{
			RED_LED = 1;
		}
		else
		{
			RED_LED = 0;
		}

		if (static_timer2_duty < blue_duty)
		{
			BLUE_LED = 1;
		}
		else
		{
			BLUE_LED = 0;
		}

		if (static_timer2_duty < green_duty)
		{
			GREEN_LED = 1;
		}
		else
		{
			GREEN_LED = 0;
		}

		INTCONbits.T0IF = 0;
	}
	
	
}

/**
 * Set up non-serial registers
 */
void init()
{
	//Set up timer 0
	// If incrementing with no prescale at 20mhz:
	// [Fosc/4] * 256 -> 0.0000512 s or 19.531 khz per tick, or 76hz per full cycle (256 interrupts)
	OPTION_REGbits.T0CS = 0;	//timer0 in timer mode
	INTCONbits.T0IF = 0;
	INTCONbits.T0IE = 1;	//enable interrupt
	GIE = 1;
	PEIE = 1;
	
	LED_POWER = 0; //start led off

	//All TRISC outputs  except c5 (rx)
	TRISC = 0b100000;
	
	//
	CMCON0bits.CM = 0b111;
	ANSEL = 0;	//all analog pins off
	
	//A0,1,2 output
	TRISAbits.TRISA0 = 0;
	TRISAbits.TRISA1 = 0;
	TRISAbits.TRISA2 = 0;
	
	
	red_duty = 255;
	green_duty = 0;
	blue_duty = 128;
}

/**
 * Set up serial (uart) port
 */
void init_serial()
{
	//Setup baud rate to 9600 baud assuming 20Mhz clock
	TXSTAbits.BRGH = 1;
	BAUDCTLbits.BRG16 = 0;
	SPBRG = 129;	

	//Setup port
	RCSTA = 0; //note by default the uart will be 8 bits, asynchronous 
	RCSTAbits.SPEN = 1; //enable
	RCSTAbits.CREN = 1;
	TXSTAbits.TXEN = 1;
}

void _putc(char c)
{
	TXREG = c;

	while (TXSTAbits.TRMT == 0)
	{
		;
	}
}

//Send a string of characters (null terminated)
void send_string(char * buf)
{
	uint16_t i = 0;

	while ((buf[i] != '\0') && (i < 256))
	{
		_putc(buf[i]);

		i++;
	}
}