#include <avr/io.h>
#include <avr/interrupt.h>

#include <stdint.h>
#include <string.h>

#include "PointerTricks.h"
#include "ADCReader.h"

#define NCHANNELS 2

#define BITBOOST 4
#define AVGLENGTH (1<<BITBOOST)

static struct {
	struct ChannelData {
		uint_fast16_t store[AVGLENGTH];
		uint_fast8_t avgIdx;
		volatile uint_fast32_t accVal;
		volatile uint_fast16_t avgVal;
		volatile uint_fast16_t bias;
	} chan[NCHANNELS];
	volatile uint_fast8_t adcPendingResults;
} adcState;

void initADC()
{
	memset(&adcState, 0, sizeof(adcState));
}

void startADC()
{
	PRR &= ~(1<<PRADC); // Power on the ADC
	ADCSRA &= ~(1<<ADEN); // Disable the ADC
		ADMUX = (1<<REFS1) | (1<<REFS0) | (1<<MUX1) | (1<<MUX0); // Start off by sampling the "voltage source" line with the 1.1V source as the reference
		ADCSRA = (1<<ADATE) | (1<<ADIF) | (1<<ADIE) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0); // Set the clock rate to clk/16 or 62.5KHz at 1MHz, auto triggering, enabling interrupts while clearing any standing interrupts
	ADCSRA |= (1<<ADEN); // Enable ADC
	ADCSRA |= (1<<ADSC); // Start converting
}

void stopADC()
{
	uint_fast8_t statReg = SREG;

	cli();
		ADCSRA &= ~(1<<ADEN) & ~(1<<ADIE); // Disable the ADC
		PRR |= (1<<PRADC); // Power off the ADC
		ADCSRA |= (1<<ADIF); // Ensure no ADC interrupts are pending
	SREG = statReg;
}

uint_fast8_t testIntADC()
{
	return (ADCSRA & (1<<ADIF)); // Is ADC interrupt pending?
}

ISR(ADC_vect)
{
//PIND = (1<<PD5); // Toggle reset line
	uint_fast16_t adcVal = ADC;

	uint_fast8_t cChan = ((ADMUX & (1<<MUX2))>>MUX2);

	struct ChannelData *chan = adcState.chan + cChan;
	FIX_POINTER(chan);

	chan->avgVal -= chan->store[chan->avgIdx & (AVGLENGTH-1)];
	chan->store[chan->avgIdx & (AVGLENGTH-1)] = adcVal;
	chan->avgVal += adcVal;

	// Remove the channel bias before accumulating
	if(chan->bias <= chan->avgVal) { // Therefore (chan->avgVal - chan->bias) is +ve or zero
		chan->accVal += (chan->avgVal - chan->bias + 4) >> 3; // Do the accumulate. Sadly we have to truncate the precision otherwise we can't track enough current over the whole discharge period
	} else { // (chan->bias > chan->avgVal) Therefore (chan->avgVal - chan->bias) is -ve so (chan->bias - chan->avgVal) is positive and of the same magnitude
		uint_fast16_t subVal = (chan->bias - chan->avgVal + 4) >> 3;
		if(subVal <= chan->accVal) {
			chan->accVal -= subVal;
		} else {
			chan->accVal = 0;
		}
	}
	chan->avgIdx++;

	adcState.adcPendingResults++;

	ADMUX ^= (1<<MUX2); // Switch channels
//PIND = (1<<PD5); // Toggle reset line
}

uint_fast8_t isADCUpdated(uint_fast8_t nSamples)
{
	uint_fast8_t statReg = SREG;
	uint_fast8_t rVal;

	cli();
		rVal = adcState.adcPendingResults;
	SREG = statReg;

	rVal = rVal >= (nSamples<<1);

	if(rVal) {
		cli();
			adcState.adcPendingResults -= (nSamples<<1);
		SREG = statReg;
	}

	return rVal;
}

void adcUpdateVoltageBias()
{
	uint_fast8_t statReg = SREG;

	cli();
		adcState.chan[1].bias = adcState.chan[1].avgVal;
		adcState.chan[1].accVal = 0;
		adcState.adcPendingResults = 0;
	SREG = statReg;
}

void adcUpdateCurrentBias()
{
	uint_fast8_t statReg = SREG;

	cli();
		adcState.chan[0].bias = adcState.chan[0].avgVal;
		adcState.chan[0].accVal = 0;
		adcState.adcPendingResults = 0;
	SREG = statReg;
}

uint_fast16_t getADCVoltageReading()
{
	uint_fast8_t statReg = SREG;
	uint_fast16_t rVal, bias;

	cli();
		rVal = adcState.chan[1].avgVal;
		bias = adcState.chan[1].bias;
	SREG = statReg;

	// Remove the bias before returning
	if(bias < rVal) { // Guard against returning negative
		rVal -= bias;
	}

	return (rVal + (1<<(BITBOOST-1))) >> BITBOOST;	// Return to native resolution of the ADC (but round appropriately)
}

uint_fast16_t getADCCurrentReading()
{
	uint_fast8_t statReg = SREG;
	uint_fast16_t rVal, bias;

	cli();
		rVal = adcState.chan[0].avgVal;
		bias = adcState.chan[0].bias;
	SREG = statReg;

	// Remove the bias before returning
	if(bias < rVal) { // Guard against returning negative
		rVal -= bias;
	}

	return (rVal + (1<<(BITBOOST-1))) >> BITBOOST;	// Return to native resolution of the ADC (but round appropriately)
}

uint_fast32_t getAccumulatedCurrent()
{
	uint_fast8_t statReg = SREG;
	uint_fast32_t rVal;

	cli();
		rVal = adcState.chan[0].accVal;
	SREG = statReg;

	// Accumulated value is already de-biased (and we're unlikely to care about it being twice as big as required)

	return rVal;
}