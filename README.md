# u8hd — UTF-8 Hex Dump Utility

```sh
$ cat hello
Hello ¡Hola! こんにちは 안녕하세요 你好
$ ls -l hello
-rw-r--r--  1 minmin minmin 53 Nov 14 11:35 hello
$ u8hd hello
00000000  48 65 6C 6C 6F 20 C2 A1  48 6F 6C 61 21 20 E3 81  Hello ¡Hola! こ
00000010  93 E3 82 93 E3 81 AB E3  81 A1 E3 81 AF 20 EC 95   んにちは 안
00000020  88 EB 85 95 ED 95 98 EC  84 B8 EC 9A 94 20 E4 BD   녕하세요 你
00000030  A0 E5 A5 BD 0A                                     好.
$ u8hd -c hello
00000000  Hello ¡Hola! こんにちは 안녕하세요 你好.
$ cat hello | u8hd +0x18
00000010                           81 A1 E3 81 AF 20 EC 95          ..は 안
00000020  88 EB 85 95 ED 95 98 EC  84 B8 EC 9A 94 20 E4 BD   녕하세요 你
00000030  A0 E5 A5 BD 0A                                     好.
```

[日本語README](#概要)

## Overview

`u8hd` is a hex dump tool that displays file contents as hexadecimal
bytes alongside their UTF-8 text representation.

Traditional hex dump tools stop at 0x7f and replace non-ASCII bytes with dots.
`u8hd` decodes UTF-8 and shows the actual characters, making it possible to see
both the original bytes and their text representation at the same time.

- 16 bytes to a line (64 with `-c`), the address in 8 hexadecimal digits
  (`-O` for 11 octal ones), and `-w` to double the width of a line: 32 bytes,
  or 128 with `-c`, for a terminal that has the room
- a character that the end of a line cuts in two is drawn whole at the end of
  the line it begins in, and the line below leaves that many blanks (see below)
- lines that repeat are folded into a `*` (`-v` to see them all)
- a file in an old Japanese code set is read as it is and shown in UTF-8:
  `-e` for EUC-JP, `-s` for Shift-JIS, converted with **libiconv(3)**
- what the terminal can display is asked from the **locale**, not from an
  environment variable of its own; `-u` overrules the answer

`u8hd` is a UTF-8 successor to [`jhd`](https://github.com/belgianbeer/jhd),
a Japanese-aware hex dump utility first released in 1986.

The shape of the dump comes from jhd, which made the same choice for EUC-JP
and Shift-JIS. u8hd keeps that layout and moves its character column to
UTF-8; for data that stays below 0x80 the two programs print the same thing.

The example below shows how u8hd handles a UTF-8 character that crosses a line boundary.

The `日` whose bytes are `E6 97 A5` begins at byte 15, so the
line boundary cuts it in two: it is drawn whole at the end of the line it
begins in, and the next line leaves two blanks before its own characters.

```sh
$ u8hd sample
00000000  31 32 33 34 35 36 37 38  39 30 31 32 33 34 35 E6  123456789012345日
00000010  97 A5 39 30 31 32 33 34  35 0A                      9012345.
```

## Code sets

| condition | what the character column shows |
| --------- | ------------------------------- |
| the locale code set is UTF-8 | the UTF-8 characters of the file |
| `C`, or any locale that cannot display UTF-8 | ASCII only, bytes above 0x80 as `.` |
| `-u` | UTF-8 whatever the locale says |
| `-e` | the file is read as **EUC-JP** and converted to UTF-8 |
| `-s` | the file is read as **Shift-JIS** and converted to UTF-8 |

-*libiconv(3)** does the conversion, and nothing is converted when both sides
are UTF-8 — no iconv descriptor is even opened for a UTF-8 file.  `-e` and
`-s` cannot be used together: one byte of a file cannot belong to two code
sets at once.  `-eu` and `-su` read EUC-JP and Shift-JIS and write UTF-8 no
matter what the locale says.

## Options

```text
u8hd [ -abcehOrsuvw ] [ <file> ] [ +start[k][-length[k]|-+end[k]] ]
```

| | |
| -- | -- |
| `-a` | ASCII only (overrules the locale, and a `-u` written before it) |
| `-b` | the data as octal bytes |
| `-c` | the characters only, 64 bytes to a line |
| `-e` | read the file as EUC-JP |
| `-h` | show the help, which ends with the version |
| `-o` | the data as octal words, big-endian |
| `-r` | the data as octal words, little-endian |
| `-s` | read the file as Shift-JIS |
| `-u` | show UTF-8 whatever the locale says |
| `-v` | do not fold repeated lines |
| `-w` | double the width of a line: 32 bytes, or 128 with `-c` |
| `-O` | the address in octal |

Options may follow one dash, so `-eu` is the same as `-e -u`, and of two
options that speak about the same thing the one written last wins.

An argument that begins with `+` says where in the file to begin: in decimal,
with an `x` — or `0x` — in hexadecimal, with an `o` or a leading `0` in octal,
and with a `k` multiplied by 1024.  It may stand before or after the file name.
A dash behind it narrows the dump to a range: `+start-length` shows `length`
bytes, and `+start-+end` shows everything up to and including the byte at
`end`.  The start may be left out, which means the beginning of the file.

```text
+1000-20         twenty bytes, from byte 1000
+1000-+1019      the same twenty, told by their places
+-+511           the first 512 bytes of the file
+0x40-0x20       the 32 bytes from 0x40 to 0x5f
+1k-512          the 512 bytes behind the first kilobyte
```

```sh
$ u8hd hello +16-+23
00000010  93 E3 82 93 E3 81 AB E3                           .んに.
```

## Differences from jhd

The layout follows `jhd`: the digits of the address, the columns of
hexadecimal, the place of the character column, the `*` that folds lines.
Where the data is ASCII only, the checks of `t_u8hd.sh` compare the output
with what `jhd` itself prints.

| | jhd | u8hd |
| -- | ----- | ------ |
| the code set of the terminal | the `TTYPE` environment variable | the locale (`-u` to overrule it) |
| the code set of the file | chosen when it is compiled, `-DEUC=0/1` | on the command line: `-e`, `-s`, UTF-8 by default |
| the characters shown | as the terminal receives them | UTF-8, converted with libiconv |
| `-j` | forces the kanji display | not needed, `-u` does that |
| `-k` | half width katakana as well | `-e` and `-s` show them without an option |
| the length of a character | 1 or 2 bytes | 1 to 4 (UTF-8), 3 for the SS3 of EUC-JP |
| the width of a line | 16 or 64 bytes, fixed | the same, and `-w` for 32 or 128 |

## History

| date | version | |
| ---- | ------- | - |
| 2026-11-18 | v1.0 | Show UTF-8, based on 'jhd' |

## Contents

- `u8hd.c` — the program, ANSI C / POSIX, using libiconv
- `u8hd.1` — the manual page in English, `u8hd_ja.1` — the same in Japanese
- `Makefile` — build and install (`PREFIX`, `DESTDIR`, `JAMANSEC` overridable)
- `t_u8hd.sh` — 44 regression checks in tap format
- `LICENSE` / `README.md`

## Building

```sh
make                            # builds ./u8hd
make check                      # a syntax check with -Wall -Wextra
make test                       # the regression tests (44 checks)
sudo make install               # into /usr/local; PREFIX and DESTDIR are overridable
man -l ./u8hd.1                 # the English page (BSD man; macOS has no -l)
man -l ./u8hd_ja.1              # the Japanese page
mandoc -Tutf8 ./u8hd.1 | less   # what to read them with on macOS
```

`iconv()` sits in the C library on Linux (glibc), FreeBSD and NetBSD, while
macOS keeps it in a library of its own.  The link rule in the `Makefile`
therefore links with no extra library first and, only when that fails, tries
again with `$(ICONVLIBS)` (`-liconv` by default), printing the line it runs —
so plain `make` works everywhere:

```sh
$ make
cc -D_XOPEN_SOURCE=700  -O2 -Wall -Wextra ... -c u8hd.c
cc -O2 -o u8hd u8hd.o -liconv
```

Point `ICONVCFLAGS` and `ICONVLIBS` at Homebrew's GNU libiconv if you would
rather use that one.  `make install` puts the two pages in `man/man1/` and
`man/ja/man1/` under the same name `u8hd.1`, so that `man` finds the page of
the locale it runs in; `JAMANSEC` moves the second one.

The installation process uses a Makefile that allows for the independent
installation of the program itself, the English man pages, and the Japanese
man pages. Simply running `make install` will not install the Japanese man pages.
To install the Japanese man pages, you must run `make install-jman` separately.

## Portability

- `t_u8hd.sh` is written for POSIX `sh`.  The bytes of its test data are built
  with octal (`\346`) rather than hexadecimal (`\xe6`) `printf` escapes: the
  latter are not in POSIX, and dash — the shell behind `/bin/sh` on Debian —
  copies them to its output unparsed, which is what made `make test` fail
  there.  All 44 checks pass under dash, bash and ksh.
- The locale the character checks need is found by walking `locale -a`,
  trying every name that says UTF-8 and asking `u8hd` itself whether the
  characters come out, so a system without a locale named `C.UTF-8` is
  covered as well.
- The build defines `_XOPEN_SOURCE=700`.  `iconv()` belongs to XSI rather than
  to POSIX, and with only `_POSIX_C_SOURCE` defined NetBSD hides its
  prototype.
- NetBSD, on top of that, declares the input of `iconv()` as `const char **`
  where glibc and the C libraries of macOS and FreeBSD declare `char **`.  The
  address of the input pointer travels as a `void *`, which suits both, so the
  "incompatible pointer type" warning — an error with a recent GCC — is gone.

## Tested on

| system | result |
| ------ | ------ |
| macOS (the machine it is developed on) | `make`, `make check` and `make test` all pass |
| Linux (Debian 13, Ubuntu 24.04) | `make`, `make check` and `make test` all pass |
| FreeBSD 15.1-RELEASE-p6 | `make`, `make check` and `make test` all pass |
| NetBSD 9.4 | `make`, `make check` and `make test` all pass |

## Author

Masato Minda &lt;minmin@wide.ad.jp&gt;

## License

Free software owned by its author.  See `LICENSE` (BSD 2-clause).

---

## 概要

`u8hd` (UTF-8 HexDump)は、日本語対応 hex dump ツール [`jhd`](https://github.com/belgianbeer/jhd)
を元に、**同様の表示形式** で文字の表示部に **UTF-8 の文字を表示** します。

- 1 行 16 バイト (`-c` なら 64 バイト)、アドレス 16 進 8 桁 (`-O` で 8 進 11 桁)。`-w` で 1 行の幅を 2 倍に (32 バイト、`-c` なら 128 バイト)
- 右端には文字列を UTF-8 として表示
- 文字が行をまたぐときは、前の行の末尾に文字を置き、次の行はそのバイト分空ける。下記例参照
- 同一行の repeat は `*` でまとめる (`-v` で無効)

`日`の UTF-8 の文字コードは `E6 97 A5` で 16 バイト目の境界をまたぐ場合の表示例。

```sh
$ u8hd sample
00000000  31 32 33 34 35 36 37 38  39 30 31 32 33 34 35 E6  123456789012345日
00000010  97 A5 39 30 31 32 33 34  35 0A                      9012345.
```

## 文字コードの扱いと表示

| 条件 | 表示 |
| ------ | ------ |
| ロケールのコードセットが UTF-8 | ファイル中の UTF-8 文字を表示 |
| C など UTF-8 を表示できないロケール | ASCII のみ (0x80 以上は `.`) |
| `-u` | ロケールに関係なく UTF-8 を強制表示 |
| `-e` | ファイルは **EUC-JP**。UTF-8 へ変換して表示 |
| `-s` | ファイルは **Shift-JIS**。UTF-8 へ変換して表示 |

- 変換には **libiconv(3)** を使います (UTF-8→UTF-8 では iconv を開きません)。
- `-e` と `-s` は同時に使えません (エラーになります)。
- `-eu` / `-su` とすると、対象は EUC-JP / Shift-JIS のまま、出力はロケールに関係なく UTF-8 です。
- 端末の性質はロケールから決めます (jhd が参照していた環境変数 `TTYPE` は使いません)。

## オプション

```text
u8hd [ -abcehOrsuvw ] [ <file> ] [ +start[k][-length[k]|-+end[k]] ]
```

| | |
| -- | -- |
| `-a` | ASCII のみ表示 (ロケールより優先。後の `-u` で再び UTF-8 に) |
| `-b` | 1 バイトずつ 8 進 |
| `-c` | 文字のみ 1 行 64 バイト |
| `-e` | ファイルを EUC-JP として読む |
| `-h` | ヘルプを表示（最後に版を示す文字列） |
| `-o` | 2 バイトずつ 8 進 (ビッグエンディアン) |
| `-r` | 2 バイトずつ 8 進 (リトルエンディアン) |
| `-s` | ファイルを Shift-JIS として読む |
| `-u` | ロケールに関係なく UTF-8 で表示 |
| `-v` | 同一行のまとめ表示を禁止 |
| `-w` | 1 行の幅を 2 倍に (32 バイト、`-c` なら 128 バイト) |
| `-O` | アドレスを 8 進表示 |

`+` で始まる引数は、ファイルのどこらから見始めるかを指定します。数字は 10 進数で書き、
前に `x`（または `0x`）をつけるか `o` をつけるか先頭が `0` だとそれれ 16 進・8 進数、
後に `k` をつけると 1024 倍です。ファイル名の前でも後でも書けます。
その後にダッシをつけると表示を範囲に絞められます。`+start-length` は start
バイト目から length バイトを、`+start-+end` は start から end のバイトまで
（end も含む）を表示します。start は省略でき、それはファイル先頭を意味します。

```text
+1000-20         1000 バイト目から 20 バイト
+1000-+1019      同じ 20 バイトを位置で指定
+-+511           ファイルの先頭から 512 バイト
+0x40-0x20       0x40 から 0x5f までの 32 バイト
+1k-512          先頭の 1K バイトのうしろの 512 バイト
```

```sh
$ u8hd hello +16-+23
00000010  93 E3 82 93 E3 81 AB E3                           .んに.
```

## jhd との違い

表示形式 (アドレス桁、16 進列、文字列の桁位置、`*` まとめ)は jhd を踏襲しています。
ASCII のみのデータについては、jhd(1) を実際に走らせて、その出力を `t_u8hd.sh` の
期待値にしてあります (ランダムに作ったファイルとオフセットでも jhd とバイト単位で
一致することを確かめてあります)。

| 項目 | jhd | u8hd |
| ---- | --- | ---- |
| 端末のコードセット | 環境変数 `TTYPE` | ロケール (`-u` で強制) |
| ファイルのコードセット | ビルド時 `-DEUC=0/1` | `-e` / `-s` / 既定 UTF-8 |
| 文字の表示 | 端末のコードセットのまま | UTF-8 (libiconv で変換) |
| `-j` | 漢字表示を強制 | 不要 (`-u` が同じ働き) |
| `-k` | 半角カナまで | 半角カナも `-e`/`-s` で表示されます |
| 1 文字の長さ | 1 バイトか 2 バイト | 1〜4 バイト (UTF-8)、3 バイト (SS3) |
| 1 行の幅 | 16 / 64 バイトで固定 | 同じ、ただし `-w` で 32 / 128 バイト |

## 来歴

| 日付 | バージョン | 備考 |
| ---- | --------- | ---- |
| 2026-11-18 | v1.0 | jhd をベースに、UTF-8 表示対応 |

- 作者：民田 雅人 (Masato Minda) &lt;minmin@wide.ad.jp&gt;

## 収録物

- `u8hd.c` — ANSI C / POSIX、libiconv 利用
- `u8hd.1` — 英語 man ページ
- `u8hd_ja.1` — 日本語 man ページ
- `Makefile` — ビルド / インストール (`PREFIX`, `DESTDIR`, `JAMANSEC` 可変)
- `t_u8hd.sh` — tap 形式の回帰テスト (jhd との同値性も検査)
- `LICENSE` / `README.md`

## ビルド / インストール

```sh
make                            # → ./u8hd ができます
make check                      # 構文チェック (-Wall -Wextra 込み)
make test                       # tap 形式の回帰テスト (44 検査)
sudo make install               # 既定は /usr/local、PREFIX・DESTDIR 変更可
man -l ./u8hd.1                 # 英語 man (BSD の man なら -l でローカル man を読む)
man -l ./u8hd_ja.1              # 日本語 man
mandoc ./u8hd_ja.1 | less       # macOS の man に -l が無いときは mandoc で読む
```

iconv(3) は Linux (glibc)、FreeBSD、NetBSD 等では C ライブラリにありますが、
macOS では libiconv を別にリンクする必要があります。そこで Makefile はまず
`-liconv` を付けずにリンクし、通らなければ `$(ICONVLIBS)` (既定は
`-liconv`) を足してもう一度リンクします。
リンクがどちらになったかはそのまま表示するので、次のように 2 行目が表示されれば
`-liconv` を加えてリンクしたことになります。多くのOSでは `-liconv`
 を使わないリンク行となります。

```sh
$ make
cc -D_XOPEN_SOURCE=700  -O2 -Wall -Wextra ... -c u8hd.c
cc -O2 -o u8hd u8hd.o -liconv
```

Homebrew の GNU libiconv を使いたい場合は

```sh
make ICONVCFLAGS="-I$(brew --prefix libiconv)/include" \
     ICONVLIBS="-L$(brew --prefix libiconv)/lib -liconv"
```

としてください。

インストールは、プログラム本体と英語man、日本語man をそれぞれ独立してインストールできるように
Makefile を記述してあります。単に `make install`
を実行した場合、日本語man はインストールされません。日本語man をインストールする場合は、
別途 `make install-jman` を実行する必要があります。

また man のインストールは 2 か所になります。英語版は `$(PREFIX)/man/man1/u8hd.1`、
日本語版は `$(PREFIX)/man/ja/man1/u8hd.1` です (配布物の中では `u8hd_ja.1` という
名前ですが、locale の man が見つけるように、インストール時の名前は `u8hd.1` に
します)。置き場所は `make JAMANSEC=ja_JP install-jman` のように変えられます。

## ポータビリティ

- `t_u8hd.sh` は POSIX の `sh` で動くように書いてあります。テストデータの
  バイト列は `printf` の 16 進エスケープ (`\xe6`)ではなく 8 進エスケープ
   (`\346`)で組み立てます。16 進エスケープは POSIX に無く、Debian の
  `/bin/sh` (dash)等では解釈されずにそのまま出力されてしまいます。
  macOS 上の dash・bash・ksh で 44 検査すべてが通ることを確かめてあります。
- 文字を見る検査に使うロケールは `locale -a` を確認し、UTF-8 を名前に含むもの
  から順に実際に u8hd を走らせて確かめ、通るものを選びます。
  `C.UTF-8` という名前のロケールの無いシステムでも通ります。
- ビルドは `-D_XOPEN_SOURCE=700` としました。iconv(3) は POSIX ではなく XSI
  の関数で、`_POSIX_C_SOURCE` だけを定義すると NetBSD ではその宣言が隠れて
  しまうためです。
- 同じく NetBSD の話ですが、`iconv()` の入力引数が `char **` ではなく
  `const char **` と宣言されているため、初期の u8hd では警告 (非互換のポインタ型。
  新しい GCC ではエラーになります)が出ていました。入力側のアドレスを
  `void *` に変えて渡すようにしたので、`char **` の glibc・macOS・
  FreeBSD でも、`const char **` の NetBSD でも、警告なく通ります。
- リンクは上に書いたとおり `-liconv` を付けない方を先に試します。

## 動作確認

| システム | 結果 |
| -------- | ---- |
| macOS (開発環境) | `make` / `make check` / `make test` (dash・bash・ksh) すべてパス |
| Linux (Debian 13、Ubuntu 24.04) | `make` / `make check` / `make test` すべてパス |
| FreeBSD 15.1-RELEASE-p6 | `make` / `make check` / `make test` すべてパス |
| NetBSD 9.4 | `make` / `make check` / `make test` すべてパス |

## ライセンス

作者本人が所有するフリーソフトウェアです。`LICENSE` (BSD 2-clause) 参照。
