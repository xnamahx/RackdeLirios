RACK_DIR ?= ../..

CXXFLAGS += "-I${PWD}/dep/libusb/libusb/"
LDFLAGS += "-L${PWD}/dep/libusb/libusb/.libs"

LDFLAGS += -lusb-1.0
LDFLAGS += -lbcrypt -lopengl32

SOURCES += $(wildcard src/*.cpp)

DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)

include $(RACK_DIR)/plugin.mk