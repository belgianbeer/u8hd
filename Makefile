#
#  Makefile for u8hd - UTF-8 Hexdecimal Dump
#
#  u8hd is a POSIX program (isatty(), fileno(), fseeko()) written in C99
#  (long long, strtoll()), and it needs iconv(3) for the -e and -s options.
#  _XOPEN_SOURCE is what asks for iconv(): it is not in POSIX but in the XSI
#  extensions, and NetBSD hides the prototype when only _POSIX_C_SOURCE is
#  defined.  Linux (glibc), FreeBSD and NetBSD already have iconv() in the C
#  library, so the link rule below tries the plain link first and only
#  reaches for $(ICONVLIBS) when that fails, which is what Mac OS X needs.
#  GNU libiconv installed under its own name can be picked with ICONVCFLAGS
#  and ICONVLIBS.
#

CC	?= cc
RM	?= rm -f
CFLAGS	?= -O2
CPPFLAGS += -D_XOPEN_SOURCE=700
WARNFLAGS = -Wall -Wextra -Wstrict-prototypes -Wold-style-definition \
	    -Wmissing-prototypes -Wmissing-declarations
PROG	= u8hd
OBJS	= u8hd.o

ICONVCFLAGS	=
ICONVLIBS	?= -liconv

#  The English manual page goes to man1/ and the Japanese one to
#  $(JAMANSEC)/man1/, both under the name u8hd.1, so that man finds the
#  page of the locale it is running in.  In the distribution the Japanese
#  page keeps its own name, u8hd_ja.1.
#
PREFIX	= /usr/local
MANDIR	= $(PREFIX)/man
JAMANSEC = ja

all: $(PROG)

$(PROG): $(OBJS)
	@if $(CC) $(CFLAGS) $(LDFLAGS) -o $(PROG) $(OBJS) > /dev/null 2>&1; \
	then \
		echo $(CC) $(CFLAGS) $(LDFLAGS) -o $(PROG) $(OBJS) ; \
	else \
		echo $(CC) $(CFLAGS) $(LDFLAGS) -o $(PROG) $(OBJS) $(ICONVLIBS); \
		$(CC) $(CFLAGS) $(LDFLAGS) -o $(PROG) $(OBJS) $(ICONVLIBS); \
	fi

$(OBJS): u8hd.c
	$(CC) $(CPPFLAGS) $(ICONVCFLAGS) $(CFLAGS) $(WARNFLAGS) -c u8hd.c

#  syntax check, then the regression tests
check:
	$(CC) $(CPPFLAGS) $(ICONVCFLAGS) $(WARNFLAGS) -fsyntax-only u8hd.c

test: $(PROG)
	sh ./t_u8hd.sh

install: install-prog install-man

install-prog: $(PROG)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(PROG) $(DESTDIR)$(PREFIX)/bin/$(PROG)

install-man:
	install -d $(DESTDIR)$(MANDIR)/man1
	install -m 644 u8hd.1 $(DESTDIR)$(MANDIR)/man1/u8hd.1

install-jman:
	install -d $(DESTDIR)$(MANDIR)/$(JAMANSEC)/man1
	install -m 644 u8hd_ja.1 $(DESTDIR)$(MANDIR)/$(JAMANSEC)/man1/u8hd.1

clean:
	$(RM) $(PROG) $(OBJS)

.PHONY: all check test install clean
