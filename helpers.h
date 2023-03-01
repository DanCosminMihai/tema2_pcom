#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define BUFLEN		256	// dimensiunea maxima a calupului de date
#define MAX_CLIENTS	5	// numarul maxim de clienti in asteptare

struct client_msg{
	uint8_t type; //0-subscribe, 1-unsubscribe
	char topic[51];
	uint8_t sf;
	
};

struct server_msg{
	int len;
	struct sockaddr_in addr;
	char topic[51];
	uint8_t data_type;
};

struct client{
	char id[11];
	char subscriptions[100][51];
	int subscriptions_number;
	int sock;
	uint8_t connection_status;
};

#endif
