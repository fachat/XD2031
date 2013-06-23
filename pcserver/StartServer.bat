@echo off
rem Usage:
rem
rem fsser.exe <options> [...] <run-directory>
rem
rem -v will output some more debug messages
rem -d <device> specifies the COM-port, e.g. -d COM5
rem -A<drive>:<provider>=<parameter> assigns a drive
rem When the server starts, it changes into <run-directory>,
rem all directories will be relative to that directory.
rem A single dot specifies the current directory.

fsser.exe -v -d auto -A0:fs=sample -A1:fs=tools .
