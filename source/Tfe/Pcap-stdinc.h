/*
 * Copyright (c) 2002
 *	Politecnico di Torino.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the Politecnico
 * di Torino, and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @(#) $Header$ (LBL)
 */

#define SIZEOF_CHAR 1
#define SIZEOF_SHORT 2
#define SIZEOF_INT 4

#define _WINSOCKAPI_
#include <fcntl.h>

#include <winsock2.h>
#include "bittypes.h"
#include <time.h>
#include <io.h>

#ifndef __MINGW32__
#include "IP6_misc.h"
#endif

#define caddr_t char*

// TC: TfeArch.cpp includes pcap.h (before stdio.h), which includes Pcap_stdinc.h which redefines: snprintf & vsnprintf
// - This causes VC++2008 Express to give a compile error for stdio.h!
//#define snprintf _snprintf
//#define vsnprintf _vsnprintf

