CXX = c++
CPPFLAGS += -Iinclude
CXXFLAGS += -std=c++20 -Wall -Wextra -Wpedantic

ifeq ($(shell uname -s),Darwin)
# Use the SDK belonging to the selected Apple developer tools.
export SDKROOT ?= $(shell xcrun --sdk macosx --show-sdk-path)
endif

TARGET = build/glove

SOURCES = src/main.cpp \
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
          src/transport/udp/sender.cpp \
          src/transport/udp/receiver.cpp

HEADERS = include/glove/glove.hpp \
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
          include/transport/detail/socket.hpp \
          include/transport/tcp/detail/framing.hpp \
          include/transport/udp/sender.hpp \
          include/transport/udp/receiver.hpp \
          include/transport/udp/detail/socket.hpp

.DELETE_ON_ERROR:
.DEFAULT_GOAL := all
.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS) Makefile
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -f $(TARGET)

run: all
	./$(TARGET)
