#ifndef ADCREADER_H_
#define ADCREADER_H_

	#define ADCMAXVALUE ((1<<10) - 1)

	void initADC();
	void startADC();
	void stopADC();
	uint_fast8_t testIntADC();
	uint_fast8_t isADCUpdated(uint_fast8_t nSamples);
	void adcUpdateVoltageBias();
	void adcUpdateCurrentBias();
	uint_fast16_t getADCCurrentReading();
	uint_fast16_t getADCVoltageReading();
	uint_fast32_t getAccumulatedCurrent();

#endif /* ADCREADER_H_ */