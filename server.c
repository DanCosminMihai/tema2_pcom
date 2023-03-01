#include <arpa/inet.h>
#include <math.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "helpers.h"

void usage(char *file) {
  fprintf(stderr, "Usage: %s server_port\n", file);
  exit(0);
}

int main(int argc, char *argv[]) {
  setvbuf(stdout, NULL, _IONBF, BUFSIZ);

  int tcp, udp, newtcp, portno;
  char buffer[BUFLEN];
  struct sockaddr_in serv_addr, cli_addr;
  struct client *clients = malloc(1000 * sizeof(struct client));
  int n, i, ret, clients_num = 0;
  socklen_t clilen;

  fd_set read_fds;  // multimea de citire folosita in select()
  fd_set tmp_fds;   // multime folosita temporar
  int fdmax;        // valoare maxima fd din multimea read_fds

  if (argc < 2) {
    usage(argv[0]);
  }

  // se goleste multimea de descriptori de citire (read_fds) si multimea
  // temporara (tmp_fds)
  FD_ZERO(&read_fds);
  FD_ZERO(&tmp_fds);

  tcp = socket(AF_INET, SOCK_STREAM, 0);
  DIE(tcp < 0, "socket");

  portno = atoi(argv[1]);
  DIE(portno == 0, "atoi");

  memset((char *)&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(portno);
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  ret = bind(tcp, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
  DIE(ret < 0, "bind");

  // disable nagle
  int flag = 1;
  ret = setsockopt(tcp, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
  DIE(ret < 0, "nagle");

  ret = listen(tcp, MAX_CLIENTS);
  DIE(ret < 0, "listen");

  udp = socket(AF_INET, SOCK_DGRAM, 0);
  DIE(udp < 0, "udp socket");

  ret = bind(udp, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
  DIE(ret < 0, "bind");

  // se adauga noul file descriptor (socketul pe care se asculta conexiuni) in
  // multimea read_fds
  FD_SET(tcp, &read_fds);
  fdmax = tcp;

  FD_SET(udp, &read_fds);
  if (udp > tcp) fdmax = udp;

  FD_SET(STDIN_FILENO, &read_fds);
  while (1) {
    tmp_fds = read_fds;
    ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
    DIE(ret < 0, "select");

    for (i = 0; i <= fdmax; i++) {
      if (FD_ISSET(i, &tmp_fds)) {
        if (i == STDIN_FILENO) {
          memset(buffer, 0, sizeof(buffer));
          n = read(0, buffer, sizeof(buffer));
          DIE(n < 0, "read");

          if (strncmp(buffer, "exit", 4) == 0) {
            close(tcp);
            return 0;
          } else {
            for (int k = 0; k < clients_num; k++) {
              printf("id:%s subs:%d status:%d\n", clients[k].id,
                     clients[k].subscriptions_number,
                     clients[k].connection_status);
              for (int kk = 0; kk < clients[k].subscriptions_number; kk++)
                printf("-%s\n", clients[k].subscriptions[kk]);
            }
          }
        } else if (i == udp) {
          struct server_msg smsg;
          char buff[1551];
          memset(buff, 0, 1551);
          ret = recvfrom(udp, buff, sizeof(buff), 0,
                         (struct sockaddr *)&(smsg.addr), &clilen);
          DIE(ret < 0, "recvfrom");
          memcpy(smsg.topic, buff, 50);
          memcpy(&(smsg.data_type), buff + 50, 1);
          smsg.len = ret - 51;
          // copiaza doar cat trebuie
          // memcpy(smsg.payload, buff + 51, ret - 51);
          // trimite
          for (int j = 0; j < clients_num; j++)
            for (int k = 0; k < clients[j].subscriptions_number; k++)
              if (strcmp(clients[j].subscriptions[k], smsg.topic) == 0 &&
                  clients[j].connection_status == 1) {
                n = send(clients[j].sock, &smsg, sizeof(struct server_msg), 0);
                DIE(n < 0, "send");
                n = send(clients[j].sock, buff + 51, smsg.len, 0);
                DIE(n < 0, "send");
              }

        } else if (i == tcp) {
          // a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
          // pe care serverul o accepta
          clilen = sizeof(cli_addr);
          newtcp = accept(tcp, (struct sockaddr *)&cli_addr, &clilen);
          DIE(newtcp < 0, "accept");

          // se adauga noul socket intors de accept() la multimea descriptorilor
          // de citire
          FD_SET(newtcp, &read_fds);
          if (newtcp > fdmax) {
            fdmax = newtcp;
          }
          char client_id[11];
          memset(client_id, 0, 11);
          n = recv(newtcp, client_id, 10, 0);
          DIE(n < 0, "recv");

          int found_client = -1;
          for (int k = 0; k < clients_num && found_client == -1; k++)
            if (strcmp(clients[k].id, client_id) == 0) found_client = k;

          if (found_client == -1) {
            strcpy(clients[clients_num].id, client_id);
            clients[clients_num].connection_status = 1;
            clients[clients_num].subscriptions_number = 0;
            clients[clients_num].sock = newtcp;
            clients_num++;
            printf("New client %s connected from %s:%d\n", client_id,
                   inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
          } else {
            if (clients[found_client].connection_status == 1) {
              close(newtcp);
              FD_CLR(newtcp, &read_fds);
              printf("Client %s already connected.\n", client_id);
            } else {
              clients[found_client].connection_status = 1;
              printf("New client %s connected from %s:%d\n", client_id,
                     inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
            }
          }

        } else {
          // s-au primit date pe unul din socketii de client,
          // asa ca serverul trebuie sa le receptioneze
          char buff[sizeof(struct client_msg)];
          memset(buff, 0, sizeof(struct client_msg));
          struct client_msg msg;
          n = recv(i, &msg, sizeof(struct client_msg), 0);
          DIE(n < 0, "recv");

          int index = -1;
          for (int k = 0; k < clients_num && index == -1; k++)
            if (clients[k].sock == i) index = k;
          if (n == 0) {
            // conexiunea s-a inchis
            printf("Client %s disconnected.\n", clients[index].id);
            close(i);
            clients[index].connection_status = 0;

            // se scoate din multimea de citire socketul inchis
            FD_CLR(i, &read_fds);
          } else {
            if (n == sizeof(struct client_msg)) {
              if (msg.type == 0) {
                strcpy(clients[index]
                           .subscriptions[clients[index].subscriptions_number],
                       msg.topic);
                clients[index].subscriptions_number++;
              }
              if (msg.type == 1) {
                int j = 0;
                while (j < clients[index].subscriptions_number &&
                       strcmp(clients[index].subscriptions[j], msg.topic) != 0)
                  j++;
                if (j < clients[index].subscriptions_number) {
                  while (j < clients[index].subscriptions_number - 1) {
                    strcpy(clients[index].subscriptions[j],
                           clients[index].subscriptions[j + 1]);
                    j++;
                  }
                  clients[index].subscriptions_number--;
                }
              }
            }
          }
        }
      }
    }
  }
  // close all sockets
  for (int i = 0; i <= fdmax; i++)
    if (FD_ISSET(i, &read_fds)) close(i);

  close(tcp);
  close(udp);
  free(clients);

  return 0;
}
