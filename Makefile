# NOTE: Every line in a recipe must begin with a tab character.
BUILD_DIR ?= target

LIBS ?= -lzmq -lczmq
PREFIX ?= /usr/local
INSTALL ?= install

CPPUTEST_HOME ?= /usr/local/Cellar/cpputest/3.7.2/
CPPFLAGS += -I$(CPPUTEST_HOME)/include
CXXFLAGS += -include $(CPPUTEST_HOME)/include/CppUTest/MemoryLeakDetectorNewMacros.h
CFLAGS += -include $(CPPUTEST_HOME)/include/CppUTest/MemoryLeakDetectorMallocMacros.h
LD_LIBRARIES = -L$(CPPUTEST_HOME)/lib -lCppUTest -lCppUTestExt

.PHONY: all install clean

all: ;

process-resources:
	rm -rf $(BUILD_DIR)/*
	mkdir -p $(BUILD_DIR)/test_classes
	mkdir -p $(BUILD_DIR)/classes

install: process-resources
	gcc -c src/GwZmqAdaptor.c -o $(BUILD_DIR)/classes/GwZmqAdaptor.o -lpthread
	gcc $(BUILD_DIR)/classes/GwZmqAdaptor.o src/api-gateway-zmq-adaptor.c -o $(BUILD_DIR)/api-gateway-zmq-adaptor -lpthread  $(LIBS)
	cp $(BUILD_DIR)/api-gateway-zmq-adaptor $(PREFIX)/api-gateway-zmq-adaptor

run:
	$(PREFIX)/api-gateway-zmq-adaptor

test: process-resources
	gcc -c tests/test_published_messages.c -o $(BUILD_DIR)/test_classes/test_published_messages.o -Wall -Werror
	gcc -c src/GwZmqAdaptor.c -o $(BUILD_DIR)/classes/GwZmqAdaptor.o -Wall -Werror
	gcc $(BUILD_DIR)/classes/GwZmqAdaptor.o  $(BUILD_DIR)/test_classes/test_published_messages.o -o $(BUILD_DIR)/check_test_runner -lcheck $(LIBS) -Wall -Werror
	$(BUILD_DIR)/check_test_runner

test-cpp : all
	#gcc -lcheck -o quick_check -c ./tests/test_published_messages.c
	#gcc
	#quick_check
	#g++ $(LD_LIBRARIES) $(CPPFLAGS) -o quick_test ./tests/quick_test.cpp

clean:
	rm -rf target

docker:
	docker build -t adobeapiplatform/apigateway-zmq-adaptor .

.PHONY: docker-ssh
docker-ssh:
	docker run -ti --entrypoint='bash' adobeapiplatform/apigateway:latest

.PHONY: docker-run
docker-run: process-resources
	docker run --rm --name="apigateway-zmq-adaptor" --ipc="host" --net="host"  \
			--cpuset-cpus="2-2" \
			-p 6001:6001 \
			-e "DEBUG=false" -e "GATEWAY_LISTEN_QUEUE=ipc://@nginx_queue_listen" \
			adobeapiplatform/apigateway-zmq-adaptor:latest

.PHONY: docker-debug
docker-debug: process-resources
	docker run --rm --name="apigateway-zmq-adaptor" --ipc="host" --net="host"  \
			--cpuset-cpus="2-3" \
			-p 6001:6001 \
			-e "DEBUG=true" -e "GATEWAY_LISTEN_QUEUE=ipc://@nginx_queue_listen" \
			adobeapiplatform/apigateway-zmq-adaptor:latest

.PHONY: docker-attach
docker-attach:
	docker exec -i -t apigateway-zmq-adaptor bash

.PHONY: docker-stop
docker-stop:
	docker stop apigateway-zmq-adaptor
	docker rm apigateway-zmq-adaptor
