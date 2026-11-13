# u8hd — UTF-8 Hex Dump Utility

[English](#english) / [日本語](#日本語)

## English

`u8hd` is a hex dump that shows you what the bytes **say**.  The offset, then
the bytes in hexadecimal, then the text — and the text is the text, with its
Japanese, Hangul, Chinese, Cyrillic, Thai and accented Latin in it, not a row
of dots.  It is a filter, so it belongs at the end of a pipe.

### Overview

A byte above 0x7f is where the classic dumps give up: `od -c` writes an
escape, `hexdump -C` and `xxd` write a dot, and you are left counting hex
by hand to learn whether the file holds `日本` or `??`.  `u8hd` shows the
character instead, in UTF-8, which is the code set every one of those
languages has settled on.

- 16 bytes to a line (64 with `-c`), the address in 8 hexadecimal digits
  (`-O` for 11 octal ones), and `-w` to double the width of a line: 32 bytes,
  or 128 with `-c`, for a terminal that has the room
- a character that the end of a line cuts in two is drawn whole at the end of
  the line it begins in, and the line below leaves that many blanks
- lines that repeat are folded into a `*` (`-v` to see them all)
- a file in an old Japanese code set is read as it is and shown in UTF-8:
  `-e` for EUC-JP, `-s` for Shift-JIS, converted with **libiconv(3)**
- what the terminal can display is asked from the **locale**, not from an
  environment variable of its own; `-u` overrules the answer

The shape of the dump comes from [`jhd`](https://github.com/belgianbeer/jhd),
a Japanese hex dump tool of 1986, which made the same choice for EUC-JP and
Shift-JIS.  `u8hd` keeps that layout and moves its character column to UTF-8;
for data that stays below 0x80 the two programs print the same thing.

### Examples

UTF-8 data.  The `日` whose bytes are `E6 97 A5` begins at byte 15, so the
line boundary cuts it in two: it is drawn whole at the end of the line it
begins in, and the next line leaves two blanks before its own characters.

```sh
$ u8hd sample
00000000  31 32 33 34 35 36 37 38  39 30 31 32 33 34 35 E6  123456789012345日
00000010  97 A5 36 37 38 39 30 31  32 33 34 35 36 37 38 39    67890123456789
00000020  30 31 32 33 34 35 0A                              012345.
```

A file that mixes scripts still reads from left to right.  These lines take
one slot per byte rather than one per cell, so the text drifts a little from
line to line; in a pipe it is the same.

```sh
$ cat hello
Hello こんにちは 안녕하세요 你好 ¡Hola! Добрий день สวัสดี
$ u8hd hello
00000000  48 65 6C 6C 6F 20 E3 81  93 E3 82 93 E3 81 AB E3  Hello こんにち
00000010  81 A1 E3 81 AF 20 EC 95  88 EB 85 95 ED 95 98 EC    は 안녕하세
00000020  84 B8 EC 9A 94 20 E4 BD  A0 E5 A5 BD 20 C2 A1 48    요 你好 ¡H
00000030  6F 6C 61 21 20 D0 94 D0  BE D0 B1 D1 80 D0 B8 D0  ola! Добрий
00000040  B9 20 D0 B4 D0 B5 D0 BD  D1 8C 20 E0 B8 AA E0 B8    день สว
00000050  A7 E0 B8 B1 E0 B8 AA E0  B8 94 E0 B8 B5 0A         ัสดี.
```

### Code sets

| condition | what the character column shows |
|-----------|---------------------------------|
| the locale code set is UTF-8 | the UTF-8 characters of the file |
| `C`, or any locale that cannot display UTF-8 | ASCII only, bytes above 0x80 as `.` |
| `-u` | UTF-8 whatever the locale says |
| `-e` | the file is read as **EUC-JP** and converted to UTF-8 |
| `-s` | the file is read as **Shift-JIS** and converted to UTF-8 |

**libiconv(3)** does the conversion, and nothing is converted when both sides
are UTF-8 — no iconv descriptor is even opened for a UTF-8 file.  `-e` and
`-s` cannot be used together: one byte of a file cannot belong to two code
sets at once.  `-eu` and `-su` read EUC-JP and Shift-JIS and write UTF-8 no
matter what the locale says.

### Options

```text
u8hd [ -abceOrsuvw ] [ <file> ] [ +[x|o]<offset>[k] ]
```

| | |
|---|---|
| `-a` | ASCII only (overrules the locale, and a `-u` written before it) |
| `-b` | the data as octal bytes |
| `-c` | the characters only, 64 bytes to a line |
| `-e` | read the file as EUC-JP |
| `-o` | the data as octal words, big-endian |
| `-r` | the data as octal words, little-endian |
| `-s` | read the file as Shift-JIS |
| `-u` | show UTF-8 whatever the locale says |
| `-v` | do not fold repeated lines |
| `-w` | double the width of a line: 32 bytes, or 128 with `-c` |
| `-O` | the address in octal |

Options may follow one dash, so `-eu` is the same as `-e -u`, and of two
options that speak about the same thing the one written last wins.  An offset
says where in the file to begin: in decimal, with an `x` in hexadecimal, with
an `o` or a leading `0` in octal, and with a `k` multiplied by 1024.  It may
stand before or after the file name.

### Differences from jhd

The layout follows `jhd`: the digits of the address, the columns of
hexadecimal, the place of the character column, the `*` that folds lines.
Where the data is ASCII only, the checks of `t_u8hd.sh` compare the output
with what `jhd` itself prints.

| | jhd | u8hd |
|---|-----|------|
| the code set of the terminal | the `TTYPE` environment variable | the locale (`-u` to overrule it) |
| the code set of the file | chosen when it is compiled, `-DEUC=0/1` | on the command line: `-e`, `-s`, UTF-8 by default |
| the characters shown | as the terminal receives them | UTF-8, converted with libiconv |
| `-j` | forces the kanji display | not needed, `-u` does that |
| `-k` | half width katakana as well | `-e` and `-s` show them without an option |
| the length of a character | 1 or 2 bytes | 1 to 4 (UTF-8), 3 for the SS3 of EUC-JP |
| the width of a line | 16 or 64 bytes, fixed | the same, and `-w` for 32 or 128 |

### History

| date | version | |
|------|---------|--|
| 2026-11-11 | v1.0 | written after `jhd`, with UTF-8 and libiconv |
| 2026-11-XX | v1.1 | `-w`, which doubles the width of a line |

### Contents

- `u8hd.c` — the program, ANSI C / POSIX, using libiconv
- `u8hd.1` — the manual page in English, `u8hd_ja.1` — the same in Japanese
- `Makefile` — build and install (`PREFIX`, `DESTDIR`, `JAMANSEC` overridable)
- `t_u8hd.sh` — 32 regression checks in tap format
- `LICENSE` / `README.md`

### Building

```sh
make                            # builds ./u8hd
make check                      # a syntax check with -Wall -Wextra
make test                       # the regression tests (32 checks)
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

### Portability

* `t_u8hd.sh` is written for POSIX `sh`.  The bytes of its test data are built
  with octal (`\346`) rather than hexadecimal (`\xe6`) `printf` escapes: the
  latter are not in POSIX, and dash — the shell behind `/bin/sh` on Debian —
  copies them to its output unparsed, which is what made `make test` fail
  there.  All 32 checks pass under dash, bash and ksh.
* The locale the character checks need is found by walking `locale -a`,
  trying every name that says UTF-8 and asking `u8hd` itself whether the
  characters come out, so a system without a locale named `C.UTF-8` is
  covered as well.
* The build defines `_XOPEN_SOURCE=700`.  `iconv()` belongs to XSI rather than
  to POSIX, and with only `_POSIX_C_SOURCE` defined NetBSD hides its
  prototype.
* NetBSD, on top of that, declares the input of `iconv()` as `const char **`
  where glibc and the C libraries of macOS and FreeBSD declare `char **`.  The
  address of the input pointer travels as a `void *`, which suits both, so the
  "incompatible pointer type" warning — an error with a recent GCC — is gone.

### Tested on

| system | result |
|--------|--------|
| macOS (the machine it is developed on) | `make`, `make check` and `make test` all pass |
| Linux (Debian 13, Ubuntu 24.04) | passes |
| FreeBSD 15.1-RELEASE-p6 | passes |
| NetBSD 9.4 | passes |

### Author

Masato Minda &lt;minmin@wide.ad.jp&gt;

### License

Free software owned by its author.  See `LICENSE` (BSD 2-clause).

---

## 日本語

`u8hd` は、ダンプしたバイトが何を書いているかまで見せる hex dump
ツールです。アドレス、16 進のバイト列、そして右端に文字列。日本語も
ハングルも中国語もキリル文字もタイ語もアクセント付きラテン文字も、
ドットの並びではなく文字として出ます。パイプの先に置けるように
作ってあります。

### 概要

`u8hd`（UTF-8 HexDump）は、日本語対応 hex dump ツール [`jhd`](https://github.com/belgianbeer/jhd) を元に、**同様の表示形式** で文字の表示部に **UTF-8 の文字を表示する** ダンププログラムです。

- 1 行 16 バイト（`-c` なら 64 バイト）、アドレス 16 進 8 桁（`-O` で 8 進 11 桁）。`-w` で 1 行の幅を 2 倍に（32 バイト、`-c` なら 128 バイト）
- 右端の文字列は、ファイル中の文字をそのまま表示
- 文字が行をまたぐときは、前の行の末尾に文字を置き、次の行はそのバイトぶん空ける
  （jhd が漢字の 2 バイト目を次の行にまたがって表示するのと同じ流儀）
- 同一行の repeat は `*` でまとめる（`-v` で無効）

### 表示例

UTF-8 を含むデータは、このように表示します。

UTF-8 のファイル（`日`が 16 バイト目の境界をまたいでいる）：

```sh
$ u8hd sample
00000000  31 32 33 34 35 36 37 38  39 30 31 32 33 34 35 E6  123456789012345日
00000010  97 A5 36 37 38 39 30 31  32 33 34 35 36 37 38 39    67890123456789
00000020  30 31 32 33 34 35 0A                              012345.
```

世界各地の文字が混ざったファイルでも、行は左から右へ読めます。これらの
行は 1 セルではなく 1 バイトに 1 枠を取るので、文字列は行ごとに少しずれ
ます。パイプの先でも同じように動きます。

```sh
$ cat hello
Hello こんにちは 안녕하세요 你好 ¡Hola! Добрий день สวัสดี
$ u8hd hello
00000000  48 65 6C 6C 6F 20 E3 81  93 E3 82 93 E3 81 AB E3  Hello こんにち
00000010  81 A1 E3 81 AF 20 EC 95  88 EB 85 95 ED 95 98 EC    は 안녕하세
00000020  84 B8 EC 9A 94 20 E4 BD  A0 E5 A5 BD 20 C2 A1 48    요 你好 ¡H
00000030  6F 6C 61 21 20 D0 94 D0  BE D0 B1 D1 80 D0 B8 D0  ola! Добрий
00000040  B9 20 D0 B4 D0 B5 D0 BD  D1 8C 20 E0 B8 AA E0 B8    день สว
00000050  A7 E0 B8 B1 E0 B8 AA E0  B8 94 E0 B8 B5 0A         ัสดี.
```

### 文字コードの方針

| 条件 | 表示 |
|------|------|
| ロケールのコードセットが UTF-8 | ファイル中の UTF-8 文字を表示 |
| C など UTF-8 を表示できないロケール | ASCII のみ（0x80 以上は `.`） |
| `-u` | ロケールに関係なく UTF-8 を強制表示 |
| `-e` | ファイルは **EUC-JP**。UTF-8 へ変換して表示 |
| `-s` | ファイルは **Shift-JIS**。UTF-8 へ変換して表示 |

- 変換には **libiconv(3)** を使います（UTF-8→UTF-8 では iconv を開きません）。
- `-e` と `-s` は同時に使えません（エラーになります）。
- `-eu` / `-su` とすると、対象は EUC-JP / Shift-JIS のまま、出力はロケールに関係なく UTF-8 です。
- 端末の性質はロケールから決めます（jhd が参照していた環境変数 `TTYPE` は使いません）。

### オプション

```text
u8hd [ -abceOrsuvw ] [ <file> ] [ +[x|o]<offset>[k] ]
```

| | |
|---|---|
| `-a` | ASCII のみ表示（ロケールより優先。後の `-u` で再び UTF-8 に） |
| `-b` | 1 バイトずつ 8 進 |
| `-c` | 文字のみ 1 行 64 バイト |
| `-e` | ファイルを EUC-JP として読む |
| `-o` | 2 バイトずつ 8 進（ビッグエンディアン） |
| `-r` | 2 バイトずつ 8 進（リトルエンディアン） |
| `-s` | ファイルを Shift-JIS として読む |
| `-u` | ロケールに関係なく UTF-8 で表示 |
| `-v` | 同一行のまとめ表示を禁止 |
| `-w` | 1 行の幅を 2 倍に（32 バイト、`-c` なら 128 バイト） |
| `-O` | アドレスを 8 進表示 |

### jhd との違い

表示形式（アドレス桁、16 進列、文字列の桁位置、`*` まとめ）は jhd を踏襲しています。
ASCII のみのデータについては、jhd(1) を実際に走らせて、その出力を `t_u8hd.sh` の
期待値にしてあります（ランダムに作ったファイルとオフセットでも jhd とバイト単位で
一致することを確かめてあります）。

| 項目 | jhd | u8hd |
|------|-----|------|
| 端末のコードセット | 環境変数 `TTYPE` | ロケール（`-u` で強制） |
| ファイルのコードセット | ビルド時 `-DEUC=0/1` | `-e` / `-s` / 既定 UTF-8 |
| 文字の表示 | 端末のコードセットのまま | UTF-8（libiconv で変換） |
| `-j` | 漢字表示を強制 | 不要（`-u` が同じ働き） |
| `-k` | 半角カナまで | 半角カナも `-e`/`-s` で表示されます |
| 1 文字の長さ | 1 バイトか 2 バイト | 1〜4 バイト（UTF-8）、3 バイト（SS3） |
| 1 行の幅 | 16 / 64 バイトで固定 | 同じ、ただし `-w` で 32 / 128 バイト |

### 来歴

| 日付 | バージョン | 備考 |
|------|-----------|------|
| 2026-11-11 | v1.0 | jhd を元典に、UTF-8 表示・libiconv 変換版として新規作成 |
| 2026-11-XX | v1.1 | 1 行のデータ量を 2 倍にする `-w` オプションを追加 |

- 作者：民田 雅人（Masato Minda） &lt;minmin@wide.ad.jp&gt;

### 収録物

- `u8hd.c` — ANSI C / POSIX、libiconv 利用
- `u8hd.1` — 英語 man ページ
- `u8hd_ja.1` — 日本語 man ページ
- `Makefile` — ビルド / インストール（`PREFIX`, `DESTDIR`, `JAMANSEC` 可変）
- `t_u8hd.sh` — tap 形式の回帰テスト（jhd との同値性も検査）
- `LICENSE` / `README.md`

### ビルド / インストール

```sh
make                            # → ./u8hd ができます
make check                      # 構文チェック（-Wall -Wextra 込み）
make test                       # tap 形式の回帰テスト（32 検査）
sudo make install               # 既定は /usr/local、PREFIX・DESTDIR 変更可
man -l ./u8hd.1                 # 英語 man（BSD の man なら -l でローカル man を読む）
man -l ./u8hd_ja.1              # 日本語 man
mandoc ./u8hd_ja.1 | less       # macOS の man に -l が無いときは mandoc で読む
```

iconv(3) は Linux (glibc)、FreeBSD、NetBSD では C ライブラリに入っていますが、
macOS では libiconv を別にリンクする必要があります。そこで Makefile はまず
`-liconv` を付けずにリンクし、それが通らなければ `$(ICONVLIBS)`（既定は
`-liconv`）を足してもう一度リンクします。リンクに何を使ったかはそのまま表示
するので、次のように 2 行目が見えれば `-liconv` を足した方を通ったということです。
多くの OS では 1 行目だけになります。

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

man のインストールは 2 か所に置きます。英語版は `$(PREFIX)/man/man1/u8hd.1`、
日本語版は `$(PREFIX)/man/ja/man1/u8hd.1` です（配布物の中では `u8hd_ja.1` という
名前ですが、locale の man が見つけるように、インストール時の名前は `u8hd.1` に
します）。置き場所は `make JAMANSEC=ja_JP install` のように変えられます。

### ポータビリティ

- `t_u8hd.sh` は POSIX の `sh` で動くように書いてあります。テストデータの
  バイト列は `printf` の 16 進エスケープ（`\xe6`）ではなく 8 進エスケープ
  （`\346`）で組み立てます。16 進エスケープは POSIX に無く、Debian の
  `/bin/sh`（dash）等では解釈されずにそのまま出力されてしまい、それが
  `make test` が通らなかった原因でした。macOS 上の dash・bash・ksh で
  32 検査すべてが通ることを確かめてあります。
- 文字を見る検査に使うロケールは `locale -a` を歩いて、UTF-8 を名前に含むもの
  から順に実際に u8hd を走らせて確かめ、通るものを選びます。それで
  `C.UTF-8` という名前のロケールの無いシステムでも通ります。
- ビルドは `-D_XOPEN_SOURCE=700` としました。iconv(3) は POSIX ではなく XSI
  の関数で、`_POSIX_C_SOURCE` だけを定義すると NetBSD ではその宣言が隠れて
  しまうためです。
- 同じく NetBSD の話ですが、`iconv()` の入力引数が `char **` ではなく
  `const char **` と宣言されているため、u8hd 側に警告（非互換のポインタ型。
  新しい GCC ではエラーになります）が出ていました。入力側のアドレスを
  `void *` に変えて渡すようにしたので、`char **` の glibc・macOS・
  FreeBSD でも、`const char **` の NetBSD でも、警告なく通ります。
- リンクは上に書いたとおり `-liconv` を付けない方を先に試します。

### 動作確認

| システム | 結果 |
|----------|------|
| macOS（開発環境） | `make` / `make check` / `make test`（dash・bash・ksh）すべて通過 |
| Linux（Debian 13、Ubuntu 24.04） | 通過 |
| FreeBSD 15.1-RELEASE-p6 | 通過 |
| NetBSD 9.4 | 通過 |

### ライセンス

作者本人が所有するフリーソフトウェアです（→ `LICENSE` 参照）。BSD 2 クローズ。
