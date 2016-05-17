/*
dynip-server - Client & server to update dynamic IP addresses (DIY DynDNS).
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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "config.h"
#include "daemonize.h"

static const char *cmd = "dynip-client";

static char buffer[BUFFER_SIZE] = "DynIP";
static size_t length = 6;

static void help() {
	printf("Usage: %s [OPTION]... HOSTNAME [ARGUMENT]...\n", cmd);
	printf("Client for updating dynamic IP addresses.\n");
	printf("\n");
	printf("  -f             don't daemonize\n");
	printf("  -i INTERVAL    change the update interval\n");
	printf("  -p PORT        use a different port\n");
	printf("      --help     display this help and exit\n");
	printf("      --version  output version information and exit\n");
	printf("\n");
	printf("The default port is 2342. The update interval is 5 minutes.\n");

	exit(EXIT_SUCCESS);
}

static void version() {
	printf("dynip-client %s\n", VERSION);
	printf("Copyright (C) 2013  Thomas Huehn <thomas.huehn@gmx.net>\n");
	printf("License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n");
	printf("This is free software: you are free to change and redistribute it.\n");
	printf("There is NO WARRANTY, to the extent permitted by law.\n");

	exit(EXIT_SUCCESS);
}

static void client(const char *hostname, const char *port, bool foreground, unsigned int interval)
{
	struct addrinfo hints, *first_res, *res;
	int sockfd;
	int i;

	if(foreground == false) {
		if(daemonize() == false) {
			fprintf(stderr, "%s: fork(): %s\n", cmd, strerror(errno));
		}
	}

	while(true) {
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;

		i = getaddrinfo(hostname, port, &hints, &first_res);
		if(i != 0) {
			fprintf(stderr, "%s: getaddrinfo(): %s\n", cmd, gai_strerror(i));
			exit(EXIT_FAILURE);
		}

		for(res = first_res; res != NULL; res = res->ai_next) {
			sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
			if(sockfd == -1) {
				fprintf(stderr, "%s: socket(): %s\n", cmd, strerror(errno));
				continue;
			}

			if(sendto(sockfd, buffer, length, 0, res->ai_addr, res->ai_addrlen) == -1) {
				fprintf(stderr, "%s: sendto(): %s\n", cmd, strerror(errno));
				continue;
			}

			break;
		}

		freeaddrinfo(first_res);

		sleep(interval);
	}
}

static bool add_argument(const char *str) {
	size_t len = strlen(str) + 1;

	if(length + len <= sizeof(buffer))
	{
		memcpy(&buffer[length], str, len);
		length += len;
		return true;
	}

	return false;
}

static int parse_interval(const char *str)
{
	char *endptr;
	long int interval;

	interval = strtol(str, &endptr, 10);

	if(interval < 1 || interval >= LONG_MAX) {
		errno = ERANGE;
		return -1;
	}

	if(strcmp(endptr, "s") == 0 || strcmp(endptr, "") == 0) {
	}
	else if(strcmp(endptr, "m") == 0) {
		interval *= 60;
	}
	else if(strcmp(endptr, "h") == 0) {
		interval *= 60 * 60;
	}
	else if(strcmp(endptr, "d") == 0) {
		interval *= 60 * 60 * 24;
	}
	else {
		errno = EINVAL;
		return -1;
	}

	if(interval < 1 || interval > INT_MAX) {
		errno = ERANGE;
		return -1;
	}

	return (int) interval;
}

int main(int argc, char *argv[]) {
	bool foreground = false;
	const char *hostname = NULL;
	const char *port = "2342";
	int interval = 5 * 60;
	int c;

	if(argc > 0) cmd = argv[0];

	if(argc > 1) {
		if(strcmp(argv[1], "--help") == 0) {
			help();
		}
		else if(strcmp(argv[1], "--version") == 0) {
			version();
		}
	}

	while((c = getopt(argc, argv, ":fi:p:")) != -1) {
		switch(c) {
			case 'f':
				foreground = true;
				break;
			case 'i':
				interval = parse_interval(optarg);
				if(interval < 1) {
					fprintf(stderr, "%s: %s: -i\n", cmd, strerror(errno));
					exit(EXIT_FAILURE);
				}
				break;
			case 'p':
				port = optarg;
				break;
			case '?':
				fprintf(stderr, "%s: Invalid option: -%c\n", cmd, optopt);
				exit(EXIT_FAILURE);
			case ':':
				fprintf(stderr, "%s: Option requires an argument: -%c\n", cmd, optopt);
				exit(EXIT_FAILURE);
		}
	}

	if(optind < argc) {
		hostname = argv[optind];
		optind++;
	}
	else {
		fprintf(stderr, "%s: Hostname not specified\n", cmd);
		exit(EXIT_FAILURE);
	}

	if(argc - optind > ARGUMENT_MAX) {
		fprintf(stderr, "%s: Too many arguments\n", cmd);
		exit(EXIT_FAILURE);
	}

	for(; optind < argc; optind++) {
		if(add_argument(argv[optind]) == false) {
			fprintf(stderr, "%s: Buffer size exceeded\n", cmd);
			exit(EXIT_FAILURE);
		}
	}

	client(hostname, port, foreground, interval);

	return EXIT_SUCCESS;
}
