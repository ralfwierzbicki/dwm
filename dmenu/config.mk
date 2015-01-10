# dmenu version
VERSION = 4.5

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# Xinerama, comment if you don't want it
XINERAMALIBS  = -lXinerama
XINERAMAFLAGS = -DXINERAMA

# Pango
PANGOINC = -I/usr/include/pango-1.0 -I/usr/include/pango-1.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/freetype2
PANGOLIBS  = -lpangoxft-1.0 -lpangoft2-1.0 -lpango-1.0 -lfontconfig -lXft

# includes and libs
INCS = -I${X11INC} ${PANGOINC}
LIBS = -L${X11LIB} -lX11 ${XINERAMALIBS} ${PANGOLIBS}

# flags
CPPFLAGS = -D_BSD_SOURCE -D_POSIX_C_SOURCE=2 -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS}
#CFLAGS   = -g -std=c99 -pedantic -Wall -O0 ${INCS} ${CPPFLAGS}
CFLAGS   = -std=c99 -pedantic -Wall -O2 -mavx -ipo ${INCS} ${CPPFLAGS}
LDFLAGS  = -s ${LIBS}

# compiler and linker
CC = icc
