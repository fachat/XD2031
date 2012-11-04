/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

****************************************************************************/

/**
 manages the -X command line options, which are sent to the device on
 start and when the device resets.
 Note that the registry does not copy the options, just stores the pointers
 into the argv[] array.
*/

// init the command line option registry
void xcmd_init();

// register a new -X cmdline option; 
// pointer points to the char _after_ the "X"
void xcmd_register(const char *option);

// get the number of registered options
int xcmd_num_options();

// get the Nth option
// pos from zero to xcmd_num_options()-1
const char *xcmd_option(int pos);
 

