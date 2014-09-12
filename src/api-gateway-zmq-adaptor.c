//  Espresso Pattern
//  This shows how to capture data using a pub-sub proxy

#include "czmq.h"

#define DEFAULT_XPUB "tcp://127.0.0.1:6001"
#define DEFAULT_XSUB "ipc:///tmp/xsub"


//  The subscriber thread requests messages starting with
//  A and B, then reads and counts incoming messages.
//app=test-app,serv=_undefined_,req=test-api-key,resp=GET.200

static void
subscriber_thread (void *args, zctx_t *ctx, void *pipe)
{
    printf("Starting subscriber thread [%s] ... \n", args);
    //  Subscribe to "A" and "B"
    void *subscriber = zsocket_new (ctx, ZMQ_SUB);
    zsocket_connect (subscriber, "%s", args);
    zsocket_set_subscribe (subscriber, "");

    int count = 0;
    while (!zctx_interrupted) {
        char *string = zstr_recv (subscriber);
        if (!string) {
            break;              //  Interrupted
        }
        printf("> got: %s\n", string);
        free (string);
        count++;
        zclock_sleep(1);
    }
    zsocket_destroy (ctx, subscriber);
}

//  .split publisher thread
//  The publisher sends random messages starting with A-J:

static void
publisher_thread (void *args, zctx_t *ctx, void *pipe)
{
    printf("Starting publisher thread [%s] ... \n", args);
    void *publisher = zsocket_new (ctx, ZMQ_PUB);
    int socket_bound = zsocket_connect (publisher, "%s", args);

    while (!zctx_interrupted) {
        for ( int i=0; i<1; i++) {
            char string [20];
            sprintf (string, "%c-%05d", randof (10) + 'A', randof (100000));
            int send_response = zstr_send(publisher, string);
            if (send_response == -1) {
                break;              //  Interrupted
            }
            printf(" ... sending:%s\n", string);
        }
        zclock_sleep (1000);
    }
}

//  .split listener thread
//  The listener receives all messages flowing through the proxy, on its
//  pipe. In CZMQ, the pipe is a pair of ZMQ_PAIR sockets that connect
//  attached child threads. In other languages your mileage may vary:

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

//  .split main thread
//  The main task starts the subscriber and publisher, and then sets
//  itself up as a listening proxy. The listener runs as a child thread:
//   usage: api-gateway-zmq-adaptor -p *:6001 -s ipc:///tmp/xsub -d
//                 -p specifies the address:IP pair where to publish the message
//                 -s specifies the address where to listen for messages
//                 -s activates debug option, printing the messages on the output
//                 -t test mode that simulates a publisher as well
//

int main (int argc, char *argv[])
{
    int major, minor, patch, lmajor, lminor, lpatch;
    zmq_version (&major, &minor, &patch);
    zsys_version (&lmajor, &lminor, &lpatch);
    char str_version[60];
    sprintf( str_version, "ZeroMQ version %d.%d.%d (czmq %d.%d.%d)", major, minor, patch, lmajor, lminor, lpatch);
    puts(str_version);

    // parse command line args
    char c;
    char *subscriberAddress = DEFAULT_XSUB;
    char *publisherAddress = DEFAULT_XPUB;
    int debugFlag = 0;
    int testFlag = 0;

    while ( (c = getopt(argc, argv, "s:p:dt") ) != -1)
    {
        switch (c)
        {
            case 's':
                subscriberAddress = strdup(optarg);
                break;
            case 'p':
                publisherAddress = strdup(optarg);
                break;
            case 'd':
                debugFlag = 1;
                puts("RUNNING IN DEBUGGING MODE");
                break;
            case 't':
                debugFlag = 1;
                testFlag = 1;
                puts("RUNNING IN TEST MODE");
                break;
            case '?':
                puts("Unrecognized option!");
                break;
        }
    }

    //  Start child threads
    zctx_t *ctx = zctx_new ();

    // Start XSUB Proxy -> local publishers connect here
    puts("Starting XPUB/XSUB Proxy ... ");
    void *subscriber = zsocket_new (ctx, ZMQ_XSUB);
    zsocket_bind (subscriber, "%s", subscriberAddress);
    printf("XSUB is bound to: %s\n", subscriberAddress);

    // Start XPUB Proxy -> remote consumers connect here
    void *publisher = zsocket_new (ctx, ZMQ_XPUB);
    zsocket_bind (publisher, "%s", publisherAddress);
    printf("XPUB is bound to: %s\n", publisherAddress);


    if ( testFlag == 1 ) {
        zthread_fork (ctx, publisher_thread, subscriberAddress);
    }

    // Add a listener thread and start the proxy
    // NOTE: where there are no remote consumers, messages are simply dropped
    if ( debugFlag == 1 ) {
        zthread_fork (ctx, subscriber_thread, publisherAddress);
        void *listener = zthread_fork (ctx, listener_thread, NULL);
        zmq_proxy (subscriber, publisher, listener);
    }
    else {
        zmq_proxy (subscriber, publisher, NULL);
    }

    puts (" interrupted");
    //  Tell attached threads to exit
    zctx_destroy (&ctx);
    return 0;
}
