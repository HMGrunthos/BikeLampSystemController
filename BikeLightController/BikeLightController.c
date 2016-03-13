#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/sleep.h>

#include <stdint.h>

#include "twi.h"
#include "PinControl.h"
#include "ADCReader.h"
#include "OverCurrentDetect.h"
#include "LowPowerDetect.h"
#include "TimerServices.h"
#include "LampControl.h"
#include "BikeLightController.h"

static void goToSleep();
static uint_fast8_t interruptsPending();
static uint_fast8_t initAVR();

int main(void)
{
	static uint_fast8_t resetSource;

	resetSource = initAVR();

	// if(resetSource & ((1<<WDRF) | (1<<BORF))) { // If either brown-out or watchdog caused a reset
	if(resetSource & (1<<WDRF)) { // If watchdog caused a reset
		for(;;); // Then it's probably best to stay shutdown
	}

	wdt_enable(WDTO_500MS);

	initTimers();
	initADC();
	initOverCurrentDetection();
	initLowPowerDetection();

	sei(); // Interrupts on as soon as possible

	// Calibrate the analogue channels for bias...
	for(int tCnt = 0; tCnt < 20; tCnt++) { // Wait for a bit (about 2 seconds)
		wdt_reset();
		noIntWait(781);
	}
	startADC();
		while(!isADCUpdated(64)) { // Wait for the ADC and power levels to settle
			wdt_reset();
		}
	stopADC();

	// We can't update the voltage bias because we get some leakage through the diode when the startup power is applied
	adcUpdateCurrentBias();

	// If the over current is already tripped then we're in trouble and can't detect the over current condition
	if(overCurrentTripped() || testIntOverCurrent()) {
		for(;;); // In which case trigger the watchdog
	}

	noIntTimerStart(); // This starts a timer period (256mS) that's not governed by timer interrupts
	fetOn(); // Apply main power

	do {
		if(overCurrentTripped() || testIntOverCurrent()) { // If the over current has tripped
			fetOff(); // Power off
			holdLampInReset();
			for(;;); // Trigger watchdog
		} else { // If the over current hasn't tripped
			startOverCurrentDetection(); // Then enable interrupt based over current detection
		}
		wdt_reset();
	} while(!noIntTimerEnded()); // And leave it a while to let the current stabilise before continuing

	// We're now confident that main power has been applied is stable and is not over current
	if(lowPowerTripped() || testIntLowPower()) { // If the low power signal has tripped
		doShutdownProcess();
		fetOff(); // Power off
		for(;;); // Trigger watchdog
	} else {
		startLowPowerDetection(); // Then enable interrupt based low power detection
	}

	startTimers();
	startADC();
	twi_init();
	// Lamp reset...
	fullInit23008();
	lampPowerDown(96); // Make sure the lamp rheostat starts off in a known state (lowest power)

	uint_fast8_t sampleDelay = 32;
	uint_fast32_t lastTick = getTickNumber();
	uint_fast32_t lastTLast = getTime();

	for(;;) {
		wdt_reset();
		goToSleep();
		// PIND = (1<<PD5); // Toggle reset line
		if(isADCUpdated(sampleDelay)) {
			PIND = (1<<PD5); // Toggle reset line
			uint_fast32_t accCurrent = getAccumulatedCurrent();
			uint_fast16_t curCurrent = getADCCurrentReading();
			uint_fast16_t curVoltage = getADCVoltageReading();

			if(accCurrent > 1574074) {
				doShutdownProcess();
				fetOff();
				for(;;);
			}


			if(curCurrent > 20) {
				fetOff();
				for(;;);
			}


			if(curVoltage < 600) {
				doShutdownProcess();
				fetOff();
				for(;;);
			}

			testLampState(&lastTLast);

			sampleDelay = 1;
		} else {
			uint_fast32_t currentTick = getTickNumber();
			if(currentTick != lastTick) {
				lastTick = currentTick;
				// Poll the lamp controller
				PIND = (1<<PD5); // Toggle reset line
			}
		}
	}
}

static void goToSleep()
{
	set_sleep_mode(SLEEP_MODE_IDLE);
	cli();
		if(!interruptsPending()) {
			sleep_enable();
			sei();
			sleep_cpu();
			sleep_disable();
		}
	sei();
}

static uint_fast8_t interruptsPending()
{
	return testIntADC() || testIntOverCurrent() || testIntLowPower() || testIntTimers();
}

static uint_fast8_t initAVR()
{
	uint_fast8_t resetSource = MCUSR;
	MCUSR = 0; // Reset the reset source register

	wdt_disable(); // Disable the watchdog

	DDRC |= (1<<PC1); // Only totem pole output is the FET driver
	DDRD |= (1<<PD5); // Only output is the lamp reset driver
	
	// Activate internal pull-ups for I2C
	PORTC = (1<<PC4) | (1<<PC5);

	DIDR0 = (1<<ADC0D) | (1<<ADC1D) | (1<<ADC2D) | (1<<ADC3D);// Power off the unused digital inputs on the ADC port
	DIDR1 = (1<<AIN0D) | (1<<AIN1D); // Disable digital input ports on AIN0 and AIN1 (comparator inputs)

	PRR = (1<<PRTIM2) | (1<<PRTIM0) | (1<<PRSPI) | (1<<PRUSART0) | (1<<PRADC);

	fetOff();
	holdLampInReset();

	return resetSource;
}

void doShutdownProcess()
{
	holdLampInReset();
}

/*
// This is a handy indicator tone
for(;;) {
	noIntTone(3, 1);
	noIntTone(3, 0);
	noIntTone(2, 1);
	noIntTone(2, 0);
	noIntTone(1, 1);
	noIntTone(1, 0);
}
*/