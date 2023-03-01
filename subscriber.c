#include <arpa/inet.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "helpers.h"

void display_msg(struct server_msg smsg, char *buff) {
  printf("%s:%d - %s - ", inet_ntoa(smsg.addr.sin_addr),
         ntohs(smsg.addr.sin_port), smsg.topic);

  if (smsg.data_type == 0) {
    printf("INT - ");
    uint32_t nr;
    uint8_t sign;
    memcpy(&sign, buff, 1);
    memcpy(&nr, buff + 1, 4);
    nr = ntohl(nr);
    if (sign == 1) nr = -nr;
    printf("%d\n", nr);
  }
  if (smsg.data_type == 1) {
    printf("SHORT_REAL - ");
    uint16_t nr;
    memcpy(&nr, buff, 2);
    nr = ntohs(nr);
    double num = nr / (double)100;
    printf("%.2f\n", num);
  }
  if (smsg.data_type == 2) {
    printf("FLOAT - ");
    uint32_t nr;
    uint8_t p;
    uint8_t sign;
    memcpy(&sign, buff, 1);
    memcpy(&nr, buff + 1, 4);
    memcpy(&p, buff + 5, 1);
    nr = ntohl(nr);
    double num = nr / (double)pow(10, p);
    if (sign == 1) num = -num;
    printf("%.*f\n", p, num);
  }
  if (smsg.data_type == 3) {
    printf("STRING - ");
    char text[1501];
    memcpy(text, buff, smsg.len);
    text[smsg.len] = '\0';
    printf("%s\n", text);
  }
}

void usage(char *file) {
  fprintf(stderr, "Usage: %s client_id server_address server_port\n", file);
  exit(0);
}

int main(int argc, char *argv[]) {
  setvbuf(stdout, NULL, _IONBF, BUFSIZ);

  int tcp, n, ret;
  struct sockaddr_in serv_addr;
  char buffer[BUFLEN];

  if (argc < 4) {
    usage(argv[0]);
  }

  tcp = socket(AF_INET, SOCK_STREAM, 0);
  DIE(tcp < 0, "tcp socket");

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(atoi(argv[3]));
  ret = inet_aton(argv[2], &serv_addr.sin_addr);
  DIE(ret == 0, "inet_aton");

  // disable nagle
  int flag = 1;
  ret = setsockopt(tcp, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
  DIE(ret < 0, "nagle");

  ret = connect(tcp, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(ret < 0, "connect");

  int fdmax = tcp;

  // send client id
  char client_id[11];
  strcpy(client_id, argv[1]);
  n = send(tcp, client_id, strlen(client_id), 0);
  DIE(n < 0, "send");

  while (1) {
    fd_set read_set;
    FD_SET(STDIN_FILENO, &read_set);
    FD_SET(tcp, &read_set);
    int rc = select(fdmax + 1, &read_set, NULL, NULL, NULL);
    DIE(rc < 0, "select");

    if (FD_ISSET(STDIN_FILENO, &read_set)) {
      memset(buffer, 0, sizeof(buffer));
      n = read(0, buffer, sizeof(buffer));
      DIE(n < 0, "read");

      char cmd[12];
      struct client_msg msg;
      int sf;
      sscanf(buffer, "%s %s %d", cmd, msg.topic, &sf);
      msg.sf = sf;
      if (strcmp(cmd, "exit") == 0) {
        close(tcp);
        break;
      } else if (strcmp(cmd, "subscribe") == 0) {
        msg.type = 0;
        if (msg.topic != NULL && (msg.sf == 0 || msg.sf == 1)) {
          n = send(tcp, &msg, sizeof(struct client_msg), 0);
          DIE(n < 0, "send");
          printf("Subscribed to topic.\n");
        }
      } else if (strncmp(cmd, "unsubscribe", 11) == 0) {
        msg.type = 1;
        if (msg.topic != NULL) {
          n = send(tcp, &msg, sizeof(struct client_msg), 0);
          DIE(n < 0, "send");
          printf("Unsubscribed from topic.\n");
        }
      }

    } else {
      // Socket
      int rc;
      struct server_msg smsg;
      rc = recv(tcp, &smsg, sizeof(struct server_msg), 0);
      DIE(rc < 0, "recv");
      if (rc == 0) {
        // printf("Connection terminated.\n");
        break;
      }

      char buff[smsg.len];
      rc = recv(tcp, buff, smsg.len, 0);
      DIE(rc < 0, "recv");
      if (rc == smsg.len) display_msg(smsg, buff);
    }
  }

  close(tcp);

  return 0;
}
