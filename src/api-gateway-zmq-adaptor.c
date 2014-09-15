//  Espresso Pattern
//  This shows how to capture data using a pub-sub proxy

#include "czmq.h"

#define DEFAULT_XPUB "tcp://127.0.0.1:6001"
#define DEFAULT_XSUB "ipc:///tmp/nginx_queue_listen"
#define DEFAULT_SUB  "tcp://127.0.0.1:5000"
#define DEFAULT_PUSH "ipc:///tmp/nginx_queue_push"

struct _zmq_proxy_args_t {
    char *frontend;
    char *backend;
};


//  The subscriber thread requests messages starting with
//  A and B, then reads and counts incoming messages.
//app=test-app,serv=_undefined_,req=test-api-key,resp=GET.200

static void
subscriber_thread (void *args, zctx_t *ctx, void *pipe)
{
    printf("Starting Debug subscriber thread [%s] ... \n", args);
    //  Subscribe to "A" and "B"
    void *subscriber = zsocket_new (ctx, ZMQ_SUB);
    zsocket_connect (subscriber, "%s", args);
    zsocket_set_subscribe (subscriber, "");

    while (!zctx_interrupted) {
        char *string = zstr_recv (subscriber);
        if (!string) {
            break;              //  Interrupted
        }
        printf("> got: %s\n", string);
        free (string);
        zclock_sleep(1);
    }
    zsocket_destroy (ctx, subscriber);
}

//  .split publisher thread
//  The publisher sends random messages starting with A-J:

static void
publisher_thread (void *args, zctx_t *ctx, void *pipe)
{
    printf("Starting Test publisher thread [%s] ... \n", args);
    void *publisher = zsocket_new (ctx, ZMQ_PUB);
    int socket_bound = zsocket_connect (publisher, "%s", args);

    while (!zctx_interrupted) {
        for ( int i=0; i<1; i++) {
            char string [20];
            sprintf (string, "PUB-%c-%05d", randof (10) + 'A', randof (100000));
            int send_response = zstr_send(publisher, string);
            if (send_response == -1) {
                break;              //  Interrupted
            }
            printf(" ... sending:%s\n", string);
        }
        zclock_sleep (1000);
    }
}


static void
publisher_thread_for_black_box (void *args, zctx_t *ctx, void *pipe)
{
    printf("Starting Test publisher thread for BlackBox [%s] ... \n", args);
    void *publisher = zsocket_new (ctx, ZMQ_PUB);
    int socket_bound = zsocket_connect (publisher, "%s", args);

    while (!zctx_interrupted) {
        for ( int i=0; i<1; i++) {
            char string [20];
            sprintf (string, "SEND-%05d", randof (100000));
            int send_response = zstr_send(publisher, string);
            if (send_response == -1) {
                break;              //  Interrupted
            }
            printf(" ... sending:%s\n", string);
        }
        zclock_sleep (1000);
    }
}


// PULLS from the PUSH socket
static void
pull_receiver_thread (void *args, zctx_t *ctx, void *pipe)
{
    printf("Starting Debug receiver thread [%s] ... \n", args);

    void *receiver = zsocket_new (ctx, ZMQ_PULL);
    int receiverConnectResult = zsocket_connect (receiver, "%s", args);
    assert( receiverConnectResult >= 0 );

    while (!zctx_interrupted) {
        char *string = zstr_recv (receiver);
        if (!string) {
            puts(" ... Debug receiver thread interrupted !");
            break;              //  Interrupted
        }
        printf("> receiver got: %s\n", string);
        zstr_send(pipe, string);
        free (string);
        zclock_sleep(1);
    }
    zsocket_destroy (ctx, receiver);

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
//   usage: api-gateway-zmq-adaptor -d -p tcp://127.0.0.1:6001 -b ipc:///tmp/nginx_listener_queue -l tcp://127.0.0.1:5000 -u ipc:///tmp/nginx_queue_push
//                 -p public address where messages from nginx are published
//                 -b the local address to listen for messages from Nginx which are then proxied to -p address
//
//                 -l public address to listen for incoming messages
//                 -u local address where messages from -l are pushed to Nginx
//
//                 -d activates debug option, printing the messages on the output
//                 -t test mode simulates a publisher for XSUB/XPUB with random messages : PUB -> XSUB -> XPUB -> SUB
//                 -r receiver flag simulates a publisher and receiver : PUB (bind) -> SUB (connect) -> PUSH (bind) -> PULL ( connect )
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
    char *listenerAddress = DEFAULT_SUB;
    char *pushAddress = DEFAULT_PUSH;
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
            case 'l':
                listenerAddress = strdup(optarg);
                break;
            case 'u':
                pushAddress = strdup(optarg);
                break;
            case 'd':
                debugFlag = 1;
                puts("RUNNING IN DEBUGGING MODE");
                break;
            case 't':
                debugFlag = 1;
                testFlag = 1;
                puts("RUNNING IN TEST MODE & DEBUG MODE for XPUB -> XSUB");
                break;
            case 'r':
                debugFlag = 1;
                testBlackBoxFlag = 1;
                puts("RUNNING IN TEST MODE & DEBUG MODE for SUB -> PUSH");
                break;
            case '?':
                puts("Unrecognized option!");
                break;
        }
    }

    //  Set the context for the child threads
    zctx_t *ctx = zctx_new ();

    //
    // Black Box Pattern impl
    // @see http://zguide.zeromq.org/page:all#header-119
    // -------------------------------------
    //     SUB        ->      PUSH
    //  public Addr   ->    internal Addr
    //   CONNECT      ->      BIND
    // -------------------------------------
    //

    void *listenerSocket = zsocket_new(ctx, ZMQ_SUB);
    int listenerSocketResult = -1;
    if ( testBlackBoxFlag == 0 ) {
        listenerSocketResult = zsocket_connect(listenerSocket, "%s", listenerAddress);
    } else {
        listenerSocketResult = zsocket_bind(listenerSocket, "%s", listenerAddress);
    }
    assert( listenerSocketResult >= 0 );

    zsocket_set_subscribe (listenerSocket, ""); // NOTE: Don't miss this directive, otherwise the SUB doesn't get anything

    void *pushSocket = zsocket_new(ctx, ZMQ_PUSH);
    int pushSocketResult = zsocket_bind(pushSocket, "%s", pushAddress);
    assert( pushSocketResult >= 0 );


    printf("\nStarting SUB->PUSH Proxy [%s] -> [%s] \n", listenerAddress, pushAddress );
    zproxy_t *sub_push_thread = zproxy_new(ctx, listenerSocket, pushSocket);

    if ( testBlackBoxFlag == 1 ) {
        zthread_fork (ctx, publisher_thread_for_black_box, listenerAddress);
    }

    if ( debugFlag == 1 ) {
        // you have to have at least 1 socket to PULL to see the messages
        zthread_fork (ctx, pull_receiver_thread, pushAddress);
    }

    //
    // Espresso Pattern impl
    // @see http://zguide.zeromq.org/page:all#header-116
    // --------------------------------------
    //   XPUB           ->       XSUB
    //  public Addr     ->    internal Addr
    //   BIND           ->       BIND
    // ---------------------------------------
    //

    void *subscriber = zsocket_new (ctx, ZMQ_XSUB);
    int subscriberSocketResult = zsocket_bind (subscriber, "%s", subscriberAddress);
    assert( subscriberSocketResult >= 0 );

    // Start XPUB Proxy -> remote consumers connect here
    void *publisher = zsocket_new (ctx, ZMQ_XPUB);
    int publisherBindResult = zsocket_bind (publisher, "%s", publisherAddress);
    assert( publisherBindResult >= 0 );

    printf("\nStarting XPUB->XSUB Proxy [%s] -> [%s] \n", subscriberAddress, publisherAddress );
    zproxy_t *xpub_xsub_thread = zproxy_new(ctx, subscriber, publisher);

    if ( testFlag == 1 ) {
        zthread_fork (ctx, publisher_thread, subscriberAddress);
    }

    // Add a listener thread and start the proxy
    // NOTE: where there are no remote consumers, messages are simply dropped
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
        zclock_sleep(5000);
    }

    puts (" ... interrupted");
    //  Tell attached threads to exit
    zctx_destroy (&ctx);
    return 0;
}
