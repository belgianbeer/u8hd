/*
 *  U8hd - UTF-8 Hexdecimal Dump
 *
 *    Written by Masato Minda <minmin@wide.ad.jp>
 *
 *    Created 2026-11-18  Version 1.0
 *
 *  u8hd dumps a file in the very same layout the Japanese hex dump tool
 *  jhd(1) has always used, but the character column of the dump is built
 *  for UTF-8: a multi byte character read from the file is displayed as
 *  the character itself, and a character cut in two by the end of a dump
 *  line is completed at the end of that line, where jhd completes a Kanji.
 *
 *  What the character column shows follows the locale.  A locale whose
 *  code set is UTF-8 displays the characters; the C locale, which cannot
 *  carry them, falls back to plain ASCII.  -u asks for UTF-8 whatever the
 *  locale says.  -e and -s say that the bytes of the dumped file are
 *  EUC-JP or Shift-JIS rather than UTF-8: those characters are converted
 *  to UTF-8 for the display, with libiconv(3).  The TTYPE environment
 *  variable that jhd looks at is not used here.
 */
static char rcsid[] =
	"@(#)$Id: u8hd.c,v 1.0 2026/11/11 05:20:07 minmin Exp $";

/*
 *  NORETURN - tell the compiler that a function never comes back
 */
#if  defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define  NORETURN   _Noreturn
#elif  defined(__GNUC__)
#define  NORETURN   __attribute__((__noreturn__))
#else
#define  NORETURN
#endif

#include  <sys/types.h>			/*  off_t, size_t  */
#include  <ctype.h>
#include  <errno.h>
#include  <iconv.h>			/*  the conversion of -e and -s  */
#include  <inttypes.h>			/*  intmax_t, strtoimax(), INTMAX_MAX  */
#include  <langinfo.h>			/*  nl_langinfo(), CODESET  */
#include  <locale.h>
#include  <stdint.h>			/*  uint64_t  */
#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <unistd.h>			/*  isatty(), fileno(), STDIN_FILENO  */

#define  BSIZE       65536		/*  input buffer size  */
#define  DWIDTH         16		/*  a dump line, in all modes but -c  */
#define  LWIDTH         64		/*  a dump line, in char mode  */
#define  XWIDTH        128		/*  the widest line, the -c one with -w  */
#define  MAXSEQ          6		/*  longest character we recognise  */
#define  MAXUTF         16		/*  room for the UTF-8 of one char  */
#define  GROUP           8		/*  byte between two blanks in a line  */

#define  HEXDMP          0		/*  dump in hexdecimal  */
#define  CHRDMP          1		/*  dump in charactor  */
#define  OCTDMP          2		/*  dump in octal word  */
#define  OCTBYT          3		/*  dump in octal byte  */
#define  OCTREV          4		/*  dump in octal reverse  */

/*
 *  the code sets: Incode is the one the file is written in, the display
 *  is either UTF-8 (Mbflag set) or a column that cannot show more than
 *  the 7 bit ASCII (Mbflag clear).
 */
#define  C_UTF8          1		/*  UTF-8  */
#define  C_EUC           2		/*  EUC-JP  */
#define  C_SJIS          3		/*  Shift-JIS  */

static uint8_t	Buf[BSIZE];	/*  input buffer  */
static uint8_t	Tbuf[XWIDTH];	/*  copy of the last dumped line  */
static const uint8_t *Lbuf;	/*  last Dwidth bytes dumped  */

static const char	*Prgnam = "u8hd";
static off_t		Offset;		/*  where the dump begins  */
static off_t		Stop = -1;	/*  one past the last byte, -1 for the end  */
static off_t		Addr;		/*  current address  */
static off_t		Dwidth = DWIDTH;	/*  dump width in byte  */
static int		Admode;		/*  address display mode  */
static FILE		*Fp;		/*  file pointer  */
static int		Mode = HEXDMP;	/*  dump mode  */
static int		Vflag;		/*  visual option  */
static int		Wflag;		/*  the double width option  */
static int		Putast;		/*  '*' display flag  */
static int		Incode = C_UTF8;	/*  code set of the file  */
static int		Mbflag;		/*  the display holds UTF-8  */
static uint8_t	Pend[MAXSEQ];	/*  head of a character cut by EOL  */
static int		Pendlen;	/*  bytes kept, 0 when none  */
static iconv_t		Cvt = (iconv_t)-1;	/*  Incode into UTF-8  */

/*
 *  function prototypes
 */
static void		setcodeset(void);
static void		openconv(void);
static int		charlen(const uint8_t *p, int avail);
static int		utf8len(const uint8_t *p, int avail);
static int		euclen(const uint8_t *p, int avail);
static int		sjislen(const uint8_t *p, int avail);
static int		utf8disp(const uint8_t *p, int len);
static void		setpend(const uint8_t *p, int size);
static void		putmb(const uint8_t *p, int len);
static void		putspc(int n);
static void		putoct(uint64_t value, int width);
static void		puthex(uint64_t value, int width);
static void		putaddr(void);
static int		dmpgap(int n, int cols);
static int		dmplead(int adv, int cols);
static int		chksame(const uint8_t *cur,
				    const uint8_t *old, int n);
static void		dmpmb(const uint8_t *base, int size);
static void		dmpascii(const uint8_t *base, int size);
static void		dmptext(const uint8_t *base, int size);
static void		octdmp0(const uint8_t *base, int size);
static void		octdmp1(const uint8_t *base, int size, int adr);
static void		octdmp2(const uint8_t *base, int size);
static void		hexdmp(const uint8_t *base, int size, int adr);
static int		putpartial(const uint8_t *buf, int size);
static void		dmpmode(const uint8_t *buf, int size, int adv);
static void		dmpnl(int how);
static void		dmpall(const uint8_t *buf, int size);
static void		setlast(const uint8_t *ptr);
static void		dmpmain(void);
static const char	*setname(const char *path);
static void		setwidth(void);
static void		setoption(int opt);
static void		setkanji(int code, char opt);
static NORETURN void	usage(void);
static void		usageput(FILE *fp);
static NORETURN void	helput(void);
static NORETURN void	noconv(const char *name);
static NORETURN void	toobig(void);
static NORETURN void	badend(void);
static NORETURN void	no_open(const char *name);
static void		skipread(FILE *fp);
static void		skipfile(FILE *fp);
static off_t		getnum(const char *p, const char **tail);
static void		chkofst(const char *p);
static off_t		linestart(off_t addr);
static int		parseargs(int argc, char **argv);

/*
 *  setcodeset - ask the locale what it is able to display
 *
 *  The name of the code set is reduced to its letters and digits in lower
 *  case, so that "UTF-8", "utf8" and "UTF8" all read alike.  UTF-8 in the
 *  locale turns the character column into the characters themselves; any
 *  other code set, that of the C locale above all, has no room for them
 *  and leaves the column at ASCII.  jhd asked the TTYPE environment
 *  variable which kind of terminal was in front of it; u8hd asks setlocale
 *  instead, and -u can overrule the answer.
 */
static void
setcodeset(void)
{
	const char	*cs;
	char		norm[32];
	size_t		i;
	size_t		j;

	cs = nl_langinfo(CODESET);
	if (cs == NULL) {
		cs = "";
	}
	for (i = 0, j = 0; cs[i] != '\0' && j < sizeof(norm) - 1; ++i) {
		if (isalnum((uint8_t)cs[i])) {
			norm[j++] = (char)tolower((uint8_t)cs[i]);
		}
	}
	norm[j] = '\0';
	Mbflag = strcmp(norm, "utf8") == 0;
}

/*
 *  openconv - get the code set conversion the options ask for
 *
 *  The file is read in Incode and the character column is written in
 *  UTF-8, so the two have to be joined by a conversion unless the file
 *  already is UTF-8.  A locale that displays no UTF-8 and that -u has not
 *  overruled leaves the column at ASCII, where nothing has to be
 *  converted: a Kanji of the file would only become a pair of dots.
 */
static void
openconv(void)
{
	static const char *const	eucs[] = {
	    "EUC-JP", "EUCJP", "EUC", NULL
	};
	static const char *const	sjis[] = {
	    "SHIFT_JIS", "SHIFT-JIS", "MS_Kanji", "CP932", NULL
	};
	const char *const	*names;
	int			i;

	if (!Mbflag || Incode == C_UTF8) {
		return;
	}
	names = Incode == C_EUC ? eucs : sjis;
	for (i = 0; names[i] != NULL; ++i) {
		Cvt = iconv_open("UTF-8", names[i]);
		if (Cvt != (iconv_t)-1) {
			return;
		}
	}
	noconv(names[0]);
	/*  NOTREACHED  */
}

/*
 *  Code set recognition.  Every function below answers how many bytes the
 *  character that begins at p is made of:
 *
 *	 n > 0	the character is complete and sound
 *	 n == 0	p does not begin a character
 *	 n < 0		the bytes are the head of a character whose tail
 *			lies past the avail bytes we were given
 *
 *  With a tail left to find the caller keeps the head for the next dump
 *  line, which is what jhd did with the leading half of a Kanji.
 */

/*
 *  utf8len - a character of UTF-8
 *
 *  Overlong forms, surrogates and anything past U+10FFFF are refused, so
 *  that a stream of random bytes cannot make a character out of them.
 */
static int
utf8len(const uint8_t *p, int avail)
{
	static const unsigned long	first[] = {0, 0, 0x80, 0x800, 0x10000};
	int		c = p[0];
	int		need;
	int		i;
	unsigned long	cp;

	if (c < 0x80) {
		return 1;			/*  ASCII  */
	}
	if (c < 0xC2 || c > 0xF4) {
		return 0;			/*  a tail byte, or out of range  */
	}
	need = c < 0xE0 ? 2 : (c < 0xF0 ? 3 : 4);
	for (i = 1; i < need; ++i) {
		if (i >= avail) {
			return -1;
		}
		if ((p[i] & 0xC0) != 0x80) {
			return 0;
		}
	}
	cp = (unsigned long)c & (0x1FUL >> (need - 2));
	for (i = 1; i < need; ++i) {
		cp = (cp << 6) | (unsigned long)(p[i] & 0x3F);
	}
	if (cp < first[need] || cp > 0x10FFFF) {
		return 0;
	}
	if (cp >= 0xD800 && cp <= 0xDFFF) {
		return 0;			/*  a surrogate never travels  */
	}
	return need;
}

/*
 *  euclen - a character of EUC-JP
 *
 *  Two bytes of 0xA1 to 0xFE make a Kanji, as jhd tested them; 0x8E and
 *  0x8F lead the half width katakana and JIS X 0212, which jhd left
 *  undisplayed.
 */
static int
euclen(const uint8_t *p, int avail)
{
	int	c = p[0];

	if (c < 0x80) {
		return 1;				/*  ASCII  */
	}
	if (c >= 0xA1 && c <= 0xFE) {			/*  two byte Kanji  */
		if (avail < 2) {
			return -1;
		}
		return (p[1] >= 0xA1 && p[1] <= 0xFE) ? 2 : 0;
	}
	if (c == 0x8E) {				/*  SS2: half width kana  */
		if (avail < 2) {
			return -1;
		}
		return (p[1] >= 0xA1 && p[1] <= 0xDF) ? 2 : 0;
	}
	if (c == 0x8F) {				/*  SS3: JIS X 0212  */
		if (avail < 2) {
			return -1;
		}
		if (p[1] < 0xA1 || p[1] > 0xFE) {
			return 0;
		}
		if (avail < 3) {
			return -1;
		}
		return (p[2] >= 0xA1 && p[2] <= 0xFE) ? 3 : 0;
	}
	return 0;			/*  0x80-0x8D, 0x90-0xA0, 0xFF  */
}

/*
 *  sjislen - a character of Shift-JIS
 *
 *  The ranges are the ones jhd tested.  A byte of 0xA1 to 0xDF stands for
 *  itself, as the half width katakana of Shift-JIS does.
 */
static int
sjislen(const uint8_t *p, int avail)
{
	int	c = p[0];

	if (c < 0x80) {
		return 1;				/*  ASCII  */
	}
	if (c >= 0xA1 && c <= 0xDF) {
		return 1;				/*  half width kana  */
	}
	if ((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC)) {
		if (avail < 2) {
			return -1;
		}
		if ((p[1] >= 0x40 && p[1] <= 0x7E) ||
		    (p[1] >= 0x80 && p[1] <= 0xFC)) {
			return 2;
		}
		return 0;
	}
	return 0;					/*  0x80, 0xA0, 0xFD-0xFF  */
}

/*
 *  charlen - how long is the character at p, Incode being its code set
 */
static int
charlen(const uint8_t *p, int avail)
{
	switch (Incode) {
	case C_EUC:
		return euclen(p, avail);
	case C_SJIS:
		return sjislen(p, avail);
	default:
		return utf8len(p, avail);
	}
}

/*
 *  utf8disp - can that character be drawn, or is it a control
 *
 *  The bytes are a sound UTF-8 sequence already; only their meaning is
 *  looked at.
 */
static int
utf8disp(const uint8_t *p, int len)
{
	unsigned long	cp;
	int		i;

	cp = (unsigned long)p[0] & (len == 1 ? 0x7FUL : 0x1FUL >> (len - 2));
	for (i = 1; i < len; ++i) {
		cp = (cp << 6) | (unsigned long)(p[i] & 0x3F);
	}
	return !(cp < 0x20 || (cp >= 0x7F && cp <= 0x9F));
}

/*
 *  setpend - keep the head of a character the line boundary cut
 */
static void
setpend(const uint8_t *p, int size)
{
	if (size > (int)sizeof(Pend)) {
		size = (int)sizeof(Pend);
	}
	(void)memcpy(Pend, p, (size_t)size);
	Pendlen = size;
}

/*
 *  putmb - display one character of the file
 *
 *  UTF-8 travels straight to the terminal; a character of EUC-JP or of
 *  Shift-JIS is handed to iconv(3) first.  What cannot be read as a
 *  character, or that the conversion makes no sense of, becomes a dot, as
 *  jhd shows an unprintable byte.
 */
static void
putmb(const uint8_t *p, int len)
{
	char		inbuf[MAXSEQ];
	char		outbuf[MAXUTF];
	char		*inp;
	char		*outp;
	size_t		inleft;
	size_t		outleft;

	if (len == 1 && (*p & 0x80) == 0) {
		putchar(isprint(*p) ? *p : '.');	/*  ASCII needs nobody  */
		return;
	}
	if (Incode == C_UTF8) {
		if (utf8disp(p, len)) {
			(void)fwrite((const char *)p, 1, (size_t)len, stdout);
		} else {
			putchar('.');
		}
		return;
	}
	(void)memcpy(inbuf, p, (size_t)len);
	inp = inbuf;
	inleft = (size_t)len;
	outp = outbuf;
	outleft = sizeof(outbuf);
	/*  iconv() takes the input buffer as "char **" in glibc and in the
	 *  C libraries of Mac OS X and FreeBSD, but as "const char **" on
	 *  NetBSD, which keeps to the XSI text.  What the argument points
	 *  at is a pointer into inbuf either way, so its address travels
	 *  here as a void * and suits every one of those prototypes.
	 */
	(void)iconv(Cvt, NULL, NULL, &outp, &outleft);	/*  shift state  */
	if (iconv(Cvt, (void *)&inp, &inleft, &outp, &outleft) == (size_t)-1 ||
	    inleft != 0 || outp == outbuf ||
	    !utf8disp((uint8_t *)outbuf, (int)(outp - outbuf))) {
		putchar('.');
		return;
	}
	(void)fwrite(outbuf, 1, (size_t)(outp - outbuf), stdout);
}

/*
 *  putspc - print n blanks
 */
static void
putspc(int n)
{
	while (n--) {
		putchar(' ');
	}
}

/*
 *  putoct - print the low `width' octal digits of `value'
 */
static void
putoct(uint64_t value, int width)
{
	if (width > 1) {
		putoct(value >> 3, width - 1);
	}
	putchar((int)(value & 0x7) + '0');
}

/*
 *  puthex - print the low `width' hexdecimal digits of `value'
 */
static void
puthex(uint64_t value, int width)
{
	int	c;

	if (width > 1) {
		puthex(value >> 4, width - 1);
	}
	c = (int)(value & 0xf);
	c += c <= 9 ? '0' : '7';
	putchar(c);
}

/*
 *  putaddr - print the current address, octal or hexdecimal
 */
static void
putaddr(void)
{
	if (Admode) {
		putoct((uint64_t)Addr, 11);
	} else {
		puthex((uint64_t)Addr, 8);
	}
}

/*
 *  dmpgap - the blanks that follow the data of a hexdecimal dump line
 *
 *  The bytes missing up to Dwidth spend three columns each, and so does every
 *  blank that separates two groups of eight bytes; one column is added for
 *  the blank in front of the character column.  A line that is full needs
 *  none of the separators, as the blank of its own stands where the data end.
 */
static int
dmpgap(int n, int cols)
{
	if (n >= (int)Dwidth) {
		return 1;
	}
	return ((int)Dwidth - n) * cols
	    + ((int)Dwidth - 1) / GROUP - n / GROUP + 1;
}

/*
 *  dmplead - the blanks in front of the data of a dumped line
 *
 *  adv bytes of the line belong to the character that putpartial() has just
 *  finished, or were skipped by an offset, and are shown as blanks.
 */
static int
dmplead(int adv, int cols)
{
	return adv * cols + adv / GROUP;
}

/*
 *  chksame - check n byte same data
 */
static int
chksame(const uint8_t *cur, const uint8_t *old, int n)
{
	if (old == NULL) {
		return 0;
	}
	while (n--) {
		if (*cur++ != *old++) {
			return 0;
		}
	}
	return 1;
}

/*
 *  dmpmb - the character column, drawn with the characters of the file
 *
 *  A character that would run past the end of the line is not drawn here:
 *  its head is kept by setpend() and putpartial() shows the whole of it at
 *  the end of the line it belongs to, exactly where jhd leaves a Kanji
 *  half on one line and half on the next.
 */
static void
dmpmb(const uint8_t *base, int size)
{
	int	len;

	while (size > 0) {
		len = charlen(base, size);
		if (len == 0) {
			putchar('.');			/*  not a character  */
			++base;
			--size;
		} else if (len < 0) {
			setpend(base, size);		/*  cut by the line  */
			break;
		} else {
			putmb(base, len);
			base += len;
			size -= len;
		}
	}
}

/*
 *  dmpascii - the character column of a locale that shows no UTF-8
 */
static void
dmpascii(const uint8_t *base, int size)
{
	int	c;

	while (size--) {
		c = *base++;
		putchar((c & 0x80) == 0 && isprint(c) ? c : '.');
	}
}

/*
 *  dmptext - dump the character column the locale can show
 */
static void
dmptext(const uint8_t *base, int size)
{
	putchar(' ');
	if (Mbflag) {
		dmpmb(base, size);
	} else {
		dmpascii(base, size);
	}
}

/*
 *  octdmp0 - dump octal in word
 */
static void
octdmp0(const uint8_t *base, int size)
{
	while (size > 0) {
		putchar(' ');
		putoct(((uint64_t)base[0] << 8) + base[1], 6);
		base += 2;
		size -= 2;
	}
}

/*
 *  octdmp1 - dump octal in byte
 */
static void
octdmp1(const uint8_t *base, int size, int adr)
{
	while (size--) {
		putchar(' ');
		putoct(*base++, 3);
		if (++adr % GROUP == 0 && adr < Dwidth) {
			putchar(' ');
		}
	}
}

/*
 *  octdmp2 - dump octal in reverse word
 */
static void
octdmp2(const uint8_t *base, int size)
{
	while (size > 0) {
		putchar(' ');
		putoct(((uint64_t)base[1] << 8) + base[0], 6);
		base += 2;
		size -= 2;
	}
}

/*
 *  hexdmp - dump hexdecimal
 *
 *  A blank is put between two groups of eight bytes, but not after the last
 *  byte of a full line, whose own blank to the character column follows.
 */
static void
hexdmp(const uint8_t *base, int size, int adr)
{
	while (size--) {
		putchar(' ');
		puthex(*base++, 2);
		if (++adr % GROUP == 0 && adr < Dwidth) {
			putchar(' ');
		}
	}
}

/*
 *  putpartial - finish the character that the end of the line cut
 *
 *  The head of it was kept by dmpmb(); its tail is the first bytes of the
 *  line that is dumped now.  The whole character is written before the
 *  line feed that closes the previous line, so that it stands where its
 *  head byte stands, and the number of bytes taken from this line is
 *  returned, so that dmpmode() can leave that many blanks in front of the
 *  character column.
 */
static int
putpartial(const uint8_t *buf, int size)
{
	uint8_t	tmp[MAXSEQ + 4];
	int		n = Pendlen;
	int		k;
	int		len;

	if (n == 0) {
		return 0;
	}
	Pendlen = 0;
	k = size < (int)sizeof(tmp) - n ? size : (int)sizeof(tmp) - n;
	(void)memcpy(tmp, Pend, (size_t)n);
	(void)memcpy(tmp + n, buf, (size_t)k);
	len = charlen(tmp, n + k);
	if (len <= n) {
		putchar('.');		/*  the tail did not belong to it  */
		putchar('\n');
		return 0;
	}
	putmb(tmp, len);
	putchar('\n');
	return len - n;
}

/*
 *  dmpmode - print one dumped line
 *
 *  buf	dump starts `adv' bytes before the data to be displayed, because
 *	the head of a character may sit on the previous line.
 *  size	number of bytes to display.
 *
 *  The blanks that the character column leaves in front of the data are
 *  the larger of adv and the number of bytes this line gave to the
 *  character putpartial() has just finished.
 */
static void
dmpmode(const uint8_t *buf, int size, int adv)
{
	int	n;
	int	cross;		/*  bytes this line gave to the last one  */
	int	blanks;		/*  slots left empty in front of the column  */
	int	lead;		/*  of them, bytes that belong to this line  */

	Putast = 0;
	n = size + adv;				/*  bytes of this line  */
	cross = putpartial(buf, n);
	blanks = cross > adv ? cross : adv;
	lead = blanks - adv;
	putaddr();
	putchar(' ');
	buf += adv;
	switch (Mode) {
	case HEXDMP:				/*  hex dump with char column  */
		putspc(dmplead(adv, 3));
		hexdmp(buf, size, adv);
		putspc(dmpgap(n, 3) + blanks);
		dmptext(buf + lead, size - lead);
		break;

	case CHRDMP:				/*  char dump only  */
		putspc(blanks);
		dmptext(buf + lead, size - lead);
		break;

	case OCTDMP:				/*  octal dump in word  */
		putspc((adv / 2) * 7);
		octdmp0(buf, size);
		break;

	case OCTBYT:				/*  octal dump in byte  */
		putspc(dmplead(adv, 4));
		octdmp1(buf, size, adv);
		break;

	case OCTREV:				/*  octal dump in reverse word  */
		putspc(adv * 7);
		octdmp2(buf, size);
		break;
	}
}

/*
 *  end of line, with the two flavours a character cut in two may ask for
 */
#define	LINEEND		0		/*  close the line, dots if pending  */
#define	LINECONT	1		/*  the character crosses to next line  */
#define	LINESKIP	2		/*  repeated data, nothing displayed  */

/*
 *  dmpnl - terminate a dumped line
 */
static void
dmpnl(int how)
{
	int	pend = Pendlen;

	if (how == LINECONT) {
		if (pend == 0) {
			putchar('\n');		/*  the line ends right here  */
		}
		return;		/*  else putpartial() ends the open line  */
	}
	if (how == LINESKIP && pend == 0) {
		return;				/*  the '*' follows at once  */
	}
	while (pend-- > 0) {
		putchar('.');		/*  one dot for the byte left behind  */
	}
	putchar('\n');
	Pendlen = 0;
}

/*
 *  dmpall - dump a block that was read in
 */
static void
dmpall(const uint8_t *buf, int size)
{
	int	first = (int)(Offset - Addr);	/*  bytes to skip, 1st line  */
	int	width = (int)Dwidth;

	if (size < width) {
		if (size < first) {
			/*
			 *  The offset sits past the end of the file, so that
			 *  nothing of this line is left to show.  The line is
			 *  dumped empty, as it is for an offset that lands on
			 *  the end of the file.
			 */
			first = size;
		}
		dmpmode(buf, size - first, first);
		dmpnl(LINEEND);
		Addr += size;
		return;
	}
	if (Addr != Offset) {
		dmpmode(buf, width - first, first);
		dmpnl(LINECONT);
		Addr += width;
		buf += width;
		size -= width;
	}
	while (size >= width) {
		if (!Vflag && chksame(buf, Lbuf, width)) {
			if (!Putast) {
				dmpnl(LINESKIP);
				putchar('*');
				putchar('\n');
				Putast = 1;
			}
		} else {
			dmpmode(buf, width, 0);
			dmpnl(LINECONT);
		}
		Lbuf = buf;
		Addr += width;
		buf += width;
		size -= width;
	}
	if (size != 0) {
		dmpmode(buf, size, 0);
		Addr += size;
		dmpnl(LINEEND);
	}
	Offset = Addr;
}

static void
setlast(const uint8_t *ptr)
{
	int	n;
	uint8_t	*bp;

	n = (int)Dwidth;
	Lbuf = bp = Tbuf;
	while (n--) {
		*bp++ = *ptr++;
	}
}

/*
 *  dump file
 */
static void
dmpmain(void)
{
	size_t	size;
	size_t	want;

	Addr = linestart(Offset);
	for (;;) {
		want = sizeof(Buf);
		if (Stop >= 0) {
			if (Addr >= Stop) {
				break;			/*  the range is spent  */
			}
			if ((off_t)want > Stop - Addr) {
				want = (size_t)(Stop - Addr);
			}
		}
		size = fread(Buf, 1, want, Fp);
		if (size == 0) {
			break;
		}
		dmpall(Buf, (int)size);
		if (size >= (size_t)Dwidth) {
			setlast(Buf + size - (size_t)Dwidth);
		}
	}
	if (Putast) {
		--Addr;
		putaddr();
		putchar('\n');
	}
	if (Pendlen != 0) {		/*  the very last line of all  */
		dmpnl(LINEEND);
	}
}

/*
 *  setname - return the tail name of a path
 */
static const char *
setname(const char *path)
{
	const char	*pt;

	while ((pt = strchr(path, '/')) != NULL) {
		path = pt + 1;
	}
	return path;
}

/*
 *  setwidth - the length of a dumped line
 *
 *  A line is sixteen bytes long, or sixty four in the char mode, and -w
 *  doubles both of them for a terminal wide enough to hold the line.  The
 *  width stays a power of two, which linestart() counts on.
 */
static void
setwidth(void)
{
	if (Mode == CHRDMP) {
		Dwidth = Wflag ? 2 * LWIDTH : LWIDTH;
	} else {
		Dwidth = Wflag ? 2 * DWIDTH : DWIDTH;
	}
}

/*
 *  setoption - apply one option letter of the command line
 */
static void
setoption(int opt)
{
	switch (opt) {
	case 'b':
		Mode = OCTBYT;
		setwidth();
		break;

	case 'c':
		Mode = CHRDMP;
		setwidth();
		break;

	case 'o':
		Mode = OCTDMP;
		setwidth();
		break;

	case 'r':
		Mode = OCTREV;
		setwidth();
		break;

	case 'w':
		Wflag = 1;		/*  twice as many bytes to a line  */
		setwidth();
		break;

	case 'a':
		Mbflag = 0;		/*  the ASCII column, nothing else  */
		break;

	case 'e':
		setkanji(C_EUC, 'e');
		break;

	case 's':
		setkanji(C_SJIS, 's');
		break;

	case 'u':
		Mbflag = 1;		/*  UTF-8 whatever the locale says  */
		break;

	case 'h':
		helput();
		/*  NOTREACHED  */

	case 'v':
		Vflag = 1;
		break;

	case 'O':
		Admode = 1;
		break;

	default:
		usage();
	}
}

/*
 *  setkanji - the file is written in that Kanji code
 *
 *  -e and -s cannot live together: one byte of the file cannot be a
 *  character of two code sets at once.
 */
static void
setkanji(int code, char opt)
{
	if (Incode == code) {
		return;
	}
	if (Incode != C_UTF8) {
		fputs(Prgnam, stderr);
		fputs(": -e and -s cannot be used together (-", stderr);
		fputc(opt, stderr);
		fputs(" follows -", stderr);
		fputc(Incode == C_EUC ? 'e' : 's', stderr);
		fputs(").\n", stderr);
		usage();
	}
	Incode = code;
}

/*
 *  usageput - the way to run this program, on the stream asked for
 */
static void
usageput(FILE *fp)
{
	fputs("Usage: ", fp);
	fputs(Prgnam, fp);
	fputs(" [ -abcehOrsuvw ] [ <file> ] ", fp);
	fputs("[ +<start>[-<length>|-+<end> ]]\n", fp);
}

/*
 *  usage - complain about the way this program was called
 */
static NORETURN void
usage(void)
{
	usageput(stderr);
	exit(EXIT_FAILURE);
}

/*
 *  helput - answer -h: the usage, a line about every option, the
 *  numbers an offset takes, and the id
 *
 *  What every option does is what the English part of the README says
 *  about it, in short.  The line about the numbers stands for the [k] of
 *  the synopsis, which is left out of the usage to keep it to a line.
 *  The last line is the id string that CVS expands, taken from its fifth
 *  byte so that the "@(#)" of it stays out of the display; it stands for
 *  the version, which needs no separate option.
 */
#define puttabstrlf(s)	{putchar('\t'); fputs(s, stdout); putchar('\n');}

static NORETURN void
helput(void)
{
	static const char *const line[] = {
		"-a:  ASCII only, overrules the locale",
		"-b:  the data as octal bytes",
		"-c:  the characters only, 64 bytes to a line",
		"-e:  read the file as EUC-JP",
		"-h:  this help",
		"-o:  the data as octal words, big-endian",
		"-r:  the data as octal words, little-endian",
		"-s:  read the file as Shift-JIS",
		"-u:  show UTF-8 whatever the locale says",
		"-v:  do not fold repeated lines",
		"-w:  double the width of a line, 32 bytes, or 128 with -c",
		"-O:  the address in octal",
	};
	unsigned int	i;

	usageput(stdout);
	for (i = 0; i < sizeof(line) / sizeof(line[0]); ++i) {
		puttabstrlf(line[i]);
	}
	puttabstrlf("number:  x or 0x for hexdecimal, o for octal,"
	    " k times 1024");		/*  what +start, -length and -+end take  */
	puttabstrlf(rcsid + 4);			/*  4:  past the "@(#)"  */
	exit(EXIT_SUCCESS);
}

/*
 *  noconv - libiconv(3) cannot join the code set of the file to UTF-8
 */
static NORETURN void
noconv(const char *name)
{
	fputs(Prgnam, stderr);
	fputs(": ", stderr);
	fputs(name, stderr);
	fputs(" to UTF-8: iconv cannot convert.\n", stderr);
	exit(EXIT_FAILURE);
}

/*
 *  toobig - the offset reaches beyond the data
 */
static NORETURN void
toobig(void)
{
	fputs(Prgnam, stderr);
	fputs(": Offset too large!\n", stderr);
	exit(EXIT_FAILURE);
}

/*
 *  badend - the end of the range falls behind its start
 */
static NORETURN void
badend(void)
{
	fputs(Prgnam, stderr);
	fputs(": The end of the range is behind its start!\n", stderr);
	exit(EXIT_FAILURE);
}

static NORETURN void
no_open(const char *name)
{
	fputs(Prgnam, stderr);
	fputs(": ", stderr);
	fputs(name, stderr);
	fputs(": can't open.\n", stderr);
	exit(EXIT_FAILURE);
}

/*
 *  skipread - walk the stream up to the offset, reading it
 *
 *  Used for input that cannot be seeked (a terminal or a pipe); the data
 *  in front of the offset is read into the buffer and dropped.
 */
static void
skipread(FILE *fp)
{
	off_t	offset;

	if (Offset == 0) {
		return;
	}
	offset = linestart(Offset);
	while (offset > (off_t)sizeof(Buf)) {
		if (fread(Buf, 1, sizeof(Buf), fp) != sizeof(Buf)) {
			toobig();
		}
		offset -= (off_t)sizeof(Buf);
	}
	if (fread(Buf, 1, (size_t)offset, fp) != (size_t)offset) {
		toobig();
	}
}

/*
 *  skipfile - seek the file to the offset
 */
static void
skipfile(FILE *fp)
{
	if (fseeko(fp, linestart(Offset), SEEK_SET) != 0) {
		toobig();
	}
}

/*
 *  linestart - the address of the dump line that holds addr
 *
 *  Dwidth is a power of two, so this rounds down without a division.
 */
static off_t
linestart(off_t addr)
{
	return addr - (addr & (Dwidth - 1));
}

/*
 *  getnum - read a number, and say where it stopped
 *
 *  x/X in front, or 0x in front, selects hexdecimal; o/O or a leading 0
 *  selects octal; decimal else.  A trailing k multiplies what came before it
 *  by 1024.  A field that holds no number at all reads as zero, which is what
 *  "+-100" needs.
 */
static off_t
getnum(const char *p, const char **tail)
{
	char		*end;
	intmax_t	value;
	int		basesize = 10;

	if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
		basesize = 16;			/*  0x20 is 32, as everywhere  */
		p += 2;
	} else if (*p == 'x' || *p == 'X') {
		basesize = 16;
		++p;
	} else if (*p == 'o' || *p == 'O') {
		basesize = 8;
		++p;
	} else if (*p == '0') {
		basesize = 8;
	}
	errno = 0;
	value = strtoimax(p, &end, basesize);
	if (end == p) {
		value = 0;			/*  nothing in front of the dash  */
	}
	if (errno == ERANGE || value < 0 || value > INTMAX_MAX >> 10) {
		toobig();
	}
	if (*end == 'K' || *end == 'k') {
		value <<= 10;			/*  0k is no bytes at all  */
		++end;
	}
	*tail = end;
	return (off_t)value;
}

/*
 *  chkofst - decode "+..." into Offset and Stop
 *
 *  "+start" is the offset u8hd always took: the dump runs from there to the
 *  end of the file.  A dash behind the number narrows the dump, and the way
 *  the dash is written says how.  "+start-length" shows length bytes, and
 *  "+start-+end" shows everything from start to end, that last byte being a
 *  part of the dump.  Both numbers take the x, the o and the k of start, and
 *  start itself may be left out, which means the beginning of the file:
 *  "+-length" and "+-+end".
 */
static void
chkofst(const char *p)
{
	const char	*tail;
	off_t		value;
	off_t		limit;

	Offset = 0;
	if (*p != '-') {
		Offset = getnum(p, &tail);
	} else {
		tail = p;
	}
	if (*tail == '\0') {
		return;				/*  the plain offset of old  */
	}
	if (*tail != '-') {
		usage();
	}
	if (*++tail == '+') {
		value = getnum(tail + 1, &tail);	/*  the last byte to show  */
		if (*tail != '\0') {
			usage();
		}
		if (value < Offset) {
			badend();		/*  an end behind its start  */
		}
		limit = value + 1;
	} else {
		value = getnum(tail, &tail);	/*  the bytes to show  */
		if (*tail != '\0') {
			usage();
		}
		if (Offset > (INTMAX_MAX >> 10) - value) {
			toobig();
		}
		limit = Offset + value;
	}
	Stop = limit;
}

/*
 *  parseargs - pick the options, the offset and the file out of argv.
 *
 *  Options are every argument up to the first one that does not begin
 *  with '-' (a lone "-" means the standard input), and they may be
 *  clustered, as in "-euO".  The offset is "+start[k]", with a range behind
 *  it as "+start[k]-length[k]" or "+start[k]-+end[k]", and it may be given
 *  either before or after the file name.
 *
 *  Returns the index of the file to dump, or 0 when the standard input has
 *  to be used.  Usage is displayed when there is nothing to dump and the
 *  standard input is a terminal.
 */
static int
parseargs(int argc, char **argv)
{
	int	i;
	int	file;

	for (i = 1; i < argc; ++i) {
		const char	*p;

		if (argv[i][0] != '-' || argv[i][1] == '\0') {
			break;
		}
		for (p = argv[i] + 1; *p != '\0'; ++p) {
			setoption(*p);
		}
	}
	file = i;
	if (i < argc && argv[i][0] == '+') {
		chkofst(argv[i] + 1);
		file = i + 1;
	} else if (i + 1 < argc && argv[i + 1][0] == '+') {
		chkofst(argv[i + 1] + 1);
	}
	if (file >= argc) {
		if (isatty(STDIN_FILENO)) {
			usage();
		}
		return 0;
	}
	return argv[file][0] == '-' ? 0 : file;
}

int
main(int argc, char **argv)
{
	int	file;

	Prgnam = setname(argv[0]);
	(void)setlocale(LC_CTYPE, "");	/*  ask the locale before anything  */
	setcodeset();
	file = parseargs(argc, argv);
	openconv();
	if (file == 0) {			/*  the standard input  */
		Fp = stdin;
		skipread(Fp);
		dmpmain();
		goto done;
	}
	Fp = fopen(argv[file], "r");
	if (Fp == NULL) {
		no_open(argv[file]);
	}
	if (file + 1 < argc && argv[file + 1][0] == '+') {
		chkofst(argv[file + 1] + 1);
	}
	if (isatty(fileno(Fp))) {
		skipread(Fp);
	} else {
		skipfile(Fp);
	}
	dmpmain();
	fclose(Fp);
done:
	if (Cvt != (iconv_t)-1) {
		(void)iconv_close(Cvt);
	}
	return 0;
}
