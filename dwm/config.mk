# dwm version
VERSION = 6.0

# Customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# Xinerama
XINERAMALIBS = -L${X11LIB} -lXinerama
XINERAMAFLAGS = -DXINERAMA

# includes and libs
INCS = -I. -I/usr/include -I${X11INC} `pkg-config --cflags xft pango pangoxft` `pkg-config --cflags gtk+-3.0`
LIBS = -L/usr/lib -lc -lm -L${X11LIB} -lX11 ${XINERAMALIBS} `pkg-config --libs xft pango pangoxft` `pkg-config --libs gtk+-3.0` -lXcomposite -lXdamage -lXfixes -lXrender -lXext

# flags
CPPFLAGS = -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS}
#CFLAGS = -DDEBUG -D__USE_XOPEN -DSN_API_NOT_YET_FROZEN=1 -g -Wall -O0 ${INCS} ${CPPFLAGS}
CFLAGS = -DSN_API_NOT_YET_FROZEN=1 -std=c99 -O2 -pipe -mavx -fomit-frame-pointer -ipo -pedantic -Wall ${INCS} ${CPPFLAGS}
#LDFLAGS = -g ${LIBS}
LDFLAGS = -s ${LIBS} -Wl,--as-needed -Wl,-O1 -Wl,--hash-style=gnu -Wl,--sort-common ${CFLAGS}

# Solaris
#CFLAGS = -fast ${INCS} -DVERSION=\"${VERSION}\"
#LDFLAGS = ${LIBS}

# compiler and linker
CC = icc
