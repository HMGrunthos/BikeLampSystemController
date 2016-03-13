#include <avr/io.h>
#include <avr/interrupt.h>

#include <stdint.h>

#include "PinControl.h"
#include "BikeLightController.h"
#include "LowPowerDetect.h"

void initLowPowerDetection()
{
	EICRA = 0;
}

void startLowPowerDetection()
{
	EIMSK = (1<<INT0);
}

uint_fast8_t testIntLowPower()
{
	return (EIFR & (INTF0)) || lowPowerTripped();
}

uint_fast8_t lowPowerTripped()
{
	return !(PIND & (1<<PD2));
}

ISR(INT0_vect)
{
	doShutdownProcess();
	fetOff();
	for(;;);
}