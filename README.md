# QYNO 0.25.1

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
現在は `io`（コンソール入出力）と `math`（数学関数）が該当する。

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

## 開発ブランチ

通常の開発は `dev` で行い、検証した変更を `main` へ反映する。
Git の運用方針は [AGENTS.md](AGENTS.md) を参照。
