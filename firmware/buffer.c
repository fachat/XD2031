/* 
    XD-2031 - Serial line file server for CBMs
    Copyright (C) 2012  Andre Fachat

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

*/

#include <stdio.h>
#include <stdint.h>

/*
#include "bus.h"
#include "errormsg.h"

#include "term.h"
#include "led.h"
*/

#include "channel.h"
#include "config.h"
#include "provider.h"
#include "packet.h"
#include "buffer.h"
#include "debug.h"




char buf[CMD_BUFFER_LENGTH];

// taken from device-specific configuration
//#define CONFIG_NUM_DIRECT_BUFFERS       1


packet_t buf_cmdpack;
packet_t buf_datapack;
static uint8_t cbstat;
static int8_t cberr;


cmdbuf_t buffers[CONFIG_NUM_DIRECT_BUFFERS];

void buffer_init(void) {
	// init the buffers
	uint8_t i;
	for (i = 0; i < CONFIG_NUM_DIRECT_BUFFERS; i++) {
		buffers[i].channel_no = -1;
	}
}

// reserve a free buffer for the given channel
cmdbuf_t *buf_reserve(int8_t channel_no) {
	// reserve a direct buffer
	uint8_t i;
	for (i = 0; i < CONFIG_NUM_DIRECT_BUFFERS; i++) {
		if (buffers[i].channel_no < 0) {
			buffers[i].channel_no = channel_no;
			buffers[i].real_endpoint = NULL;
			buffers[i].recordlen = 0;
			return buffers+i;
		}
	}
	return NULL;
}

// reserve a given buffer for the channel
cmdbuf_t *buf_reserve_buf(int8_t channel_no, uint8_t bufno) {
	// reserve a direct buffer
	if (buffers[bufno].channel_no < 0) {
		buffers[bufno].channel_no = channel_no;
		return buffers+bufno;
	}
	return NULL;
}

// find buffer for channel
cmdbuf_t *buf_find(int8_t channel_no) {
	// find a direct buffer for a channel
	
	uint8_t i;
	for (i = 0; i < CONFIG_NUM_DIRECT_BUFFERS; i++) {
		if (buffers[i].channel_no == channel_no) {
			return buffers+i;
		}
	}
	return NULL;
}

uint8_t buf_free(int8_t channel_no) {
	// find a direct buffer for a channel
	uint8_t n = 0;
	for (uint8_t i = 0; i < CONFIG_NUM_DIRECT_BUFFERS; i++) {
		if (buffers[i].channel_no == channel_no) {
			buffers[i].channel_no = -1;
			n++;
			// no break or return, to clean up others just in case
		}
	}
	return n;	// number of freed buffers
}

// ----------------------------------------------------------------------------------
/**
 * command callback
 */
static uint8_t cmd_callback(int8_t channelno, int8_t errnum, packet_t *rxpacket) {
	cberr = packet_get_buffer(rxpacket)[0];
	cbstat = 1;
	return 0;
}

static uint8_t cmd_wait_cb() {
	while (cbstat == 0) {
		delayms(1);
		main_delay();
	}

	return cberr;	// cberr is set to an FS_REPLY error in the actual callback
}


// ----------------------------------------------------------------------------------

uint8_t buf_call(endpoint_t *ep, void *provdata, uint8_t channel_no, packet_t *sendbuf, packet_t *rxbuf) {

	cbstat = 0;
	ep->provider->submit_call(provdata, channel_no, 
			sendbuf, rxbuf, cmd_callback);

	return cmd_wait_cb();
}


// ----------------------------------------------------------------------------------

void buffer_close(uint8_t channel_no, endpoint_t *endpoint) {
        // close file
        packet_init(&buf_cmdpack, CMD_BUFFER_LENGTH, (uint8_t*) buf);
        packet_set_filled(&buf_cmdpack, channel_no, FS_CLOSE, 0);

        buf_call(endpoint, NULL, channel_no, &buf_cmdpack, &buf_cmdpack);
/*
        endpoint->provider->submit_call(NULL, channel_no, 
                &buf_cmdpack, &buf_cmdpack, cmd_callback);
        cmd_wait_cb();
*/
}


uint8_t buffer_write_buffer(uint8_t channel_no, endpoint_t *endpoint,
                uint8_t start_of_data, uint16_t send_nbytes) {

        uint16_t restlength = send_nbytes;

        uint8_t ptype = FS_WRITE;
        uint8_t plen;
        uint8_t rv = CBM_ERROR_OK;

        debug_printf("bufcmd_write_buffer: my chan=%d\n", channel_no);

        cmdbuf_t *buffer = buf_find(channel_no);
        if (buffer == NULL) {
                return CBM_ERROR_NO_CHANNEL;
        }

        while (ptype != FS_EOF && restlength != 0 && rv == CBM_ERROR_OK) {

                packet_init(&buf_datapack, DATA_BUFLEN, buffer->buffer + start_of_data + send_nbytes - restlength);
                packet_init(&buf_cmdpack, CMD_BUFFER_LENGTH, (uint8_t*) buf);

                plen = (restlength > DATA_BUFLEN) ? DATA_BUFLEN : restlength;
                if (plen >= restlength) {
                        ptype = FS_EOF;
                } else {
                        ptype = FS_WRITE;
                }
                packet_set_filled(&buf_datapack, channel_no, ptype, plen);

for (uint8_t i = 0; i < plen; i++) {
        debug_printf(" %02x", buffer->buffer[start_of_data + send_nbytes - restlength + i]);
}
debug_puts(" < sent\n");

                rv = buf_call(endpoint, endpoint->provdata, channel_no, &buf_datapack, &buf_cmdpack);
/*
                cbstat = 0;
                endpoint->provider->submit_call(endpoint->provdata, channel_no, 
                        &buf_datapack, &buf_cmdpack, cmd_callback);
                rv = cmd_wait_cb();
*/
                if (rv == CBM_ERROR_OK) {
                        restlength -= plen;
                }
                debug_printf("write buffer: sent %d bytes, %d bytes left, rv=%d, ptype=%d, rxd cmd=%d\n",
                                plen, restlength, rv, ptype, packet_get_type(&buf_cmdpack));
        }

        if (restlength > 0) {
                // received error on write
                term_printf("ONLY SHORT BUFFER, WAS ACCEPTED, SENDING %d, ACCEPTED %d\n", 256, 256-restlength);
        }

        return rv;
}

uint8_t buffer_read_buffer(uint8_t channel_no, endpoint_t *endpoint, uint16_t receive_nbytes) {

        uint16_t lengthread = 0;

        uint8_t ptype = FS_REPLY;

        uint8_t rv = CBM_ERROR_OK;

        debug_printf("bufcmd_read_buffer: my chan=%d\n", channel_no);

        cmdbuf_t *buffer = buf_find(channel_no);
        if (buffer == NULL) {
                return CBM_ERROR_NO_CHANNEL;
        }

        while (ptype != FS_EOF && lengthread < receive_nbytes) {

                packet_init(&buf_datapack, 128, buffer->buffer + lengthread);
                packet_init(&buf_cmdpack, CMD_BUFFER_LENGTH, (uint8_t*) buf);
                packet_set_filled(&buf_cmdpack, channel_no, FS_READ, 0);

                buf_call(endpoint, endpoint->provdata, channel_no, &buf_cmdpack, &buf_datapack);
/*      
                cbstat = 0;
                endpoint->provider->submit_call(endpoint->provdata, channel_no, 
                        &buf_cmdpack, &buf_datapack, cmd_callback);
                cmd_wait_cb();
*/
                ptype = packet_get_type(&buf_datapack);

                if (ptype == FS_REPLY) {
                        // error (should not happen though)
                        rv = packet_get_buffer(&buf_datapack)[0];
                        break; // out of loop
                } else
                if (ptype == FS_WRITE || ptype == FS_EOF) {

                        lengthread += packet_get_contentlen(&buf_datapack);
                }
        }

        if (lengthread < receive_nbytes) {
                // received short package
                term_printf("RECEIVED SHORT BUFFER, EXPECTED %d, GOT %d\n",
                                        receive_nbytes, lengthread);
        }

        buffer->rptr = 0;
        buffer->wptr =lengthread & 0xff;

        return rv;
}


