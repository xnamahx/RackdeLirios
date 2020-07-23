RACK_DIR ?= ../..

LDFLAGS += -lusb-1.0

SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += $(wildcard LICENSE*) res

include $(RACK_DIR)/plugin.mk