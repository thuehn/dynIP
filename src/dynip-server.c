/*
dynip-server - Client & server to update dynamic IP addresses (DIY DynDNS).
Copyright (C) 2013 Thomas Huehn <thomas.huehn@gmx.net>

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <poll.h>

#include "config.h"
#include "daemonize.h"

static const char *cmd = "dynip-server";

static char buffer[BUFFER_SIZE];
static size_t length;

static const char **arguments;
static int arguments_max;
static int arguments_local;
static int arguments_remote;

static bool init_arguments(int local_arguments)
{
	arguments_max = 1 + local_arguments + 1 + ARGUMENT_MAX + 1;
	arguments_local = 0;
	arguments_remote = -1;

	arguments = malloc(arguments_max);
	if(arguments == NULL) return false;

	return true;
}

static void remote_arguments()
{
	arguments_remote = arguments_local;
}

static bool add_argument(const char *str) {
	if(arguments_remote == -1) {
		if(arguments_local + 1 >= arguments_max)
			return false;

		arguments[arguments_local++] = str;
		arguments[arguments_local] = NULL;
	}
	else {
		if(arguments_remote + 1 >= arguments_max)
			return false;

		arguments[arguments_remote++] = str;
		arguments[arguments_remote] = NULL;
	}

	return true;
}

static char strsockaddr_buf[INET6_ADDRSTRLEN];
const char *strsockaddr(const struct sockaddr *address) {
	const struct sockaddr_in *inet;
	const struct sockaddr_in6 *inet6;

	if(address->sa_family == AF_INET) {
		inet = (const struct sockaddr_in *) address;
		return inet_ntop(AF_INET, &inet->sin_addr.s_addr, strsockaddr_buf, sizeof(strsockaddr_buf));
	}
	else if(address->sa_family == AF_INET6) {
		inet6 = (const struct sockaddr_in6 *) address;
		return inet_ntop(AF_INET6, inet6->sin6_addr.s6_addr, strsockaddr_buf, sizeof(strsockaddr_buf));
	}

	errno = EINVAL;
	return NULL;
}

static void help() {
	printf("Usage: %s [OPTION]... COMMAND [ARGUMENT]...\n", cmd);
	printf("Server for updating dynamic IP addresses.\n");
	printf("\n");
	printf("  -f             don't daemonize\n");
	printf("  -h HOSTNAME    bind to a specific hostname\n");
	printf("  -p PORT        use a different port\n");
	printf("      --help     display this help and exit\n");
	printf("      --version  output version information and exit\n");
	printf("\n");
	printf("The default port is 2342.\n");

	exit(EXIT_SUCCESS);
}

static void version() {
	printf("dynip-server %s\n", VERSION);
	printf("Copyright (C) 2013  Thomas Huehn <thomas.huehn@gmx.net>\n");
	printf("License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n");
	printf("This is free software: you are free to change and redistribute it.\n");
	printf("There is NO WARRANTY, to the extent permitted by law.\n");

	exit(EXIT_SUCCESS);
}

static void server(const char *hostname, const char *port, bool foreground) {
	struct addrinfo hints, *first_res, *res;
	struct pollfd *sockfds;
	int sockfds_count;
	const int on = 1;
	struct sockaddr_storage address;
	socklen_t address_len = sizeof(address);
	const char *address_str;
	int status;
	int i;
	pid_t pid;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	status = getaddrinfo(hostname, port, &hints, &first_res);
	if(status != 0) {
		fprintf(stderr, "%s: getaddrinfo(): %s\n", cmd, gai_strerror(status));
		exit(EXIT_FAILURE);
	}

	sockfds_count = 0;
	for(res = first_res; res != NULL; res = res->ai_next) {
		sockfds_count++;
	}

	if(sockfds_count == 0) exit(EXIT_FAILURE);

	sockfds = malloc(sizeof(struct pollfd) * sockfds_count);
	if(sockfds == NULL)
	{
		fprintf(stderr, "%s: malloc(): %s\n", cmd, strerror(errno));
		exit(EXIT_FAILURE);
	}

	sockfds_count = 0;
	for(res = first_res; res != NULL; res = res->ai_next) {
		sockfds[sockfds_count].fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if(sockfds[sockfds_count].fd < 0) {
			fprintf(stderr, "%s: socket(): %s\n", cmd, strerror(errno));
			continue;
		}

		if(setsockopt(sockfds[sockfds_count].fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
			fprintf(stderr, "%s: setsockopt(SO_REUSEADDR): %s\n", cmd, strerror(errno));
		}

		if(bind(sockfds[sockfds_count].fd, res->ai_addr, res->ai_addrlen) == -1) {
			fprintf(stderr, "%s: bind(): %s\n", cmd, strerror(errno));
			close(sockfds[sockfds_count].fd);
			continue;
		}

		sockfds[sockfds_count].events = POLLIN;
		sockfds_count++;
	}

	if(sockfds_count == 0) exit(EXIT_FAILURE);

	freeaddrinfo(first_res);

	if(foreground == false) {
		if(daemonize() == false) {
			fprintf(stderr, "%s: fork(): %s\n", cmd, strerror(errno));
		}
	}

	while(true) {
		if(poll(sockfds, sockfds_count, -1) <= 0) {
			continue;
		}

		for(i = 0; i < sockfds_count; i++) {
			if((sockfds[i].revents & POLLIN) != 0) {
				remote_arguments();

				length = recvfrom(sockfds[i].fd, buffer, sizeof(buffer), 0, (struct sockaddr *) &address, &address_len);
				if(length == -1) {
					fprintf(stderr, "%s: recvfrom(): %s\n", cmd, strerror(errno));
					continue;
				}

				if(length < 6 || memcmp(&buffer[0], "DynIP", 6) != 0 || buffer[length - 1] != '\0')
				{
					fprintf(stderr, "%s: Received invalid packet\n", cmd);
					continue;
				}

				address_str = strsockaddr((const struct sockaddr *) &address);
				if(address_str == NULL) {
					fprintf(stderr, "%s: inet_ntop(): %s\n", cmd, strerror(errno));
					continue;
				}

				if(add_argument(address_str) == false) {
					fprintf(stderr, "%s: Too many remote arguments\n", cmd);
					continue;
				}

				status = 0;
				for(i = 0; i + 1 < length; i++) {
					if(buffer[i] == '\0') {
						if(add_argument(&buffer[i + 1]) == false) {
							status = -1;
							break;
						}
					}
				}
				if(status == -1) {
					fprintf(stderr, "%s: Too many remote arguments\n", cmd);
					continue;
				}

				pid = fork();
				if(pid == 0) {
					if(execvp(arguments[0], (char * const *) arguments) == -1) {
						fprintf(stderr, "%s: execvp(): %s\n", cmd, strerror(errno));
					}
				}
				else if(pid == -1) {
					fprintf(stderr, "%s: fork(): %s\n", cmd, strerror(errno));
				}

				if(waitpid(pid, &status, 0) == (pid_t) -1) {
					fprintf(stderr, "%s: waitpid(): %s\n", cmd, strerror(errno));
				}
				else if(WEXITSTATUS(status) != EXIT_SUCCESS) {
					fprintf(stderr, "%s: Command returned: %i\n", cmd, WEXITSTATUS(status));
				}
			}
		}
	}
}

int main(int argc, char *argv[]) {
	bool foreground = false;
	const char *hostname = "";
	const char *port = "2342";
	const char *command = NULL;
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
			case 'h':
				hostname = optarg;
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
		command = argv[optind];
		optind++;
	}
	else {
		fprintf(stderr, "%s: Command not specified\n", cmd);
		exit(EXIT_FAILURE);
	}

	if(init_arguments(argc - optind) == false) {
		fprintf(stderr, "%s: malloc(): %s\n", cmd, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if(add_argument(command) == false) {
		fprintf(stderr, "%s: Too many arguments\n", cmd);
		exit(EXIT_FAILURE);
	}

	for(; optind < argc; optind++) {
		if(add_argument(argv[optind]) == false) {
			fprintf(stderr, "%s: Too many arguments\n", cmd);
			exit(EXIT_FAILURE);
		}
	}

	if(hostname[0] == '\0') hostname = NULL;

	server(hostname, port, foreground);

	return EXIT_SUCCESS;
}
