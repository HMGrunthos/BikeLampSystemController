#ifndef TIMERSERVICES_H_
#define TIMERSERVICES_H_

	void initTimers();
	void startTimers();
	void stopTimers();
	uint_fast8_t testIntTimers();
	void msWait(uint_fast32_t duration);
	void tickWait();
	uint_fast32_t getTime();
	uint_fast32_t getTickNumber();

	uint_fast16_t noIntTimerStart();
	uint_fast8_t noIntTimerEnded();
	void noIntWait(uint_fast16_t nTicks);
	void noIntTone(uint_fast8_t freq, uint_fast8_t output);

#endif /* TIMERSERVICES_H_ */