#!/bin/sh
#/*
# * Copyright (c) 2015 Adobe Systems Incorporated. All rights reserved.
# *
# * Permission is hereby granted, free of charge, to any person obtaining a
# * copy of this software and associated documentation files (the "Software"),
# * to deal in the Software without restriction, including without limitation
# * the rights to use, copy, modify, merge, publish, distribute, sublicense,
# * and/or sell copies of the Software, and to permit persons to whom the
# * Software is furnished to do so, subject to the following conditions:
# *
# * The above copyright notice and this permission notice shall be included in
# * all copies or substantial portions of the Software.
# *
# * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# * DEALINGS IN THE SOFTWARE.
# *
# */
debug_mode=$(echo $DEBUG)
gateway_listen_queue=${GATEWAY_LISTEN_QUEUE:-ipc://@nginx_queue_listen}

mkdir -p /var/log/api-gateway

# ldd /usr/local/sbin/api-gateway-zmq-adaptor

echo "Starting ZeroMQ adaptor ..."
zmq_port=$(echo $ZMQ_PUBLISHER_PORT)
# use -d flag to start API Gateway ZMQ adaptor in debug mode to print all messages sent by the GW
zmq_adaptor_cmd="api-gateway-zmq-adaptor -b ${gateway_listen_queue}"
if [[ -n "${zmq_port}" ]]; then
    echo "... ZMQ will publish messages on:" ${zmq_port}
    zmq_adaptor_cmd="${zmq_adaptor_cmd} -p ${zmq_port}"
fi
if [ "${debug_mode}" == "true" ]; then
    echo "   ...  in DEBUG mode "
    zmq_adaptor_cmd="${zmq_adaptor_cmd} -d"
fi

echo "Running "$zmq_adaptor_cmd
$zmq_adaptor_cmd