/*
  twi.h - TWI/I2C library for Wiring & Arduino
  Copyright (c) 2006 Nicholas Zambetti.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef twi_h
#define twi_h

	#ifndef TWI_FREQ
		#define TWI_FREQ 16000L
	#endif

	#define TWI_BUFFER_LENGTH 2

	void twi_init(void);
	uint8_t twi_readFrom(uint8_t, uint8_t*, uint8_t, uint8_t);
	uint8_t twi_writeTo(uint8_t, uint8_t*, uint8_t, uint8_t, uint8_t);

#endif

