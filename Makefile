RACK_DIR ?= ../..

CXXFLAGS += "-I${PWD}/dep/libusb/libusb/"
LDFLAGS += "-L${PWD}/dep/libusb/libusb/.libs"

LDFLAGS += -lbcrypt -lopengl32

SOURCES = \
		$(wildcard lib/oscpack/ip/*.cpp) \
		$(wildcard lib/oscpack/osc/*.cpp) \
		$(wildcard src/*.cpp) \

include $(RACK_DIR)/arch.mk

ifeq ($(ARCH), win)
	SOURCES += $(wildcard lib/oscpack/ip/win32/*.cpp) 
	LDFLAGS += -lws2_32 -lwinmm
	LDFLAGS +=  -L$(RACK_DIR)/dep/lib 
else
	SOURCES += $(wildcard lib/oscpack/ip/posix/*.cpp) 
endif

DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)

libusb := dep/libusb/libusb/.libs/libusb-1.0.a
OBJECTS += $(libusb)

DEPS += $(libusb)

$(libusb):
	cd dep/libusb && ./autogen.sh
	cd dep/libusb && $(MAKE)

include $(RACK_DIR)/plugin.mk