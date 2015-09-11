api-gateway-zmq-adaptor
=======================

A ZMQ adaptor for the API-Gateway to facilitate a high performant async messaging queue.
It runs along the `api-gateway` process on each node where the gateway is installed.


### Status
This module is under development and it's considered production ready, being used without any problems with the API Gateway.

### How it works
This adapter establishes a communication channel with the API Gateway.
The API Gateway sends messages to this adaptor via `inproc://`, the ZMQ adaptor making them available for consumption further through the `XPUB` socket.

```
 --------------------------------------------------------------------
|                            API-GATEWAY BOX                         |
|                                                                    |
|                           +---------------+                        |
|                           |  API Gateway  |                        |
|                           |---------------|                        |
|                           |     PUB       |                        |
|                           +---------------+                        |
|                              (connect)                             |
|                                  |                                 |
|                                  v                                 |
|                                (bind)                              |
|   +------------------------------------------------------------+   |
|   |                            XSUB                            |   |
|   |                                                            |   |
|   |        ( default: ipc:///tmp/nginx_queue_listen )          |   |
|   |------------------------------------------------------------|   |
|   |                         ZMQ ADAPTOR                        |   |
|   |------------------------------------------------------------|   |
|   |                            XPUB                            |   |
|   |                                                            |   |
|   |                 ( default: tcp://0.0.0.0:6001 )            |   |
|   +------------------------------------------------------------+   |
|                               (bind)                               |
|                                 |                                  |
 --------------------------------------------------------------------
                                  ^
                                  |
                                  |
                                  |
                     -----------------------------
                     ^                           ^
                     |                           |
                 (connect)                   (connect)
             +--------------------+   +--------------------+
             |        SUB         |   |        SUB         |
             |--------------------|   |--------------------|
             |  Gateway Tracking  |   |  Gateway Tracking  |
             |     Service        |   |     Service        |
             +--------------------+   +--------------------+
```

For better performance, the adapter can bind to a separate NIC for the external communication;
this allows the Gateway to use all the ports for the regular API traffic, making the message queue as less intrusive as possible.

#### Capturing and sending usage data
The ZMQ Adapter gets usage data from the API Gateway by opening a listening socket via IPC.
The API Gateway publishes the messages to that address ( i.e. `ipc:///tmp/nginx_queue_listen` ).

The ZMQ Adapter makes the messages available on a specific port, binding it to all IPs `0.0.0.0` by default, on the API Gateway Node.

There are 2 flags that control where to listen and where to publish messages as the following example shows:

```
api-gateway-zmq-adaptor -p tcp://0.0.0.0:6001 -b ipc:///tmp/nginx_queue_listen
```

* `-p` flag defines the publishing address; consumers connect to this address in order to receive messages from the API Gateway
* `-b` flag defines the internal address the adapter binds to in order to listen for messages sent by the Gateway

### Debugging
Start the adapter with the `-d` flag to see all the messages published by the API Gateway and flowing through the adapter.

### Usages
* Performant logging mechanism
* Report usage and tracking
 
## Developer guide

To build the adaptor use:

```
make install
```

To run the unit tests you can execute:
```
make test
```
Unit tests require the [check](http://check.sourceforge.net/doc/check_html/index.html#Top) library.

For another quick test you can also run the adaptor with the `-t` flag using `^C` to stop it:

```
$ /usr/local/api-gateway-zmq-adaptor -t
ZeroMQ version 4.1.0 (czmq 2.2.0)
RUNNING IN TEST MODE & DEBUG MODE for XPUB -> XSUB

Starting SUB->PUSH Proxy [tcp://0.0.0.0:5000] -> [ipc:///tmp/nginx_queue_push]
Starting Debug receiver thread [ipc:///tmp/nginx_queue_push] ...

Starting XPUB->XSUB Proxy [ipc:///tmp/nginx_queue_listen] -> [tcp://0.0.0.0:6001]
Starting Test publisher thread [ipc:///tmp/nginx_queue_listen] ...
 ... sending:PUB-I-39438
Starting Debug subscriber thread [tcp://0.0.0.0:6001] ...
 ... sending:PUB-H-79844
> got: PUB-H-79844
 ... sending:PUB-J-19755
> got: PUB-J-19755
```
