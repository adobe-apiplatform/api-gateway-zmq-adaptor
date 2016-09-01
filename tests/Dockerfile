# apigateway-zmq-adaptor-tests
#
# VERSION               0.1.0
#
# From https://hub.docker.com/_/alpine/
#
FROM adobeapiplatform/apigateway-zmq-adaptor:latest

# --- DEV ONLY ---
RUN apk update \
    && apk add \
           gcc tar libtool zlib make musl-dev openssl-dev g++ zlib-dev curl \
    && apk add check-dev
COPY src /tmp/api-gateway-zmq-adaptor-${ZMQ_ADAPTOR_VERSION}/src
COPY tests /tmp/api-gateway-zmq-adaptor-${ZMQ_ADAPTOR_VERSION}/tests
COPY Makefile /tmp/api-gateway-zmq-adaptor-${ZMQ_ADAPTOR_VERSION}/Makefile
RUN cd /tmp/api-gateway-zmq-adaptor-* \
         && make test \
         && mkdir -p /usr/local/sbin \
         && PREFIX=/usr/local/sbin make install \
         && rm -rf /tmp/api-gateway-zmq-adaptor-*
# ------------------

ENTRYPOINT ["/etc/init-container.sh"]
