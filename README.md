# cserver
work writing a basic tcp server in c

This project implements a simple client and server written in C to demonstrate different techniques for multiprocessing while maintaining global state.

## Client implementation with pthreads and mutex
The client is implemented using a multi-threaded model using pthreads and a simple static global variable which is made thread-safe with a simple mutex. Threads count the number of reqeusts they have handled to enable the main thread to report a total count.

## Server implementation using fork and shared memory
The server is implemented using forked child processes which communicate with the parent process using shared memory. Child processes write the number of requests they have handled into shared memory segments enabling the parent process to get a total count of requests handled. Both child processes and the parent write into an append-only log file.



## Results


1000 requests in 171 milliseconds

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


```
arijort@mowawa:~/shadows/cserver$ egrep 'recvd.*lalala' /tmp/myserver.log  | head -n 2  ; egrep 'recvd.*lalala' /tmp/myserver.log  | tail -n 2
2020.07.06 16:44:57.270823600 -- server 29756 recvd message "lalala 140263099414272" from user arijort
2020.07.06 16:44:57.270972039 -- server 29761 recvd message "lalala 140263124592384" from user arijort
2020.07.06 16:44:57.439988962 -- server 29714 recvd message "lalala 140254812686080" from user arijort
2020.07.06 16:44:57.441281868 -- server 29740 recvd message "lalala 140254837864192" from user arijort
arijort@mowawa:~/shadows/cserver$ egrep 'comple.*lalala' /tmp/myserver.log  | head -n 1  ; egrep 'comple.*lalala' /tmp/myserver.log  | tail -n 1
2020.07.06 16:45:00.271549006 -- completed req "lalala 140263099414272" on 29756 total reqs 2
2020.07.06 16:45:00.441710420 -- completed req "lalala 140254837864192" on 29740 total reqs 3
arijort@mowawa:~/shadows/cserver$
```

