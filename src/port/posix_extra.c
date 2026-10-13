/*
 * posix_extra.c - POSIX functions KEGS uses that circle-newlib lacks.
 *
 * waitpid(): KEGS opens compressed disk images (.gz and the like) by
 * running a decompressor in a child process (config.c,
 * config_file_to_pipe()). There are no processes on the Pi: newlib's fork()
 * already fails, so KEGS never gets as far as waiting for a child, but the
 * call must still link.
 *
 * This file is part of BMApple2, a bare metal Apple II and IIgs emulator for
 * the Raspberry Pi. See LICENSE for licensing details.
 */

#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

pid_t waitpid(pid_t pid, int *status, int options)
{
	(void) pid;
	(void) status;
	(void) options;
	errno = ECHILD;
	return -1;
}
