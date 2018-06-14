/*
* Copyright 2015 Adobe Systems Incorporated. All rights reserved.
*
* This file is licensed to you under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*  http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software distributed
* under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR RESPRESENTATIONS
* OF ANY KIND, either express or implied.  See the License for the
* specific language governing permissions and limitations under the License.
*/

#include "GwZmqAdaptor.h"
#include "czmq.h"
#include "time.h"

char*
timestamp() {
    time_t rawtime;

    time(&rawtime);

    struct tm* timeinfo;

    timeinfo = localtime(&rawtime);

    char* result = asctime(timeinfo);

    result[strlen(result) - 1] = 0;

    return result;
}

zctx_t *
gw_zmq_init()
{
    //  Set the context for the child threads
    zctx_t *ctx = zctx_new ();
    return ctx;
}

void
gw_zmq_destroy( zctx_t **ctx )
{
    //  Tell attached threads to exit
    zctx_destroy(ctx);
}

static int
read_message(void* socket, zmq_event_t* event, char* endpoint) {
    zmq_msg_t binary;

    zmq_msg_init(&binary);

    int result = zmq_msg_recv(&binary, socket, 0);

    if (result == -1) {
        return -1;
    }

    assert(zmq_msg_more(&binary) != 0);

    zmq_msg_t address;

    zmq_msg_init(&address);
    result = zmq_msg_recv(&address, socket, 0);

    if (result == -1) {
        return -1;
    }

    assert(zmq_msg_more(&address) == 0);

    const char* binary_data = (char*)zmq_msg_data(&binary);

    memcpy(&(event->event), binary_data, sizeof(event->event));
    memcpy(&(event->value), binary_data + sizeof(event->event), sizeof(event->value));

    const size_t len = zmq_msg_size(&address);

    endpoint = memcpy(endpoint, zmq_msg_data(&address), len);

    *(endpoint + len) = 0;

    return 0;
}

static void*
monitor_generic_socket(void* ctx, const char* endpoint) {
    fprintf(stderr, "[%s] - Monitoring endpoint %s...\n", timestamp(), endpoint);

    void* socket = zsocket_new(ctx, ZMQ_PAIR);

    assert(socket);
    fprintf(stderr, "[%s] - Created PAIR socket for endpoint %s\n", timestamp(), endpoint);

    int result = zmq_connect(socket, endpoint);

    assert(result == 0);
    fprintf(stderr, "[%s] - Connected PAIR socket to endpoint %s\n", timestamp(), endpoint);

    zmq_event_t event;
    static char address[512];

    while(!read_message(socket, &event, address)) {
        int code = event.event;
        int value = event.value;

        switch(code) {
            case ZMQ_EVENT_CONNECTED:
                fprintf(stderr, "[%s] - Received ZMQ_EVENT_CONNECTED event %d with value=%d for address=%s\n", timestamp(), code, value, address);
                break;
            case ZMQ_EVENT_CONNECT_DELAYED:
                fprintf(stderr, "[%s] - Received ZMQ_EVENT_CONNECT_DELAYED event %d with value=%d for address=%s\n", timestamp(), code, value, address);
                break;
            case ZMQ_EVENT_CONNECT_RETRIED:
                fprintf(stderr, "[%s] - Received ZMQ_EVENT_CONNECT_RETRIED event %d with value=%d for address=%s\n", timestamp(), code, value, address);
                break;
            case ZMQ_EVENT_LISTENING:
                fprintf(stderr, "[%s] - Received ZMQ_EVENT_LISTENING event %d with value=%d for address=%s\n", timestamp(), code, value, address);
                break;
            case ZMQ_EVENT_BIND_FAILED:
                fprintf(stderr, "[%s] - Received ZMQ_EVENT_BIND_FAILED event %d with value=%d for address=%s\n", timestamp(), code, value, address);
                break;
            case ZMQ_EVENT_ACCEPTED:
                fprintf(stderr, "[%s] - Received ZMQ_EVENT_ACCEPTED event %d with value=%d for address=%s\n", timestamp(), code, value, address);
                break;
            case ZMQ_EVENT_ACCEPT_FAILED:
                fprintf(stderr, "[%s] - Received ZMQ_EVENT_ACCEPT_FAILED event %d with value=%d for address=%s\n", timestamp(), code, value, address);
                break;
            case ZMQ_EVENT_CLOSED:
                fprintf(stderr, "[%s] - Received ZMQ_EVENT_CLOSED event %d with value=%d for address=%s\n", timestamp(), code, value, address);
                break;
            case ZMQ_EVENT_CLOSE_FAILED:
                fprintf(stderr, "[%s] - Received ZMQ_EVENT_CLOSE_FAILED event %d with value=%d for address=%s\n", timestamp(), code, value, address);
                break;
            case ZMQ_EVENT_DISCONNECTED:
                fprintf(stderr, "[%s] - Received ZMQ_EVENT_DISCONNECTED event %d with value=%d for address=%s\n", timestamp(), code, value, address);
                break;
            default:
                fprintf(stderr, "[%s] - Received unknown ZMQ event %d with value=%d for address=%s\n", timestamp(), code, value, address);
        }
    }

    zmq_close(socket);

    return 0;
}

static void*
monitor_xpub_socket(void *ctx) {
    monitor_generic_socket(ctx, DEFAULT_INPROC_XPUB_MONITOR_ENDPOINT);
    return NULL;
}

static void*
monitor_xsub_socket(void *ctx) {
    monitor_generic_socket(ctx, DEFAULT_INPROC_XSUB_MONITOR_ENDPOINT);
    return NULL;
}

/*

Espresso Pattern impl
@see http://zguide.zeromq.org/page:all#header-116
--------------------------------------
   XPUB           ->       XSUB
  public address     ->    internal address
   BIND           ->       BIND
---------------------------------------

This method starts a new thread subscribing to the messages sent by the Gateway on XSUB
and proxying them to a local IP address on XPUB . Remote consumers should connect to the XPUB's socket address.

*/

void
start_gateway_listener(zctx_t *ctx, char *subscriberAddress, char *publisherAddress, int debugFlag)
{
    fprintf(stderr,"[%s] - Starting Gateway Listener \n", timestamp());

    void *subscriber = zsocket_new (ctx, ZMQ_XSUB);
    int subscriberSocketResult = zsocket_bind (subscriber, "%s", subscriberAddress);
    assert( subscriberSocketResult >= 0 );

    // Start XPUB Proxy -> remote consumers connect here
    void *publisher = zsocket_new (ctx, ZMQ_XPUB);
    zsocket_set_xpub_verbose (publisher, 1);
    int publisherBindResult = zsocket_bind (publisher, "%s", publisherAddress);
    assert( publisherBindResult >= 0 );

    fprintf(stderr, "[%s] - Starting XPUB->XSUB Proxy [%s] -> [%s] \n", timestamp(), subscriberAddress, publisherAddress);
    zproxy_t *xpub_xsub_thread = zproxy_new(ctx, subscriber, publisher);
    assert( xpub_xsub_thread );

    if(!debugFlag) {
        return;
    }

    int result;

    fprintf(stderr, "[%s] - Creating socket monitor for %s using %s\n", timestamp(), publisherAddress, DEFAULT_INPROC_XPUB_MONITOR_ENDPOINT);
    result = zmq_socket_monitor(publisher, DEFAULT_INPROC_XPUB_MONITOR_ENDPOINT, ZMQ_EVENT_ALL);
    assert(result == 0);

    fprintf(stderr, "[%s] - Creating socket monitor for %s using %s\n", timestamp(), subscriberAddress, DEFAULT_INPROC_XSUB_MONITOR_ENDPOINT);
    result = zmq_socket_monitor(subscriber, DEFAULT_INPROC_XSUB_MONITOR_ENDPOINT, ZMQ_EVENT_ALL);
    assert(result == 0);

    pthread_t thread;

    fprintf(stderr, "[%s] - Creating thread for %s socket monitor\n", timestamp(), publisherAddress);
    result = pthread_create(&thread, NULL, monitor_xpub_socket, ctx);
    assert (result == 0);

    fprintf(stderr, "[%s] - Creating thread for %s socket monitor\n", timestamp(), subscriberAddress);
    result = pthread_create(&thread, NULL, monitor_xsub_socket, ctx);
    assert (result == 0);
}

