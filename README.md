# QYNO 0.26.6

C99 で実装した、Linux x64 / Windows x64 向けのコンパイラ。
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
	print("Hello, QYNO!")
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

[VSCode Option](vscode-extensions/README.md) は QYNO 0.26.6 のハイライト、
ユーザー定義の関数名・変数名の補完、定義への移動、引数ヒントに対応する。
[vscode-extensions/0.26.6_VSCode_option.vsix](vscode-extensions/0.26.6_VSCode_option.vsix) を
VS Code の「VSIX からのインストール…」で指定する。

## ローカル変数の型推論

`auto count = 3`、`auto text = "QYNO"`、`auto values = [1, 2]` のように、
初期値から型を推論できる。ローカルの `const auto LIMIT = 10` にも対応する。
推論後の型は固定され、配列・モデルのコピーと file の明示 close の規則も維持する。
この段階の int は従来どおり64bit。型名を省略した `y = 3` や、関数の型推論は後続の段階で実装する。
詳細は [型拡張の実装順と現時点の仕様](docs/types-roadmap.md)、
実行例は [examples/types/auto.qy](examples/types/auto.qy) を参照。

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
現在は `io`（コンソール入出力）、`io.file`（UTF-8ファイル操作）と `math`（数学関数）が該当する。

```text
int main();
	str name
	io.input("name> {name}")
	io.print("Hello, {name}!")
	return 0
```

`using io` を書けば `print` / `input` に省略できる。
従来の裸の呼び出しも互換性のため維持する。
使用例は [examples/io/greeting.qy](examples/io/greeting.qy)、詳細は [io の仕様](docs/io.md) を参照。

## ファイル操作

`io.file.open(path, mode)` のモードは r / w / a。省略時は r。
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
	print((flags and 4) != 0)
	print("{flags:0b}, {flags:0o}, {flags:0d}, {flags:0x}")
	str text = str(flags, base=16, prefix=true)
	int restored = int(text)
	input("hex> {flags:0x}")
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
