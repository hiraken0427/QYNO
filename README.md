# QYNO 0.27.8

著作権およびライセンスに関する情報は、本ファイルの末尾に記載している。

[概要](#概要)

[ビルド手順](#ビルド手順)

[新機能](#新機能)

[使用方法](#使用方法)

[言語仕様](#言語仕様)

[標準モジュール](#標準モジュール)

[開発状況](#開発状況)

[ライセンス情報](#ライセンス情報)

## 概要

QYNOは、静的型付けのコンパイル言語である。UTF-8で記述したソースコードから、Linux x64およびWindows x64の実行ファイルを生成する。

コンパイラはC言語で実装している。拡張子は.qyである。数式に近い範囲指定や型推論を備え、整数、小数、複素数、配列を扱うことができる。

## ビルド手順

ビルドにはGCCとMakeが必要である。GCCの128bit数値拡張を使用している。

Linux x64では、ソースコードを取得したディレクトリで次のコマンドを実行する。

```sh
make
```

生成されるコンパイラはqycである。ビルド時の生成物は次のコマンドで削除できる。

```sh
make clean
```

## 新機能

0.27.8では、パスを指定してファイルを一括で読み書きする機能を追加した。read_textで全体を文字列として読み込み、write_textで上書き、append_textで追記を行う。read_linesは行の配列を返し、write_linesは行の配列を書き込む。書き込みの戻り値はint_64のバイト数である。

```text
using io.file as fs

int main();
	fs.write_text("input.txt", " Ken \nAlice\n")
	fs.append_text("input.txt", "Bob\n")
	str names[] = fs.read_lines("input.txt")
	for(0 <= i < names.length, i++);
		names[i] = names[i].trim()
	fs.write_lines("output.txt", names)
	io.print(fs.read_text("output.txt"))
	return 0
```

これらの関数で開いたファイルは、処理が終わると内部で閉じる。OSによる失敗は既存のtryとcaseで処理できる。

read_textとread_linesはUTF-8を読み込み、BOMとNULを保持する。read_linesはLF、CRLF、CRを改行として扱う。空行は保持し、空のファイルは空の配列となる。末尾の改行から余分な空行は作らない。

読み込みの上限は既定で64MiBである。read_textの第2引数で変更できる。read_linesは第2引数でバイト数、第3引数で行数を指定でき、行数の既定値は1048576である。上限を超えた場合はio.ErrorCode.out_of_rangeとなる。

write_textとappend_textは改行を追加しない。write_linesは既定で各行の末尾にLFを付ける。第3引数でLF、CRLF、CRを指定でき、第4引数をfalseにすると最後の行には改行を付けない。要素内の改行は許可しない。空の配列は空のファイルとして書き込む。

テキストの引数はstrまたはchar、行の配列はstr配列である。引数は記述順に指定する。書き込む文字列はファイルを開く前に検査するが、書き込み開始後の変更を巻き戻す機能ではない。

## 使用方法

次のコードをmain.qyとして保存する。インデントにはタブを使用する。

```text
using io

int main();
	print("Hello, World!")
	return 0
```

コンパイルして実行する。

```sh
./qyc main.qy -o main
./main
```

Windows向けの実行ファイルは、出力先を指定して生成する。

```sh
./qyc main.qy --target windows -o main.exe
```

出力先は--targetで指定する。指定しない場合は、コンパイラを実行しているOSに合わせる。-Sはアセンブリの出力、-Iはモジュールの探索先の追加に使用する。--helpで使い方を確認できる。

mainで実行時引数を受け取る場合は、str配列を宣言する。実行ファイル名は引数に含まれない。

```text
int main(str args[]);
	for(0 <= i < args.length, i++);
		io.print(args[i])
	return 0
```

## 言語仕様

関数や制御文の本体は、ヘッダー末尾のセミコロンとインデントで記述する。条件分岐にはif、繰り返しにはforとwhileを使用する。

```text
int main();
	int values[] = [0, 1, 2, 3]
	for(values.length > i >= 0, i--);
		io.print(values[i])
	return 0
```

整数は8bitから128bitまでを扱い、intは32bitである。unsignedで符号なしを指定できる。小数はhalf、float、double、quadrupleに対応する。複素数はcomplex、complex_64、complex_128で扱う。

charはUnicodeの1書記素、strは書記素の列である。boolは真偽値、voidは()だけを値に持つ型である。これらの型は配列でも使用できる。

autoは初期値から型を推論する。未宣言の名前への代入でも型を推論し、宣言後の型は固定される。関数の引数や戻り値の型も省略できる。constは定数、refは参照渡しに使用する。

modelではフィールドとメソッドを定義する。生成時の値は型名を繰り返さず、Point p = {x=3.0, y=4.0}のように記述できる。publicとprivateで公開範囲を指定し、readonlyで変更を制限する。共通の操作はinterfaceで定義できる。

enumはmodelから独立した列挙型である。列挙値による分岐にはmatchを使用する。

importでモジュールを読み込み、usingで名前の省略や別名を指定する。探索先はエントリファイルのディレクトリを起点とし、-Iで追加できる。

```text
import geometry.circle
using geometry.circle as circle
```

and、or、xorは、boolでは論理演算、整数ではビット演算として動作する。シフトには<<と>>を使用する。整数は2進数、8進数、10進数、16進数で記述でき、接頭辞は0b、0o、0d、0xである。接頭辞を省略した場合は10進数となる。

## 標準モジュール

io、math、lib、timeはimportなしで使用できる。名前を省略する場合はusingが必要である。using ioを記述した場合はprintとinputを使用でき、記述しない場合はio.printとio.inputを使用する。

ioはコンソールとファイルの入出力を扱う。io.pathはパスの操作、io.dirは作業ディレクトリの取得と変更に使用する。パスはstrで扱う。

ファイルはio.file.openで開き、io.file.closeで明示的に閉じる。closeを確認できない経路はコンパイルエラーとなる。テキストとバイナリの読み書きに対応し、位置、サイズ、更新日時を取得できる。I/Oエラーはtry、case、finallyで処理する。パス指定の一括読み書きでは、ファイルの開閉をライブラリが行う。

mathは数学関数を提供する。lib.sizeは値のビットサイズ、lib.lengthは文字列や配列の長さ、数値の整数部分の桁数を返す。

lib.randomは型名と不等式で乱数を生成する。lib.random.intは整数、lib.random.doubleは小数、lib.random.boolは真偽値を返す。シードを指定した生成と、独立した生成器にも対応する。

timeは時刻の取得、経過時間の計測、待機に使用する。Stopwatchによる計測と、日時の変換にも対応する。

## 開発状況

現在のQYNOは開発中のプロトタイプである。仕様は予告なく変更される場合がある。

動作、互換性および特定の用途への適合性を保証しない。仕様、実装および本ソフトウェアの利用により生じた損害について、法令で認められる範囲で作者は責任を負わない。

開発はdevブランチで行い、確認した変更をmainブランチへ反映する。

## ライセンス情報

QYNO本体はBSD 2-Clause Licenseで公開する。著作権はhiraken0427に帰属する。

以下はQYNO本体のライセンス全文である。

```text
BSD 2-Clause License

Copyright (c) 2026, hiraken0427
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

UnicodeのデータにはUnicode License V3が適用される。著作権表記とライセンス全文は[grapheme_data.h](grapheme_data.h)に収録している。

GCC実行時コードにはGPLv3以降とGCC Runtime Library Exception 3.1が適用される。著作権表記、ライセンス全文および配布元は[wide_blob.h](wide_blob.h)に収録している。
