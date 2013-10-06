#include <stdio.h>
#include <string.h>
#include "cmd.h"
#include "name.h"

#define MAX_LINE 255
int main(int argc, char** argv) {
	char line[MAX_LINE + 1]; int had_a_comment = 1;
	cmd_t in;
	nameinfo_t result;
	uint8_t parsehint = PARSEHINT_COMMAND;

	while(fgets(line, MAX_LINE, stdin) != NULL) {
	line[strlen(line) - 1] = 0; // drop '\n'
	if(line[0] == '#') {
		if(!had_a_comment) puts("\n");
		puts(line);
		had_a_comment=1;
		continue;
	} else if(line[0] == '!') {
		printf("%s\n", line);
		if(!strcmp(line, "!PARSEHINT_COMMAND")) parsehint = PARSEHINT_COMMAND;
		else if(!strcmp(line, "!PARSEHINT_LOAD")) parsehint = PARSEHINT_LOAD;
		else printf("*** SYNTAX ERROR: '%s'\n", line);
		continue;
	} else {
		if(!had_a_comment) puts("\n");
		had_a_comment=0;
		printf("Test: '%s'\n", line);
	}
	// --------------------------------------
	strcpy((char*)in.command_buffer, line);
	in.command_length = strlen(line) + 1; // length including zero-byte
	printf("parsehint: ");
	if(parsehint == PARSEHINT_COMMAND) printf("PARSEHINT_COMMAND\n");
	else if(parsehint == PARSEHINT_LOAD) printf("PARSEHINT_LOAD\n");
	else printf("%d ?\n", parsehint);
	parse_filename(&in, &result, parsehint);
	}

	return 0;
}
