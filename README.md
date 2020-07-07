# Server and Client implementations in C

This project implements a simple client and server written in C to demonstrate different techniques for multiprocessing while maintaining global state.

## Client implementation with pthreads and mutex
The client is implemented using a multi-threaded model using pthreads and a simple static global variable which is made thread-safe with a simple mutex. Threads count the number of reqeusts they have created to enable the main thread to report a total count.

Clients create the protocol string and send it to the server over connected socket.

## Server implementation using fork and shared memory
The server is implemented using forked child processes which communicate with the parent process using shared memory. Child processes write the number of requests they have handled into shared memory segments enabling the parent process to get a total count of requests handled. Both child processes and the parent write into an append-only log file.

The parent process creates an array of shared memory pointers, one for each child. The shared memory pointer is passed to the child function `do_child_work()`.  

Server child processes use a naive sleep to simulate some computation which would introduce latency into the response to the client.

In the basic configuration, the server creates 1000 child processes to demonstrate the viability of sustaining 1000 simultaneous connections on one machine.

A very basic measurement of server memory usage indicates 350MB for 1000 threads.  Each instance consumes 144KB of resident memory
Before:
```
arijort@mowawa:~/shadows/cserver$ free
              total        used        free      shared  buff/cache   available
Mem:        2035480      449528      814580        4156      771372     1380372
```

During server runtime.
```
arijort@mowawa:~/shadows/cserver$ free
              total        used        free      shared  buff/cache   available
Mem:        2035480      800580      462876        4156      772024     1029024
```

The server logs to the file `/tmp/myserver.log`.


## Protocol

The client sends a message in this format:

`<username>:<message>`

The `username` is used for trivial authorization. The authorized users are hard-coded for now. They are "arijort" and "foobar".

The `message` can be any string terminated by a newline.

The server responds with this and the identifying pid of the child process which handled the request.


## Invocation

The server component can be run from a shell. By default it listens on localhost port 31337. It will remain in the foreground. 

```
arijort@mowawa:~/shadows/cserver$ ./myserver 
Running with 1000 children on host:port localhost:31337
```

The client takes parameters as follows:

cclient <host> <port> <number of child threads> <message> 

The message must conform to the protocol ( username:message ) or will be rejected.

```
arijort@mowawa:~/shadows/cserver$ ./cclient localhost 31337 1000 "arijort:message for you"
connecting to host:port localhost:31337 with 1000 threads
handled 1000 requests
```

## Performance Results

The client is able to create 1000 threads sending 1 message each to the server in 170 milliseconds.

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


This snippet from the server log file indicates 1000 client requests arrived within 170 milliseconds. After the artificial 3 second latency, they were handled in the same amount of time.

Note: each server handler process logs the "recvd message" immediately after it authorizes the client request. It then logs the "completed" line after the artiticial latency. This analysis shows the server is able to sustain 1000 simultaneous connections.

```
arijort@mowawa:~/shadows/cserver$ egrep 'recvd.*lalala' /tmp/myserver.log | wc -l
1000
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

## Possibilities

The server child processes could be managed with a dynamic pool of processes in the manner of the pre-forking server model used by apache.

Future work could identify a clear breaking point for this server model but it lies beyond 1000 simultaneous connections.

