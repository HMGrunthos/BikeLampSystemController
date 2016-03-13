#ifndef POINTERTRICKS_H_
#define POINTERTRICKS_H_

	#define FIX_POINTER(_ptr) __asm__ __volatile__("" : "=b" (_ptr) : "0" (_ptr))

#endif /* POINTERTRICKS_H_ */