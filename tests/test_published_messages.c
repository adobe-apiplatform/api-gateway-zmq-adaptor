// TODO

#include <stdlib.h>
#include <check.h>
#include "zmq.h"
#include "../src/GwZmqAdaptor.h"

START_TEST(test_zmq_context_lifecycle)
{
    // 1. start the adaptor
    // 2. start a test publisher on XSUB
    // 3. start a consumer on XPUB
    // 4. test that the messages sent by the publishers are available on the XPUB socket

    zctx_t *ctx = gw_zmq_init();
    ck_assert_msg(ctx != NULL, "ZMQ Context can't be null. ");

    gw_zmq_destroy( &ctx );
    ck_assert_msg(ctx == NULL, "ZMQ Context should be destroyed. ");

}
END_TEST

/*

test utility methods

*/

static void
mock_gateway_publisher_thread (void *args, zctx_t *ctx, void *pipe)
{
    printf("Starting Gateway publisher thread [%s] ... \n", args);

    void *publisher = zsocket_new (ctx, ZMQ_PUB);
    int socket_bound = zsocket_connect (publisher, "%s", args);

    char string [20];
    int send_response = -100;
    int i = 0;
    while (!zctx_interrupted) {
        i = 0;
        for ( i=0; i<5; i++) {
            sprintf (string, "PUB-%c-%05d", randof (10) + 'A', randof (100000));
            send_response = zstr_send(publisher, string);
            if (send_response == -1) {
                break;              //  Interrupted
            }
            printf(" ... sending:%s\n", string);
        }
        zclock_sleep (100);
    }
    zsocket_destroy (ctx, publisher);
}

/**
* Counter used to test how  many messages are received by the subscriber
*/
int messages_received_counter = 0;

/*

 Starts a listener thread in the background just to print all the messages.
 Use it for debugging purposes.
 This method is activated with the '-d' flag.

*/
static void
mock_subscriber_thread (void *args, zctx_t *ctx, void *pipe)
{
    printf("Starting Debug subscriber thread [%s] ... \n", args);
    messages_received_counter = 0;

    void *subscriber = zsocket_new (ctx, ZMQ_SUB);
    zsocket_connect (subscriber, "%s", args);
    zsocket_set_subscribe (subscriber, "");

    while (!zctx_interrupted) {
        char *string = zstr_recv (subscriber);
        if (!string) {
            break;              //  Interrupted
        }
        time_t now;
        time(&now);
        printf("> %s got: [%s]\n", ctime(&now), string);
        free (string);
        messages_received_counter ++;
        // zclock_sleep(1);
    }
    zsocket_destroy (ctx, subscriber);
}


START_TEST(test_gateway_listener)
{
    zctx_t *ctx = gw_zmq_init();
    ck_assert_msg(ctx != NULL, "ZMQ Context can't be null. ");

    start_gateway_listener(ctx, "ipc:///tmp/nginx_queue_listen", "tcp://127.0.0.1:6001");

    // simulate a consumer
    char *publisherAddress = "tcp://127.0.0.1:6001";
    void *pipe2 = zthread_fork (ctx, mock_subscriber_thread, publisherAddress);
    ck_assert_msg(pipe2 != NULL, "Subscriber Thread should have been created. ");

    zclock_sleep (100);

    // simulate gateway
    char *subscriberAddress = "ipc:///tmp/nginx_queue_listen";
    void *pipe = zthread_fork (ctx, mock_gateway_publisher_thread, subscriberAddress);
    ck_assert_msg(pipe != NULL, "Publisher Thread should have been created. ");

    // wait for some messages to be passed
    zclock_sleep(400);
    zctx_interrupted = true;

//    ck_assert_msg( messages_received_counter >= 10, "The consumer should have received at least 10 messages");
    ck_assert_int_eq( messages_received_counter, 15 );

    gw_zmq_destroy( &ctx );
    ck_assert_msg(ctx == NULL, "ZMQ Context should be destroyed. ");
}
END_TEST



Suite * adaptor_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("ZMQ-Adaptor");

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_zmq_context_lifecycle);
    tcase_add_test(tc_core, test_gateway_listener);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = adaptor_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}