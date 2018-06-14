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
    printf("Starting Gateway publisher thread [%s] ... \n", (char*)args);

    void *publisher = zsocket_new (ctx, ZMQ_PUB);
    int socket_bound = zsocket_connect (publisher, "%s", (char*)args);
    assert( socket_bound == 0 );

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
    printf("Starting Test Debug subscriber thread [%s] ... \n", (char*)args);
    messages_received_counter = 0;

    void *subscriber = zsocket_new (ctx, ZMQ_SUB);
    assert(subscriber);
    zsocket_connect (subscriber, "%s", (char*)args);
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
    zctx_interrupted = false;
    ck_assert_msg(ctx != NULL, "ZMQ Context can't be null. ");
    char *publisherAddress = "tcp://127.0.0.1:6001";
    char *subscriberAddress = "ipc:///tmp/nginx_queue_listen";

    start_gateway_listener(ctx, subscriberAddress, publisherAddress, 0);

    // simulate a consumer
    void *pipe2 = zthread_fork (ctx, mock_subscriber_thread, publisherAddress);
    ck_assert_msg(pipe2 != NULL, "Subscriber Thread should have been created. ");

    zclock_sleep (100);

    // simulate gateway
    void *pipe = zthread_fork (ctx, mock_gateway_publisher_thread, subscriberAddress);
    ck_assert_msg(pipe != NULL, "Publisher Thread should have been created. ");

    // wait for some messages to be passed
    zclock_sleep(400);
    zctx_interrupted = true;

    char s_counter[100] = "";
    int expected_min_messages = 15;
    sprintf(s_counter, "The consumer should have received at least [%d] messages, but got [%d]", expected_min_messages, messages_received_counter);
    ck_assert_msg( messages_received_counter >= expected_min_messages, s_counter);

    gw_zmq_destroy( &ctx );
    ck_assert_msg(ctx == NULL, "ZMQ Context should be destroyed. ");
}
END_TEST


START_TEST(test_gateway_listener_over_abstract_socket)
{
    zctx_t *ctx = gw_zmq_init();
    zctx_interrupted = false;
    ck_assert_msg(ctx != NULL, "ZMQ Context can't be null. ");
    char *subscriberAddress = "ipc://@nginx_queue_listen";
    char *publisherAddress = "tcp://127.0.0.1:6001";

    start_gateway_listener(ctx, subscriberAddress, publisherAddress, 0);

    // simulate a consumer
    void *pipe2 = zthread_fork (ctx, mock_subscriber_thread, publisherAddress);
    ck_assert_msg(pipe2 != NULL, "Subscriber Thread should have been created. ");

    zclock_sleep (100);

    // simulate gateway
    void *pipe = zthread_fork (ctx, mock_gateway_publisher_thread, subscriberAddress);
    ck_assert_msg(pipe != NULL, "Publisher Thread should have been created. ");

    // wait for some messages to be passed
    zclock_sleep(400);
    zctx_interrupted = true;

    char s_counter[100] = "";
    int expected_min_messages = 15;
    sprintf(s_counter, "The consumer should have received at least [%d] messages, but got [%d]", expected_min_messages, messages_received_counter);
    ck_assert_msg( messages_received_counter >= expected_min_messages, s_counter);

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
    tcase_add_test(tc_core, test_gateway_listener_over_abstract_socket);
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
