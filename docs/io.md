# 標準モジュール io

## 標準モジュールの方針

よく使う機能は標準モジュールとして用意し、import なしで利用可能にする。
現在の標準モジュールは `io`・`math`・`lib`。
標準モジュールの登録は組み込み関数の一覧から行い、ユーザーの `.qy` ファイルを探索しない。
明示的な `import io` / `import math` / `import lib` も許可するが、必須ではない。

標準モジュールの修飾名と using は依存ファイル内でも利用できる。
using の省略名・別名は、通常のモジュールと同じファイル・ブロックスコープに従う。

## 修飾名・using・別名

```text
using io

int main();
	str name
	print("Hello, World!")
	input("What your name? > {name}")
	print("Hello, {name}!")
	return 0
```

同じ処理を修飾名だけで記述できる。

```text
int main();
	str name
	io.print("Hello, World!")
	io.input("What your name? > {name}")
	io.print("Hello, {name}!")
	return 0
```

| 記述 | 意味 |
| --- | --- |
| `io.print(...)` / `io.input(...)` | 修飾名で呼び出す。import・using は不要 |
| `using io` | io の公開名をまとめて省略可能にする |
| `using io.print, io.input` | 選択して省略可能にする |
| `using io as console` | `console.print(...)` / `console.input(...)` と呼べる |

0.27.0から裸の `print(...)` / `input(...)` にもusingが必要。
`using io`・選択usingを宣言するか、修飾名で記述する。importだけでは省略名は導入しない。
いずれの表記でも入出力の動作・出力書式・エラー時の動作は変わらない。
関数そのものの別名・第一級関数は導入しない。`using io.print as out` は未対応。

## io.print

出力文として使用する。戻り値はなく、代入や式の値には使用できない。

```text
int main();
	int n = 7
	io.print("{n} の2乗は {n^2}")
	io.print(n + 1, end="!")
	io.print()
	return 0
```

- 文字列リテラル内の `{式}` を評価して出力する。
- 文字列以外の式の出力も、既存の型ごとの書式を使用する。
- 引数省略時は本文を出力しない。
- 既定の終端は改行。`end=値` で変更でき、終端の文字列リテラルでも `{式}` が使える。
- エスケープ、日本語・絵文字、埋め込まれた NUL、小数の丸め・表示書式を維持する。

## io.input

値を読み取る形式と、入力先の変数を指定する形式の両方を維持する。

```text
int main();
	int age = io.input("age> ")
	str name = io.input("name> ")
	io.print("{name}: {age}")
	return 0
```

値を読み取る形式では、代入先・引数・戻り値などの型の文脈に応じて読み取る。
プロンプトは省略可能。既存の int / float / double / bool / str / char の扱いを変更しない。

入力先を引数として直接指定することもできる。変数・配列要素・ref 変数が対象となる。
int / float / double / bool / str / char の入力に対応する。

```text
int main();
	int num
	int matrix[2][3]
	io.input(num)
	io.input(matrix[0][0])
	io.input("入力> {matrix[0][1]}")
	io.print(matrix[0])
```

入力先を指定する形式の `{}` 内では、変数名に加えて多次元配列の要素も使える。
添字式は入力前に左から順に1回だけ評価し、読み取り後も保存した添字へ代入する。
たとえば `io.input("{i}, {items[i]}")` は、入力前の i で選んだ要素へ書き込む。
文字列の添字を入力先にする場合は、Unicode拡張書記素クラスタ1個だけを受け取る。
定数や配列全体への直接入力は許可しない。

**変数をプロンプトに使う場合は `prompt=` を明示する。**
従来の `name = io.input(message)` は、message が変数なら直接入力として解釈されるため、
`name = io.input(prompt=message)` へ変更する必要がある。

| 記述 | 動作 |
| --- | --- |
| `io.input(num)` | num の型で入力して代入する。結果は bool |
| `io.input(items[i])` | 選んだ要素の型で入力して代入する。結果は bool |
| `io.input("入力> {items[i]}")` | プロンプトを表示して入力先へ代入する。結果は bool |
| `num = io.input("入力> ")` | 入力値を num の型で返す |
| `num = io.input(prompt=message)` | message を表示し、入力値を num の型で返す |
| `num = io.input(123)` | 123 を表示し、入力値を num の型で返す |

プロンプトは int / float / double / bool / str / char を受け付け、
print と同じ書式で改行を追加せずに表示する。
直接指定した文字列リテラルの `{}` は入力先を表す。
`prompt=` を付けた値や通常の式を、入力先として解釈することはない。

```text
int main();
	int age
	if(io.input("age> {age}"));
		io.print("age = {age}")
	else;
		io.print("入力を確認してください")
	return 0
```

直接入力と入力先を指定する形式は bool を返す。条件・値として使う場合、形式不一致や EOF は false。
複数の欄を読み取る場合、全体が成功してから変数を更新し、失敗時は既存値を保持する。
文として呼び出した場合は、失敗を既存の実行時エラーとして扱う。

入力欄の区切り、右側の枠、`ref` 変数の `.value`、CRLF の扱いも維持する。
値の形式の失敗・EOF は従来と同じ終了コード 65。その他の失敗も既存の診断・終了コードに従う。
端末ではカーソル移動、挿入・削除、日本語・絵文字の編集、EOF / Ctrl+C と端末設定の復元を維持する。

## 名前解決と制約

- `io` と `math` を変数・関数・別名で上書きできない。
- 同名の io.qy / math.qy があっても標準モジュールを置き換えない。
- using による名前の衝突とスコープは通常のモジュールと同じ規則で検証する。
- `using io` で math の関数が省略可能になることはない。
- 修飾された print / input は実際に io の公開名を参照することを確認する。
  `math.print(...)` やスコープ外の別名は io の呼び出しとして扱わず、コンパイルエラーにする。

## io.file

0.25.2 では、import 不要の標準サブモジュール io.file を利用できる。

- モジュール名は `io.file`。
- `open` / `close` と file ハンドルを用意する。モードは r / w / a。
- `read` は現在位置から末尾までの UTF-8 テキストを読む。
- `readline(f, ref line)` は改行を除いた1行を読み、行があれば true、EOF なら false。
- `write` / `exists` / `is_open`、読み書き失敗や閉じたハンドルの診断も扱う。
- 正常終了する全経路で明示的な close を確認できないコードはコンパイルエラー。

API、モード、改行・EOF、ハンドル共有、パス、close の検査は
[io.file の仕様](io-file-design.md) を参照。

## 検証

```bash
make test-io
make test-lineedit
make test-modules
make test
```

入出力の回帰テストは従来形・修飾名・using・選択 using・別名の各表記で、
出力、終了コード、エラー、入力先の更新を同じ期待値と比較する。
端末編集は擬似端末上で従来形・修飾名・別名の 3 形式をそれぞれ検証する。

## 整数の基数指定（0.26.5）

print / end= / file.write の整数埋め込みに `{n:0b}` / `{n:0o}` / `{n:0d}` / `{n:0x}` を指定できる。
接頭辞付きの2進・8進・10進・16進で出力し、省略時は従来どおり接頭辞なしの10進数になる。

input でも `{n:0x}` のように入力先の基数を指定できる。指定した基数の接頭辞は省略可能。
指定なしのint入力は接頭辞を自動判定し、接頭辞がなければ10進数として読む。先頭0だけでは8進数にしない。
入力失敗時の全体保持、参照先の再検査、boolの戻り値とエラー65の規則は維持する。

`str(n, base=16, prefix=true)` と `int("0xff")` でも相互変換できる。
詳細は [整数の基数表記・変換・入出力](integer-bases-design.md) を参照。

## io.pathとlib（0.27.0）

`io.path` はimport不要で、join / basename / dirname / extension / stem / is_absolute /
is_relative / normalize / with_extensionを提供する。ファイルへアクセスせず、ターゲットOSのパス規則で処理する。
`using io` は `path`・`file` の名前空間も導入するが、それらの関数は再帰展開しない。
`join(...)` と省略するには `using io.path` または `using io.path.join` が必要。

`lib.size(value)` は値のビット数、`lib.length(value)` は文字列の書記素数・配列長・数値の整数部分の桁数。
詳細と後続の入出力拡張候補は [0.27.0仕様](standard-library-0270-design.md) を参照。

## バイナリファイル（0.27.1）

io.file.openのrb / wb / abとread_bytes / write_bytesを利用できる。
read_bytesは必須の非負countを上限に読み、実際の長さのunsigned int_8配列を返す。
EOFは空配列、エラーは診断付き停止。write_bytesは全量を書いてバイト数をint_64で返す。
モードとテキストAPIは混在させず、明示closeの検査も維持する。
詳細: [バイナリI/O仕様](binary-file-design.md)。
