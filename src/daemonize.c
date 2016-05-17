/*
dynip-utils - Client & server to update dynamic IP addresses (DIY DynDNS).
Copyright (C) 2013  Thomas Huehn <thomas.huehn@gmx.net>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "daemonize.h"

bool daemonize() {
	pid_t pid;
	int fd;

	pid = fork();
	if(pid == 0) {
		chdir("/");

		close(0);
		close(1);
		close(2);

		fd = open("/dev/null", O_RDWR);
		if(fd < 0) exit(EXIT_FAILURE);

		if(setsid() == (pid_t) -1) exit(EXIT_FAILURE);
		if(dup2(fd, 0) == -1) exit(EXIT_FAILURE);
		if(dup2(fd, 1) == -1) exit(EXIT_FAILURE);
		if(dup2(fd, 2) == -1) exit(EXIT_FAILURE);

		return true;
	}
	else if(pid == -1) {
		return false;
	}

	exit(EXIT_SUCCESS);
}
