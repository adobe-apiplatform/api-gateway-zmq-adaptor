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

void *
gw_zmq_init()
{
    //  Set the context for the child threads
    return zmq_ctx_new ();
}

int
gw_zmq_destroy( void *ctx )
{
    //  Tell attached threads to exit
    return zmq_ctx_term(ctx);
}

/*

Espresso Pattern impl
@see http://zguide.zeromq.org/page:all#header-116
--------------------------------------
   XPUB           ->       XSUB
  public Addr     ->    internal Addr
   BIND           ->       BIND
---------------------------------------

This method starts a new thread subscribing to the messages sent by the Gateway on XSUB
and proxying them to a local IP address on XPUB . Remote consumers should connect to the XPUB's socket address.

*/

void
start_gateway_listener(void *ctx, char *subscriberAddress, char *publisherAddress)
{
    fprintf(stderr,"Starting Gateway Listener \n");

    void *subscriber = zmq_socket (ctx, ZMQ_XSUB);
    int subscriberSocketResult = zmq_bind (subscriber, subscriberAddress);
    assert( subscriberSocketResult == 0 );

    // Start XPUB Proxy -> remote consumers connect here
    void *publisher = zmq_socket (ctx, ZMQ_XPUB);
    zmq_setsockopt (publisher, ZMQ_XPUB_VERBOSE, "1", 1);
    int publisherBindResult = zmq_bind (publisher, publisherAddress);
    assert( publisherBindResult == 0 );

    fprintf(stderr, "Starting XPUB->XSUB Proxy [%s] -> [%s] \n", subscriberAddress, publisherAddress);
    // TODO: use actor
    int fork_result = fork ();

    zmq_proxy( subscriber, publisher, NULL);
}
