/*
 * ubr		Frontend for the unassuming bridge.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Richard Alpe <richard.alpe@ericsson.com>
 *		Joachim Nilsson <troglobit@gmail.com>
 */
#ifndef UBR_PRIVATE_H_
#define UBR_PRIVATE_H_

/* From The Practice of Programming, by Kernighan and Pike */
#ifndef NELEMS
#define NELEMS(array) (sizeof(array) / sizeof(array[0]))
#endif

extern char *bridge;
extern int help_flag;

int atob(const char *str);

#endif /* UBR_PRIVATE_H_ */
