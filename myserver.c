/*
 * Design:
 *
 * 1) Server will bind to a TCP or UDP port and listen for client communications
 * 2) Client performs some kind of auth transaction with the server, such as providing a valid username
 * 3) Client may send multiple messages or transactions to the server.  The purpose of this exchange is unspecified, except that these transactions should mutate some global state such as an append-only log.
 * 4) Server may accept multiple client connections.  Whether those connections are serviced in serial or parallel is unspecified.
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
#include<stdbool.h>

#define usage "myserver <server> <port>"
#define default_host "localhost"
#define default_port "31337"

static const int MAXARGBUF = 32; // max size of parameters for host, port.
static const int MAXBUF = 256; // max size of message received from client
static int count = 0;
static FILE *fp;
static const char delim = ':';
static const char *users[] = { "arijort", "foobar" }; // externalize to config file if possible.
static const int num_users = sizeof(users) / sizeof(users[0]);

// declarations
int readline(int fd, void *buf, size_t maxlen);
int do_auth_read(const void *msg, void *username, void *buf, size_t maxlen);
void log_write(char *msg);

/*
 * get_socket_fd: function for setting up the server socket.
 * This function is given the host and port the server should bind to.
 * Error out if any of getaddrinfo(), socket() setsockopt() or bind() fail.
 *
 * Returns the file descriptor for this server process after it's bound to the given host:port
 */
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

/*
 * do_thread_work: function performed in worker processes
 */
void do_thread_work(int sockfd) {
  int client_fd;
  struct sockaddr_in client_addr;
  char prompt[MAXARGBUF];
  char request_line[MAXBUF + 32];
  char end_line[MAXBUF + 32];
  size_t buflen;
  pid_t childpid = getpid();

  // Handling data from the client
  char client_buf[MAXBUF];
  char client_msg[MAXBUF];
  char username[MAXARGBUF];
  int client_msg_size;
  socklen_t client_addr_len = sizeof(client_addr);
  printf("in work thread\n");

  sprintf(prompt, "this is server thread %d\n", childpid);
  // int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
  for(;;) {
    client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
    if ( client_fd == -1 ) {
      perror("could not accept a connection");
    }
    buflen = send(client_fd, prompt, strlen(prompt), 0);
    if ( buflen == -1 ) {
      perror("could not send message to client");
      break;
    }
    //printf("  sent %d bytes to child\n", (int)buflen);

    // get data from client
    client_msg_size = readline(client_fd, client_buf, MAXBUF);
    if ( client_msg_size == -1 ) {
      log_write("protocol error from client");
      close(client_fd);
      break;
    }
    client_msg_size = do_auth_read(client_buf, username, client_msg, MAXBUF);
    if ( client_msg_size == -1 ) {
      log_write("authorization error");
      close(client_fd);
      break;
    }
    if ( client_msg_size == -2 ) {
      log_write("protocol error");
      close(client_fd);
      break;
    }
    sprintf(request_line, "server %d recvd message \"%s\" from user %s", childpid, client_msg, username);
    log_write(request_line);
    //sleep(3); // do computationally intensive work which adds latency
    send(client_fd, request_line, strlen(request_line), 0);
    sprintf(end_line, "completed request on %d", childpid );
    log_write(end_line);
    close(client_fd);
  }
}

/*
 * do_auth_user: Implements a very basic check to authorize a user. We are only using a static list as an auth list in this case.
 * Normally this function would be performed by a lookup into ldap or a passwd map.
 *
 * Takes a char pointer to a string conttaining the client asserted username.
 * Returns a boolean whether this user is authorized.
 */
bool do_auth_user(const char *username) {
  for ( int n = 0; n < num_users ; n++ ) {
    if ( strcmp(username, users[n]) == 0 )
      return true;
  }
  return false;
}
/*
 * do_auth_read: Read from a string passed from a client and authorize the sender based on the string.
 *
 * Protocol assumption: message from the client is composed of a string with a single colon ":" delimiter. For example: "username:this is the msg".
 * The username to the left of the colon is used for authorization logic. There is no attempt at authentication. Identity is simply asserted by the client.
 * Return the number of bytes read in the message. 
 * Return -1 for an authorization error.
 * Return -2 for a protocol error e.g. missing colon.
 * Put the message without auth info into *buf.
 */
int do_auth_read(const void *msg, void *username, void *buf, size_t maxlen) {
  int rc = 0;
  char *delim_point;
  size_t msg_len = strlen(msg);
  delim_point = strchr(msg, delim);
  if ( delim_point == NULL )
    return -2; // protocol error if no colon
  rc = delim_point - (char *)msg +1;
  strncpy(username, msg, rc - 1);
  if ( ! do_auth_user(username) )
    return -1;
  // Copy message, IOW string to the right of the colon, into buffer for caller
  strncpy(buf, msg + rc, msg_len - rc);
  return strlen(buf);
}
/*
 * readline inspired by Stevens from https://www.informit.com/articles/article.aspx?p=169505&seqNum=9
 * Correct this would not be thread safe but we are forking processes here.
 */
int readline(int fd, void *buf, size_t maxlen) {
  size_t rc;
  char c[MAXBUF];
  char *newline;
  rc = read(fd, buf, maxlen);
  newline = strchr(buf, '\n');
  if ( newline == NULL )
    return -1; // protocol error if no newline
  *newline = '\0'; // null-terminate string at first newline
  //printf("new buf -%s-\n", (char *)buf); 
  return rc;
}

int readline_slow(int fd, void *buf, size_t maxlen) {
  size_t n, rc;
  char c, *ptr;
  ptr = buf;
  for (n=1 ; n < maxlen; n++) {
    if ( (rc = read(fd, &c, 1)) == 1 ) {
      if ( c == '\n' )
        break;
      *ptr++ = c;
    }
    else if ( rc == 0 ) {
      *ptr = 0;
      return (n - 1);
    }
    else
      return -1;
  }
  *ptr = 0;
  return (n);
}

void log_write(char *msg) {
  struct tm *now;
  struct timespec spec;
  time_t seconds;
  long ns;
  clock_gettime(CLOCK_REALTIME, &spec);
  seconds = spec.tv_sec;
  ns = spec.tv_nsec;
  now = localtime(&seconds);
  char time_str[MAXARGBUF];
  strftime(time_str, sizeof(time_str), "%Y.%m.%d %H:%M:%S", now); // use strftime so I can customize timetamp string
  fprintf(fp, "%s.%ld -- %s\n", time_str, ns, msg);
  fflush(fp);
}

int main(int argc, char** argv) {
  int sockfd;
  int numchild = 3;
  char host[MAXARGBUF], port[MAXARGBUF];
  pid_t childpid;
  if ( argc != 3 ) {
    printf("Using default host:port %s:%s\n", default_host, default_port);
    strncpy(host, default_host, MAXARGBUF);
    strncpy(port, default_port, MAXARGBUF);
  }
  else {
    strncpy(host, argv[1], MAXARGBUF);
    strncpy(port, argv[2], MAXARGBUF);
  }

  char logfile[] = "/tmp/myserver.log";
  fp = fopen(logfile, "a");
  if ( fp == NULL ) {
    perror("could not open log file");
    exit(1);
  }

  char buf[] = "Server init";
  log_write(buf);

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
