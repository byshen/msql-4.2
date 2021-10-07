#
# Site specific configuration
#


COMPILER= wllvm 
INST_DIR= /opt/msql
HAVE_DYNAMIC= -DHAVE_DYNAMIC
CC_ONLY_FLAGS=
LINK_ONLY_FLAGS= -rdynamic


#
# Things below here shouldn't need to be changed
#
CC= $(COMPILER) $(CC_ONLY_FLAGS)
LINK= $(COMPILER) $(LINK_ONLY_FLAGS)
CPP= wllvm -E
RANLIB= ranlib
AR= ar
LN_S= ln -s
BASE_SOURCE_DIR= $(TOP)
TARGET= Linux-4.15.0-76-generic-x86_64
YACC= bison
PIC= -fPIC


# Extra libraries if required
EXTRA_LIB= -ldl -lnsl 

# Any other CFlags required
EXTRA_CFLAGS= 

# Directory for pid file
PID_DIR= 

CFLAGS= -g -O0 -I$(TOP)/ $(EXTRA_CFLAGS) $(LOCAL_EXTRA_CFLAGS) -D$(OS_TYPE)
LDLIBS= $(EXTRA_LIB)
