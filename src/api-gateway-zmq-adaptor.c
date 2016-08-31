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

/**
*  The functions bellow up to the main() are used for debugging or quick testing purposes only
*/

/**
* Starts a listener thread in the background just to print all the messages.
* Use it for debugging purposes.
* This method is activated with the '-d' flag.
*
*/
static void
subscriber_thread (void *args, zctx_t *ctx, void *pipe)
{
    fprintf(stderr, "Starting Debug subscriber thread [%s] ... \n", args);
    void *subscriber = zsocket_new (ctx, ZMQ_SUB);

    zsocket_set_tcp_keepalive_idle( subscriber, 300 );
    zsocket_set_tcp_keepalive_cnt( subscriber, 300 );
    zsocket_set_tcp_keepalive_intvl( subscriber, 300 );

    zsocket_connect (subscriber, "%s", args);

    zsocket_set_subscribe (subscriber, "");


    void *watch;
    unsigned long elapsed_counter = 0;
    unsigned long elapsed ;
    unsigned long elapsed_since_last_message;
    double min_latency = DBL_MAX;
    double max_latency = -1;
    double avg_latency = -1;
    int messages_received_counter = 0;
    int messages_received_latency = 0;

    while (!zctx_interrupted) {

        watch = zmq_stopwatch_start ();
        char *string = zstr_recv (subscriber);
        if (!string) {
            break;              //  Interrupted
        }
        elapsed_since_last_message = zmq_stopwatch_stop (watch);

        watch = zmq_stopwatch_start ();

        /*time_t now;
        time(&now);
        fprintf(stderr, "> %s got: [%s]\n", ctime(&now), string);*/

        free (string);

        messages_received_counter ++;
        elapsed = zmq_stopwatch_stop (watch);
        messages_received_latency += elapsed;

        if ( elapsed < min_latency ) min_latency = elapsed;
        if ( elapsed > max_latency ) max_latency = elapsed;
        avg_latency = messages_received_latency / messages_received_counter;

        elapsed_counter += elapsed + elapsed_since_last_message;
        if ( elapsed_counter >= 1000*1000 ) {
            fprintf(stderr, "api-gateway-zmq-adaptor - %d messages processed in %lu [ms] with latency min=%.4f[ms], max=%.4f[ms], avg=%.4f[ms]\n", messages_received_counter, elapsed_counter/1000, min_latency/1000, max_latency/1000, avg_latency/1000);
            min_latency = DBL_MAX;
            max_latency = -1;
            avg_latency = -1;
            elapsed_counter = 0;
            messages_received_counter = 0;
        }
    }
    zsocket_destroy (ctx, subscriber);
}

/**
*
*  This method is activated with '-t' option and it's used for testing purposes only
*  The publisher sends random messages starting with A-J:
*
*/
static void
publisher_thread (void *args, zctx_t *ctx, void *pipe)
{
    fprintf(stderr, "Starting Test publisher thread [%s] ... \n", args);
    void *publisher = zsocket_new (ctx, ZMQ_PUB);
    int socket_bound = zsocket_connect (publisher, "%s", args);

    char string [20];
    int send_response = -100;
    int i = 0;
    while (!zctx_interrupted) {
        i = 0;
        for ( i=0; i<1; i++) {
            sprintf (string, "PUB-%c-%05d", randof (10) + 'A', randof (100000));
            send_response = zstr_send(publisher, string);
            if (send_response == -1) {
                break;              //  Interrupted
            }
            fprintf(stderr, " ... sending:%s\n", string);
        }
        zclock_sleep (1000);
    }
}

/**
*
*  The listener receives all messages flowing through the proxy, on its
*  pipe. In CZMQ, the pipe is a pair of ZMQ_PAIR sockets that connect
*  attached child threads. In other languages your mileage may vary:
*/
static void
listener_thread (void *args, zctx_t *ctx, void *pipe)
{
    //  Print everything that arrives on pipe
    while (true) {
        zframe_t *frame = zframe_recv (pipe);
        if (!frame) {
            puts("empty frame. stopping the listener ...");
            break;              //  Interrupted
        }
        zframe_print (frame, NULL);
        zframe_destroy (&frame);
    }
}

/**
*  .split main thread
*  The main task starts the subscriber and publisher, and then sets
*  itself up as a listening proxy. The listener runs as a child thread:
*   usage: api-gateway-zmq-adaptor -d -p tcp://127.0.0.1:6001 -b ipc:///tmp/nginx_listener_queue -l tcp://127.0.0.1:5000 -u ipc:///tmp/nginx_queue_push
*         -p public address where messages from API Gateway are published. This is where you can listen for messages coming from the API Gateway
*         -b the local address to listen for messages from API Gateway which are then proxied ( forwarded ) to -p address
*
*         -d activates debug option, printing the messages on the output
*         -t test mode simulates a publisher for XSUB/XPUB with random messages : PUB -> XSUB -> XPUB -> SUB
*/
int main (int argc, char *argv[])
{
    int major, minor, patch, lmajor, lminor, lpatch;
    zmq_version (&major, &minor, &patch);
    zsys_version (&lmajor, &lminor, &lpatch);
    fprintf(stderr, "ZeroMQ version %d.%d.%d (czmq %d.%d.%d) \n", major, minor, patch, lmajor, lminor, lpatch);

    // parse command line args
    char c;
    char *subscriberAddress = DEFAULT_XSUB;
    char *publisherAddress = DEFAULT_XPUB;
    int debugFlag = 0;
    int testFlag = 0;
    int testBlackBoxFlag = 0;

    while ( (c = getopt(argc, argv, "b:p:l:u:dtr") ) != -1)
    {
        switch (c)
        {
            case 'b':
                subscriberAddress = strdup(optarg);
                break;
            case 'p':
                publisherAddress = strdup(optarg);
                break;
            case 'd':
                debugFlag = 1;
                fprintf(stderr,"RUNNING IN DEBUGGING MODE\n");
                break;
            case 't':
                debugFlag = 1;
                testFlag = 1;
                fprintf(stderr,"RUNNING IN TEST MODE & DEBUG MODE for XPUB -> XSUB\n");
                break;
            case '?':
                fprintf(stderr,"Unrecognized option!\n");
                break;
        }
    }

    //  Set the context for the child threads
    zctx_t *ctx = gw_zmq_init();

    //
    // Espresso Pattern impl
    // @see http://zguide.zeromq.org/page:all#header-116
    // --------------------------------------
    //   XPUB           ->       XSUB
    //  public Addr     ->    internal Addr
    //   BIND           ->       BIND
    // ---------------------------------------
    //

    start_gateway_listener(ctx, subscriberAddress, publisherAddress);

    if ( testFlag == 1 ) {
        zthread_fork (ctx, publisher_thread, subscriberAddress);
    }

    // Add a listener thread and start the proxy
    // NOTE: when there are no consumers, messages are simply dropped
    if ( debugFlag == 1 ) {
        zthread_fork (ctx, subscriber_thread, publisherAddress);
        //void *listener = zthread_fork (ctx, listener_thread, NULL);
        //zmq_proxy (subscriber, publisher, listener);
    }
    //else {
    //    zmq_proxy (subscriber, publisher, NULL);
    //}

    // just making sure the current thread doesn't exit
    while( !zctx_interrupted ) {
        zclock_sleep(500);
    }

    fprintf(stderr," ... interrupted");
    //  Tell attached threads to exit
    gw_zmq_destroy( &ctx );
    return 0;
}
