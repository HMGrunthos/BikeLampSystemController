#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

#include <stdint.h>

#include "ADCReader.h"
#include "TimerServices.h"

// Timer-1 period is 256.000mS

static struct TimerState {
	volatile uint_fast32_t t0Overflow;
} tState = {0};

void initTimers()
{
	// Use Timer-1 as a generic time keeping device
	TCCR1A = (1<<WGM11);
	TCCR1B = (1<<WGM13) | (1<<WGM12) | (1<<CS12) | (1<<CS10);
	ICR1 = 1999;	// Period .256 seconds
}

void startTimers()
{
	uint_fast16_t tThen, tNow;

	tThen = tNow = TCNT1;
	do {
		tNow = TCNT1;
	} while(tThen == tNow); // Synchronise on timer tick

	// Ensure all timer states are reset
	TCNT1 = 0;
	TIFR1 |= (1<<TOV1);
	tState.t0Overflow = 0;

	// And enable interrupts
	TIMSK1 = (1<<TOIE1); // Enable timer overflow interrupt at 3.90625Hz (period of 256ms)
}

void stopTimers()
{
	TIMSK1 &= ~(1<<TOIE1); // Disable timer overflow interrupt
}

uint_fast8_t testIntTimers()
{
	return TIFR1 & (1<<TOV1);
}

void msWait(uint_fast32_t duration)
{
	uint_fast32_t msStart;
	uint_fast32_t msNow;

	msStart = getTime();

	do {
		msNow = getTime();
		if(msNow < msStart) { // Clock rollover!
			msWait(duration - (0xFFFFFFFF - msStart)); // Wait for the remaining time
			break;
		}
	} while((msNow - msStart) < duration);
}

void tickWait()
{
	uint_fast32_t firstTick, lastTick;

	firstTick = lastTick = getTickNumber();

	do {
		lastTick = getTickNumber();
	} while(firstTick == lastTick);
}

uint_fast32_t getTime()
{
	uint_fast32_t tOverflows;
	uint_fast16_t tTicks;
	uint_fast8_t tIntReg;
	uint_fast8_t statReg = SREG;

	cli();
		tOverflows = tState.t0Overflow;
		tTicks = TCNT1;
		tIntReg = TIFR1;
	SREG = statReg;

	if((tIntReg & (1<<TOV1)) && (tTicks < 1999)) { // Account for any missing interrupts (and we've not crossed the overflow boundary)
		tOverflows++;
	}

	return ((tOverflows << 8) + ((((tTicks + (tTicks>>1))>>6) + tTicks + 4)>>3));
}

uint_fast32_t getTickNumber()
{
	uint_fast32_t lastTick;
	uint_fast8_t statReg = SREG;

	cli();
		lastTick = tState.t0Overflow;
	SREG = statReg;

	return lastTick;
}

uint_fast16_t noIntTimerStart()
{
	TCNT1 = 0;
	TIFR1 |= (1<<TOV1);
	return 0;
}

uint_fast8_t noIntTimerEnded()
{
	return TIFR1 & (1<<TOV1);
}

void noIntWait(uint_fast16_t nTicks)
{
	uint_fast16_t tThen, tNow;

	tThen = tNow = noIntTimerStart();

	for(uint_fast16_t tTicks = 0; tTicks < nTicks; tTicks++) {
		do {
			tNow = TCNT1;
		} while(tThen == tNow);
		tThen = tNow;
	}
}

void noIntTone(uint_fast8_t freq, uint_fast8_t output)
{
	uint_fast16_t tThen, tNow;

	tThen = tNow = noIntTimerStart();

	do {
		for(int i = 0; i < freq; i++) {
			do {
				tNow = TCNT1;
			} while(tThen == tNow);
			tThen = tNow;
		}
		if(output) {
			PIND = (1<<PD5); // Toggle reset line
		}
		wdt_reset();
	} while(!noIntTimerEnded());
}

ISR(TIMER1_OVF_vect)
{
	tState.t0Overflow++;
}