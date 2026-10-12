CXX = c++
CPPFLAGS += -Iinclude
CXXFLAGS += -std=c++20 -Wall -Wextra -Wpedantic

ifeq ($(shell uname -s),Darwin)
# Use the SDK belonging to the selected Apple developer tools.
export SDKROOT ?= $(shell xcrun --sdk macosx --show-sdk-path)
endif

TARGET = build/glove

# Shared native implementation/settings; packaging belongs to each target.
BLE_NATIVE_SOURCES = src/transport/ble/platform/macos/session.mm \
                     src/transport/ble/platform/macos/central.mm \
                     src/transport/ble/platform/macos/peripheral.mm
BLE_CXXFLAGS = -fobjc-arc -mmacosx-version-min=10.15
BLE_LDFLAGS = -framework Foundation -framework CoreBluetooth

SOURCES = src/main.cpp \
          src/mock.cpp \
          src/nodes/ble.cpp \
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
          include/transport/ble/detail/ble_message.hpp \
          include/transport/ble/config.hpp \
          include/transport/ble/experiment.hpp \
          include/transport/ble/central.hpp \
          include/transport/ble/peripheral.hpp \
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
BLE ?= 0
ifeq ($(BLE),1)
ifneq ($(shell uname -s),Darwin)
$(error BLE=1 requires macOS)
endif
SOURCES += $(BLE_NATIVE_SOURCES)
HEADERS += include/transport/ble/platform/macos/session.hpp config/macos/glove-Info.plist
NODE_CPPFLAGS = -DGLOVER_ENABLE_BLE
NODE_CXXFLAGS = $(BLE_CXXFLAGS)
NODE_LDFLAGS = $(BLE_LDFLAGS) -Wl,-sectcreate,__TEXT,__info_plist,config/macos/glove-Info.plist
endif

.PHONY: all clean run ble-node FORCE
ble-node:
	$(MAKE) BLE=1 all

# Rebuild on mode switches even with Make versions using whole-second mtimes.
ifneq ($(shell cat build/.glove-mode 2>/dev/null),$(BLE))
.PHONY: $(TARGET)
endif

# Rebuild the same executable when switching optional platform support.
build/.glove-mode: FORCE
	@mkdir -p build
	@if ! test -f $@ || test "$$(cat $@)" != "$(BLE)"; then printf '%s\n' "$(BLE)" > $@; fi

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS) Makefile build/.glove-mode
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(NODE_CPPFLAGS) $(CXXFLAGS) $(NODE_CXXFLAGS) $(SOURCES) $(LDFLAGS) $(NODE_LDFLAGS) $(LDLIBS) -o $@
ifeq ($(BLE),1)
	codesign --force --sign - $@
endif

clean:
	rm -f $(TARGET)

run: all
	./$(TARGET)

# Optional macOS-only connectivity experiment. Never part of all/run.
BLE_BUNDLE = build/ble-feasibility.app
BLE_TARGET = $(BLE_BUNDLE)/Contents/MacOS/ble-feasibility
BLE_SOURCES = src/cli/ble_feasibility.cpp $(BLE_NATIVE_SOURCES)
BLE_HEADERS = include/transport/ble/config.hpp \
              include/transport/ble/central.hpp \
              include/transport/ble/peripheral.hpp \
              include/transport/ble/experiment.hpp \
              include/transport/ble/platform/macos/session.hpp

.PHONY: ble-feasibility
ble-feasibility: $(BLE_TARGET)

$(BLE_TARGET): $(BLE_SOURCES) $(BLE_HEADERS) examples/ble_feasibility/Info.plist Makefile
	@test "$$(uname -s)" = Darwin || { echo "BLE experiment requires macOS" >&2; exit 1; }
	mkdir -p $(BLE_BUNDLE)/Contents/MacOS
	cp examples/ble_feasibility/Info.plist $(BLE_BUNDLE)/Contents/Info.plist
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(BLE_CXXFLAGS) $(BLE_SOURCES) $(LDFLAGS) $(BLE_LDFLAGS) $(LDLIBS) -o $@
	codesign --force --sign - $(BLE_BUNDLE)
