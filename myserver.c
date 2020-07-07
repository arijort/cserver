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
#include<sys/socket.h>
#include<sys/types.h>
#include<unistd.h> /* fork() dependency */
#include<sys/wait.h>
#include<netdb.h> /* getaddrinfo() dependency */
#include<fcntl.h> /* open() dependency */
#include<string.h> /* strlen() dependency */
#include<time.h>
#include<stdbool.h>
#include <sys/mman.h>

#define usage "myserver <server> <port>"
#define default_host "localhost"
#define default_port "31337"

typedef enum protocol_result {exception = -2, authorization = -1, success = 0} protocol_result;

static const int MAXARGBUF = 32; // max size of parameters for host, port.
static const int MAXBUF = 1024; // max size of message received from client
static const int BACKLOG = 100; // backlog on listen()
static int count = 0;
static FILE *fp;
static const char delim = ':';
static const char *users[] = { "arijort", "foobar" }; // externalize to config file if possible.
static const int num_users = sizeof(users) / sizeof(users[0]);

// declarations
int get_socket_fd(char *host, char *port);
int readline(int fd, char *client_buf);
protocol_result do_auth_read(const void *msg, void *username, void *buf);
void log_write(char *msg);
int *create_shared_mem();
void do_child_work(int sockfd, int *p_mmap);

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

  // 1 setup address info
  if ( getaddrinfo(host, port, NULL, &addy) != 0 ) {
    perror("could not get address info\n");
    exit(1);
  }

  // 2 create socket
  sockfd = socket(addy->ai_family, addy->ai_socktype, addy->ai_protocol);
  if (sockfd == -1 ) {
    perror("could not create a socket\n");
    exit(1);
  }
  // 3 set reuse option
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&sockopt, sizeof(int)) == -1) {
    perror("could not create a socket\n");
    exit(1);
  }

  // 4 bind to the socket
  if ( bind(sockfd, addy->ai_addr, addy->ai_addrlen) != 0 ) {
    perror("could not bind");
    exit(1);
  }
  freeaddrinfo(addy);

  // 5 set listener
  if (listen(sockfd, BACKLOG) == -1) {
    perror("could not listen");
  }

  return sockfd;
}

/*
 * do_child_work: function performed in worker processes
 */
void do_child_work(int sockfd, int *p_mmap) {
  int client_fd;
  struct sockaddr_in client_addr;
  char prompt[MAXARGBUF]; // buffer for sending prompt to client on connect
  char request_line[MAXBUF + 32];
  char end_line[MAXBUF + 32];
  size_t buflen;
  pid_t childpid = getpid();

  // Handling data from the client
  char client_buf[MAXBUF];
  char client_msg[MAXBUF];
  char username[MAXARGBUF];
  int client_msg_size;
  protocol_result pr;
  socklen_t client_addr_len = sizeof(client_addr);

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

    // get data from client
    client_msg_size = readline(client_fd, client_buf);
    if ( client_msg_size == -1 ) {
      log_write("protocol error from client");
      close(client_fd);
      break;
    }
    pr = do_auth_read(client_buf, username, client_msg);
    if ( pr == authorization ) {
      log_write("authorization error");
      close(client_fd);
      break;
    }
    if ( pr == exception ) {
      log_write("protocol error");
      close(client_fd);
      break;
    }
    sprintf(request_line, "server %d recvd message \"%s\" from user %s", childpid, client_msg, username);
    log_write(request_line);
    sleep(3); // do computationally intensive work which adds latency
    send(client_fd, request_line, strlen(request_line), 0);
    (*p_mmap)++;
    sprintf(end_line, "completed req \"%s\" on %d total reqs %d", client_msg, childpid, *p_mmap );
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
protocol_result do_auth_read(const void *msg, void *username, void *buf) {
  int rc = 0;
  char *delim_point;
  delim_point = strchr(msg, delim);
  if ( delim_point == NULL )
    return exception; // protocol error if no colon
  rc = delim_point - (char *)msg +1;
  strncpy(username, msg, rc - 1);
  if ( ! do_auth_user(username) )
    return authorization;
  // Copy message, IOW string to the right of the colon, into buffer for caller
  strcpy(buf, msg + rc);
  return success;
  
}
/*
 * readline inspired by Stevens from https://www.informit.com/articles/article.aspx?p=169505&seqNum=9
 * Correct this would not be thread safe but we are forking processes here.
 */
int readline(int fd, char *client_buf) {
  size_t client_read;
  char buffer[MAXBUF];
  int msglen = 0;
  while ( (client_read = read(fd, buffer+msglen, MAXBUF)) > 0 ) {
    msglen += client_read;
    if ( buffer[msglen-1] == '\n' ) break;
  }
  if ( client_read < 0 ) {
    perror("error on receiving data from client");
  }
  buffer[msglen-1] = 0;
  strcpy(client_buf, buffer);
  return msglen;
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
  fprintf(fp, "%s.%09ld -- %s\n", time_str, ns, msg);
  fflush(fp);
}

int main(int argc, char** argv) {
  int sockfd;
  int numchild = 1000;
  char host[MAXARGBUF], port[MAXARGBUF];
  int pids[numchild]; // track pids of child procs
  int *statuses[numchild]; // array of shared memory pointers for children
  char cyclebuf[MAXARGBUF]; // buffer for logging thread status
  pid_t childpid;
  if ( argc != 3 ) {
    strcpy(host, default_host);
    strcpy(port, default_port);
  }
  else {
    strcpy(host, argv[1]);
    strcpy(port, argv[2]);
  }
  printf("Running with %d children on host:port %s:%s\n", numchild, host, port );

  char logfile[] = "/tmp/myserver.log";
  fp = fopen(logfile, "a");
  if ( fp == NULL ) {
    perror("could not open log file");
    exit(1);
  }

  char buf[] = "Server init";
  log_write(buf);

  sockfd = get_socket_fd(host, port);

  for ( int i = 0 ; i < numchild; i++) {
    statuses[i] = create_shared_mem();
  }
  for ( int i = 0 ; i < numchild; i++) {
    childpid = fork();
    if ( childpid == -1 ) {
      perror("could not fork");
    }
    else if ( childpid == 0 ) {
      do_child_work(sockfd, statuses[i]);
    }
    else { // this is parent code
      pids[i] = childpid;
    }
  }

  for ( int i = 0 ; i< numchild; i++ ) {
    sprintf(cyclebuf, "have child pid %d", pids[i]);
    log_write(cyclebuf);
  }
  while (true) {
    int total_reqs = 0;
    sleep(2);
    for ( int i = 0 ; i < numchild; i++ ) {
      total_reqs += *statuses[i];
    }
    sprintf(cyclebuf, "total requests %d", total_reqs);
    log_write(cyclebuf);
  }
  while (waitpid(-1, NULL, 0) > 0);
  close(sockfd);
  printf("finished all work counted %d\n", count);
  return 0;
}

int *create_shared_mem() {
  int *result =  mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  return result;
}
