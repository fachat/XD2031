/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat

    Derived from:
    OS/A65 Version 1.3.12
    Multitasking Operating System for 6502 Computers
    Copyright (C) 1989-1997 Andre Fachat

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation;
    version 2 of the License ONLY.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

****************************************************************************/

/**
 * Basic definitions
 */

#define   DIRSIGN        "/"       /* Trennzeichen zwischen Verzeichnissen */

/* Befehle an das File-System                                              */

#define	  FS_TERM	 0	   /* send message to terminal output      */
#define   FS_OPEN_RD     1         /* Datei zum Lesen oeffnen              */
#define   FS_OPEN_WR     2         /* Datei zum Schreiben oeffnen          */
#define   FS_OPEN_RW     3         /* Datei zum Lesen und Schreiben oeffnen*/
#define   FS_OPEN_OW     4         /* Datei zum Ueberschreiben oeffnen     */
#define   FS_OPEN_AP     5         /* Datei zum Anhaengen oeffnen          */
#define   FS_OPEN_DR     6         /* Verzeichnis zum Lesen oeffnen        */
#define   FS_RENAME      7         /* Datei Umbennen                       */
#define   FS_DELETE      8         /* Datei loeschen                       */
#define   FS_FORMAT      9         /* Diskette formatieren                 */
#define   FS_CHKDSK      10        /* Diskette auf Konsistenz testen       */
#define   FS_CLOSE       11        /* Datei loeschen (nur bei OPEN_RW)     */
#define   FS_RMDIR       12        /* Unterverzeichnis loeschen            */
#define   FS_MKDIR       13        /* Unterverzeichnis erstellen           */
#define   FS_CHDIR       14        /* ----                                 */
#define   FS_ASSIGN      15        /* ----                                 */

/* Struktur, die bei OPEN gesendet werden muss                             */

#define   FS_OPEN_DRV    0         /* Laufwerk = FM_OPEN_DRV               */
#define   FS_OPEN_STR    1         /* Stream, ueber den Daten laufen sollen */
#define   FS_OPEN_PFAD   2         /* ----                                 */
#define   FS_OPEN_NAME   3         /* Name der zu îffnenden Datei,0        */

/* Struktur, die als Antwort zurÅckgesendet wird                           */

#define   FS_X_ERR       0         /* Fehlermeldung, auch im Akku          */
#define   FS_X_ENV       1         /* Task-Nummer des File-Systems         */
#define   FS_X_FIL       2         /* File-Handle (nur bei OPEN_RW)        */
#define   FS_X_SLEN      3         /* (LÑnge der Struktur)                 */

/* Struktur eines Dir-Eintrags in der Directory-Datei                      */

#define   FS_DIR_LEN     0    /* in Bytes, 4 Byte lang, lo byte first      */
#define   FS_DIR_YEAR    4    /* Jahr -1900                                */
#define   FS_DIR_MONTH   5    /* Monat                                     */
#define   FS_DIR_DAY     6    /* Tag                                       */
#define   FS_DIR_HOUR    7    /* Stunde                                    */
#define   FS_DIR_MIN     8    /* Minute                                    */
#define   FS_DIR_SEC     9    /* Sekunde der Erstellung/letzten énderung   */
#define   FS_DIR_MODE    10   /* Art von Eintrag                           */
#define   FS_DIR_NAME    11   /* Ab hier Name,0                            */

/* Arten der Directory-EintrÑge                                            */

#define   FS_DIR_MOD_FIL 0    /* Datei                                     */
#define   FS_DIR_MOD_NAM 1    /* Diskettenname                             */
#define   FS_DIR_MOD_FRE 2    /* Anzahl freier Bytes in DIR_LEN            */
#define   FS_DIR_MOD_DIR 3    /* Unterverzeichnis                          */

/* Struktur, die bei Kommandos auûer SEND gesendet werden muû              */
/*        identisch FS_OPEN_xxx                                            */ 

#define   FS_CMD_DRV     0
#define   FS_CMD_PFAD    1
#define   FS_CMD_FIL     2
#define   FS_CMD_NAME    3    /* Nach dem Nullbyte am Ende des ersten      */
                              /* Namens folgt bei Rename noch der zweite   */  
                              /* Name, ebenfalls mit NUllbyte abgeschlossen*/

