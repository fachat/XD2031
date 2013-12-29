/**************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2013 Andre Fachat, Nils Eilers

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA  02110-1301, USA.

***************************************************************************/


/* ----- FatFs FRESULT errors vs CBM errno_t errors --------------------------

   Some FatFs functions depend on the error code of other internal functions,
   so making them an alias to CBM_ERROR-ones does not seem to be a good
   idea if some information is lost by mapping several FatFs errors to a
   single CBM error code (e.g. CBM_ERROR_DRIVE_NOT_READY)

*/

#ifndef ERRCOMPAT_H
#define ERRCOMPAT_H

#include "ff.h"
#include "errors.h"

errno_t conv_fresult(FRESULT fres);
errno_t combine (errno_t cres, FRESULT fres);

#endif
