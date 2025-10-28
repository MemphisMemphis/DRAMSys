/*
 * log.h
 *
 *  Created on: 28 Oct 2025
 *      Author: gangj
 */

#pragma once
#include <iostream>
using namespace std;

#ifdef DEBUG_LOG
#define LOG(mesg)	cout << __FUNCTION__ << ":" << mesg << endl
#define LOG_SC(mesg)	cout << this->name() << "." << __FUNCTION__ << "[" << sc_core::sc_time_stamp() << "]:" << mesg << endl
#else
#define LOG(mesg)
#define LOG_SC(mesg)
#endif
