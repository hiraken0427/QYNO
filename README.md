# QYNO 0.27.3

C99とGCCの128bit数値拡張で実装した、Linux x64 / Windows x64 向けのコンパイラ。
UTF-8 の `.qy` ソースから実行ファイルを直接生成する。

## ビルドとテスト

Linux x86_64 では GCC、Make、Bash、Python 3 が必要。
Python はテストに使用し、追加パッケージは不要。

```bash
make -j4
make test
```

## 実行

```text
using io

int main();
	io.print("Hello, QYNO!")
	return 0
```

上記を `hello.qy` として保存する。インデントはタブを使う。

```bash
./qyc hello.qy -o hello
./hello
```

`--target windows` で Windows 用 PE32+、`--target linux` で Linux 用 ELF64 を生成する。
既定はコンパイラを実行している OS。`-S` はアセンブリ出力、`--help` はオプション一覧。

## VS Code 拡張

[QYNO 0.27.3](vscode-extensions/README.md) はハイライト、ユーザー定義の関数名・変数名の補完、
定義への移動、引数ヒント、セミコロン後の自動インデント、qyc連携の構文・型エラーの赤線表示に対応する。
[vscode-extensions/0.27.3_VSCode_option.vsix](vscode-extensions/0.27.3_VSCode_option.vsix) を
VS Code の「VSIX からのインストール…」で指定する。
診断には別途qycが必要。PATHにない場合は拡張設定の `qyno.compilerPath` に実行ファイルを指定する。

## モジュールの公開制御（新段階7）

トップレベルの関数・const・model・interfaceにpublic / privateを指定できる。
省略時はpublic。privateは宣言元モジュール内で使用し、外部の参照・usingからは隠す。
公開APIにprivate型が露出する場合も検査する。
[仕様](docs/module-visibility.md)と[実行例](examples/module-visibility/main.qy)を参照。

## 書記素クラスタとvoid（新段階6）

char / strの長さ・添字・入力をUnicode 17の拡張書記素クラスタ単位へ統一した。
void / Voidは唯一の値 `()` を持ち、変数・配列・ref・auto・関数引数にも使用できる。
[仕様](docs/graphemes-void.md)と[実行例](examples/types/graphemes-void.qy)を参照。

## 128bit整数・half・quadruple（新段階5）

int_128 / unsigned int_128、half（binary16）、quadruple（binary128）に対応する。
配列・ref・定数・入出力・forで使用でき、広い小数をdoubleへ一度丸めず解析する。

```text
int main();
	int_128 n = 18446744073709551616
	half small = 1.5
	quadruple precise = 1.0000000000000000000000000000000002
	io.print("{n} {small} {precise}")
	return 0
```

詳細: [128bit整数・half・quadruple](docs/wide-types.md)。
検証: `make test-wide-types`。

## 複素数（新段階4）

`complex` / `complex_64`はfloat2成分、`complex_128`はdouble2成分。
`complex(real=3.0, imag=4.0)`、推奨の`3.0 + i4.0`、`3.0 + 4.0i`を使用できる。
四則演算・整数乗・絶対値・主平方根、配列・ref・型推論・入出力に対応する。
`.real` / `.imag`で成分を読み、更新は複素数全体へ代入する。
例は [examples/types/complex.qy](examples/types/complex.qy)、
詳細は [型の仕様](docs/types-roadmap.md) を参照。

## 整数型（新段階3）

`int_8` / `int_16` / `int_32` / `int_64` と、`unsigned int_8` などの符号なし整数型を使用できる。
`int` = `int_32`、`unsigned int` = `unsigned int_32`。64bitの値には型を明示する。
配列・モデル・ref・型推論、math・入出力・基数変換も対応する。
狭い型や符号の異なる型への変換は範囲を検査し、暗黙の切り捨てを行わない。
演算結果は型のbit幅で折り返し、forの範囲外更新ではループを終了する。
32bitへの移行例は [examples/types/default-integers.qy](examples/types/default-integers.qy)。
`.length`と`io.file.write`のバイト数は`int_64`を返す。
型別の例は [examples/types/integers.qy](examples/types/integers.qy)、詳細は
[整数型の仕様](docs/types-roadmap.md) を参照。

## ローカル変数の型推論

`auto count = 3`、`auto text = "QYNO"`、`auto values = [1, 2]` のように、
初期値から型を推論できる。`const auto LIMIT = 10` と `const LIMIT = 10` は、
ローカルとファイル先頭の両方に対応する。定数の前方参照・import / using による参照も型を含めて解決する。
推論後の型は固定され、配列・モデルのコピーと file の明示 close の規則も維持する。
未宣言の名前への `y = 3` も、`auto y = 3` と同様にローカル変数を宣言する。
既存の名前への代入では型と参照先を維持し、内側で別変数を作る場合は `auto y = ...` または型を明示する。
型名を省略した宣言が未使用の場合は警告し、似た名前の既存変数があれば候補を示す。
intは32bit。大きな整数リテラルはint_64、さらに大きい正の値はunsigned int_64へ推論する。

関数の戻り値は `auto square(int x) = x^2` または `square(int x) = x^2` と書いて推論できる。
ブロック形式とモデルのメソッドにも対応する。
`square(x) = x^2` や `auto square(auto x) = x^2` のように引数の型も省略でき、
呼び出し時の型ごとに関数を具体化する。`int square(x)` は戻り値の int 制約を維持する。
戻り値の推論が循環する再帰では必要な関数の戻り値型を明示する。
詳細は [型拡張の実装順と現時点の仕様](docs/types-roadmap.md)、
実行例は [examples/types/auto.qy](examples/types/auto.qy) を参照。
型名省略とスコープの例は [examples/types/implicit.qy](examples/types/implicit.qy) を参照。
ファイル先頭の定数とモジュールの例は [examples/types/constants.qy](examples/types/constants.qy) を参照。
関数の戻り値推論の例は [examples/types/functions.qy](examples/types/functions.qy) を参照。
引数の型推論の例は [examples/types/generic.qy](examples/types/generic.qy) を参照。

## モデル型

`model` はフィールドとメソッドをまとめる値型。通常の代入・引数・戻り値はコピー、
`ref` は呼び出し元を参照する。メソッドの `this` は暗黙の ref 引数になる。

```text
model Point;
	double x = 0.0
	double y = 0.0

	void move(double dx, double dy);
		this.x += dx
		this.y += dy

int main();
	Point p = {x=3.0, y=4.0}
	Point copy = p
	copy.move(1.0, 2.0)
	io.print(p)       // Point{x=3.0, y=4.0}
	io.print(copy)    // Point{x=4.0, y=6.0}
	return 0
```

配列・入れ子・フィールドの ref 渡しにも対応する。省略したフィールドは既定値で初期化する。
フィールド・メソッドの `public` / `private` に対応。省略時は public、
private は宣言元のモデル内部だけで利用できる。自動表示から private を除外し、コピー・比較では保持する。
`public` と `private` は予約語になる。
詳細は [モデルの仕様](docs/models.md)、実行例は [examples/models/main.qy](examples/models/main.qy) を参照。

const モデル、readonly メソッド、`ref readonly` 引数に対応する。
const の値はコンパイル時に確定し、readonly の経路からは入れ子・配列を含めて変更できない。
独立したコピーは変更できる。file の共有ハンドルの持ち出しも検査する。
[読み取り専用の仕様](docs/model-readonly-design.md) と
[実行例](examples/models/readonly.qy) を参照。

0.26.3 ではモデル生成時の `init` を定義できる。`Counter counter = {initial=3}` のように、
型名を繰り返さずに名前付き引数を渡し、private の設定や入力検証を行う。
コピーでは init を再実行せず、必須引数・未完了フィールド・file の close 責任も検査する。
[カスタム初期化の仕様](docs/model-initialization-design.md) と
[実行例](examples/models/initialization.qy) を参照。

## インターフェース

0.26.4 では `interface Shape;` と `model Rectangle implements Shape;` に対応する。
`ref Shape` / `ref readonly Shape` 引数で異なるモデルを共通の操作から利用できる。
具体型へコンパイル時に特殊化し、値コピー・readonly・file の明示 close の規則を維持する。

```text
interface Shape;
	readonly double area();

model Rectangle implements Shape;
	double width
	double height
	readonly double area() = this.width * this.height

void print_area(ref readonly Shape shape);
	io.print(shape.area())

int main();
	Rectangle rect = {width=3.0, height=4.0}
	print_area(ref rect)
	return 0
```

interface は参照引数の契約に限定し、interface 型の変数・異種配列は導入しない。
詳細は [インターフェースの仕様](docs/interfaces-design.md)、
実行例は [examples/models/interfaces.qy](examples/models/interfaces.qy) を参照。

## モジュール

`import geometry.circle` でモジュールを読み込み、
`using geometry.circle as circle` でそのファイルやブロック内の別名を宣言する。
関数・定数は完全修飾名でも利用できる。

```bash
./qyc examples/modules/app/main.qy -I examples/modules/lib -o /tmp/qyno-modules-example
/tmp/qyno-modules-example
```

探索ルートはエントリファイルのディレクトリ、その後に `-I` の指定順。
`-I` は複数指定でき、親ディレクトリにあるモジュールも明示的に追加できる。

詳しくは [モジュールの仕様](docs/modules.md) と [テストの説明](tests/README.md) を参照。

## 標準モジュール

よく使う機能は標準モジュールとして用意し、import なしで利用できる。
現在は `io`（コンソール入出力）、`io.file`（UTF-8ファイル操作）、`io.path`（パス文字列操作）、
`math`（数学関数）、`lib`（サイズ・長さの取得）が該当する。

```text
int main();
	str name
	io.input("name> {name}")
	io.print("Hello, {name}!")
	return 0
```

`using io` を書けば `print` / `input` に省略できる。
0.27.0から裸の呼び出しにはusingが必要。importだけでは関数名を省略できない。
使用例は [examples/io/greeting.qy](examples/io/greeting.qy)、詳細は [io の仕様](docs/io.md) を参照。

## ファイル操作

`io.file.open(path, mode)` のモードは r / w / a（UTF-8）、rb / wb / ab（バイナリ）。省略時は r。
`readline(f, ref line)` は、1行読めれば true、EOFなら false を返す。
取得した file は、正常終了へ至る全経路で明示的な close が必要。
コンパイラが close を確認できない場合はエラーになる。

```text
using io.file as fs

int main();
	file source = fs.open("input.txt")
	str line
	while(fs.readline(source, ref line));
		io.print(line)
	fs.close(source)
	return 0
```

相対パスは実行時の作業ディレクトリを基準にする。
詳細は [io.file の仕様](docs/io-file-design.md)、
実行例は [examples/io/files.qy](examples/io/files.qy) を参照。

## 配列と参照渡し

配列全体は `change(ref matrix)` と `void change(ref int matrix[][]);` で渡す。
1要素は `increase(ref matrix[i][j])` と `void increase(ref int value);` で渡せる。
int・float・double・bool・str・char の配列で同じ規則を使う。

`matrix.add()` は初期値を持つ要素を1個追加する。上限を指定しない2次元配列なら空の行を追加する。
直接入力は `io.input(matrix[i][j])`、表示付きの入力は `io.input("入力> {matrix[i][j]}")` と書ける。
変数を表示用のプロンプトにする場合は `io.input(prompt=message)` と明示する。

詳細は [配列と参照の仕様](docs/arrays.md)、使用例は
[examples/arrays/references.qy](examples/arrays/references.qy) を参照。

## 範囲 for

開始側を含まない `<` / `>` も使用できる。開いた開始側ではヘッダーの更新を 1 回適用してから始める。

```text
int main();
	int values[] = [0, 1, 2, 3]
	for(values.length > i >= 0, i--);
		io.print(values[i])
	for(0.0 < double x <= 1.0, x += 0.25);
		io.print(x)
	return 0
```

整数の範囲外への更新は折り返さず終了する。小数の非有限値・前進できない更新は診断する。
bool は数値範囲のループ変数にしない。詳細は [for の仕様](docs/for-open-start-design.md) を参照。

## ビット演算と基数表記

0.26.5以降、`and` / `or` / `xor` を bool と int で共用する。
boolのand / orは短絡し、intのビット演算は両辺を評価する。ビット反転はbnot、シフトは << / >>。
シフト量は0〜63、右シフトは符号を保つ算術右シフトになる。

```text
using io

int main();
	int flags = 0b0011 or 0o4
	io.print((flags and 4) != 0)
	io.print("{flags:0b}, {flags:0o}, {flags:0d}, {flags:0x}")
	str text = str(flags, base=16, prefix=true)
	int restored = int(text)
	io.input("hex> {flags:0x}")
	return 0
```

0b / 0o / 0d / 0xの整数リテラルと、int / strの相互変換、print / input / file.writeの基数指定に対応する。
指定のない表記・出力は10進数。整数入力は接頭辞を判定し、接頭辞がなければ10進数として読む。
詳細は [ビット演算とシフト演算](docs/bitwise-design.md)、
[整数の基数表記・変換・入出力](docs/integer-bases-design.md) を参照。
実行例は [bitwise.qy](examples/numbers/bitwise.qy) と [bases.qy](examples/numbers/bases.qy)。

## 開発ブランチ

通常の開発は `dev` で行い、検証した変更を `main` へ反映する。
Git の運用方針は [AGENTS.md](AGENTS.md) を参照。

## 標準ライブラリ（0.27.0）

裸のprint/inputにも `using io` または選択usingが必要。修飾名の `io.print(...)` / `io.input(...)` はimport・using不要。
`math`・`lib`・`io.path`でも同じ規則を使う。importだけでは関数名を省略できない。

`lib.size(value)` は型のビット幅・UTF-8本体・現在の全配列要素などの合計を返す。
メモリ管理情報・予約容量・ABIスロットのサイズは含まない。
`lib.length(value)` は文字列の書記素数、配列の外側の要素数、数値の整数部分の10進桁数をint_64で返す。

```text
using io

int main();
	int matrix[2][3]
	print(lib.size(matrix))       // 192
	print(lib.size(matrix[0][0])) // 32
	print(lib.length(42))         // 2
	print(lib.length(3.1415))     // 1
	print(io.path.join("data", "settings.json"))
	return 0
```

io.pathは結合・ファイル名・親・拡張子・stem・絶対/相対判定・normalize・拡張子置換の基本9関数を提供する。
詳細: [0.27.0仕様と拡張候補](docs/standard-library-0270-design.md)、[実行例](examples/stdlib/main.qy)。

## バイナリI/O（0.27.1）

`io.file.read_bytes(source, count)` は最大countバイトをunsigned int_8の1次元配列で返す。
正のcountでEOFなら空配列、不足分の0埋めは行わない。受け取り先は上限なしのdata[]を推奨する。
`io.file.write_bytes(destination, data)` は配列を1要素1バイトで全量書き、バイト数をint_64で返す。

read_bytesはrb、write_bytesはwb / abで開いたハンドルを使用する。
UTF-8検証・改行変換・BOM操作はせず、任意のバイトを保持する。パスのUTF-8検証と明示closeの検査は維持する。

詳細: [バイナリI/O仕様](docs/binary-file-design.md)、[実行例](examples/io/binary.qy)。

## enumとmatch（0.27.1追加改訂）

独立した列挙型を `enum sex;` で定義し、typedefなしで `sex gender` のように使用できる。
先頭0からの連番または明示値に対応し、値0のメンバーを既定値とする。
名前の表示・入力・明示変換、配列・model・ref・readonly・const・型推論・モジュール公開制御に対応する。

```text
enum sex;
	undetermined
	male
	female=3

int main();
	sex gender=female
	match(gender);
		case undetermined,male;
			io.print("other")
		case female;
			io.print(gender) // female
	return 0
```

matchはenumの全メンバーを列挙するかelseが必要。重複と列挙漏れはコンパイルエラーになり、対象式は1回だけ評価する。
メソッド定義・呼び出しの括弧は従来どおり必要。回復可能I/Oは今回追加していない。
[詳細な仕様](docs/enums-design.md)・[ユーザー例](examples/enums/main.qy)。

## 回復可能I/Oとtry（0.27.2）

標準の`io.ErrorCode`と`io.Error`でI/O失敗を扱う。
`io.file.try_open(path, ref destination, ref error, mode="r")`はboolで成否を返す。
通常のopen/read/write等は`try`の`case`で捕捉し、`finally`で明示的なcloseを共通化できる。

```text
using io

int main();
	file source
	try(source = file.open("settings.txt")) as error;
		print(file.read(source))
	case(ErrorCode.not_found);
		print("設定ファイルがありません")
	case();
		print("{error.operation}: {error.message}")
	finally;
		file.close(source)
	return 0
```

case内の`throw;`で再送出する。try_openのfalse・EOFは送出しない。
回復するエラー経路のclose不足もコンパイルエラーになる。
詳細は[回復可能I/Oとtryの仕様](docs/recoverable-io-design.md)、
[実行例](examples/recoverable-io/main.qy)を参照。

## ファイル位置・時刻・時間計測（0.27.3）

`io.file.tell / seek / size / modified_at`を追加。位置とサイズはint_64のバイト数、
更新日時はquadrupleのUNIX秒数。seekはバイナリの先頭〜EOFを扱い、先読みを考慮する。
標準`time`はimport不要で、短い名前には`using time`が必要。
時計・単位変換・待機・Stopwatch・UTC/固定オフセットのDateTime・文字列処理を提供する。

```text
using io
using time

int main();
	Stopwatch watch
	watch.start()
	sleep(milliseconds(20))
	print(format_duration(watch.stop()))
	print(isoformat(now(), 540))
	return 0
```

処理時間はmonotonic、日時の記録はnowを使う。秒数の戻り値はquadrupleで固定し、
doubleが必要なら明示変換する。時計の実際の精度はOSとハードウェアに依存する。
仕様: [ファイル位置](docs/file-position-design.md)、[time](docs/time-design.md)。
例: [時間計測](examples/time/main.qy)、[ファイル位置と更新日時](examples/io/positions.qy)。
