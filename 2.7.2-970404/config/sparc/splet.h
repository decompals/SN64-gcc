/* Definitions of target machine for GNU compiler, for SPARClet.
   Copyright (C) 1996 Free Software Foundation, Inc.
   Contributed by Doug Evans (dje@cygnus.com).

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "sparc/aout.h"

/* -mbroken-saverestore is not included here because the long term
   default is -mno-broken-saverestore.  */
#undef TARGET_DEFAULT
#define TARGET_DEFAULT (MASK_APP_REGS + MASK_EPILOGUE)

/* -mlive-g0 is only supported on the sparclet.  */
#undef SUBTARGET_SWITCHES
#define SUBTARGET_SWITCHES \
{"big-endian", -MASK_LITTLE_ENDIAN},		\
{"little-endian", MASK_LITTLE_ENDIAN},		\
{"live-g0", MASK_LIVE_G0},			\
{"no-live-g0", -MASK_LIVE_G0},			\
{"broken-saverestore", MASK_BROKEN_SAVERESTORE},	\
{"no-broken-saverestore", -MASK_BROKEN_SAVERESTORE},

#undef CPP_SPEC
#define CPP_SPEC "\
%(cpp_cpu) \
%{mlittle-endian:-D__LITTLE_ENDIAN__} \
"

#undef ASM_SPEC
#define ASM_SPEC "%{mlittle-endian:-EL} %(asm_cpu)"

/* Require the user to supply crt0.o.  */
#undef STARTFILE_SPEC
#define STARTFILE_SPEC ""

#undef LINK_SPEC
#define LINK_SPEC "%{mlittle-endian:-EL}"

/* sparclet chips are bi-endian.  */
#undef BYTES_BIG_ENDIAN
#define BYTES_BIG_ENDIAN (! TARGET_LITTLE_ENDIAN)
#undef WORDS_BIG_ENDIAN
#define WORDS_BIG_ENDIAN (! TARGET_LITTLE_ENDIAN)
