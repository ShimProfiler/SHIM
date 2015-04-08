
/*
   ----------------------------------------------------------------

   Notice that the following BSD-style license applies to this one
   file (callgrind.h) only.  The rest of Valgrind is licensed under the
   terms of the GNU General Public License, version 2, unless
   otherwise indicated.  See the COPYING file in the source
   distribution for details.

   ----------------------------------------------------------------

   This file is part of callgrind, a valgrind tool for cache simulation
   and call tree tracing.

   Copyright (C) 2003-2009 Josef Weidendorfer.  All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

   2. The origin of this software must not be misrepresented; you must
      not claim that you wrote the original software.  If you use this
      software in a product, an acknowledgment in the product
      documentation would be appreciated but is not required.

   3. Altered source versions must be plainly marked as such, and must
      not be misrepresented as being the original software.

   4. The name of the author may not be used to endorse or promote
      products derived from this software without specific prior written
      permission.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
   OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
   GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   ----------------------------------------------------------------

   Notice that the above BSD-style license applies to this one file
   (callgrind.h) only.  The entire rest of Valgrind is licensed under
   the terms of the GNU General Public License, version 2.  See the
   COPYING file in the source distribution for details.

   ----------------------------------------------------------------
*/

#ifndef __CACHEGRIND_H
#define __CACHEGRIND_H

#include "valgrind.h"

/* !! ABIWARNING !! ABIWARNING !! ABIWARNING !! ABIWARNING !!
   This enum comprises an ABI exported by Valgrind to programs
   which use client requests.  DO NOT CHANGE THE ORDER OF THESE
   ENTRIES, NOR DELETE ANY -- add new ones at the end.
 */

typedef
   enum {
      VG_USERREQ__PROBE_BEGIN = VG_USERREQ_TOOL_BASE('C','G'),
      VG_USERREQ__PROBE_END,
      VG_USERREQ__PROBE_REPORT
   } Vg_CachegrindClientRequest;

/* Start of a benchmark iteration via the probe API */
#define CACHEGRIND_PROBE_BEGIN(_qzz_itr,_qzz_warmup)			\
   {unsigned int _qzz_res;                                              \
    VALGRIND_DO_CLIENT_REQUEST(_qzz_res, 0,                             \
                            VG_USERREQ__PROBE_BEGIN,                    \
                            _qzz_itr, _qzz_warmup, 0, 0, 0);            \
   }

/* End of a benchmark iteration via the probe API */
#define CACHEGRIND_PROBE_END(_qzz_itr,_qzz_warmup)			\
   {unsigned int _qzz_res;                                              \
    VALGRIND_DO_CLIENT_REQUEST(_qzz_res, 0,                             \
                            VG_USERREQ__PROBE_END,                      \
                            _qzz_itr, _qzz_warmup, 0, 0, 0);            \
   }

/* End of a benchmark iteration via the probe API */
#define CACHEGRIND_PROBE_REPORT(_qzz_itr,_qzz_warmup)			\
   {unsigned int _qzz_res;                                              \
    VALGRIND_DO_CLIENT_REQUEST(_qzz_res, 0,                             \
                            VG_USERREQ__PROBE_REPORT,                   \
                            _qzz_itr, _qzz_warmup, 0, 0, 0);            \
   }


#endif /* __CACHEGRIND_H */
