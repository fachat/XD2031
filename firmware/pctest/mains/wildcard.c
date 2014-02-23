#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

#include "wildcard.h"

#define MAX_LINE	255
#define MAX_NAMES	255
#define MAX_PATTERNS	255



char name[MAX_NAMES][MAX_LINE];
char pattern[MAX_PATTERNS][MAX_LINE];
int names = 0;
int patterns = 0;

void compare(bool advanced) {
	int n, p;
	int matches;

	for(p = 0; p < patterns; p++) {
		printf("Testing pattern (%d/%d) '%s': ", p+1, patterns, pattern[p]);
		matches = 0;
		for (n = 0; n < names; n++) {
			if (compare_pattern(name[n], pattern[p], advanced)) matches++;
		}
		if (matches == names) {
			printf("matches all %d names.\n", names);
		} else {
			printf("%d matches\n", matches);
			for (n = 0; n < names; n++) {
				if (compare_pattern(name[n], pattern[p], advanced))
					printf("\t%s\n", name[n]);
			}
		}
		puts("");
	}
}

int main(int argc, char** argv) {
	char line[MAX_LINE + 1];
	int had_a_comment = true;

	while(fgets(line, MAX_LINE, stdin) != NULL) {
		line[strlen(line) - 1] = 0; // drop '\n'
		if(line[0] == '#') {
			if(!had_a_comment) puts("\n");
			puts(line);
			had_a_comment=1;
			continue;
		} else if(line[0] == '!') {
			printf("%s\n", line);
			if(!strcmp(line, "!NAMES")) {
				while (1) {
					if(fgets(name[names], MAX_LINE, stdin)) {
						name[names][strlen(name[names]) - 1] = 0;
						puts(name[names]);
						if(strlen(name[names])) {
							names++;
							continue;
						}
						printf("%d names read.\n\n", names);
						break;
					}
				}
			} else if(!strcmp(line, "!PATTERNS")) {
				while (1) {
					if(fgets(pattern[patterns], MAX_LINE, stdin)) {
						pattern[patterns][strlen(pattern[patterns]) - 1] = 0;
						puts(pattern[patterns]);
						if(strlen(pattern[patterns])) {
							patterns++;
							continue;
						}
						printf("%d patterns read.\n\n", patterns);
						break;
					}
				}
			} else if(!strcmp(line, "!CLASSIC_MATCH")) {
				compare(false);
			} else if(!strcmp(line, "!ADVANCED_MATCH")) {
				compare(true);
			}
			else printf("*** SYNTAX ERROR: '%s'\n", line);
			continue;
		} else {
			printf("ERROR: Unexpected input '%s'\n", line);
		}
		// --------------------------------------
	}

	return 0;
}
