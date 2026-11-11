# u8hd — UTF-8 Hex Dump ユーティリティ（v1.0）

[日本語](#日本語) / [English](#english)

## 日本語

### 概要

`u8hd`（UTF-8 HexDump）は、日本語対応 hex dump ツール [`jhd`](https://github.com/belgianbeer/jhd) と
**同じ表示形式**で、文字の表示部に **UTF-8 の文字を表示する**ダンププログラムです。
行の幅、アドレス桁数、16 進表示と文字表示の桁位置、同一行のまとめ表示（`*`）などは jhd のそのままで、
ASCII のみのデータでは jhd とバイト単位で同一の出力になります。

- 1 行 16 バイト、アドレス 16 進 8 桁（`-O` で 8 進 11 桁）
- 右端の文字列は、ファイル中の文字をそのまま表示
- 文字が行をまたぐときは、前の行の末尾に文字を置き、次の行はそのバイトぶん空ける
  （jhd が漢字の 2 バイト目を次の行にまたがって表示するのと同じ流儀）
- 同一行の repeat は `*` でまとめる（`-v` で無効）

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
- jhd が参照していた環境変数 **`TTYPE` は使いません**。端末の性質はロケールから決めます。

### オプション

```
u8hd [ -abceOrsuv ] [ <file> ] [ +[x|o]<offset>[k] ]
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
| `-O` | アドレスを 8 進表示 |

### jhd との違い

表示形式（アドレス桁、16 進列、文字列の桁位置、`*` まとめ）は jhd と同じです。
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

細かい違いを 1 つだけ挙げておきます。ファイルの末尾が行の境で 1 文字の途中に
なったとき、u8hd は「.」を打って改行で行を閉じます（jhd はそこで改行を出さず、
行を開いたままにします）。

### 来歴

| 日付 | バージョン | 備考 |
|------|-----------|------|
| 2026-11-10 | v1.0 | jhd を元典に、UTF-8 表示・libiconv 変換版として新規作成 |

- 作者：民田 雅人（Masato Minda）　<minmin@wide.ad.jp>

### 収録物

- `u8hd.c` — ANSI C / POSIX、libiconv 利用
- `u8hd.1` — 英語 man ページ
- `u8hd_ja.1` — 日本語 man ページ
- `Makefile` — ビルド / インストール（`PREFIX`, `DESTDIR` 可変）
- `t_u8hd.sh` — tap 形式の回帰テスト（jhd との同値性も検査）
- `LICENSE` / `README.md`

### ビルド / インストール

```sh
make                            # → ./u8hd ができます
make check                      # 構文チェック（-Wall -Wextra 込み）
make test                       # tap 形式の回帰テスト（21 検査）
sudo make install               # 既定は /usr/local、PREFIX・DESTDIR 変更可
man -l ./u8hd.1                 # 英語 man（BSD の man なら -l でローカル man を読む）
man -l ./u8hd_ja.1              # 日本語 man
mandoc ./u8hd_ja.1 | less       # Mac OS X の man に -l が無いときは mandoc で読む
```

iconv(3) は Linux (glibc)、FreeBSD、NetBSD では C ライブラリに入っていますが、
Mac OS X では libiconv を別にリンクする必要があります。そこで Makefile はまず
`-liconv` を付けずにリンクし、それが通らなければ `$(ICONVLIBS)`（既定は
`-liconv`）を足してもう一度リンクします。どの OS でも `make` だけで通ります。
Mac OS X では 2 回目のリンクが次のように知らせます（他の OS ではこの行は出ません）。

```sh
$ make
cc -D_XOPEN_SOURCE=700 -O2 -Wall -Wextra ... -c u8hd.c
u8hd: iconv(3) is not in libc, linking with -liconv
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

* `t_u8hd.sh` は POSIX の `sh` で動くように書いてあります。テストデータの
  バイト列は `printf` の 16 進エスケープ（`\xe6`）ではなく 8 進エスケープ
  （`\346`）で組み立てます。16 進エスケープは POSIX に無く、Debian の
  `/bin/sh`（dash）では解釈されずにそのまま出力されてしまい、それが
  `make test` が通らなかった原因でした。macOS 上の dash・bash・ksh で
  21 検査すべてが通ることを確かめてあります。
* 文字を見る検査に使うロケールは `locale -a` を歩いて、UTF-8 を名前に含むもの
  から順に実際に u8hd を走らせて選べば、`C.UTF-8` という名前のロケールの無い
  システムでも通ります。
* ビルドは `-D_XOPEN_SOURCE=700` としました。iconv(3) は POSIX ではなく XSI
  の関数で、`_POSIX_C_SOURCE` だけを定義すると NetBSD ではその宣言が隠れて
  しまうためです。
* リンクは上に書いたとおり `-liconv` を付けない方を先に試します。

### 表示例

UTF-8 のファイル（`日`が 16 バイト目の境界をまたいでいる）：

```
$ u8hd sample
00000000  31 32 33 34 35 36 37 38  39 30 31 32 33 34 35 E6  123456789012345日
00000010  97 A5 36 37 38 39 30 31  32 33 34 35 36 37 38 39    67890123456789
00000020  30 31 32 33 34 35 0A                              012345.
```

同じテキストを EUC-JP で書いたファイルを `-e` で読む：

```
$ u8hd -e sample.euc
00000000  48 65 6C 6C 6F 2C 20 C6  FC CB DC B8 EC A5 C6 A5  Hello, 日本語テス
00000010  B9 A5 C8 0A                                        ト.
```

### ライセンス

作者本人が所有するフリーソフトウェアです（→ `LICENSE` 参照）。BSD 2 クローズ。

---

## English

### Overview

`u8hd` (UTF-8 HexDump) dumps a file in the very same layout as the Japanese
hex dump tool [`jhd`](https://github.com/belgianbeer/jhd), but its character
column is built for **UTF-8**: multi byte characters found in the file are
shown as the characters themselves. For data without any byte above 0x80 the
output is byte for byte what `jhd` prints.

- 16 bytes a line, an 8 digit hexadecimal address (`-O` for 11 octal ones)
- a character that crosses the line boundary is drawn at the end of the line
  it starts in, and the next line leaves that many blanks — the way `jhd`
  carries the trailing half of a Kanji over to the next line
- repeated lines are folded into a `*` (`-v` to turn that off)

### Code sets

| condition | what the character column shows |
|-----------|---------------------------------|
| locale code set is UTF-8 | the UTF-8 characters of the file |
| C or any locale that cannot display UTF-8 | ASCII only, bytes above 0x80 as `.` |
| `-u` | UTF-8 whatever the locale says |
| `-e` | the file is read as **EUC-JP** and converted to UTF-8 |
| `-s` | the file is read as **Shift-JIS** and converted to UTF-8 |

The conversion is done with **libiconv(3)**; nothing is converted when both
sides are UTF-8. `-e` and `-s` cannot be used together. `-eu` and `-su` read
EUC-JP and Shift-JIS and always write UTF-8. The `TTYPE` environment variable
that `jhd` looks at is **not** used: what the terminal can display is asked
from the locale.

### Building

```sh
make                            # builds ./u8hd
make check                      # syntax check with -Wall -Wextra
make test                       # the regression tests (21 checks)
sudo make install               # default /usr/local; PREFIX and DESTDIR are overridable
man -l ./u8hd.1                 # the English manual page (BSD man; macOS has no -l)
man -l ./u8hd_ja.1              # the Japanese manual page
mandoc -Tutf8 ./u8hd.1 | less   # what to read them with on Mac OS X
```

`iconv()` sits in the C library on Linux (glibc), FreeBSD and NetBSD, while
Mac OS X keeps it in a library of its own.  The link rule in the `Makefile`
therefore links without any extra library first and, only when that fails,
tries again with `$(ICONVLIBS)` (`-liconv` by default), so plain `make` works
everywhere.  On Mac OS X the second attempt says so (the line does not appear
on the other systems):

```sh
$ make
cc -D_XOPEN_SOURCE=700 -O2 -Wall -Wextra ... -c u8hd.c
u8hd: iconv(3) is not in libc, linking with -liconv
```

Point `ICONVCFLAGS` and `ICONVLIBS` at Homebrew's GNU libiconv if you would
rather use that one.

### Portability

* `t_u8hd.sh` is written for POSIX `sh`.  The bytes of the test data are
  built with octal (`\346`) rather than hexadecimal (`\xe6`) `printf` escapes:
  the latter are not in POSIX, and dash -- the shell behind `/bin/sh` on
  Debian -- copies them to the output unparsed, which is what made
  `make test` fail there.  All 21 checks pass under dash, bash and ksh on this
  machine.
* The locale that the character checks need is looked for by walking
  `locale -a`, trying every name that says UTF-8, and asking u8hd itself
  whether the Kanji comes out; a system without a locale named `C.UTF-8` is
  covered that way.
* The build defines `_XOPEN_SOURCE=700`.  `iconv()` belongs to XSI rather than
  to POSIX, and with only `_POSIX_C_SOURCE` defined NetBSD hides its
  prototype.
* Linking tries the plain libc first and `$(ICONVLIBS)` after it, as told
  above.

### Examples

A UTF-8 file.  The `日` whose three bytes are `E6 97 A5` begins at byte 15,
so it crosses the end of the line: it is drawn whole at the end of the line
it begins in, and the line below leaves two blanks before its own characters.

```
$ u8hd sample
00000000  31 32 33 34 35 36 37 38  39 30 31 32 33 34 35 E6  123456789012345日
00000010  97 A5 36 37 38 39 30 31  32 33 34 35 36 37 38 39    67890123456789
00000020  30 31 32 33 34 35 0A                              012345.
```

The same text written in EUC-JP, read with `-e` and converted to UTF-8 for
the display.  Here it is the kana `ス` that crosses the line boundary; the
`A5` at the end of the first line joins the `B9` at the beginning of the
second, whose character column therefore starts one byte late.

```
$ u8hd -e sample.euc
00000000  48 65 6C 6C 6F 2C 20 C6  FC CB DC B8 EC A5 C6 A5  Hello, 日本語テス
00000010  B9 A5 C8 0A                                        ト.
```

### Contents

- `u8hd.c` — the program, ANSI C / POSIX, with libiconv
- `u8hd.1` — the manual page in English
- `u8hd_ja.1` — the manual page in Japanese
- `Makefile` — build and install (`PREFIX`, `DESTDIR`, `JAMANSEC` are overridable)
- `t_u8hd.sh` — the regression tests

`make install` puts the English page in `$(PREFIX)/man/man1/u8hd.1` and the
Japanese one in `$(PREFIX)/man/ja/man1/u8hd.1`, both under the name `u8hd.1`
so that `man` finds the page of the locale it runs in.

### Author

Masato Minda <minmin@wide.ad.jp>

### License

Free software owned by its author. See `LICENSE` (BSD 2-clause).

