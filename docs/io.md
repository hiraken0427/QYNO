# 標準モジュール io

## 標準モジュールの方針

よく使う機能は標準モジュールとして用意し、import なしで利用可能にする。
現在の標準モジュールは `io` と `math`。
標準モジュールの登録は組み込み関数の一覧から行い、ユーザーの `.qy` ファイルを探索しない。
明示的な `import io` / `import math` も許可するが、必須ではない。

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

従来の裸の `print(...)` / `input(...)` は互換性のため引き続き利用可能。
新しいコードでは using または修飾名で io の使用を明示する書き方を推奨する。
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

```text
int main();
	int age
	if(io.input("age> {age}"));
		io.print("age = {age}")
	else;
		io.print("入力を確認してください")
	return 0
```

入力先を指定する形式は bool を返す。条件・値として使う場合、形式不一致や EOF は false。
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

## 今後の io.file

今回の実装はコンソール入出力と標準モジュールの整備まで。io.file は未実装。
次のファイル API の方針を記録する。

- モジュール名は `io.file`。
- `open` / `close` と file ハンドルを用意する。open には r モードを用意する。
- `read` は現在位置から末尾までの UTF-8 テキストを読む。
- 追加の `readline` は現在位置から改行コードまでを UTF-8 で読み出す。
- `write` / `exists`、読み書き失敗や閉じたハンドルの診断も扱う。

改行の返却方法・EOF の表現など、readline の詳細はファイル API の実装時に定義する。

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
