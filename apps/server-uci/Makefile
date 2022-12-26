#Makefile to build BACnet Application using GCC compiler

# tools - only if you need them.
# Most platforms have this already defined
# CC = gcc

# Choose a BACnet Ports Directory for the example applications target OS
# linux, win32, bsd
BACNET_PORT ?= linux

# choose a datalink to build the example applications
# Use BACDL=mstp or BACDL=bip and BBMD=server when invoking make

ifeq (${BACNET_PORT},linux)
BACDL_DEFINE += -DBACDL_ARCNET
BACNET_PORT_SRC += \
	$(BACNET_PORT_DIR)/arcnet.c
endif
ifneq (,$(filter $(BACNET_PORT),linux rx62n win32))
BACDL_DEFINE += -DBACDL_ETHERNET
BACNET_PORT_SRC += \
	$(BACNET_PORT_DIR)/ethernet.c
endif
ifneq (,$(filter $(BACNET_PORT),arduino_uno bsd linux win32 zephyr))
BACDL_DEFINE += -DBACDL_BIP
BACDL_DEFINE += -DBBMD_ENABLED=1 -DBBMD_CLIENT_ENABLED
ifeq (${BUILD},debug)
BACNET_DEFINES += -DBIP_DEBUG
endif
BACNET_PORT_SRC += \
	$(BACNET_PORT_DIR)/bip-init.c \
	$(BACNET_SRC_DIR)/bacnet/datalink/bvlc.c \
	$(BACNET_SRC_DIR)/bacnet/basic/bbmd/h_bbmd.c
endif
ifneq (,$(filter $(BACNET_PORT),bsd linux win32))
BACDL_DEFINE += -DBACDL_BIP6
BACNET_PORT_SRC += \
	$(BACNET_PORT_DIR)/bip6.c \
	$(BACNET_SRC_DIR)/bacnet/basic/bbmd6/h_bbmd6.c \
	$(BACNET_SRC_DIR)/bacnet/basic/bbmd6/vmac.c \
	$(BACNET_SRC_DIR)/bacnet/datalink/bvlc6.c
endif
ifneq (,$(filter $(BACNET_PORT),at91sam7s atmega168 bdk-atxx4-mstp linux pic18f6720 pic18f97j60 stm32f10x stm32f4xx win32 xplained))
BACDL_DEFINE += -DBACDL_MSTP
BACNET_PORT_SRC += \
	$(BACNET_PORT_DIR)/rs485.c \
	$(BACNET_PORT_DIR)/dlmstp.c \
	$(BACNET_SRC_DIR)/bacnet/datalink/mstp.c \
	$(BACNET_SRC_DIR)/bacnet/datalink/mstptext.c \
	$(BACNET_SRC_DIR)/bacnet/datalink/crc.c
endif

BACNET_PORT_SRC += \
	$(BACNET_PORT_DIR)/mstimer-init.c \
	$(BACNET_PORT_DIR)/datetime-init.c

# Define WEAK_FUNC for unsupported or specific compilers
BACNET_DEFINES =
BACNET_DEFINES += $(BACDL_DEFINE)
BACNET_DEFINES += $(BBMD_DEFINE)
BACNET_DEFINES += -DWEAK_FUNC=
BACNET_DEFINES += $(MAKE_DEFINE)

#BACNET_LIB =
# build in uci integration - use UCI=1 when invoking make
ifeq (${UCI},1)
BACNET_DEFINES += -DBAC_UCI
UCI_LIB_DIR ?= /usr/local/lib
BACNET_LIB += -L$(UCI_LIB_DIR) -luci
endif

# OS specific builds
ifeq (${BACNET_PORT},linux)
PFLAGS = -pthread
TARGET_EXT =
SYSTEM_LIB=-lc,-lgcc,-lrt,-lm
endif
ifeq (${BACNET_PORT},bsd)
PFLAGS = -pthread
TARGET_EXT =
SYSTEM_LIB=-lc,-lm
endif
ifeq (${BACNET_PORT},win32)
TARGET_EXT = .exe
SYSTEM_LIB=-lws2_32,-lgcc,-lm,-liphlpapi,-lwinmm
BACNET_DEFINES += -D_NO_OLDNAMES
endif

# source file locations
BACNET_PORT_DIR ?=  $(realpath ../../ports/$(BACNET_PORT))
BACNET_SRC_DIR ?=  $(realpath ../../src)

# Compiler flag to set the C Standard level.
# c89   - "ANSI" C - ISO C90
# gnu89 - c89 plus GNU extensions
# c90   - "ANSI" C - ISO C90
# gnu90 - C90 with GNU extensions
# c99   - ISO C99 standard
# gnu99 - C99 plus GNU extensions
# c11   - C11
# gnu11 - C11 plus GNU extensions
# c17   - C11 with corrections
# note: default is compiled as ANSI C for widest compiler compatibilty
# note: code that requires newer language features must use __STDC_VERSION__
CSTANDARD ?= -std=gnu89

#build for release (default) or debug
OPTIMIZATION ?= -Os
DEBUGGING ?=
WARNING_ALL := -Wall -Wextra -Wall -Wfloat-equal -Wconversion -Wparentheses
WARNING_ALL += -pedantic -Wunused-parameter -Wunused-variable -Wreturn-type
WARNING_ALL += -Wunused-function -Wreturn-type -Wunused-value
WARNING_ALL += -Wswitch-default -Wuninitialized -Winit-self
WARNING_ALL += -Wno-sign-conversion -Wno-conversion -Wno-sign-compare
WARNING_ALL += -Wno-long-long
#WARNING_ALL += -Wredundant-decls
#WARNING_ALL += -Werror
WARNINGS ?= $(WARNING_ALL)
# dead code removal
ifeq (${BUILD},debug)
OPTIMIZATION = -O0
DEBUGGING = -g -DDEBUG_ENABLED=1
ifeq (${BACDL_DEFINE},-DBACDL_BIP=1)
BACNET_DEFINES += -DBIP_DEBUG
endif
endif

#BACNET_DEFINES += -DPRINT_ENABLED=1
#BACNET_DEFINES += -DBACAPP_ALL
#BACNET_DEFINES += -DBACFILE
BACNET_DEFINES += -DINTRINSIC_REPORTING
#BACNET_DEFINES += -DBACNET_TIME_MASTER
BACNET_DEFINES += -DBACNET_PROPERTY_LISTS=1
BACNET_DEFINES += -DBACNET_PROTOCOL_REVISION=24

# put all the flags together
INCLUDES = -I./ -I$(BACNET_SRC_DIR) -I$(BACNET_PORT_DIR)
CFLAGS += $(WARNINGS) $(DEBUGGING) $(OPTIMIZATION) $(BACNET_DEFINES) $(INCLUDES)
LFLAGS =
ifneq (${BACNET_LIB},)
LFLAGS += -Wl,$(BACNET_LIB)
endif
# BACnet library depends on system flags.
# System flags must go after BACnet library flags.
LFLAGS += -Wl,$(SYSTEM_LIB)
# GCC dead code removal
CFLAGS += -ffunction-sections -fdata-sections
ifeq ($(shell uname -s),Darwin)
LFLAGS += -Wl,-dead_strip
else
LFLAGS += -Wl,--gc-sections
endif

# Executable file name
TARGET = bacserv-uci
# TARGET_EXT is defined in apps/Makefile as .exe or nothing
TARGET_BIN = ${TARGET}$(TARGET_EXT)

# BACnet objects that are used with this app
BACNET_OBJECT_DIR = $(BACNET_SRC_DIR)/bacnet/basic/object
SRC = main.c \
	bacnet/basic/object/device.c \
	bacnet/datalink/datalink.c \
	bacnet/datalink/dlenv.c \
	$(BACNET_OBJECT_DIR)/netport.c \
	bacnet/basic/object/ao.c \
	bacnet/basic/object/av.c \
	bacnet/basic/object/trendlog.c \
	bacnet/basic/object/nc.c

BACNET_SRC = \
	$(wildcard $(BACNET_SRC_DIR)/bacnet/*.c) \
	$(wildcard $(BACNET_SRC_DIR)/bacnet/basic/*.c) \
	$(wildcard $(BACNET_SRC_DIR)/bacnet/basic/binding/*.c) \
	$(wildcard $(BACNET_SRC_DIR)/bacnet/basic/service/*.c) \
	$(wildcard $(BACNET_SRC_DIR)/bacnet/basic/sys/*.c) \
	$(wildcard $(BACNET_SRC_DIR)/bacnet/basic/ucix/*.c) \
	$(BACNET_SRC_DIR)/bacnet/basic/npdu/h_npdu.c \
	$(BACNET_SRC_DIR)/bacnet/basic/npdu/s_router.c \
	$(BACNET_SRC_DIR)/bacnet/basic/tsm/tsm.c

SRCS = ${SRC} ${BACNET_SRC} ${BACNET_PORT_SRC}
OBJS += ${SRCS:.c=.o}

all: Makefile ${TARGET_BIN}

${TARGET_BIN}: ${OBJS} Makefile
	${CC} ${PFLAGS} ${OBJS} ${LFLAGS} -o $@
	size $@
	cp $@ ../../bin

.c.o:
	${CC} -c ${CFLAGS} $*.c -o $@

.PHONY: depend
depend:
	rm -f .depend
	${CC} -MM ${CFLAGS} *.c >> .depend

.PHONY: clean
clean:
	rm -f core ${TARGET_BIN} ${OBJS} $(TARGET).map

.PHONY: include
include: .depend

