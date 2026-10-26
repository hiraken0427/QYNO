# QYNO

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

## 開発ブランチ

通常の開発は `dev` で行い、検証した変更を `main` へ反映する。
Git の運用方針は [AGENTS.md](AGENTS.md) を参照。
