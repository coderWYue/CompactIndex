#   BSD LICENSE
#
#   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
# version type
VERSION = debug
#VERSION = release or debug
# build commands
CC = gcc
CPP = g++
LINK = g++
RM := rm -fr
INSTALL := cp -fr
# build options
ENABLE_MYSQL = Y

TARGET := libtdms.so

BASE_INC_DIR := $(CURDIR)/inc
BASE_SRC_DIR := $(CURDIR)/src
BASE_LIB_DIR := $(CURDIR)/lib

LIB_INSTALL_INC := /usr/include
LIB_INSTALL_LIB1 := /usr/local/lib64
LIB_INSTALL_LIB2 := /usr/local/lib
LIB_INSTALL_LIB3 := /usr/lib64

WARNINGS := -Wall -Winline -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-label

INC_FLAGS := -I$(BASE_INC_DIR) -I/usr/include -I/usr/include/mysql

SO_FLAGS := -fPIC -shared -llz4 -lpthread

CX_FLAGS := -mssse3 -D_XOPEN_SOURCE

OBJS := $(patsubst $(BASE_SRC_DIR)/%.c,$(BASE_SRC_DIR)/%.o,$(wildcard $(BASE_SRC_DIR)/*.c)) 
OBJS += $(patsubst $(BASE_SRC_DIR)/%.cpp,$(BASE_SRC_DIR)/%.o,$(wildcard $(BASE_SRC_DIR)/*.cpp)) 

# Version-specific Flags
ifeq ($(VERSION), debug)
CX_FLAGS += -ggdb -O0  -rdynamic
else
CX_FLAGS += -Os
endif

ifeq ($(ENABLE_MYSQL), Y)
SO_FLAGS += -lmysqlclient
INC_FLAGS += -I/usr/local/mysql/include 
endif


# Execute script to make release.h.
# NOTE: colon equal (:=) must be used!
release_hdr := $(shell sh -c './inc/mkreleasehdr.sh')

.PHONY: all clean install

all:$(TARGET)
$(TARGET):$(OBJS) 
	$(LINK) $(CX_FLAGS) $(SO_FLAGS) $(WARNINGS) $(INC_FLAGS) -o $(BASE_LIB_DIR)/$@ $^
%.o: %.c
	$(CC) -c -std=c99 $(CX_FLAGS) $(SO_FLAGS) $< $(INC_FLAGS) -o $@
%.o: %.cpp
	$(CPP) -c -std=c++11 $(CX_FLAGS) $(SO_FLAGS) $< $(INC_FLAGS) -o $@


clean:
	-$(RM) $(BASE_LIB_DIR)/*
	-$(RM) $(BASE_SRC_DIR)/*.o
	
.PHONY: clean

distclean: clean
	rm -f ./inc/release.h 

.PHONY: distclean

install:
	$(INSTALL) $(BASE_INC_DIR)/* $(LIB_INSTALL_INC)
	$(INSTALL) $(BASE_LIB_DIR)/* $(LIB_INSTALL_LIB1)
	$(INSTALL) $(BASE_LIB_DIR)/* $(LIB_INSTALL_LIB2)
	$(INSTALL) $(BASE_LIB_DIR)/* $(LIB_INSTALL_LIB3)
	


