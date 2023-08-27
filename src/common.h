// common.h

#pragma once

#ifndef _COMMON_h
#define _COMMON_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include "N2kMsg.h"
#include "N2kTypes.h"

extern uint8_t gN2KSource;

extern int8_t gDeviceCount;

extern char Version[];
extern bool gParamsChanged;

struct tTemperatur {
	
	tN2kTempSource Source;
	double Value;

public:
	tTemperatur() {
		Source = N2kts_SeaTemperature;
		Value = 0;
	}
	tTemperatur(tN2kTempSource S, double V){
		Source = S;
		Value = V;
	}
	
};

extern tTemperatur gTemperaturs[4];


#endif
