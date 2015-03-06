# NOTE: Every line in a recipe must begin with a tab character.
BUILD_DIR ?= target

LIBS ?= -lzmq -lczmq
PREFIX ?= /usr/local
INSTALL ?= install

.PHONY: all install clean

all: ;

install: all
	gcc $(LIBS) src/api-gateway-zmq-adaptor.c -o $(PREFIX)/api-gateway-zmq-adaptor

run:
	$(PREFIX)/api-gateway-zmq-adaptor

test: all
	echo "nothing to do for now"

clean:
	rm -rf target