#include <stdint.h>

#include "TimerServices.h"
#include "LampControl.h"
#include "twi.h"

#define IODIR ((uint_fast8_t)0x00)
#define IPOL ((uint_fast8_t)0x01)
#define GPINTEN ((uint_fast8_t)0x02)
#define DEFVAL ((uint_fast8_t)0x03)
#define INTCON ((uint_fast8_t)0x04)
#define IOCON ((uint_fast8_t)0x05)
#define GPPU ((uint_fast8_t)0x06)
#define INTF ((uint_fast8_t)0x07)
#define INTCAP ((uint_fast8_t)0x08)
#define GPIO ((uint_fast8_t)0x09)
#define OLAT ((uint_fast8_t)0x0A)

#define MCP23008ADDR ((uint_fast8_t)0b00100000)
#define MCP23008NCS ((uint_fast8_t)0b00010000)
#define MCP23008UD ((uint_fast8_t)0b00100000)

#define LAMPONOFF 0b00000010
#define LAMPUP 0b00000100
#define LAMPDOWN 0b00001000

#define LEDLAMPINITIAL (((uint_fast8_t)0b00000001) | MCP23008NCS) // LED and lamp off initially; nCS pin high U/nD low

static struct {
	enum LightState {
		Off = 0,
		On = 1
	} ledState, lampState;
	uint_fast8_t cRegVal;
	uint_fast8_t rheostatState;
} driverState = {Off, Off, 0, 0};

static uint_fast8_t processBits(uint_fast8_t pinVals, uint_fast32_t *tLast);
static uint_fast8_t sendRegByte(uint_fast8_t regAddr, uint_fast8_t regVal);
static uint_fast8_t sendOLAT(uint_fast8_t regVal);
static uint_fast8_t readRegs(uint_fast8_t regAddr, uint_fast8_t nBytes, uint8_t *bOut);

uint_fast8_t shortInit23008(void)
{
	uint_fast8_t rVal;

	driverState.cRegVal = LEDLAMPINITIAL;
	driverState.ledState = Off;
	driverState.lampState = Off;

	rVal = sendOLAT(driverState.cRegVal);
	rVal |= sendRegByte(IODIR, 0b01001110); // Switches to inputs, unused line to input
	rVal |= sendRegByte(GPPU, 0b00001110); // Use built in pullups on switches

	return rVal;
}

void fullInit23008(void)
{
	uint_fast8_t rVal;
	uint_fast8_t intRegVal;

	rVal = sendRegByte(IOCON, 0b00000000);
	rVal |= shortInit23008();

	rVal |= sendRegByte(GPINTEN, 0b00001110); // Enable interrupts
	rVal |= sendRegByte(DEFVAL, 0b00001110); // This is the uninterrupted level
	rVal |= sendRegByte(INTCON, 0b00001110); // These lines honour the DEFVAL register
	rVal |= readRegs(INTCAP, 1, &intRegVal); // Clear the interrupt state
}

void testLampState(uint_fast32_t *tLast)
{
	uint_fast8_t intRegVal;
	uint_fast8_t update = 0;

	readRegs(INTF, 1, &intRegVal);
	if(intRegVal != 0x0) {
		struct {
			uint_fast8_t intCap;
			uint_fast8_t gpioNow;
		} regVals;

		readRegs(INTCAP, 2, (uint_fast8_t*)&regVals);

		update = processBits(regVals.intCap, tLast);

		if(regVals.intCap != regVals.gpioNow) {
			update |= processBits(regVals.gpioNow, tLast);
		}
	}
	if((getTime() - *tLast) > 1000) { // Large change since last power level change (>1s), this ensures that we don't accidently turn the lamp on or off
		if(driverState.lampState == Off && driverState.rheostatState > 0) {
			lampPowerDown(96); // Make sure the rheostat starts off in a known state (lowest power)
		} else if(driverState.lampState == On && driverState.rheostatState == 0) {
			lampPowerUp(1); // Make sure we're not in the lowest power state anymore
		}
	}
	if(((getTime() - *tLast) > 10000) && (driverState.lampState==Off)) {
		// return...
	}

	if(update) {
		driverState.cRegVal = MCP23008NCS; // nCS high initially
		driverState.cRegVal |= driverState.lampState==On ? 0b10000000 : 0b00000000;
		driverState.cRegVal |= driverState.ledState==On ? 0b0: 0b1;
	
		sendOLAT(driverState.cRegVal);
	}
}

void lampPowerDown(uint_fast8_t nSteps)
{
	uint_fast8_t rVal;
	uint_fast8_t localReg;

	for(; nSteps != 0; --nSteps) {
		localReg = driverState.cRegVal | MCP23008NCS | MCP23008UD;

		rVal = sendOLAT(localReg); // nCS and U/nD high

		localReg &= ~MCP23008NCS;

		rVal |= sendOLAT(localReg); // U/nD high, nCS low

		localReg &= ~MCP23008UD;

		rVal |= sendOLAT(localReg); // nCS and U/nD low

		localReg |= MCP23008UD;

		rVal |= sendOLAT(localReg); // nCS low and U/nD high

		rVal |= sendOLAT(driverState.cRegVal); // nCS high and U/nD low (default)

		driverState.rheostatState -= driverState.rheostatState > 0 ? 1 : 0;
	}
}

void lampPowerUp(uint_fast8_t nSteps)
{
	uint_fast8_t rVal;
	uint_fast8_t localReg;

	for(; nSteps != 0; --nSteps) {
		localReg = driverState.cRegVal & ~MCP23008NCS;

		rVal = sendOLAT(localReg); // nCS low and U/nD low

		localReg |= MCP23008UD;

		rVal |= sendOLAT(localReg); // nCS low and U/nD high

		rVal |= sendOLAT(driverState.cRegVal); // nCS high and U/nD low (default)

		driverState.rheostatState += driverState.rheostatState < 31 ? 1 : 0;
	}
}

static uint_fast8_t processBits(uint_fast8_t pinVals, uint_fast32_t *tLast)
{
	uint_fast32_t tNow = getTime();

	if((pinVals & LAMPONOFF) == 0) {
		if(driverState.lampState == Off && driverState.rheostatState > 0) {
			driverState.lampState = On;
			lampPowerDown(96); // Make sure the rheostat starts off in a known state (lowest power)
			lampPowerUp(1);
			*tLast = tNow;
			return 1;
		} if(driverState.lampState == On && driverState.rheostatState == 0) {
			driverState.lampState = Off;
			lampPowerDown(96); // Make sure the rheostat starts off in a known state (lowest power)
			*tLast = tNow;
			return 1;
		}
	} else {
		if((tNow - *tLast) > (driverState.rheostatState<<3)) { // Small time since last power level change
			if((pinVals & LAMPUP) == 0) {
				lampPowerUp(1);
				*tLast = tNow;
			} else if((pinVals & LAMPDOWN) == 0){
				lampPowerDown(1);
				*tLast = tNow;
			}
		}
	}

	return 0;
}

static uint_fast8_t sendRegByte(uint_fast8_t regAddr, uint_fast8_t regVal)
{
	uint8_t i2cBuffer[TWI_BUFFER_LENGTH];

	i2cBuffer[0] = regAddr;
	i2cBuffer[1] = regVal;
	return twi_writeTo(MCP23008ADDR, i2cBuffer, sizeof(uint8_t) * 2, (uint8_t)1, (uint8_t)1);
	// Wire.beginTransmission(MCP23008ADDR);
	// Wire.write(regAddr);
	// Wire.write(regVal);
	// return Wire.endTransmission(1);
}

static uint_fast8_t sendOLAT(uint_fast8_t regVal)
{
	return sendRegByte(OLAT, regVal);
}

static uint_fast8_t readRegs(uint_fast8_t regAddr, uint_fast8_t nBytes, uint8_t *bOut)
{
	uint_fast8_t rVal;
	uint8_t i2cBuffer[TWI_BUFFER_LENGTH];

	i2cBuffer[0] = regAddr;
	rVal = twi_writeTo(MCP23008ADDR, i2cBuffer, sizeof(uint8_t) * 1, (uint8_t)1, (uint8_t)1);
	// Wire.beginTransmission(MCP23008ADDR);
	// Wire.write(regAddr);
	// rVal = Wire.endTransmission(1); // At lower clock rates the receiver doesn't like a repeated start here
	twi_readFrom(MCP23008ADDR,  bOut, nBytes, (uint8_t)1);
	// Wire.requestFrom(MCP23008ADDR, nBytes, (uint_fast8_t)1);
	// for(uint_fast8_t idx = 0; idx < nBytes; idx++) {
	// 	bOut[idx] = Wire.read();
	// }
	return rVal;
}