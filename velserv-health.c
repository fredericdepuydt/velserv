#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_DEVICE "/dev/ttyACM0"
#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT "3788"
#define DEFAULT_TIMEOUT 5
#define MAX_FRAME_SIZE 100

static const char *env_or_default(const char *name, const char *fallback)
{
	const char *value = getenv(name);
	return (value && value[0] != '\0') ? value : fallback;
}

static int parse_positive_int(const char *value, int *result)
{
	char *endptr = NULL;
	long parsed;

	if (!value || value[0] == '\0')
	{
		return -1;
	}

	errno = 0;
	parsed = strtol(value, &endptr, 10);
	if (errno != 0 || *endptr != '\0' || parsed < 1 || parsed > 86400)
	{
		return -1;
	}

	*result = (int)parsed;
	return 0;
}

static int check_device(const char *device)
{
	struct stat st;

	if (!device || device[0] == '\0')
	{
		fprintf(stderr, "velserv-health: empty serial device path\n");
		return -1;
	}

	if (stat(device, &st) != 0)
	{
		fprintf(stderr, "velserv-health: cannot stat %s: %s\n", device, strerror(errno));
		return -1;
	}

	if (!S_ISCHR(st.st_mode))
	{
		fprintf(stderr, "velserv-health: %s is not a character device\n", device);
		return -1;
	}

	if (access(device, R_OK | W_OK) != 0)
	{
		fprintf(stderr, "velserv-health: cannot access %s for read/write: %s\n", device, strerror(errno));
		return -1;
	}

	return 0;
}

static int connect_to_velserv(const char *host, const char *port)
{
	struct addrinfo hints;
	struct addrinfo *addresses = NULL;
	struct addrinfo *address;
	int gai_rc;
	int fd = -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	gai_rc = getaddrinfo(host, port, &hints, &addresses);
	if (gai_rc != 0)
	{
		fprintf(stderr, "velserv-health: cannot resolve %s:%s: %s\n", host, port, gai_strerror(gai_rc));
		return -1;
	}

	for (address = addresses; address != NULL; address = address->ai_next)
	{
		fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
		if (fd < 0)
		{
			continue;
		}

		if (connect(fd, address->ai_addr, address->ai_addrlen) == 0)
		{
			break;
		}

		close(fd);
		fd = -1;
	}

	freeaddrinfo(addresses);

	if (fd < 0)
	{
		fprintf(stderr, "velserv-health: cannot connect to velserv at %s:%s\n", host, port);
	}

	return fd;
}

static int wait_for_velbus_frame(int fd, int timeout_seconds)
{
	unsigned char frame[MAX_FRAME_SIZE];
	int started = 0;
	int frame_pos = 0;
	int expected_len = 0;
	time_t deadline = time(NULL) + timeout_seconds;

	while (time(NULL) <= deadline)
	{
		unsigned char byte;
		fd_set read_fds;
		struct timeval timeout;
		time_t now = time(NULL);
		int remaining = (int)(deadline - now);
		int ready;
		ssize_t received;

		if (remaining < 0)
		{
			break;
		}

		FD_ZERO(&read_fds);
		FD_SET(fd, &read_fds);
		timeout.tv_sec = remaining;
		timeout.tv_usec = 0;

		ready = select(fd + 1, &read_fds, NULL, NULL, &timeout);
		if (ready < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			fprintf(stderr, "velserv-health: select failed: %s\n", strerror(errno));
			return -1;
		}
		if (ready == 0)
		{
			break;
		}

		received = recv(fd, &byte, 1, 0);
		if (received <= 0)
		{
			fprintf(stderr, "velserv-health: velserv closed the healthcheck connection\n");
			return -1;
		}

		if (!started)
		{
			if (byte != 0x0F)
			{
				continue;
			}

			started = 1;
			frame_pos = 0;
			expected_len = 0;
		}

		if (frame_pos >= MAX_FRAME_SIZE)
		{
			started = 0;
			frame_pos = 0;
			expected_len = 0;
			continue;
		}

		frame[frame_pos++] = byte;

		if (frame_pos == 4)
		{
			expected_len = (frame[3] & 0x0F) + 6;
			if (expected_len <= 0 || expected_len > MAX_FRAME_SIZE)
			{
				started = 0;
				frame_pos = 0;
				expected_len = 0;
			}
		}

		if (expected_len > 0 && frame_pos == expected_len)
		{
			if (frame[frame_pos - 1] == 0x04)
			{
				return 0;
			}

			started = 0;
			frame_pos = 0;
			expected_len = 0;
		}
	}

	fprintf(stderr, "velserv-health: no valid Velbus frame received within %d seconds\n", timeout_seconds);
	return -1;
}

static void usage(FILE *stream, const char *program)
{
	fprintf(stream,
		"Usage: %s [--device PATH] [--host HOST] [--port PORT] [--timeout SECONDS] [--require-frame]\n",
		program);
}

int main(int argc, char **argv)
{
	const char *device = env_or_default("VELSERV_DEVICE", DEFAULT_DEVICE);
	const char *host = env_or_default("VELSERV_HEALTH_HOST", DEFAULT_HOST);
	const char *port = env_or_default("VELSERV_PORT", DEFAULT_PORT);
	const char *timeout_env = env_or_default("VELSERV_HEALTH_TIMEOUT", "5");
	int timeout_seconds = DEFAULT_TIMEOUT;
	int require_frame = 0;
	int sock;
	int i;

	if (parse_positive_int(timeout_env, &timeout_seconds) != 0)
	{
		fprintf(stderr, "velserv-health: invalid VELSERV_HEALTH_TIMEOUT '%s'\n", timeout_env);
		return EXIT_FAILURE;
	}

	for (i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--device") == 0 && i + 1 < argc)
		{
			device = argv[++i];
		}
		else if (strcmp(argv[i], "--host") == 0 && i + 1 < argc)
		{
			host = argv[++i];
		}
		else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
		{
			port = argv[++i];
		}
		else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc)
		{
			if (parse_positive_int(argv[++i], &timeout_seconds) != 0)
			{
				fprintf(stderr, "velserv-health: invalid timeout\n");
				return EXIT_FAILURE;
			}
		}
		else if (strcmp(argv[i], "--require-frame") == 0)
		{
			require_frame = 1;
		}
		else if (strcmp(argv[i], "--help") == 0)
		{
			usage(stdout, argv[0]);
			return EXIT_SUCCESS;
		}
		else
		{
			usage(stderr, argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (check_device(device) != 0)
	{
		return EXIT_FAILURE;
	}

	sock = connect_to_velserv(host, port);
	if (sock < 0)
	{
		return EXIT_FAILURE;
	}

	if (require_frame && wait_for_velbus_frame(sock, timeout_seconds) != 0)
	{
		close(sock);
		return EXIT_FAILURE;
	}

	close(sock);
	return EXIT_SUCCESS;
}
