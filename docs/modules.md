# モジュール

## 読み込みと名前管理

ユーザー定義モジュールはファイル 1 つを単位として扱う。`import` は依存ファイルの読み込みを宣言し、
`using` はこのファイル・ブロック内で使う省略名や別名を宣言する。
import したファイルの内容をテキストとして展開せず、関数・定数の名前はモジュールごとに区別する。

```text
import geometry.circle
using geometry.circle as circle

int main();
	io.print(circle.area(2.0))
	io.print(geometry.circle.PI)
	return 0
```

`geometry/circle.qy` の例：

```text
const double PI = 3.141592653589793

double area(double radius) = PI * radius^2
```

名前空間宣言は不要。探索ルートから見たパスがモジュール名になる。

| 記述 | 意味 |
| --- | --- |
| `import geometry.circle` | `geometry/circle.qy` を読み込む |
| `import geometry.circle, geometry.square` | 複数のモジュールを読み込む |
| `using geometry.circle as circle` | モジュールを `circle` と呼べるようにする |
| `using geometry.circle.area` | 関数を `area(...)` と省略できる |
| `using geometry.circle.PI` | 定数を `PI` と省略できる |
| `using geometry.circle` | このモジュールの全公開名を省略できる |

通常は別名か選択的 using を推奨する。`using geometry.circle` は全公開名の展開であり、
`circle` という別名を自動作成する意味ではない。
別名はモジュールだけに付けられる。関数・定数への `as` は未対応。
既存の別名を経由した `using circle.area` や `using circle as c` も利用できる。

## 探索ルート

`import a.b` は各ルートの `a/b.qy` を探す。探索順は次のとおり。

1. エントリ `.qy` ファイルの実体があるディレクトリ。
2. コマンドラインの `-I` で指定されたディレクトリ。複数ある場合は指定順。

最初に見つかったファイルを使用する。親ディレクトリを自動探索せず、
すべての依存モジュールで同じ探索ルートを使用する。
依存ファイル自身のディレクトリへ探索起点を変更しない。

```text
project/
  app/
    main.qy
  modules/
    geometry/
      circle.qy
```

`project` で実行する場合：

```bash
./qyc app/main.qy -I modules -o app/program
./app/program
```

`-Imodules` の形式も使用できる。相対指定の `-I` はコンパイラを起動したディレクトリ基準。
ファイルの探索はエントリのディレクトリと明示されたルートだけで行うため、
エントリの絶対パスと `-I` の絶対パスを渡せば起動ディレクトリに依存しない。

モジュール名の各部分は予約語を除く識別子。大文字・小文字を区別する。
パス文字列、`<...>`、相対 import、`--module-root`、フォルダ一括読み込みは未対応。

## スコープ・公開範囲

- import はトップレベルでのみ記述でき、そのファイル全体で有効。
- トップレベルの using はファイル全体で有効。import・using を定義の前にまとめる書き方を推奨する。
- ブロック内の using は、宣言位置以降の同じブロックと内側のブロックで有効。
  他の関数・外側のブロック・隣のブロックへ漏れない。
- import 先が使用している import・using は呼び出し側に引き継がれない。
  同じ依存先を参照する場合は各ファイルで明示する。
- トップレベルの関数・定数・model・interface・enumは省略時public。0.26.16からpublic / privateを指定できる。詳細は[モジュールの公開制御](module-visibility.md)を参照。exportキーワードは導入しない。
  定数や定数配列は他のモジュールからも変更できない。
- ライブラリに main は不要。main を定義できるのはエントリファイルのみで、
  エントリには従来どおり引数なしの `int main();` が必要。
- import 時に実行する文・可変グローバル・モジュール初期化処理は導入しない。

```text
import geometry.circle

int main();
	if(true);
		using geometry.circle as circle
		io.print(circle.area(2.0))
	io.print(geometry.circle.area(3.0))
	return 0
```

## 名前の衝突

`a.run` と `b.run` は共存できる。両方を `run` に省略する using は、
未使用であっても衝突エラーになる。後の宣言で上書きしない。
モジュールの別名・import の先頭部分・標準モジュール名と、ローカル変数や関数・定数が
同じ名前になる場合もエラーにする。外側の名前を隠す形の using は認めない。

同じ定義への重複 import・重複 using は許可する。異なるブロックで同じ別名を使うこともできる。
ファイルの実体を正規化して重複読み込みを防止する。
シンボリックリンク等で同じ実体に別のモジュール名を付けた場合はエラーにする。

## import 不要の標準モジュール

よく使う機能は標準モジュールとしてあらかじめ用意し、import なしで利用可能にする。
現在は `io`・`math`・`lib` と、`io.file`・`io.path` が該当する。明示的な import も許可するが必須ではない。
標準モジュールでも、using・選択 using・モジュールの別名を利用できる。

```text
using io

int main();
	io.print(math.sqrt(9))
	io.print("Hello, World!")
	return 0
```

0.27.0から全標準関数の省略名はusingで明示的に導入する。importだけでは省略名は使えない。
`io.print` / `io.input` とusingで導入した `print` / `input` は同じ処理を使用する。
型変換のint・str等は言語機能としてusing不要。
`lib.size`・`lib.length`とio.pathの詳細は[0.27.0仕様](standard-library-0270-design.md)を参照。
詳しい入出力仕様と今後の io.file の方針は [io の仕様](io.md) を参照。

### math

`math.sqrt(...)`、`using math`、`using math.sqrt` は従来どおり使用できる。
math は暗黙に利用可能で、明示的な `import math` も許可する。

```text
import math
using math as m

int main();
	io.print(m.sqrt(9))
	return 0
```

標準モジュールの上書きは認めない。math.qy / io.qy を配置しても標準モジュールを参照する。

## エラーと検証

循環 import はエラーにし、依存経路と import の位置を表示する。
ファイルが見つからない場合は探索ルートも表示する。
構文・型エラー、生成プログラムの実行時エラーは、元の依存ファイル名と行・列を保持する。
using による衝突では、衝突した名前と using の参照先を表示する。

```bash
make test-modules
make test
```

モジュール用の 78 テストで複数ファイルの動作・診断と標準モジュールの登録を確認する。
Linux ネイティブ実行に加え、Windows 用 PE ヘッダーと、同名関数を含むアセンブリ出力の
アセンブル成功も検証する。Windows 上での実行は別途確認が必要。

## enum（0.27.1追加改訂）

enumはmodelとは別の名前付き型。`profile.sex.female` のようにモジュール・型・メンバーを指定できる。
`using profile.sex` は型sexを、`using profile.sex.female` は値femaleを導入する。
型のusingで全メンバーを自動展開せず、期待型がenumの位置では未解決の裸名をそのメンバーとして解決する。
public/privateと公開APIの型検査はenumにも適用する。詳細: [列挙型の仕様](enums-design.md)。
