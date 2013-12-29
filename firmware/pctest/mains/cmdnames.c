#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "cmdnames.h"

int main(int argc, char *argv[])
{
	char line[256];
	uint8_t len;
	uint8_t cmd;

	while (fgets(line, 256, stdin)) {
		if (strlen(line)) line[strlen(line) - 1] = 0; // drop '\n'
		else printf("\n");
		if (line[0] == '#') printf("\n%s\n", line);
		else {
			printf ("Input:  '%s'\n", line);
			cmd = command_find( (uint8_t *) line, &len);
			printf("Parsed: cmd = %3d   len = %2d   ", cmd, len);
			printf("<%s>\n\n", (char *) command_to_name(cmd));
		}
	}
	return 0;
}
