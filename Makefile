CXX = c++
CPPFLAGS += -Iinclude -Ithird_party/cpp-httplib -Ithird_party
CXXFLAGS += -std=c++20 -Wall -Wextra -Wpedantic -pthread

ifeq ($(shell uname -s),Darwin)
# Use the SDK belonging to the selected Apple developer tools.
export SDKROOT ?= $(shell xcrun --sdk macosx --show-sdk-path)
endif

TARGET = build/glove

SOURCES = src/codec/glove_json.cpp \
          src/main.cpp \
          src/mock.cpp \
          src/cli/parser.cpp \
          src/glove/glove.cpp \
          src/glove/pair.cpp \
          src/nodes/primary.cpp \
          src/nodes/secondary.cpp \
          src/nodes/client.cpp \
          src/transport/state_codec.cpp \
          src/transport/udp/udp_envelope.cpp \
          src/transport/tcp/client.cpp \
          src/transport/tcp/server.cpp \
          src/transport/http/client.cpp \
          src/transport/http/server.cpp \
          src/transport/udp/sender.cpp \
          src/transport/udp/receiver.cpp

HEADERS = include/codec/glove_json.hpp \
          third_party/nlohmann/json.hpp \
          include/glove/glove.hpp \
          include/cli/parser.hpp \
          include/mock.hpp \
          include/glove/pair.hpp \
          include/nodes/node.hpp \
          include/nodes/network.hpp \
          include/nodes/primary.hpp \
          include/nodes/secondary.hpp \
          include/nodes/client.hpp \
          include/transport/detail/state_codec.hpp \
          include/transport/udp/detail/udp_envelope.hpp \
          include/transport/tcp/client.hpp \
          include/transport/tcp/server.hpp \
          include/transport/http/client.hpp \
          include/transport/http/server.hpp \
          third_party/cpp-httplib/httplib.h \
          include/transport/detail/socket.hpp \
          include/transport/tcp/detail/framing.hpp \
          include/transport/udp/sender.hpp \
          include/transport/udp/receiver.hpp \
          include/transport/udp/detail/socket.hpp

.DELETE_ON_ERROR:
.DEFAULT_GOAL := all
.PHONY: all clean run test-unit test-integration test

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS) Makefile
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

UNIT_TARGET = build/test-unit
UNIT_SOURCES = tests/unit/test_main.cpp \
               tests/unit/test_mock.cpp \
               tests/unit/glove/test_glove.cpp \
               tests/unit/glove/test_pair.cpp \
               tests/unit/transport/test_state_codec.cpp \
               tests/unit/transport/udp/test_udp_envelope.cpp \
               tests/unit/transport/tcp/test_framing.cpp \
               tests/unit/codec/test_glove_json.cpp \
               tests/unit/cli/test_parser.cpp
UNIT_PRODUCTION_SOURCES = $(filter-out src/main.cpp,$(SOURCES))

$(UNIT_TARGET): $(UNIT_SOURCES) tests/unit/test_support.hpp $(UNIT_PRODUCTION_SOURCES) $(HEADERS) Makefile
	mkdir -p build
	$(CXX) $(CPPFLAGS) -Itests/unit $(CXXFLAGS) $(UNIT_SOURCES) $(UNIT_PRODUCTION_SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

test-unit: $(UNIT_TARGET)
	./$(UNIT_TARGET) "$(abspath tests/fixtures)"

TRANSPORT_TEST_TARGET = build/test-transport
TRANSPORT_TEST_SOURCES = tests/integration/transport/transport_driver.cpp \
                         src/transport/tcp/client.cpp src/transport/tcp/server.cpp \
                         src/transport/udp/sender.cpp src/transport/udp/receiver.cpp \
                         src/transport/http/client.cpp src/transport/http/server.cpp

$(TRANSPORT_TEST_TARGET): $(TRANSPORT_TEST_SOURCES) $(HEADERS) Makefile
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(TRANSPORT_TEST_SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

test-integration: $(TARGET) $(TRANSPORT_TEST_TARGET)
	python3 -B -m unittest discover -s tests/integration -t tests -p 'test_*.py' -v

# Ordered recursive recipes keep the suites sequential even with make -j.
test:
	$(MAKE) test-unit
	$(MAKE) test-integration

clean:
	rm -f $(TARGET) $(UNIT_TARGET) $(TRANSPORT_TEST_TARGET)

run: all
	./$(TARGET)
