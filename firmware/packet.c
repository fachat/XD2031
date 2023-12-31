
#include "packet.h"
#include "debug.h"

/*
 * writes a byte into the buffer.
 * Note: no bounds check!
 */
void packet_write_char(packet_t * buf, uint8_t ch)
{
        if (buf->wp >= buf->len) {
                debug_printf("Buffer overflow at %d bytes\n", buf->wp);
        } else {
                buf->buffer[buf->wp++] = ch;
        }
}


