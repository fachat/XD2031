
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

#include "myunit.h"

#include "name.h"


static void fuzz_nameinfo(nameinfo_t *nameinfo) {
	// fuzz with garbage
	nameinfo->access = 1;
	nameinfo->options = 2;
	nameinfo->drive = 96;
	nameinfo->name = NULL;	//(char*)7697;
}

void name_without_anything_to_open()
{
	cmd_t cmd;
	nameinfo_t nameinfo;

	fuzz_nameinfo(&nameinfo);

	char *filename="testname";

	// setting up the test data
	strcpy((char*)cmd.command_buffer, filename);
	cmd.command_length = strlen(filename)+1;

	parse_filename(&cmd, &nameinfo, 0);

	mu_assert_info("nameinfo drive", nameinfo.drive == NAMEINFO_UNUSED_DRIVE);
	mu_assert_info_str_eq("nameinfo filename", (char*)nameinfo.name, filename);
	mu_assert_info("nameinfo access", nameinfo.access == 'R');
	mu_assert_info("nameinfo options", nameinfo.options == 0);
	mu_assert_info("nameinfo cmd", nameinfo.cmd == CMD_NONE);
}

void name_with_drive_to_open() 
{
	cmd_t cmd;
	nameinfo_t nameinfo;

	fuzz_nameinfo(&nameinfo);

	char *filename="3:testname";

	// setting up the test data
	strcpy((char*)cmd.command_buffer, filename);
	cmd.command_length = strlen(filename)+1;

	parse_filename(&cmd, &nameinfo, 0);

	mu_assert_info("nameinfo drive", nameinfo.drive == 3);
	mu_assert_info_str_eq("nameinfo filename", (char*)nameinfo.name, filename+2);
	mu_assert_info("nameinfo access", nameinfo.access == 'R');
	mu_assert_info("nameinfo options", nameinfo.options == 0);
	mu_assert_info("nameinfo cmd", nameinfo.cmd == CMD_NONE);
}

void name_without_anything_to_load()
{
	cmd_t cmd;
	nameinfo_t nameinfo;

	fuzz_nameinfo(&nameinfo);

	char *filename="testname";

	// setting up the test data
	strcpy((char*)cmd.command_buffer, filename);
	cmd.command_length = strlen(filename)+1;

	parse_filename(&cmd, &nameinfo, PARSEHINT_LOAD);

	mu_assert_info("nameinfo drive", nameinfo.drive == NAMEINFO_UNUSED_DRIVE);
	mu_assert_info_str_eq("nameinfo filename", (char*)nameinfo.name, filename);
	mu_assert_info("nameinfo access", nameinfo.access == 'R');
	mu_assert_info("nameinfo options", nameinfo.options == 0);
	mu_assert_info("nameinfo cmd", nameinfo.cmd == CMD_NONE);
}

void name_with_drive_to_load()
{
	cmd_t cmd;
	nameinfo_t nameinfo;

	fuzz_nameinfo(&nameinfo);

	char *filename="4:testname";

	// setting up the test data
	strcpy((char*)cmd.command_buffer, filename);
	cmd.command_length = strlen(filename)+1;

	parse_filename(&cmd, &nameinfo, PARSEHINT_LOAD);

	mu_assert_info("nameinfo drive", nameinfo.drive == 3);
	mu_assert_info_str_eq("nameinfo filename", (char*)nameinfo.name, filename);
	mu_assert_info("nameinfo access", nameinfo.access == 'R');
	mu_assert_info("nameinfo options", nameinfo.options == 0);
	mu_assert_info("nameinfo cmd", nameinfo.cmd == CMD_NONE);
}

void cmd_without_anything_short()
{
	cmd_t cmd;
	nameinfo_t nameinfo;

	fuzz_nameinfo(&nameinfo);

	char *filename="I";

	// setting up the test data
	strcpy((char*)cmd.command_buffer, filename);
	cmd.command_length = strlen(filename)+1;

	parse_filename(&cmd, &nameinfo, PARSEHINT_COMMAND);

	mu_assert_info("nameinfo drive", nameinfo.drive == NAMEINFO_UNUSED_DRIVE);
	mu_assert_info_str_eq("nameinfo filename", (char*)nameinfo.name, "");
	mu_assert_info("nameinfo access", nameinfo.access == 0);
	mu_assert_info("nameinfo options", nameinfo.options == 0);
	mu_assert_info("nameinfo cmd", nameinfo.cmd == CMD_INITIALIZE);
}

void cmd_without_anything_med()
{
	cmd_t cmd;
	nameinfo_t nameinfo;

	fuzz_nameinfo(&nameinfo);

	char *filename="INITIA";

	// setting up the test data
	strcpy((char*)cmd.command_buffer, filename);
	cmd.command_length = strlen(filename)+1;

	parse_filename(&cmd, &nameinfo, PARSEHINT_COMMAND);

	mu_assert_info("nameinfo drive", nameinfo.drive == NAMEINFO_UNUSED_DRIVE);
	mu_assert_info_str_eq("nameinfo filename", (char*)nameinfo.name, "");
	mu_assert_info("nameinfo access", nameinfo.access == 0);
	mu_assert_info("nameinfo options", nameinfo.options == 0);
	mu_assert_info("nameinfo cmd", nameinfo.cmd == CMD_INITIALIZE);
}

void cmd_without_anything_exact()
{
	cmd_t cmd;
	nameinfo_t nameinfo;

	fuzz_nameinfo(&nameinfo);

	char *filename="INITIALIZE";

	// setting up the test data
	strcpy((char*)cmd.command_buffer, filename);
	cmd.command_length = strlen(filename)+1;

	parse_filename(&cmd, &nameinfo, PARSEHINT_COMMAND);

	mu_assert_info("nameinfo drive", nameinfo.drive == NAMEINFO_UNUSED_DRIVE);
	mu_assert_info_str_eq("nameinfo filename", (char*)nameinfo.name, "");
	mu_assert_info("nameinfo access", nameinfo.access == 0);
	mu_assert_info("nameinfo options", nameinfo.options == 0);
	mu_assert_info("nameinfo cmd", nameinfo.cmd == CMD_INITIALIZE);
}

void cmd_without_anything_too_long()
{
	cmd_t cmd;
	nameinfo_t nameinfo;

	fuzz_nameinfo(&nameinfo);

	char *filename="INITIALIZEXYZ";

	// setting up the test data
	strcpy((char*)cmd.command_buffer, filename);
	cmd.command_length = strlen(filename)+1;

	parse_filename(&cmd, &nameinfo, PARSEHINT_COMMAND);

	mu_assert_info("nameinfo drive", nameinfo.drive == NAMEINFO_UNUSED_DRIVE);
	mu_assert_info_str_eq("nameinfo filename", (char*)nameinfo.name, "");
	mu_assert_info("nameinfo access", nameinfo.access == 0);
	mu_assert_info("nameinfo options", nameinfo.options == 0);
	mu_assert_info("nameinfo cmd", nameinfo.cmd == CMD_INITIALIZE);
}

void cmd_with_drive_short()
{
	cmd_t cmd;
	nameinfo_t nameinfo;

	fuzz_nameinfo(&nameinfo);

	char *filename="I0";

	// setting up the test data
	strcpy((char*)cmd.command_buffer, filename);
	cmd.command_length = strlen(filename)+1;

	parse_filename(&cmd, &nameinfo, PARSEHINT_COMMAND);

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


