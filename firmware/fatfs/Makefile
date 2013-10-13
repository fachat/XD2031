FILES+=fatfs/ff.c       	# FatFs module
FILES+=fatfs/option/ccsbcs.c	# Unicode (media) <--> Codepages (application)
FILES+=fatfs/dir.c		# Directories / wildcards
FILES+=fatfs/errcompat.c	# FatFs FRESULT vs CBM errno_t

DEPS+=fatfs/ff.h        	# FatFs module
DEPS+=fatfs/ffconf.h		# Configuration file for FatFs module
DEPS+=fatfs/diskio.h    	# Defs and protos for disk I/O modules
DEPS+=fatfs/integer.h   	# Integer type definitions for FatFs
DEPS+=fatfs/dir.h		# Directories / wildcards
DEPS+=fatfs/errcompat.h		# FatFs FRESULT vs CBM errno_t

CFLAGS+=-I rtc -I i2c -I fatfs
