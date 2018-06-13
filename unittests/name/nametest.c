
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>

#include "myunit.h"

#include "name.h"

#define	BUFLEN		255

static void fuzz_nameinfo(nameinfo_t *nameinfo) {
	// fuzz with garbage
	nameinfo->access = 1;
	nameinfo->options = 2;
	nameinfo->drive = 96;
	nameinfo->name = NULL;	//(char*)7697;
}

static uint8_t buf[BUFLEN];

static void parse(char *filename, nameinfo_t *nameinfo, int flag) {

	// setting up the test data
	strncpy((char*)buf, filename, BUFLEN);

	parse_filename(buf, strlen(filename), BUFLEN, nameinfo, flag);
}

void name_without_anything_to_open()
{
	nameinfo_t nameinfo;

	fuzz_nameinfo(&nameinfo);

	char *filename="testname";

	parse(filename, &nameinfo, 0);

	mu_assert_info("nameinfo drive", nameinfo.drive == NAMEINFO_UNUSED_DRIVE);
	mu_assert_info_str_eq("nameinfo filename", (char*)nameinfo.name, filename);
	mu_assert_info("nameinfo access", nameinfo.access == 0);
	mu_assert_info("nameinfo options", nameinfo.options == 0);
	mu_assert_info("nameinfo cmd", nameinfo.cmd == CMD_NONE);
}

void name_with_drive_to_open() 
{
	nameinfo_t nameinfo;

	fuzz_nameinfo(&nameinfo);

	char *filename="3:testname";

	parse(filename, &nameinfo, 0);

	mu_assert_info("nameinfo drive", nameinfo.drive == 3);
	mu_assert_info_str_eq("nameinfo filename", (char*)nameinfo.name, filename+2);
	mu_assert_info("nameinfo access", nameinfo.access == 0);
	mu_assert_info("nameinfo options", nameinfo.options == 0);
	mu_assert_info("nameinfo cmd", nameinfo.cmd == CMD_NONE);
}

void name_without_anything_to_load()
{
	nameinfo_t nameinfo;

	fuzz_nameinfo(&nameinfo);

	char *filename="testname,r";

	parse(filename, &nameinfo, 0);

	mu_assert_info("nameinfo drive", nameinfo.drive == NAMEINFO_UNUSED_DRIVE);
	mu_assert_info_str_eq("nameinfo filename", (char*)nameinfo.name, filename);
	mu_assert_info("nameinfo access", nameinfo.access == 'R');
	mu_assert_info("nameinfo options", nameinfo.options == 0);
	mu_assert_info("nameinfo cmd", nameinfo.cmd == CMD_NONE);
}

void name_with_drive_to_load()
{
	nameinfo_t nameinfo;

	fuzz_nameinfo(&nameinfo);

	char *filename="4:testname";

	parse(filename, &nameinfo, 0);

	mu_assert_info("nameinfo drive", nameinfo.drive == 3);
	mu_assert_info_str_eq("nameinfo filename", (char*)nameinfo.name, filename);
	mu_assert_info("nameinfo access", nameinfo.access == 0);
	mu_assert_info("nameinfo options", nameinfo.options == 0);
	mu_assert_info("nameinfo cmd", nameinfo.cmd == CMD_NONE);
}

void cmd_without_anything_short()
{
	nameinfo_t nameinfo;

	fuzz_nameinfo(&nameinfo);

	char *filename="I";

	parse(filename, &nameinfo, 0);

	mu_assert_info("nameinfo drive", nameinfo.drive == NAMEINFO_UNUSED_DRIVE);
	mu_assert_info_str_eq("nameinfo filename", (char*)nameinfo.name, "");
	mu_assert_info("nameinfo access", nameinfo.access == 0);
	mu_assert_info("nameinfo options", nameinfo.options == 0);
	mu_assert_info("nameinfo cmd", nameinfo.cmd == CMD_INITIALIZE);
}

void cmd_without_anything_med()
{
	nameinfo_t nameinfo;

	fuzz_nameinfo(&nameinfo);

	char *filename="INITIA";

	parse(filename, &nameinfo, 0);

	mu_assert_info("nameinfo drive", nameinfo.drive == NAMEINFO_UNUSED_DRIVE);
	mu_assert_info_str_eq("nameinfo filename", (char*)nameinfo.name, "");
	mu_assert_info("nameinfo access", nameinfo.access == 0);
	mu_assert_info("nameinfo options", nameinfo.options == 0);
	mu_assert_info("nameinfo cmd", nameinfo.cmd == CMD_INITIALIZE);
}

void cmd_without_anything_exact()
{
	nameinfo_t nameinfo;

	fuzz_nameinfo(&nameinfo);

	char *filename="INITIALIZE";

	parse(filename, &nameinfo, 0);

	mu_assert_info("nameinfo drive", nameinfo.drive == NAMEINFO_UNUSED_DRIVE);
	mu_assert_info_str_eq("nameinfo filename", (char*)nameinfo.name, "");
	mu_assert_info("nameinfo access", nameinfo.access == 0);
	mu_assert_info("nameinfo options", nameinfo.options == 0);
	mu_assert_info("nameinfo cmd", nameinfo.cmd == CMD_INITIALIZE);
}

void cmd_without_anything_too_long()
{
	nameinfo_t nameinfo;

	fuzz_nameinfo(&nameinfo);

	char *filename="INITIALIZEXYZ";

	parse(filename, &nameinfo, 0);

	mu_assert_info("nameinfo drive", nameinfo.drive == NAMEINFO_UNUSED_DRIVE);
	mu_assert_info_str_eq("nameinfo filename", (char*)nameinfo.name, "");
	mu_assert_info("nameinfo access", nameinfo.access == 0);
	mu_assert_info("nameinfo options", nameinfo.options == 0);
	mu_assert_info("nameinfo cmd", nameinfo.cmd == CMD_INITIALIZE);
}

void cmd_with_drive_short()
{
	nameinfo_t nameinfo;

	fuzz_nameinfo(&nameinfo);

	char *filename="I0";

	parse(filename, &nameinfo, 0);

	mu_assert_info("nameinfo drive", nameinfo.drive == 0);
	mu_assert_info_str_eq("nameinfo filename", (char*)nameinfo.name, "");
	mu_assert_info("nameinfo access", nameinfo.access == 0);
	mu_assert_info("nameinfo options", nameinfo.options == 0);
	mu_assert_info("nameinfo cmd", nameinfo.cmd == CMD_INITIALIZE);
}



int main(int argc, const char *argv[]) {

	int number_failed = 0;

	mu_init(argc, argv);

	mu_add("name_without_anything_to_open", name_without_anything_to_open);
	mu_add("name_without_anything_to_load", name_without_anything_to_load);
	mu_add("cmd_without_anything_short", cmd_without_anything_short);
	mu_add("cmd_without_anything_med", cmd_without_anything_med);
	mu_add("cmd_without_anything_exact", cmd_without_anything_exact);
	mu_add("cmd_without_anything_too_long", cmd_without_anything_too_long);
	mu_add("cmd_with_drive_short", cmd_with_drive_short);

	mu_run();

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}


