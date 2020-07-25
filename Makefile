RACK_DIR ?= ../..

CXXFLAGS += "-I${PWD}/dep/libusb/libusb/"
LDFLAGS += "-L${PWD}/dep/libusb/libusb/.libs"

LDFLAGS += -lbcrypt -lopengl32

SOURCES += $(wildcard src/*.cpp)

DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)

libusb := dep/libusb/libusb/.libs/libusb-1.0.a
OBJECTS += $(libusb)

DEPS += $(libusb)

$(libusb):
	cd dep/libusb && ./autogen.sh
	cd dep/libusb && $(MAKE)

include $(RACK_DIR)/plugin.mk