/*
  wiring_analog.c - analog input and output
  Part of Arduino - http://www.arduino.cc/

  Copyright (c) 2005-2006 David A. Mellis

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General
  Public License along with this library; if not, write to the
  Free Software Foundation, Inc., 59 Temple Place, Suite 330,
  Boston, MA  02111-1307  USA

  Modified 28 September 2010 by Mark Sproul

  $Id: wiring.c 248 2007-02-03 15:36:30Z mellis $
*/

#include "wiring_private.h"
#include "pins_arduino.h"

#define ANALOG_ADC_OVER_SAMPLE_COUNT        64//256U        
#define ANALOG_ADC_OVER_SAMPLE_DIV          4//16

uint8_t analog_resbit = 0;
uint8_t analog_resdir = 0;
uint8_t analog_reference = DEFAULT;

//#if defined(__MD8F__)
//void analogReadResolution(uint8_t res)
//{
//    if(res > 16) res = 16;
//
//    if(res > 12) {
//        analog_resbit = res - 12;
//        analog_resdir = 0;
//    } else {
//        analog_resbit = 12 - res;
//        analog_resdir = 1;
//    }
//}
//#endif

void analogReference(uint8_t mode)
{
	// can't actually set the register here because the default setting
	// will connect AVCC and the AREF pin, which would cause a short if
	// there's something connected to AREF.
	analog_reference = mode;

#if defined(__MD328D__) || defined(__MD328P__)
	#if defined(__MD328D__)
	if(analog_reference == INTERNAL2V56) {
		VCAL = VCAL2;
	} else {
		VCAL = VCAL1;
	}
	#else
	// set analog reference for ADC/DAC
	if(analog_reference == EXTERNAL) {
		DACON = (DACON & 0x0C) | 0x1;
		if((PMX2 & 0x2) == 0x2) {
			GPIOR0 = PMX2 & 0xfd;
			PMX2 = 0x80; 
			PMX2 = GPIOR0;
		}
	} else if(analog_reference == DEFAULT) {
		DACON &= 0x0C;
	} else {
		DACON = (DACON & 0x0C) | 0x2;
		cbi(ADCSRD, REFS2);
		if(analog_reference == INTERNAL2V048) {
			VCAL = VCAL2;	// 2.048V
		} else if(analog_reference == INTERNAL4V096) {
			VCAL = VCAL3;	// 4.096V
			sbi(ADCSRD, REFS2);
		} else	{
			VCAL = VCAL1;	// 1.024V
		}
	}
	#endif

	ADMUX = (analog_reference << 6);
#endif
}

static uint16_t adcRead()
{
	volatile uint8_t tmp = 0;

	// start the conversion
	sbi(ADCSRA, ADSC);

	// ADSC is cleared when the conversion finishes
	while (bit_is_set(ADCSRA, ADSC));

	// read low byte firstly to cause high byte lock.
	tmp = ADCL;

	return (ADCH << 8) | tmp;
}

#if defined(__MD328D__) || defined(__MD328P__)
int analogRead_Bsp(uint16_t sampleCount, uint16_t divCount)
{
    uint8_t low, high;
    uint16_t i, j;
    uint8_t validCnt;
    uint16_t advTmp;
    uint32_t advSum = 0;
    //uint16_t adcBuffer[buffSize];

    //delay(1);
    for( i = 0; i < sampleCount; i++ )
    {
        // start the conversion
        sbi(ADCSRA, ADSC);

        // ADSC is cleared when the conversion finishes
        while (bit_is_set(ADCSRA, ADSC));

        // we have to read ADCL first; doing so locks both ADCL
        // and ADCH until ADCH is read.  reading ADCL second would
        // cause the results of each conversion to be discarded,
        // as ADCL and ADCH would be locked when it completed.
        low  = ADCL;
        high = ADCH;

        // combine the two bytes
        //advTmp = (high << 6) | (low >> 2);
        advTmp = high;
        advTmp <<= 8;
        advTmp |= low;

//  	if(analog_reference == DEFAULT)
//  		advTmp *= 0.98;
//  	else if(analog_reference == EXTERNAL)
//  		advTmp *= 0.97;

        //adcBuffer[i] = advTmp;

        advSum += advTmp;
    }

    advTmp = advSum / divCount;

    return (int)advTmp;
}

#endif

int __analogRead(uint8_t pin)
{
	uint16_t pVal;
#if defined(__MD328P__)
	uint16_t nVal;
	
	// enable/disable internal 1/5VCC channel
	ADCSRD &= 0xf0;
    if(pin == V5D1 || pin == V5D4) {
        ADCSRD |= 0x06;
    }
#endif

#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
	if (pin >= 54) pin -= 54; // allow for channel or pin numbers
#elif defined(__AVR_ATmega32U4__)
	if (pin >= 18) pin -= 18; // allow for channel or pin numbers
#elif defined(__AVR_ATmega1284__) || defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega644__) || defined(__AVR_ATmega644A__) || defined(__AVR_ATmega644P__) || defined(__AVR_ATmega644PA__)
	if (pin >= 24) pin -= 24; // allow for channel or pin numbers
#elif defined(analogPinToChannel) && (defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__))
	pin = analogPinToChannel(pin);
#else
	if (pin >= 14) pin -= 14; // allow for channel or pin numbers
#endif

#if defined(__AVR_ATmega32U4__)
	pin = analogPinToChannel(pin);
	ADCSRB = (ADCSRB & ~(1 << MUX5)) | (((pin >> 3) & 0x01) << MUX5);
#elif defined(ADCSRB) && defined(MUX5)
	// the MUX5 bit of ADCSRB selects whether we're reading from channels
	// 0 to 7 (MUX5 low) or 8 to 15 (MUX5 high).
	ADCSRB = (ADCSRB & ~(1 << MUX5)) | (((pin >> 3) & 0x01) << MUX5);
#endif
  
	// set the analog reference (high two bits of ADMUX) and select the
	// channel (low 4 bits).  this also sets ADLAR (left-adjust result)
	// to 0 (the default).
#if defined(ADMUX)
	ADMUX = (analog_reference << 6) | (pin & 0x1f);
#endif

	// without a delay, we seem to read from the wrong channel
	//delay(1);
#if defined(__MD328D__) || defined(__MD328P__)
    //Set ADC clock
    ADCSRA &= 0xf8;
    //ADCSRA |= 0x00;         //Adc Clock = fosc / 2
    //ADCSRA |= 0x01;         //Adc Clock = fosc / 2
    //ADCSRA |= 0x02;         //Adc Clock = fosc / 4
    ADCSRA |= 0x03;         //Adc Clock = fosc / 8
    //ADCSRA |= 0x04;         //Adc Clock = fosc / 16
    //ADCSRA |= 0x05;           //Adc Clock = fosc / 32
    //ADCSRA |= 0x06;           //Adc Clock = fosc / 64
    //ADCSRA |= 0x07;           //Adc Clock = fosc / 128
    // without a delay, we seem to read from the wrong channel

    uint16_t advTmp = analogRead_Bsp(16, 64);
//  if(analog_reference == DEFAULT)
//		advTmp *= 0.98;
//	else if(analog_reference == EXTERNAL)
//		advTmp *= 0.97;
    
    return advTmp;
    //return (advTmp >> 2);
#else
#if defined(ADCSRA) && defined(ADCL)
	#if defined(__MD328P__)
	sbi(ADCSRC, SPN);
	nVal = adcRead();
	cbi(ADCSRC, SPN);
	#endif
	
	pVal = adcRead();

	#if defined(__MD328P__)
	pVal = (pVal + nVal) >> 1;
	#endif
#else
	// we dont have an ADC, return 0
	pVal = 0;
#endif

	// gain-error correction
#if defined(__MD328D__)
	pVal -= (pVal >> 5);
#elif defined(__MD328P__)
	pVal -= (pVal >> 7);
#endif
	// standard device from atmel
	return pVal;
#endif
}

int analogRead(uint8_t pin)
{
//#if defined(__MD8F__)
//    if(analog_resbit == 0)
//        return __analogRead(pin);
//
//    if(analog_resdir == 1) {
//        return __analogRead(pin) >> analog_resbit;
//    } else {
//        return __analogRead(pin) << analog_resbit;
//    }
//#else
	return __analogRead(pin);
//#endif
	
}

// Right now, PWM output only works on the pins with
// hardware support.  These are defined in the appropriate
// pins_*.c file.  For the rest of the pins, we default
// to digital output.

void analogWrite(uint8_t pin, int val)
{
	// We need to make sure the PWM output is enabled for those pins
	// that support it, as we turn it off when digitally reading or
	// writing with them.  Also, make sure the pin is in output mode
	// for consistenty with Wiring, which doesn't require a pinMode
	// call for the analog output pins.
	
	if(MD_NOT_DACO(pin)) {
		pinMode(pin, OUTPUT);
	}

	if (val == 0 && MD_NOT_DACO(pin)) {
		digitalWrite(pin, LOW);
	} else if (val == 255 && MD_NOT_DACO(pin)) {
		digitalWrite(pin, HIGH);
	} else {
		switch(digitalPinToTimer(pin)) {
			// XXX fix needed for atmega8
			#if defined(TCCR0) && defined(COM00) && !defined(__AVR_ATmega8__)
			case TIMER0A:
				// connect pwm to pin on timer 0
				sbi(TCCR0, COM00);
				OCR0 = val; // set pwm duty
				break;
			#endif

			#if defined(TCCR0A) && defined(COM0A1)
			case TIMER0A:
				// connect pwm to pin on timer 0, channel A
				sbi(TCCR0A, COM0A1);
				OCR0A = val; // set pwm duty
				break;
			#endif

			#if defined(TCCR0A) && defined(COM0B1)
			case TIMER0B:
				// connect pwm to pin on timer 0, channel B
				sbi(TCCR0A, COM0B1);
				OCR0B = val; // set pwm duty
				break;
			#endif

			#if defined(TCCR1A) && defined(COM1A1)
			case TIMER1A:
				// connect pwm to pin on timer 1, channel A
				sbi(TCCR1A, COM1A1);
				OCR1A = val; // set pwm duty
				break;
			#endif

			#if defined(TCCR1A) && defined(COM1B1)
			case TIMER1B:
				// connect pwm to pin on timer 1, channel B
				sbi(TCCR1A, COM1B1);
				OCR1B = val; // set pwm duty
				break;
			#endif

			#if defined(TCCR2) && defined(COM21)
			case TIMER2:
				// connect pwm to pin on timer 2
				sbi(TCCR2, COM21);
				OCR2 = val; // set pwm duty
				break;
			#endif

			#if defined(TCCR2A) && defined(COM2A1)
			case TIMER2A:
				// connect pwm to pin on timer 2, channel A
				sbi(TCCR2A, COM2A1);
				OCR2A = val; // set pwm duty
				break;
			#endif

			#if defined(TCCR2A) && defined(COM2B1)
			case TIMER2B:
				// connect pwm to pin on timer 2, channel B
				sbi(TCCR2A, COM2B1);
				OCR2B = val; // set pwm duty
				break;
			#endif

			#if defined(TCCR3A) && defined(COM3A1)
			case TIMER3A:
				// connect pwm to pin on timer 3, channel A
				sbi(TCCR3A, COM3A1);
				OCR3A = val; // set pwm duty
				break;
			#endif

			#if defined(TCCR3A) && defined(COM3B1)
			case TIMER3B:
				// connect pwm to pin on timer 3, channel B
				sbi(TCCR3A, COM3B1);
				OCR3B = val; // set pwm duty
				break;
			#endif

			#if defined(TCCR3A) && defined(COM3C1)
			case TIMER3C:
				// connect pwm to pin on timer 3, channel C
				sbi(TCCR3A, COM3C1);
				OCR3C = val; // set pwm duty
				break;
			#endif

			#if defined(TCCR4A)
			case TIMER4A:
				//connect pwm to pin on timer 4, channel A
				sbi(TCCR4A, COM4A1);
				#if defined(COM4A0)		// only used on 32U4
				cbi(TCCR4A, COM4A0);
				#endif
				OCR4A = val;	// set pwm duty
				break;
			#endif
			
			#if defined(TCCR4A) && defined(COM4B1)
			case TIMER4B:
				// connect pwm to pin on timer 4, channel B
				sbi(TCCR4A, COM4B1);
				OCR4B = val; // set pwm duty
				break;
			#endif

			#if defined(TCCR4A) && defined(COM4C1)
			case TIMER4C:
				// connect pwm to pin on timer 4, channel C
				sbi(TCCR4A, COM4C1);
				OCR4C = val; // set pwm duty
				break;
			#endif
				
			#if defined(TCCR4C) && defined(COM4D1)
			case TIMER4D:				
				// connect pwm to pin on timer 4, channel D
				sbi(TCCR4C, COM4D1);
				#if defined(COM4D0)		// only used on 32U4
				cbi(TCCR4C, COM4D0);
				#endif
				OCR4D = val;	// set pwm duty
				break;
			#endif

							
			#if defined(TCCR5A) && defined(COM5A1)
			case TIMER5A:
				// connect pwm to pin on timer 5, channel A
				sbi(TCCR5A, COM5A1);
				OCR5A = val; // set pwm duty
				break;
			#endif

			#if defined(TCCR5A) && defined(COM5B1)
			case TIMER5B:
				// connect pwm to pin on timer 5, channel B
				sbi(TCCR5A, COM5B1);
				OCR5B = val; // set pwm duty
				break;
			#endif

			#if defined(TCCR5A) && defined(COM5C1)
			case TIMER5C:
				// connect pwm to pin on timer 5, channel C
				sbi(TCCR5A, COM5C1);
				OCR5C = val; // set pwm duty
				break;
			#endif
			#if defined(__MD328D__) || defined(__MD328P__)
			case MDDAO0:
				DAL0 = val; 
				break;
			#endif
			#if defined(__MD328D__)
			case MDDAO1:
				DAL1 = val;
				break;
			#endif
			case NOT_ON_TIMER:
			default:
				if (val < 128) {
					digitalWrite(pin, LOW);
				} else {
					digitalWrite(pin, HIGH);
				}
		}
	}
}

