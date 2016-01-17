/*-----------------------------------------------------------------------*/
/* MMCv3/SDv1/SDv2 (in SPI mode) control module  (C)ChaN, 2010           */
/*-----------------------------------------------------------------------*/

#include "hwdevice.h"     /* Device dependent I/O definitions */
#include "diskio.h"
#include "sdcard.h"
#include "spi.h"
#include "timer.h"
#include "packet.h"
#include "debug.h"

#undef DEBUG_CMD

/*--------------------------------------------------------------------------

   Module Private Functions

---------------------------------------------------------------------------*/

/* Definitions for MMC/SDC command */
#define CMD0    (0)         /* GO_IDLE_STATE */
#define CMD1    (1)         /* SEND_OP_COND (MMC) */
#define ACMD41  (0x80+41)   /* SEND_OP_COND (SDC) */
#define CMD8    (8)         /* SEND_IF_COND */
#define CMD9    (9)         /* SEND_CSD */
#define CMD10   (10)        /* SEND_CID */
#define CMD12   (12)        /* STOP_TRANSMISSION */
#define ACMD13  (0x80+13)   /* SD_STATUS (SDC) */
#define CMD16   (16)        /* SET_BLOCKLEN */
#define CMD17   (17)        /* READ_SINGLE_BLOCK */
#define CMD18   (18)        /* READ_MULTIPLE_BLOCK */
#define CMD23   (23)        /* SET_BLOCK_COUNT (MMC) */
#define ACMD23  (0x80+23)   /* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24   (24)        /* WRITE_BLOCK */
#define CMD25   (25)        /* WRITE_MULTIPLE_BLOCK */
#define CMD55   (55)        /* APP_CMD */
#define CMD58   (58)        /* READ_OCR */


volatile
uint8_t media_status = STA_NOINIT;  /* Disk status */

static
BYTE CardType;              /* Card type flags */



/*-----------------------------------------------------------------------*/
/* Wait for card ready                                                   */
/*-----------------------------------------------------------------------*/

static
int wait_ready (void)   /* 1:OK, 0:Timeout */
{
    BYTE d;


    timer2_set_ms(500);
    do
        d = xchg_spi(0xFF);
    while (d != 0xFF && !timer2_is_timed_out());

    return (d == 0xFF) ? 1 : 0;
}




/*-----------------------------------------------------------------------*/
/* SD card chip select                                                   */
/*-----------------------------------------------------------------------*/

static inline void sdcard_cs(uint8_t cs)
{
        if(cs) {
                PORT_SD_CS |= _BV(PIN_SD_CS);
        } else {
                PORT_SD_CS &= ~_BV(PIN_SD_CS);
        }
}


/*-----------------------------------------------------------------------*/
/* Deselect the card and release SPI bus                                 */
/*-----------------------------------------------------------------------*/

static
void deselect (void)
{
    sdcard_cs(1);
    xchg_spi(0xFF); /* Dummy clock (force DO hi-z for multiple slave SPI) */
}



/*-----------------------------------------------------------------------*/
/* Select the card and wait for ready                                    */
/*-----------------------------------------------------------------------*/

static
int select (void)   /* 1:Successful, 0:Timeout */
{
    sdcard_cs(0);
    xchg_spi(0xFF); /* Dummy clock (force DO enabled) */

    if (wait_ready()) return 1; /* OK */
    deselect();
    return 0;       /* Timeout */
}



/*-----------------------------------------------------------------------*/
/* Receive a data packet from MMC                                        */
/*-----------------------------------------------------------------------*/

static
int rcvr_datablock (
    BYTE *buff,         /* Data buffer to store received data */
    UINT btr            /* Byte count (must be multiple of 4) */
)
{
    BYTE token;


    timer2_set_ms(200);
    do {                            /* Wait for data packet in timeout of 200ms */
        token = xchg_spi(0xFF);
    } while ((token == 0xFF) && !timer2_is_timed_out());
    if (token != 0xFE) return 0;    /* If not valid data token, retutn with error */

    rcvr_spi_multi(buff, btr);      /* Receive the data block into buffer */
    xchg_spi(0xFF);                 /* Discard CRC */
    xchg_spi(0xFF);

    return 1;                       /* Return with success */
}



/*-----------------------------------------------------------------------*/
/* Send a data packet to MMC                                             */
/*-----------------------------------------------------------------------*/

static
int xmit_datablock (
    const BYTE *buff,   /* 512 byte data block to be transmitted */
    BYTE token          /* Data/Stop token */
)
{
    BYTE resp;


    if (!wait_ready()) return 0;

    xchg_spi(token);                    /* Xmit data token */
    if (token != 0xFD) {    /* Is data token */
        xmit_spi_multi(buff, 512);      /* Xmit the data block to the MMC */
        xchg_spi(0xFF);                 /* CRC (Dummy) */
        xchg_spi(0xFF);
        resp = xchg_spi(0xFF);          /* Reveive data response */
        if ((resp & 0x1F) != 0x05)      /* If not accepted, return with error */
            return 0;
    }

    return 1;
}



/*-----------------------------------------------------------------------*/
/* Send a command packet to MMC                                          */
/*-----------------------------------------------------------------------*/

BYTE send_cmd (     /* Returns R1 resp (bit7==1:Send failed) */
    BYTE cmd,       /* Command index */
    DWORD arg       /* Argument */
)
{
    BYTE n, res;

#ifdef DEBUG_CMD
      debug_printf("cmd(%02X)",cmd);
#endif

    if (cmd & 0x80) {   /* ACMD<n> is the command sequense of CMD55-CMD<n> */
        cmd &= 0x7F;
        res = send_cmd(CMD55, 0);
        if (res > 1) goto exit;
    }

    /* Select the card and wait for ready */
    deselect();
    if (!select()) {
        res = 0xFF;
	goto exit;
    }

    /* Send command packet */
    xchg_spi(0x40 | cmd);               /* Start + Command index */
    xchg_spi((BYTE)(arg >> 24));        /* Argument[31..24] */
    xchg_spi((BYTE)(arg >> 16));        /* Argument[23..16] */
    xchg_spi((BYTE)(arg >> 8));         /* Argument[15..8] */
    xchg_spi((BYTE)arg);                /* Argument[7..0] */
    n = 0x01;                           /* Dummy CRC + Stop */
    if (cmd == CMD0) n = 0x95;          /* Valid CRC for CMD0(0) + Stop */
    if (cmd == CMD8) n = 0x87;          /* Valid CRC for CMD8(0x1AA) Stop */
    xchg_spi(n);

    /* Receive command response */
    if (cmd == CMD12) xchg_spi(0xFF);   /* Skip a stuff byte when stop reading */
    n = 10;                             /* Wait for a valid response in timeout of 10 attempts */
    do
        res = xchg_spi(0xFF);
    while ((res & 0x80) && --n);

exit:

#ifdef DEBUG_CMD
      debug_printf("=%02X\n",res);
#endif

    return res;         /* Return with the response value */
}


/*-----------------------------------------------------------------------*/
/* Media change                                                          */
/*-----------------------------------------------------------------------*/

static inline void update_media_status (void) 
{
    uint8_t s = media_status;

    if (sd_card_write_protected())             /* Write protected */
        s |= STA_PROTECT;
    else                    /* Write enabled */
        s &= ~STA_PROTECT;

    if (sd_card_inserted())            /* Card inserted */
        s &= ~STA_NODISK;
    else                    /* Socket empty */
        s |= (STA_NODISK | STA_NOINIT);

    media_status  = s;      /* Update media status */
}



/*--------------------------------------------------------------------------

   Public Functions

---------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS SD_disk_initialize (
    BYTE drv        /* Physical drive number (0) */
)
{
    BYTE n, cmd, ty, ocr[4];

    if (drv) return STA_NOINIT;         /* Supports only single drive */
    power_off();                        /* Turn off the socket power to reset the card */
    update_media_status();
    if (media_status & STA_NODISK) {
      return media_status; /* No card in the socket */
    }
    power_on();                         /* Turn on the socket power */
    spi_init();
    slow_spi_clk();
    for (n = 10; n; n--) xchg_spi(0xFF);    /* 80 dummy clocks */

    ty = 0;
    if (send_cmd(CMD0, 0) == 1) {           /* Enter Idle state */
        timer2_set_ms(1000);                /* Initialization timeout of 1000 msec */
        if (send_cmd(CMD8, 0x1AA) == 1) {   /* SDv2? */
            for (n = 0; n < 4; n++) ocr[n] = xchg_spi(0xFF);    /* Get trailing return value of R7 resp */
            if (ocr[2] == 0x01 && ocr[3] == 0xAA) {             /* The card can work at vdd range of 2.7-3.6V */
                while (!timer2_is_timed_out()  && send_cmd(ACMD41, 1UL << 30));  /* Wait for leaving idle state (ACMD41 with HCS bit) */
                if (!timer2_is_timed_out()  && send_cmd(CMD58, 0) == 0) {        /* Check CCS bit in the OCR */
                    for (n = 0; n < 4; n++) ocr[n] = xchg_spi(0xFF);
                    ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;  /* SDv2 */
                }
            }
        } else {                            /* SDv1 or MMCv3 */
            if (send_cmd(ACMD41, 0) <= 1)   {
                ty = CT_SD1; cmd = ACMD41;  /* SDv1 */
            } else {
                ty = CT_MMC; cmd = CMD1;    /* MMCv3 */
            }
            while (!timer2_is_timed_out()  && send_cmd(cmd, 0));         /* Wait for leaving idle state */
            if (timer2_is_timed_out() || send_cmd(CMD16, 512) != 0)   /* Set R/W block length to 512 */
                ty = 0;
        }
    }
    CardType = ty;
    deselect();

    if (ty) {           /* Initialization succeded */
        media_status &= ~STA_NOINIT;        /* Clear STA_NOINIT */
        fast_spi_clk();
    } else {            /* Initialization failed */
        power_off();
    }

    return media_status;
}



/*-----------------------------------------------------------------------*/
/* Get Disk Status                                                       */
/*-----------------------------------------------------------------------*/

DSTATUS SD_disk_status (
    BYTE drv        /* Physical drive nmuber (0) */
)
{
    if (drv) return STA_NOINIT;     /* Supports only single drive */
    return media_status;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT SD_disk_read (
    BYTE pdrv,          /* Physical drive nmuber (0) */
    BYTE *buff,         /* Pointer to the data buffer to store read data */
    DWORD sector,       /* Start sector number (LBA) */
    UINT count          /* Sector count */
)
{
    if (pdrv || !count) return RES_PARERR;
    if (media_status & STA_NOINIT) return RES_NOTRDY;

    if (!(CardType & CT_BLOCK)) sector *= 512;  /* Convert to byte address if needed */

    if (count == 1) {   /* Single block read */
        if ((send_cmd(CMD17, sector) == 0)  /* READ_SINGLE_BLOCK */
            && rcvr_datablock(buff, 512))
            count = 0;
    }
    else {              /* Multiple block read */
        if (send_cmd(CMD18, sector) == 0) { /* READ_MULTIPLE_BLOCK */
            do {
                if (!rcvr_datablock(buff, 512)) break;
                buff += 512;
            } while (--count);
            send_cmd(CMD12, 0);             /* STOP_TRANSMISSION */
        }
    }
    deselect();

    return count ? RES_ERROR : RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
DRESULT SD_disk_write (
    BYTE drv,           /* Physical drive nmuber (0) */
    const BYTE *buff,   /* Pointer to the data to be written */
    DWORD sector,       /* Start sector number (LBA) */
    UINT count          /* Sector count */
)
{
    if (drv || !count) return RES_PARERR;
    if (media_status & STA_NOINIT) return RES_NOTRDY;
    if (media_status & STA_PROTECT) return RES_WRPRT;

    if (!(CardType & CT_BLOCK)) sector *= 512;  /* Convert to byte address if needed */

    if (count == 1) {   /* Single block write */
        if ((send_cmd(CMD24, sector) == 0)  /* WRITE_BLOCK */
            && xmit_datablock(buff, 0xFE))
            count = 0;
    }
    else {              /* Multiple block write */
        if (CardType & CT_SDC) send_cmd(ACMD23, count);
        if (send_cmd(CMD25, sector) == 0) { /* WRITE_MULTIPLE_BLOCK */
            do {
                if (!xmit_datablock(buff, 0xFC)) break;
                buff += 512;
            } while (--count);
            if (!xmit_datablock(0, 0xFD))   /* STOP_TRAN token */
                count = 1;
        }
    }
    deselect();

    return count ? RES_ERROR : RES_OK;
}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL
DRESULT SD_disk_ioctl (
    BYTE drv,       /* Physical drive number (0) */
    BYTE ctrl,      /* Control code */
    void *buff      /* Buffer to send/receive control data */
)
{
    DRESULT res;
    BYTE n, csd[16], *ptr = buff;
    DWORD csize;


    if (drv) return RES_PARERR;

    res = RES_ERROR;

    if (ctrl == CTRL_POWER) {
        switch (ptr[0]) {
        case 0:     /* Sub control code (POWER_OFF) */
            power_off();        /* Power off */
            res = RES_OK;
            break;
        case 1:     /* Sub control code (POWER_GET) */
            ptr[1] = (BYTE)power_status();
            res = RES_OK;
            break;
        default :
            res = RES_PARERR;
        }
    }
    else {
        if (media_status & STA_NOINIT) return RES_NOTRDY;

        switch (ctrl) {
        case CTRL_SYNC :        /* Make sure that no pending write process. Do not remove this or written sector might not left updated. */
            if (select()) {
                deselect();
                res = RES_OK;
            }
            break;

        case GET_SECTOR_COUNT : /* Get number of sectors on the disk (DWORD) */
            if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
                if ((csd[0] >> 6) == 1) {   /* SDC ver 2.00 */
                    csize = csd[9] + ((WORD)csd[8] << 8) + ((DWORD)(csd[7] & 63) << 16) + 1;
                    *(DWORD*)buff = csize << 10;
                } else {                    /* SDC ver 1.XX or MMC*/
                    n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
                    csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
                    *(DWORD*)buff = csize << (n - 9);
                }
                res = RES_OK;
            }
            break;

        case GET_SECTOR_SIZE :  /* Get R/W sector size (WORD) */
            *(WORD*)buff = 512;
            res = RES_OK;
            break;

        case GET_BLOCK_SIZE :   /* Get erase block size in unit of sector (DWORD) */
            if (CardType & CT_SD2) {    /* SDv2? */
                if (send_cmd(ACMD13, 0) == 0) { /* Read SD status */
                    xchg_spi(0xFF);
                    if (rcvr_datablock(csd, 16)) {              /* Read partial block */
                        for (n = 64 - 16; n; n--) xchg_spi(0xFF);   /* Purge trailing data */
                        *(DWORD*)buff = 16UL << (csd[10] >> 4);
                        res = RES_OK;
                    }
                }
            } else {                    /* SDv1 or MMCv3 */
                if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {  /* Read CSD */
                    if (CardType & CT_SD1) {    /* SDv1 */
                        *(DWORD*)buff = (((csd[10] & 63) << 1) + ((WORD)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
                    } else {                    /* MMCv3 */
                        *(DWORD*)buff = ((WORD)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
                    }
                    res = RES_OK;
                }
            }
            break;

        case MMC_GET_TYPE :     /* Get card type flags (1 byte) */
            *ptr = CardType;
            res = RES_OK;
            break;

        case MMC_GET_CSD :      /* Receive CSD as a data block (16 bytes) */
            if (send_cmd(CMD9, 0) == 0      /* READ_CSD */
                && rcvr_datablock(ptr, 16))
                res = RES_OK;
            break;

        case MMC_GET_CID :      /* Receive CID as a data block (16 bytes) */
            if (send_cmd(CMD10, 0) == 0     /* READ_CID */
                && rcvr_datablock(ptr, 16))
                res = RES_OK;
            break;

        case MMC_GET_OCR :      /* Receive OCR as an R3 resp (4 bytes) */
            if (send_cmd(CMD58, 0) == 0) {  /* READ_OCR */
                for (n = 4; n; n--) *ptr++ = xchg_spi(0xFF);
                res = RES_OK;
            }
            break;

        case MMC_GET_SDSTAT :   /* Receive SD status as a data block (64 bytes) */
            if (send_cmd(ACMD13, 0) == 0) { /* SD_STATUS */
                xchg_spi(0xFF);
                if (rcvr_datablock(ptr, 64))
                    res = RES_OK;
            }
            break;

        default:
            res = RES_PARERR;
        }

        deselect();
    }

    return res;
}
#endif // _USE_IOCTL

/*-----------------------------------------------------------------------*/
/* Media change                                                          */
/*-----------------------------------------------------------------------*/

/* This function should get called by an pin change interrupt @ card detect
 * If your HW lacks a card detect switch, define it as ordinary function
 * and call it at power-on 
 */

MEDIA_CHANGE_HANDLER
{
    update_media_status();
}

/*-----------------------------------------------------------------------*/
/* Glue to ff.c                                                          */
/*-----------------------------------------------------------------------*/

/* petSD has only SD cards, XS-1541 might have only SD-cards.
 * No need to care about ATA, USB... thus no diskio.c
 * Alias all routines to SD versions */

DSTATUS                          disk_initialize (BYTE pdrv)
__attribute__ ((weak, alias ("SD_disk_initialize")));

DSTATUS                          disk_status (BYTE pdrv)
__attribute__ ((weak, alias ("SD_disk_status")));

DRESULT                          disk_read (BYTE pdrv, BYTE*buff, DWORD sector, UINT count)
__attribute__ ((weak, alias ("SD_disk_read")));

#if _READONLY == 0
DRESULT                          disk_write (BYTE pdvr, const BYTE*buff, DWORD sector, UINT count)
__attribute__ ((weak, alias ("SD_disk_write")));
#endif

DRESULT                          disk_ioctl (BYTE pdvr, BYTE cmd, void*buff)
__attribute__ ((weak, alias ("SD_disk_ioctl")));;
