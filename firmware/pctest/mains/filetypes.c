#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filetypes.h"

#define MAX_LINE 255
int main(int argc, char** argv) {
    char line[MAX_LINE + 1]; int had_a_comment = 1;

    char filename[MAX_LINE];
    int default_no_extension = 2;	// PRG
    int default_unknown_extension = 1;	// SEQ

    for (uint8_t i=0; i<10; i++) {
        printf("%u: %s\n", i, (char*) filetype_to_extension(i));
    }
    while(fgets(line, MAX_LINE, stdin) != NULL) {
        line[strlen(line) - 1] = 0; // drop '\n'
        if(line[0] == '#') {
            if(!had_a_comment) puts("\n");
            puts(line);
            had_a_comment=1;
            continue;
        } else {
            if(!had_a_comment) puts("\n");
            had_a_comment=0;
            printf("Test: '%s'\n", line);
        }
        // --------------------------------------
        if(sscanf(line, "%s %d %d\n", filename, &default_no_extension, &default_unknown_extension)) {
            printf("--> %s\n", filetype_to_extension(
		extension_to_filetype(filename, default_no_extension, default_unknown_extension)));
        }
    }

    return 0;
}
