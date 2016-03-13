#ifndef LAMPCONTROL_H_
#define LAMPCONTROL_H_

	uint_fast8_t shortInit23008(void);
	void fullInit23008(void);
	void testLampState(uint_fast32_t *tLast);
	void lampPowerDown(uint_fast8_t nSteps);
	void lampPowerUp(uint_fast8_t nSteps);

#endif /* LAMPCONTROL_H_ */