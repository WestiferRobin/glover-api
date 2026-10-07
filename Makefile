CXX = c++
CPPFLAGS += -Iinclude
CXXFLAGS += -std=c++20 -Wall -Wextra -Wpedantic

ifeq ($(shell uname -s),Darwin)
# Use the SDK belonging to the selected Apple developer tools.
export SDKROOT ?= $(shell xcrun --sdk macosx --show-sdk-path)
endif

TARGET = build/glove

SOURCES = src/main.cpp \
          src/glove/glove.cpp \
          src/glove/pair.cpp

HEADERS = include/glove/glove.hpp \
          include/glove/pair.hpp

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS) Makefile
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -f $(TARGET)

run: all
	./$(TARGET)
