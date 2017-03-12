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
void
subscriber_thread (zsock_t *pipe, void *args)
{
    //fprintf(stderr, "Starting Debug subscriber thread [%s] ... \n", args);
    void *ctx = zmq_ctx_new ();
    void *subscriber = zmq_socket (ctx, ZMQ_SUB);

    zsock_set_tcp_keepalive_idle( subscriber, 300 );
    zsock_set_tcp_keepalive_cnt( subscriber, 300 );
    zsock_set_tcp_keepalive_intvl( subscriber, 300 );

    assert (zmq_connect (subscriber, args) == 0);

    //An actor function MUST call zsock_signal (pipe) when initialized
    // and MUST listen to pipe and exit on $TERM command.
    zsock_signal (pipe, 0);

    zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, "", 0);


    void *watch;
    unsigned long elapsed_counter = 0;
    unsigned long elapsed ;
    unsigned long elapsed_since_last_message;
    double min_latency = DBL_MAX;
    double max_latency = -1;
    double avg_latency = -1;
    int messages_received_counter = 0;
    int messages_received_latency = 0;

    bool terminated = false;
    while (!terminated) {

        watch = zmq_stopwatch_start ();
        char *string = zstr_recv (subscriber);
        if (!string) {
            fprintf(stderr, "%s\n", "Invalid string received.");
            break;              //  Interrupted
        }
        elapsed_since_last_message = zmq_stopwatch_stop (watch);

        watch = zmq_stopwatch_start ();

        // time_t now;
        // time(&now);
        // char s[1000];
        // struct tm * p = localtime(&now);
        // strftime(s, 1000, "%Y-%m-%dT%H-%M-%SZ", p);
        // printf("> At [%s] got: [%s]\n", s, string);

        messages_received_counter ++;
        elapsed = zmq_stopwatch_stop (watch);
        messages_received_latency += elapsed;

        if ( elapsed < min_latency ) min_latency = elapsed;
        if ( elapsed > max_latency ) max_latency = elapsed;
        avg_latency = messages_received_latency / messages_received_counter;

        elapsed_counter += elapsed + elapsed_since_last_message;
        if ( elapsed_counter >= 1000*1000 ) {
            fprintf(stderr, " %d messages processed in %lu [ms] with latency min=%.4f[ms], max=%.4f[ms], avg=%.4f[ms]\n", messages_received_counter, elapsed_counter/1000, min_latency/1000, max_latency/1000, avg_latency/1000);
            fprintf(stderr, "Last message received: %s\n", string);
            min_latency = DBL_MAX;
            max_latency = -1;
            avg_latency = -1;
            elapsed_counter = 0;
            elapsed_since_last_message = 0;
            messages_received_counter = 0;
        }

        if (streq(string, "PUB-499999")) {
            terminated = true;
            fprintf(stderr, " %d messages processed in %lu [ms] with latency min=%.4f[ms], max=%.4f[ms], avg=%.4f[ms]\n", messages_received_counter, elapsed_counter/1000, min_latency/1000, max_latency/1000, avg_latency/1000);
            fprintf(stderr, "Last message received: %s\n", string);
            break;
        }
        free (string);
    }
    fprintf(stderr, "%s\n", "Debug subscriber thread terminated.");
    assert(zmq_close (subscriber)==0);
    zmq_ctx_term(ctx);
}

/**
*
*  This method is activated with '-t' option and it's used for testing purposes only
*  The publisher sends random messages starting with A-J:
*
*/
void
testing_thread (zsock_t *pipe, void *args)
{
    //fprintf(stderr, "Starting Test Generator thread [%s] ... \n", args);
    //An actor function MUST call zsock_signal (pipe) when initialized
    // and MUST listen to pipe and exit on $TERM command.
    void *ctx = zmq_ctx_new ();
    void *publisher = zmq_socket (ctx, ZMQ_PUB);
    assert (zmq_connect (publisher, args) == 0);

    zsock_signal (pipe, 0);

    zclock_sleep(500);

    char string[20];
    int pause_idx = 1;
    for (int i=0; i<=500000; i++) {
        //sprintf (string, "PUB-%c-%05d", randof (10) + 'A', randof (100000));
        sprintf (string, "PUB-%06d", i);
        if (i == pause_idx * 1000) {
          // fprintf(stderr, "Sent %d messages\n", i);
          zclock_sleep(10);
          pause_idx++;
        }
        int send_response = zstr_send(publisher, string);
        if (send_response == -1) {
            break;              //  Interrupted
        }
        //fprintf(" ... sending:%s\n", string);
    }
    fprintf (stderr, "%s\n", "Sending TERM signal ...");
    sprintf (string, "%s", "TERM");
    zstr_send (publisher, string);

    zclock_sleep (20000);

    fprintf (stderr, "%s\n", "Test Generator thread terminated.");
    assert (zmq_close (publisher)==0);
    zmq_ctx_term(ctx);
}


/**
*  .split main thread
*  The main task starts the subscriber and publisher, and then sets
*  itself up as a listening proxy. The listener runs as a child thread:
*   usage: api-gateway-zmq-adaptor -d -p tcp://127.0.0.1:6001 -b ipc:///tmp/nginx_listener_queue -l tcp://127.0.0.1:5000 -u ipc:///tmp/nginx_queue_push
*         -p public address where messages from API Gateway are published. This is where you can listen for messages coming from the API Gateway
*         -b the local address to listen for messages from API Gateway which are then proxied ( forwarded ) to -p address
*
*         -l public address to listen for incoming messages sent to API Gateway
*         -u local address where messages from -l are pushed ( forwarded ) to the API Gateway
*
*         -d activates debug option, printing the messages on the output
*         -t test mode simulates a publisher for XSUB/XPUB with random messages : PUB -> XSUB -> XPUB -> SUB
*         -r receiver flag simulates a publisher and receiver : PUB (bind) -> SUB (connect) -> PUSH (bind) -> PULL ( connect )
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
    char *listenerAddress = DEFAULT_SUB;
    char *pushAddress = DEFAULT_PUSH;
    int debugFlag = 0;
    int testFlag = 0;
    int testBlackBoxFlag = 0;

    while ( (c = getopt (argc, argv, "b:p:l:u:dtr") ) != -1)
    {
        switch (c)
        {
            case 'b':
                subscriberAddress = strdup(optarg);
                break;
            case 'p':
                publisherAddress = strdup(optarg);
                break;
            case 'l':
                listenerAddress = strdup(optarg);
                break;
            case 'u':
                pushAddress = strdup(optarg);
                break;
            case 'd':
                debugFlag = 1;
                fprintf (stderr,"RUNNING IN DEBUGGING MODE\n");
                break;
            case 't':
                debugFlag = 1;
                testFlag = 1;
                fprintf (stderr,"RUNNING IN TEST MODE & DEBUG MODE for XPUB -> XSUB\n");
                break;
            case 'r':
                debugFlag = 1;
                testBlackBoxFlag = 1;
                fprintf (stderr,"RUNNING IN TEST MODE & DEBUG MODE for SUB -> PUSH\n");
                break;
            case '?':
                fprintf (stderr,"Unrecognized option!\n");
                break;
        }
    }

    //  Set the context for the child threads
    void *ctx = gw_zmq_init ();

    //
    // Espresso Pattern impl
    // @see http://zguide.zeromq.org/page:all#header-116
    // --------------------------------------
    //   XPUB           ->       XSUB
    //  public Addr     ->    internal Addr
    //   BIND           ->       BIND
    // ---------------------------------------
    //
    zactor_t *test_pipe = NULL;
    if (testFlag == 1) {
        test_pipe = zactor_new (testing_thread, subscriberAddress);
    }

    // Add a listener thread and start the proxy
    // NOTE: when there are no consumers, messages are simply dropped
    zactor_t *debug_pipe = NULL;
    if (debugFlag == 1) {
        debug_pipe = zactor_new (subscriber_thread, publisherAddress);
    }

    start_gateway_listener (ctx, subscriberAddress, publisherAddress);

    if (debug_pipe) {
        zactor_destroy (&debug_pipe);
    }

    if (test_pipe) {
      zactor_destroy (&test_pipe);
    }

    fprintf (stderr, " ... interrupted\n");
    //  Tell attached threads to exit
    gw_zmq_destroy (ctx);
    return 0;
}
