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

    void *ctx = gw_zmq_init();
    ck_assert_msg(ctx != NULL, "ZMQ Context can't be null. ");

    int destroyed = gw_zmq_destroy( ctx );
    char s_counter[100] = "";
    sprintf(s_counter, "ZMQ Context should be destroyed but returned value was: %d, errno=%s", destroyed, zmq_strerror (zmq_errno ()) );
    ck_assert_msg( destroyed == 0, s_counter);
    // ck_assert_msg(destroyed == 0, "ZMQ Context should be destroyed. ");

}
END_TEST

/*

test utility methods

*/

static void
mock_gateway_publisher_thread (zsock_t *pipe, void *args)
{
    printf("Starting Gateway publisher thread [%s] ... \n", args);
    //An actor function MUST call zsock_signal (pipe) when initialized and MUST listen to pipe and exit on $TERM command.
    zsock_signal (pipe, 0);

    void *ctx = zmq_ctx_new ();
    void *publisher = zmq_socket (ctx, ZMQ_PUB);
    zmq_connect (publisher, args);

    char string [20];
    int send_response = -100;
    int i = 0;
    // bool terminated = false;
    while (zctx_interrupted == 0) {
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

        // zmsg_t *msg = zmsg_recv (pipe);
        // if (!msg) {
        //     break;              //  Interrupted
        // }
        // char *command = zmsg_popstr (msg);
        // //  All actors must handle $TERM in this way
        //
        // if (streq (command, "$TERM")) {
        //     terminated = true;
        //     break;
        // }
    }
    zmq_ctx_destroy(ctx);
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
mock_subscriber_thread (zsock_t *pipe, void *args)
{
    printf("Starting Debug subscriber thread [%s] ... \n", args);
    //An actor function MUST call zsock_signal (pipe) when initialized and MUST listen to pipe and exit on $TERM command.
    zsock_signal (pipe, 0);
    messages_received_counter = 0;

    void *ctx = zmq_ctx_new ();
    void *subscriber = zmq_socket (ctx, ZMQ_SUB);
    zmq_connect (subscriber, args);
    // subscribe no filter
    zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, "", 0);
    //zsocket_set_subscribe (subscriber, "");

    // bool terminated = false;
    while (zctx_interrupted==0) {
        char *string = zstr_recv (subscriber);
        if (!string) {
            break;              //  Interrupted
        }
        time_t now;
        time(&now);
        printf("> %s got: [%s]\n", ctime(&now), string);
        free (string);
        messages_received_counter ++;
        fprintf(stderr, "zctx_interrupted=%d", zctx_interrupted);
        // zclock_sleep(1);
        // zmsg_t *msg = zmsg_recv (pipe);
        // if (!msg) {
        //     break;              //  Interrupted
        // }
        // char *command = zmsg_popstr (msg);
        // //  All actors must handle $TERM in this way
        // if (streq (command, "$TERM")) {
        //     terminated = true;
        //     break;
        // }
    }
    zmq_ctx_destroy(ctx);
}


START_TEST(test_gateway_listener)
{
    void *ctx = gw_zmq_init();
    ck_assert_msg(ctx != NULL, "ZMQ Context can't be null. ");

    start_gateway_listener(ctx, "ipc:///tmp/nginx_queue_listen", "tcp://127.0.0.1:6001");

    // simulate a consumer
    fprintf(stderr,"CREATING Publisher thread....\n");
    char *publisherAddress = "tcp://127.0.0.1:6001";
    zactor_t *pipe2 = zactor_new (mock_subscriber_thread, publisherAddress);
    ck_assert_msg(pipe2 != NULL, "Subscriber Thread should have been created. ");

    fprintf(stderr,"Publisher thread CREATED....");

    zclock_sleep (100);

    // simulate gateway
    fprintf(stderr,"CREATING Subscriber thread....\n");
    char *subscriberAddress = "ipc:///tmp/nginx_queue_listen";
    zactor_t *pipe = zactor_new( mock_gateway_publisher_thread, subscriberAddress);
    ck_assert_msg(pipe != NULL, "Publisher Thread should have been created. ");
    fprintf(stderr,"Subscriber thread CREATED....");

    // wait for some messages to be passed
    fprintf(stderr, "AICIIIIIIIIII");
    zclock_sleep(400);
    fprintf(stderr, "AICIIIIIIIIII DOIDDDDDDDD");
    // TODO: terminate actors
    zactor_destroy (&pipe2);
    zactor_destroy (&pipe);

    zctx_interrupted = 1;


    char s_counter[100] = "";
    int expected_min_messages = 15;
    sprintf(s_counter, "The consumer should have received at least [%d] messages, but got [%d]", expected_min_messages, messages_received_counter);
    ck_assert_msg( messages_received_counter >= expected_min_messages, s_counter);

    gw_zmq_destroy( ctx );
    ck_assert_msg(ctx == NULL, "ZMQ Context should be destroyed. ");
}
END_TEST


START_TEST(test_gateway_listener_over_abstract_socket)
{
    void *ctx = gw_zmq_init();
    ck_assert_msg(ctx != NULL, "ZMQ Context can't be null. ");

    start_gateway_listener(ctx, "ipc://@nginx_queue_listen", "tcp://127.0.0.1:6001");

    // simulate a consumer
    char *publisherAddress = "tcp://127.0.0.1:6001";
    zactor_t *pipe2 = zactor_new ( mock_subscriber_thread, publisherAddress);
    ck_assert_msg(pipe2 != NULL, "Subscriber Thread should have been created. ");

    zclock_sleep (100);

    // simulate gateway
//    char *subscriberAddress = "ipc://\\0nginx_queue_listen";
    char *subscriberAddress = "ipc://@nginx_queue_listen";
    zactor_t *pipe = zactor_new ( mock_gateway_publisher_thread, subscriberAddress);
    ck_assert_msg(pipe != NULL, "Publisher Thread should have been created. ");

    // wait for some messages to be passed
    zclock_sleep(400);
    zctx_interrupted = true;

    char s_counter[100] = "";
    int expected_min_messages = 15;
    sprintf(s_counter, "The consumer should have received at least [%d] messages, but got [%d]", expected_min_messages, messages_received_counter);
    ck_assert_msg( messages_received_counter >= expected_min_messages, s_counter);

    zactor_destroy (&pipe2);
    zactor_destroy (&pipe);
    gw_zmq_destroy( ctx );
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
    // tcase_add_test(tc_core, test_gateway_listener_over_abstract_socket);
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
