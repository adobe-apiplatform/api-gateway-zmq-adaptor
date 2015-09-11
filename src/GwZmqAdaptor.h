#ifndef GW_ADAPTOR_H
#define GW_ADAPTOR_H

// #define DEFAULT_XPUB "tcp://127.0.0.1:6001"
/**
* Default address where ZMQ publishes the tracking information
*/
#define DEFAULT_XPUB "tcp://0.0.0.0:6001"
/**
* Default address where the Gateway is sending the tracking messages
*/
#define DEFAULT_XSUB "ipc:///tmp/nginx_queue_listen"

/**
* The address used to listen for incoming messages for the Gateway
*/
#define DEFAULT_SUB  "tcp://0.0.0.0:5000"
/**
* The default address where the adaptor forwards messages to the Gateway
*/
#define DEFAULT_PUSH "ipc:///tmp/nginx_queue_push"

#include "czmq.h"

zctx_t *
gw_zmq_init();

void
gw_zmq_destroy( zctx_t **ctx );

void
start_gateway_listener(zctx_t *ctx, char *subscriberAddress, char *publisherAddress);

#endif