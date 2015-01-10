# Customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

# includes and libs
INCS =
LIBS = -lz -lxcb

# flags
CPPFLAGS = -DNDEBUG -D_BSD_SOURCE -D_POSIX_C_SOURCE=2
CFLAGS   = -std=c99 -Wall -O2 -mavx -ipo ${INCS} ${CPPFLAGS}
LDFLAGS  = -s ${LIBS}

# compiler and linker
CC = icc

VERSION = 2.5.1
