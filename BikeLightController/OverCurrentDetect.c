#include <avr/io.h>
#include <avr/interrupt.h>

#include <stdint.h>

#include "PinControl.h"
#include "OverCurrentDetect.h"

void initOverCurrentDetection()
{
	ACSR &= ~(1<<ACIE);
	ACSR |= (1<<ACIS1);
}

void startOverCurrentDetection()
{
	ACSR |= (1<<ACIE);
}

uint_fast8_t testIntOverCurrent()
{
	return (ACSR & (1<<ACI)) || overCurrentTripped();
}

uint_fast8_t overCurrentTripped()
{
	return !(ACSR & (1<<ACO));
}

ISR(ANALOG_COMP_vect)
{
	fetOff();
	holdLampInReset();
	for(;;);
}