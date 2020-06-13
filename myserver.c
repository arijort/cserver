/*
 * Design:
 *
 * 1) Server will bind to a TCP or UDP port and listen for client communications
 * 2) Client performs some kind of auth transaction with the server, such as providing a valid username
 * 3) Client may send multiple messages or transactions to the server.  The purpose of this exchange is unspecified, except that these transactions should mutate some global state such as an append-only log.
 * 4) Server may accept multiple client connections.  Whether those connections are serviced in serial or parallel is unspecified.
 *
 *
 *  Analysis:
 *
 *  1) Please provide a test that demonstrates your server's ability to handle 1000 concurrent clients.  The definition of "concurrent" is something we ask you to specify and justify.  Any analysis on transaction throughput is appreciated.
 *
 */

#include<stdlib.h>
#include<stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h> /* fork() dependency */
#include <sys/wait.h>
#include <netdb.h> /* getaddrinfo() dependency */
#include <fcntl.h> /* open() dependency */
#include <string.h> /* strlen() dependency */
#include<time.h>

#define usage "myserver <server> <port>"
#define default_host "localhost"
#define default_port "31337"

int count = 0;

int get_socket_fd(char *host, char *port) {
  const int sockopt = 1;
  struct addrinfo *addy;
  int sockfd;

  if ( getaddrinfo(host, port, NULL, &addy) != 0 ) {
    perror("could not get address info\n");
    exit(1);
  }

  sockfd = socket(addy->ai_family, addy->ai_socktype, addy->ai_protocol);
  if (sockfd == -1 ) {
    perror("could not create a socket\n");
    exit(1);
  }
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&sockopt, sizeof(int)) == -1) {
    perror("could not create a socket\n");
    exit(1);
  }

  if ( bind(sockfd, addy->ai_addr, addy->ai_addrlen) != 0 ) {
    perror("could not bind");
    exit(1);
  }
  freeaddrinfo(addy);
  return sockfd;
}

void do_thread_work(int sockfd) {
  int client_fd;
  struct sockaddr_in client_addr;
  char prompt[32];
  //char *msgbuf = malloc(64 * sizeof(char) );
  size_t buflen;
  socklen_t client_addr_len = sizeof(client_addr);
  printf("in work thread\n");

  // int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
  for(;;) {
    client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
    if ( client_fd == -1 ) {
      perror("could not accept a connection");
    }
    sprintf(prompt, "this is child thread %d", getpid());
    count++;
    // ssize_t send(int sockfd, const void *buf, size_t len, int flags);
    buflen = send(client_fd, prompt, strlen(prompt), 0);
    if ( buflen == -1 ) {
      perror("could not send message to client");
      break;
    }
    printf("  read %d bytes from child\n", (int)buflen);
    close(client_fd);
  }
}

void log_write(FILE *fp, char *msg) {
  // time invocation thanks to https://en.cppreference.com/w/c/chrono/asctime
  struct tm now = *localtime(&(time_t){time(NULL)});
  char time_str[32];
  strftime(time_str, sizeof(time_str), "%c", &now); // use strftime so I can customize timetamp string
  fprintf(fp, "%s %s", time_str, msg);
}

int main(int argc, char** argv) {
  int sockfd;
  int numchild = 3;
  size_t msglen;
  char host[32], port[32];
  pid_t childpid;
  if ( argc != 3 ) {
    printf("Using default host:port %s:%s\n", default_host, default_port);
    strcpy(host, default_host);
    strcpy(port, default_port);
  }
  else {
    strcpy(host, argv[1]);
    strcpy(port, argv[2]);
  }


  char logfile[] = "/tmp/myserver.log";
  FILE *logf;
  logf = fopen(logfile, "a");
  //logfd = open(logfile, O_WRONLY | O_APPEND | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
  if ( logf == NULL ) {
    perror("could not open log file");
    exit(1);
  }

  char buf[] = "Log file init\n";
  log_write(logf, buf);

  sockfd = get_socket_fd(host, port);
  if (listen(sockfd, 100) == -1) {
    perror("could not listen");
  }

  for ( int i = 0 ; i < numchild; i++) {
    childpid = fork();
    if ( childpid == -1 ) {
      perror("could not fork");
    }
    else if ( childpid == 0 ) {
      printf("this is the child thread with sockfd %d\n", sockfd);
      do_thread_work(sockfd);
    }
    else {
      printf("this is the parent thread at iteration %d with child pid %d\n", i, childpid);
    }
  }
  while (waitpid(-1, NULL, 0) > 0);
  close(sockfd);
  printf("finished all work counted %d\n", count);
  return 0;
}
