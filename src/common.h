// common.h

#pragma once

#ifndef _COMMON_h
#define _COMMON_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif


extern double gOutsideTemperature;
extern double gCabinTemperature;

extern uint8_t gN2KSource;

extern char Version[];


#endif
