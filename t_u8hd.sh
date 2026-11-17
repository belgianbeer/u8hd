#!/bin/sh
#
#  t_u8hd.sh - a few checks for u8hd
#
#  Every check dumps a file made here with printf(1) or iconv(1) and
#  compares the whole of the dump with the answer the layout of jhd calls
#  for:
#
#	1.	data without a byte above 0x80, where u8hd has to agree with
#		jhd byte for byte,
#	2.	a character that the end of the line cuts in two,
#	3.	the same text in UTF-8, in EUC-JP and in Shift-JIS, which has
#		to give the same character column,
#	4.	what the locale, -u and -a decide,
#	5.	the options that cannot go together,
#	6.	the character that the end of the file leaves in two,
#	7.	-w, which doubles the width of a dumped line,
#	8.	the two bytes that ASCII and the Japanese code sets read
#		apart,
#	9.	the range: where the dump begins and where it leaves off,
#	10.	-h, the help the program prints of itself.
#
#  Run it with "make test", or by hand: sh ./t_u8hd.sh
#
#  It is written for the POSIX sh, because that is what /bin/sh is on the
#  systems u8hd is meant to build on.  Two things are worth knowing:
#
#	oct(printf) escapes build the test data; the hexadecimal \x ones are
#	not in POSIX, and dash -- the /bin/sh of Debian -- copies them to the
#	output unparsed, which is how this script came to fail there.
#
#	the locale that shows characters is found by asking u8hd itself over
#	the list of locale -a, since not every system has one named C.UTF-8.
#

U8HD=./u8hd
TMP=${TMPDIR:-/tmp}/u8hd.$$.d

if [ ! -x "$U8HD" ]; then
	echo "# build u8hd first (make)" >&2
	exit 1
fi

mkdir -p "$TMP" || exit 1
trap 'rm -rf "$TMP"' EXIT

#  The checks that look at characters need a locale whose code set is
#  UTF-8.  Which one that is differs from system to system -- Linux has
#  C.UTF-8, the BSDs often only en_US.UTF-8 or ja_JP.UTF-8, and the name is
#  written "utf8" as well as "UTF-8" -- so the list of `locale -a` is walked
#  and each candidate is tried.  u8hd itself answers the question: in a
#  locale that displays UTF-8 a dump of "日本" is the dump that -u forces,
#  and in one that cannot display it, it is not.
printf '\346\227\245\346\234\254' > "$TMP/probe"
env LC_ALL=C $U8HD -u "$TMP/probe" > "$TMP/probe.u" 2>/dev/null
for loc in $(locale -a 2>/dev/null | grep -i 'utf[-_]8$') C.UTF-8 en_US.UTF-8
do
	env LC_ALL=$loc $U8HD "$TMP/probe" > "$TMP/probe.g" 2>/dev/null || continue
	if cmp -s "$TMP/probe.u" "$TMP/probe.g"; then
		LOCALE=$loc
		break
	fi
done
if [ -z "$LOCALE" ]; then
	echo "# no UTF-8 locale to check with" >&2
	exit 1
fi
export LOCALE

count=0
fails=0

#  check NAME FILE1 FILE2 - compare two dumps
check()
{
	name=$1
	count=$((count + 1))
	if cmp -s "$2" "$3"; then
		echo "ok $count - $name"
	else
		echo "not ok $count - $name"
		echo "# --- expected"
		sed -e 's/^/#     /' "$2"
		echo "# --- got"
		sed -e 's/^/#     /' "$3"
		fails=$((fails + 1))
	fi
}

#  checkit NAME EXPECTED COMMAND... - run u8hd and compare what it prints
checkit()
{
	name=$1
	expected=$2
	shift 2
	printf '%s\n' "$expected" > "$TMP/e"
	if ! "$@" > "$TMP/g" 2>"$TMP/err"; then
		count=$((count + 1))
		echo "not ok $count - $name (u8hd did not run)"
		sed -e 's/^/#     /' "$TMP/err"
		fails=$((fails + 1))
		return
	fi
	check "$name" "$TMP/e" "$TMP/g"
}

#
#  1.  ASCII and binary only: the layout of jhd, down to the last blank
#
printf 'Hello, world!\n' > "$TMP/ascii"
checkit "ascii" \
'00000000  48 65 6C 6C 6F 2C 20 77  6F 72 6C 64 21 0A        Hello, world!.' \
	env LC_ALL=$LOCALE $U8HD "$TMP/ascii"

printf 'abcdefghijklmnop' > "$TMP/wide"
checkit "eight bytes, from the middle of a line" \
'00000000                           69 6A 6B 6C 6D 6E 6F 70          ijklmnop' \
	env LC_ALL=$LOCALE $U8HD "$TMP/wide" +8

dd if=/dev/zero of="$TMP/zero" bs=48 count=1 2>/dev/null
checkit "repeated lines are skipped" \
'00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................
*
0000002F' \
	env LC_ALL=$LOCALE $U8HD "$TMP/zero"

printf 'abcdefghijklmnop' > "$TMP/one"
checkit "octal in byte" \
'00000000  141 142 143 144 145 146 147 150  151 152 153 154 155 156 157 160' \
	env LC_ALL=$LOCALE $U8HD -b "$TMP/one"

#
#  2.  UTF-8, and a character the line boundary cuts in two
#
#  "日" is three bytes; it begins at byte 15 of the first line, so the
#  first line ends with the whole of it and the second leaves two blanks
#  where its two tail bytes are.
#
printf '123456789012345\346\227\24567890123456789012345\n' > "$TMP/u8"
checkit "utf-8 across the line" \
'00000000  31 32 33 34 35 36 37 38  39 30 31 32 33 34 35 E6  123456789012345日
00000010  97 A5 36 37 38 39 30 31  32 33 34 35 36 37 38 39    67890123456789
00000020  30 31 32 33 34 35 0A                              012345.' \
	env LC_ALL=$LOCALE $U8HD "$TMP/u8"

#  the head of a character that the end of the file leaves alone is a dot
printf 'abc\346\227' > "$TMP/cut"
checkit "utf-8 cut by the end of the file" \
'00000000  61 62 63 E6 97                                    abc..' \
	env LC_ALL=$LOCALE $U8HD "$TMP/cut"

#  bytes that make no character at all
printf 'a\377\376\200b' > "$TMP/bad"
checkit "no character at all" \
'00000000  61 FF FE 80 62                                    a...b' \
	env LC_ALL=$LOCALE $U8HD "$TMP/bad"
#
#  3.  the same text in the three code sets gives the same characters
#
printf 'Hello, \346\227\245\346\234\254\350\252\236\343\203\206\343\202\271\343\203\210\n' \
	> "$TMP/text.u8"
HAVEEUC=0
HAVESJIS=0
if iconv -f UTF-8 -t EUC-JP "$TMP/text.u8" > "$TMP/text.euc" 2>/dev/null; then
	HAVEEUC=1
else
	echo "# iconv cannot write EUC-JP; those checks are skipped"
fi
if iconv -f UTF-8 -t SHIFT_JIS "$TMP/text.u8" > "$TMP/text.sjis" 2>/dev/null; then
	HAVESJIS=1
else
	echo "# iconv cannot write Shift-JIS; those checks are skipped"
fi

checkit "utf-8" \
'00000000  48 65 6C 6C 6F 2C 20 E6  97 A5 E6 9C AC E8 AA 9E  Hello, 日本語
00000010  E3 83 86 E3 82 B9 E3 83  88 0A                    テスト.' \
	env LC_ALL=$LOCALE $U8HD "$TMP/text.u8"

if [ $HAVEEUC -eq 1 ]; then
	checkit "euc-jp converted to utf-8" \
'00000000  48 65 6C 6C 6F 2C 20 C6  FC CB DC B8 EC A5 C6 A5  Hello, 日本語テス
00000010  B9 A5 C8 0A                                        ト.' \
		env LC_ALL=$LOCALE $U8HD -e "$TMP/text.euc"
fi
if [ $HAVESJIS -eq 1 ]; then
	checkit "shift-jis converted to utf-8" \
'00000000  48 65 6C 6C 6F 2C 20 93  FA 96 7B 8C EA 83 65 83  Hello, 日本語テス
00000010  58 83 67 0A                                        ト.' \
		env LC_ALL=$LOCALE $U8HD -s "$TMP/text.sjis"
fi

#  the characters of the three dumps have to be the same, the blanks that
#  a character across the line leaves behind included: only the hexdecimal
#  column knows in which code set the file was written
if [ $HAVEEUC -eq 1 ] && [ $HAVESJIS -eq 1 ]; then
	for code in u8 euc sjis; do
		case $code in
		u8)	opts= ;;
		euc)	opts=-e ;;
		sjis)	opts=-s ;;
		esac
		env LC_ALL=$LOCALE $U8HD $opts "$TMP/text.$code" \
			| cut -c 61- | tr -d ' \n' > "$TMP/x.$code"
	done
	check "the same characters, EUC-JP and UTF-8" "$TMP/x.u8" "$TMP/x.euc"
	check "the same characters, Shift-JIS and UTF-8" "$TMP/x.u8" "$TMP/x.sjis"
fi
#  a character of one byte only: the half width katakana of Shift-JIS, and
#  the SS2 pair of EUC-JP
if [ $HAVESJIS -eq 1 ]; then
	printf '\261\262\263' > "$TMP/kana.sjis"
	checkit "shift-jis half width katakana" \
'00000000  B1 B2 B3                                          ｱｲｳ' \
		env LC_ALL=$LOCALE $U8HD -s "$TMP/kana.sjis"
fi
if [ $HAVEEUC -eq 1 ]; then
	printf '\216\261\216\262\216\263' > "$TMP/kana.euc"
	checkit "euc-jp half width katakana" \
'00000000  8E B1 8E B2 8E B3                                 ｱｲｳ' \
		env LC_ALL=$LOCALE $U8HD -e "$TMP/kana.euc"
fi



#
#  4.  what the locale decides
#
printf '\346\227\245\346\234\254\350\252\236' > "$TMP/3kanji"
checkit "the C locale shows ASCII only" \
'00000000  E6 97 A5 E6 9C AC E8 AA  9E                       .........' \
	env LC_ALL=C $U8HD "$TMP/3kanji"

checkit "-u forces UTF-8 in the C locale" \
'00000000  E6 97 A5 E6 9C AC E8 AA  9E                       日本語' \
	env LC_ALL=C $U8HD -u "$TMP/3kanji"

checkit "-a keeps the C locale company in a UTF-8 one" \
'00000000  E6 97 A5 E6 9C AC E8 AA  9E                       .........' \
	env LC_ALL=$LOCALE $U8HD -a "$TMP/3kanji"

if [ $HAVEEUC -eq 1 ]; then
	printf '\306\374\313\334\270\354' > "$TMP/h.euc"
	checkit "-eu converts EUC-JP in the C locale" \
'00000000  C6 FC CB DC B8 EC                                 日本語' \
		env LC_ALL=C $U8HD -eu "$TMP/h.euc"
fi
if [ $HAVESJIS -eq 1 ]; then
	printf '\223\372\226\173\214\352' > "$TMP/h.sjis"
	checkit "-su converts Shift-JIS in the C locale" \
'00000000  93 FA 96 7B 8C EA                                 日本語' \
		env LC_ALL=C $U8HD -su "$TMP/h.sjis"
fi

#
#  5.  the options that cannot go together
#
count=$((count + 1))
if env LC_ALL=$LOCALE $U8HD -j "$TMP/ascii" > /dev/null 2>&1; then
	echo "not ok $count - the -j of jhd is gone"
	fails=$((fails + 1))
else
	echo "ok $count - the -j of jhd is gone"
fi
if [ $HAVEEUC -eq 1 ] && [ $HAVESJIS -eq 1 ]; then
	count=$((count + 1))
	if env LC_ALL=$LOCALE $U8HD -es "$TMP/ascii" > /dev/null 2>&1; then
		echo "not ok $count - -e and -s are not to be mixed"
		fails=$((fails + 1))
	else
		echo "ok $count - -e and -s are not to be mixed"
	fi
fi

#
#  6.  the character that the end of the file leaves in two
#
#  If the last byte of the file is the head of a multi byte character, and
#  that byte falls on the end of a dump line as well -- which is what an
#  exact multiple of Dwidth does --, there is no next line left to finish
#  the character on.  jhd up to 3.0 ended its output there without a line
#  feed and left the last line open; jhd 3.1 fixed that, and u8hd shows a
#  dot for every byte left over and closes the line the same way.
#
if [ $HAVEEUC -eq 1 ]; then
	printf 'Hello, \244\242\244\244\244\246\244\250\306' > "$TMP/dangle.euc"
	checkit "an EUC-JP kanji head left by the end of the file" \
'00000000  48 65 6C 6C 6F 2C 20 A4  A2 A4 A4 A4 A6 A4 A8 C6  Hello, あいうえ.' \
		env LC_ALL=$LOCALE $U8HD -e "$TMP/dangle.euc"
fi

printf '12345678901234\346\227' > "$TMP/dangle.u8"
checkit "two bytes of a UTF-8 character left by the end" \
'00000000  31 32 33 34 35 36 37 38  39 30 31 32 33 34 E6 97  12345678901234..' \
	env LC_ALL=$LOCALE $U8HD "$TMP/dangle.u8"

#
#  7.  -w, the double width
#
#  Twice as many bytes to a line: thirty two, or a hundred twenty eight with
#  -c.  The groups of eight bytes keep the blank that separates them, which
#  makes four columns of eight in the hexdecimal dump, and the character
#  column starts at column 111 of the line -- the same place on a line that
#  is full and on one that is not.
#
printf 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcd' > "$TMP/wide40"
checkit "forty bytes of ascii, thirty two to a line" \
'00000000  41 42 43 44 45 46 47 48  49 4A 4B 4C 4D 4E 4F 50  51 52 53 54 55 56 57 58  59 5A 30 31 32 33 34 35  ABCDEFGHIJKLMNOPQRSTUVWXYZ012345
00000020  36 37 38 39 61 62 63 64                                                                             6789abcd' \
	env LC_ALL=C $U8HD -w "$TMP/wide40"

dd if=/dev/zero of="$TMP/zero32" bs=32 count=1 2>/dev/null
checkit "one full line of zeros" \
'00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................................' \
	env LC_ALL=C $U8HD -w "$TMP/zero32"

dd if=/dev/zero of="$TMP/zero130" bs=130 count=1 2>/dev/null
checkit "a hundred thirty zeros, in char mode" \
'00000000  ................................................................................................................................
00000080  ..' \
	env LC_ALL=C $U8HD -c -w "$TMP/zero130"

dd if=/dev/zero of="$TMP/zero2240" bs=2240 count=1 2>/dev/null
checkit "seventy lines the same, thirty two bytes to a line" \
'00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................................
*
000008BF' \
	env LC_ALL=C $U8HD -w "$TMP/zero2240"

#  The octal dumps take the width as well; the -b one keeps the blank every
#  eight bytes, and -o and -r spend six columns on a word and no blank.
checkit "the -b dump, thirty two bytes to a line" \
'00000000  101 102 103 104 105 106 107 110  111 112 113 114 115 116 117 120  121 122 123 124 125 126 127 130  131 132 060 061 062 063 064 065
00000020  066 067 070 071 141 142 143 144 ' \
	env LC_ALL=C $U8HD -b -w "$TMP/wide40"
checkit "the -o dump, thirty two bytes to a line" \
'00000000  040502 041504 042506 043510 044512 045514 046516 047520 050522 051524 052526 053530 054532 030061 031063 032065
00000020  033067 034071 060542 061544' \
	env LC_ALL=C $U8HD -o -w "$TMP/wide40"

#  A character may cross the end of a line of thirty two bytes as well; it
#  is shown where its head stands, and the line below leaves its two bytes
#  of tail blank.  -u is there to make the check independent of the locale.
printf '1234567890123456789012345678901\346\227\245' > "$TMP/cross34"
checkit "a UTF-8 character crossing the end of a wide line" \
'00000000  31 32 33 34 35 36 37 38  39 30 31 32 33 34 35 36  37 38 39 30 31 32 33 34  35 36 37 38 39 30 31 E6  1234567890123456789012345678901日
00000020  97 A5                                                                                                 ' \
	env LC_ALL=C $U8HD -u -w "$TMP/cross34"

#
#  8.  the two bytes that ASCII and the Japanese code sets read apart
#
#  JIS X 0201 gives 0x5C to the yen sign and 0x7E to the overline, and a
#  terminal built for that shows those; other terminals show a backslash and
#  a tilde, which is what jhd printed as it came.  u8hd writes UTF-8, and a
#  byte below 0x80 travels as the ASCII it is in every code set: a backslash
#  and a tilde.  That does not ask the iconv of the system either, which
#  keeps the dump the same on every platform.
#
printf '\250\134\261' > "$TMP/yen.sjis"
checkit "0x5C of a Shift-JIS file" \
'00000000  A8 5C B1                                          ｨ\ｱ' \
	env LC_ALL=C $U8HD -s -u "$TMP/yen.sjis"

printf '\244\242\134\261' > "$TMP/yen.euc"
checkit "0x5C of a EUC-JP file" \
'00000000  A4 A2 5C B1                                       あ\.' \
	env LC_ALL=C $U8HD -e -u "$TMP/yen.euc"

#
#  9.  the range: where the dump begins and where it leaves off
#
#  "+start-length" shows length bytes and "+start-+end" shows everything up
#  to and including the byte at end; leaving start away begins at the top of
#  the file.  The two ways of writing the same range must print the same
#  thing, which the checks below compare, and an end behind its start is an
#  error rather than a silent nothing.
#
printf 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ\n\n' > "$TMP/range"

checkit "twenty bytes from the beginning" \
'00000000  41 42 43 44 45 46 47 48  49 4A 4B 4C 4D 4E 4F 50  ABCDEFGHIJKLMNOP
00000010  51 52 53 54                                       QRST' \
	env LC_ALL=C $U8HD +-20 "$TMP/range"

checkit "a range that runs into the end" \
'00000030                           55 56 57 58 59 5A 0A 0A          UVWXYZ..' \
	env LC_ALL=C $U8HD +56-16 "$TMP/range"

#  the same bytes, the two ways of asking for them
$U8HD "$TMP/range" "+-+19" > "$TMP/g9a" 2>/dev/null
$U8HD "$TMP/range" "+-20" > "$TMP/g9b" 2>/dev/null
check "+-+19 is the twenty bytes of +-20" "$TMP/g9b" "$TMP/g9a"

$U8HD "$TMP/range" "+32-+47" > "$TMP/g9a" 2>/dev/null
$U8HD "$TMP/range" "+32-16" > "$TMP/g9b" 2>/dev/null
check "+32-+47 is the sixteen bytes of +32-16" "$TMP/g9b" "$TMP/g9a"

checkit "sixteen bytes from byte 32" \
'00000020  36 37 38 39 41 42 43 44  45 46 47 48 49 4A 4B 4C  6789ABCDEFGHIJKL' \
	env LC_ALL=C $U8HD +32-16 "$TMP/range"

checkit "the char dump, sixty bytes of it" \
'00000000  ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWX' \
	env LC_ALL=C $U8HD -c "+-60" "$TMP/range"

#  an end behind its start is said out loud
count=$((count + 1))
if env LC_ALL=C $U8HD "$TMP/range" "+8-+7" > /dev/null 2>&1; then
	echo "not ok $count - the end behind the start"
	fails=$((fails + 1))
else
	echo "ok $count - the end behind the start"
fi

#
#  10.  -h, the help
#
#  The help goes to the standard output, and comes back with 0: the usage
#  first, then one line for every option, indented with a tab, then the id
#  string that stands for the version.
#
count=$((count + 1))
if [ "$($U8HD -h | wc -l | tr -d ' ')" -eq 15 ]; then
	echo "ok $count - the help has a line for every option"
else
	echo "not ok $count - the help has a line for every option"
	$U8HD -h | sed -e 's/^/#     /'
	fails=$((fails + 1))
fi

#  the two lines it begins with are the usage an error prints
count=$((count + 1))
env LC_ALL=C $U8HD -q > "$TMP/void" 2> "$TMP/usage"
$U8HD -h | head -2 > "$TMP/help2"
if cmp -s "$TMP/help2" "$TMP/usage"; then
	echo "ok $count - the help begins with the usage"
else
	echo "not ok $count - the help begins with the usage"
	echo "# --- expected"
	sed -e 's/^/#     /' "$TMP/usage"
	echo "# --- got"
	sed -e 's/^/#     /' "$TMP/help2"
	fails=$((fails + 1))
fi

#  the last line is the id string, from its fifth byte
count=$((count + 1))
tab=$(printf '\t')
if $U8HD -h | tail -1 | grep "^${tab}"'\$Id: u8hd\.c,v ' >/dev/null; then
	echo "ok $count - the help ends with the id string"
else
	echo "not ok $count - the help ends with the id string"
	$U8HD -h | tail -1 | sed -e 's/^/#     /'
	fails=$((fails + 1))
fi

#  nothing of the help goes to the error stream, nor does a file matter
count=$((count + 1))
if $U8HD -h /there/is/no/such > "$TMP/void" 2> "$TMP/err" &&
   [ ! -s "$TMP/err" ]; then
	echo "ok $count - the help stays off the error stream"
else
	echo "not ok $count - the help stays off the error stream"
	sed -e 's/^/#     /' "$TMP/err"
	fails=$((fails + 1))
fi

echo "1..$count"
[ $fails -eq 0 ] || exit 1
exit 0

