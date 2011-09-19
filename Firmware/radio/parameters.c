/* -*- Mode: C; c-basic-offset: 8; -*- */
//
// Copyright (c) 2011 Michael Smith, All Rights Reserved
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//  o Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  o Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in
//    the documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//

/// @file	parameters.c
///
/// Storage for program parameters.
///
/// Parameters are held in a contiguous array of 32-bit values.
/// It is up to the caller to decide how large a parameter is and to
/// access it accordingly.
///
/// When saved, parameters are copied bitwise to the flash scratch page,
/// with an 8-bit XOR checksum appended to the end of the flash range.
///


#include "radio.h"

// In-RAM parameter store.
//
// It seems painful to have to do this, but we need somewhere to
// hold all the parameters when we're rewriting the scratchpad
// page anyway.
//
union param {
	uint8_t		u8;
	uint16_t	u16;
	uint32_t	u32;
	uint8_t		bytes[4];
};
__xdata static union param	parameters[PARAM_MAX];

uint8_t
param_get8(enum ParamID param)
{
	return parameters[param].u8;
}

uint16_t
param_get16(enum ParamID param)
{
	return parameters[param].u16;
}

uint32_t
param_get32(enum ParamID param)
{
	return parameters[param].u32;
}

void
param_set8(enum ParamID param, uint8_t value)
{
	parameters[param].u8 = value;
}

void
param_set16(enum ParamID param, uint16_t value)
{
	parameters[param].u16 = value;
}

void
param_set32(enum ParamID param, uint32_t value)
{
	parameters[param].u32 = value;
}

/// Load the write-enable keys into the hardware in order to enable
/// one write or erase operation.
///
static void
flash_load_keys(void)
{
	FLKEY = 0xa5;
	FLKEY = 0xf1;
}

bool
param_load()
{
	const uint8_t __xdata *p;
	uint8_t		d;
	uint8_t		i;
	uint8_t		sum;

	// pointer into scratch space
	p = (const uint8_t __xdata *)0;

	// initialise checksum
	sum = 0;

	// loop reading the parameters array
	//
	for (i = 0; i < sizeof(parameters); i ++) {
		PSCTL |= 0x04;
		d = *(p++);
		PSCTL &= 0x04;
		parameters[0].bytes[i] = d;
		sum ^= d;
	}

	// verify checksum
	PSCTL |= 0x04;
	d = *(p++);
	PSCTL &= 0x04;
	if (sum != d)
		return false;

	// decide whether we read a supported version of the structure
	if (parameters[PARAM_FORMAT].u32 != PARAM_FORMAT_CURRENT)
		return false;
	return true;
}

void
param_save()
{
	uint8_t __xdata	*p;
	uint8_t		d;
	uint8_t		i;
	uint8_t		sum;

	// tag parameters with the current format
	parameters[PARAM_FORMAT].u32 = PARAM_FORMAT_CURRENT;

	// initialise checksum
	sum = 0;

	// pointer into scratch space
	p = (uint8_t __xdata *)0;

	// erase the scratch page
	flash_load_keys();	// unlock flash for one operation
	PSCTL = 0x07;		// enable flash erase of the scratch page
	*p = 0xff;		// trigger the erase
	PSCTL = 0x00;		// disable flash write & scratch access

	// save parameters to the scratch page
	for (i = 0; i < sizeof(parameters); i++) {
		d = parameters[0].bytes[i];	// byte we are going to write
		sum ^= d;
		flash_load_keys();		// unlock flash
		PSCTL = 0x05;			// enable program to scratch page
		*p++ = d;			// program the byte
		PSCTL = 0x00;			// disable program & scratch access
	}

	// write checksum
	flash_load_keys();		// unlock flash
	PSCTL = 0x05;			// enable program to scratch page
	*p = sum;			// program the byte
	PSCTL = 0x00;			// disable program & scratch access
}