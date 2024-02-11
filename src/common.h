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

#define TemperaturDevice 0
#define AlarmDevice 1

extern uint8_t gN2KInstance;
extern uint8_t gN2KSID;
extern uint8_t gN2KSource[];

extern int8_t gDeviceCount;

extern char Version[];
extern bool gParamsChanged;
extern bool gSaveParams;


#endif
