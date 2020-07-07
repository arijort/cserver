/* * Design:
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
#include<string.h>
#include<stdbool.h>
#include<pthread.h>

#define default_host "localhost"
#define default_port "31337"
#define maxbuf 512 // use preprocessor constant so it can be used in the typedef

static const int MAXBUF = maxbuf; // max size of buffer to send
static const int MAXARGBUF = 32; // max size of parameters for host, port.
const static int default_threads = 10;
static int requests;

// create typedef encapsulating server host:port and message
typedef struct server_message {
  char message[maxbuf];
  struct addrinfo *address;
} server_message;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// declarations
void *do_client_thread(void *arg);
void do_incr_counts(int n);
server_message prepare_server_message(struct addrinfo *addy, char *req_buf);

void do_incr_counts(int n) {
  pthread_mutex_lock(&mutex);
  requests += n;
  pthread_mutex_unlock(&mutex);
}

void *do_client_thread(void *arg) {
  char req_buf[1024]; // request message going to server 
  char resp_buf[1024]; // response message recevied from server
  struct addrinfo *addy;
  int client_fd;
  addy = ((server_message *)arg)->address;

  client_fd = socket(addy->ai_family, addy->ai_socktype, addy->ai_protocol);

  connect(client_fd, (struct sockaddr*)addy->ai_addr, addy->ai_addrlen);

  // send request to server
  sprintf(req_buf, "%s %ld\n", ((server_message *)arg)->message, pthread_self());
  if ( send(client_fd, req_buf, strlen(req_buf), 0) < 0 ) {
    perror("send failed");
  } 

  if ( recv(client_fd, resp_buf, 1024, 0) < 0 ) {
    perror("could not receive response");
  }
  
  close(client_fd);
  do_incr_counts(1);
  pthread_exit(NULL);
}

int main(int argc, char **argv) {
  int num_threads = default_threads;
  char req_buf[MAXBUF]; // request message going to server
  char host[MAXARGBUF], port[MAXARGBUF];
  struct addrinfo *addy;
  server_message msg_ctx; // create context for message to server including server address and the message.
  if ( argc == 5 ) {
    strcpy(host, argv[1]);
    strcpy(port, argv[2]);
    num_threads = atoi(argv[3]);
    strcpy(req_buf, argv[4]);
  }
  else {
    strcpy(host, default_host);
    strcpy(port, default_port);
    strcpy(req_buf, "arijort:omet dadage do");
  }
  printf("connecting to host:port %s:%s with %d threads\n", host, port, num_threads);
  pthread_t threads[num_threads];

  // setup server address
  // printf("setting up server address\n");
  if ( getaddrinfo(host, port, NULL, &addy) != 0 ) {
    perror("could not get address info\n");
    exit(1);
  }

  msg_ctx = prepare_server_message(addy, req_buf);

  for ( int i = 0; i < num_threads; i++ ) {
    //if (pthread_create(&threads[i], NULL, do_client_thread, addy) != 0) {
    if (pthread_create(&threads[i], NULL, do_client_thread, &msg_ctx) != 0) {
      perror("create thread failed");
    }
  }

  for ( int i = 0; i < num_threads; i++ ) {
    pthread_join(threads[i], NULL);
  }
  freeaddrinfo(addy);
  printf("handled %d requests\n", requests);
  return 0;
}

/*
 * prepare_server_message: create the structure to encapsulate the server info and message to send.
 * Returns the struct containing the server host and port data and the message to send.
 */
server_message prepare_server_message(struct addrinfo *addy, char *req_buf) {
  server_message sm;
  strcpy(sm.message, req_buf);
  sm.address = addy;
  return sm;
}
