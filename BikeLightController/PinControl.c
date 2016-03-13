#include <avr/io.h>

#include <stdint.h>

#include "PinControl.h"

void fetOff()
{
	PORTC &= ~(1<<PC1);
}

void fetOn()
{
	PORTC |= (1<<PC1);
}

void fetToggle()
{
	PINC = (1<<PC1);
}

void holdLampInReset()
{
	PORTD &= ~(1<<PD5);
}

void takeLampOutOfReset()
{
	PORTD |= (1<<PD5);
}