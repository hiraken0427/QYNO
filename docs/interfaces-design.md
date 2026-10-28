# インターフェース

状態: QYNO 0.26.4 で実装済み。
`interface` / `implements`、参照引数の契約、コンパイル時の特殊化に対応する。
実行例は [examples/models/interfaces.qy](../examples/models/interfaces.qy)。

## 1. 機能と目的

モデルの共通操作を宣言する `interface` を導入する。
モデルに初期化・public / private・readonly が揃ったため、複数のモデルを同じ関数で扱えるようにする。
フィールドや生成方法はモデルに任せ、利用側は必要なメソッドだけに依存する。

初版は、インターフェースを `ref` / `ref readonly` 引数の契約として使う。
呼び出し時の具体的なモデル型に応じて関数をコンパイル時に特殊化し、既存の直接呼び出しへ変換する。
異なるモデルを同じ型の変数・配列に格納する実行時多態性は、所有権・コピー・寿命を設計する後続段階とする。

この範囲でも、図形の面積計算、描画、シリアライズ、close などの共通処理を、モデルごとに複製せず書ける。
特に file の操作を実際のメソッドへ解決してから既存の close 解析に渡せる点が、現在の実装と整合する。

| 候補 | 今回の判断 |
| --- | --- |
| interface | 推奨。実装の共有と分離して、共通操作と読み取り権限を定義できる |
| 継承 | 後続。値コピー時の基底部分、private、初期化順序、基底 ref の更新範囲の設計が必要 |
| 実行時の interface 値 | 後続。異種配列・動的ディスパッチに加え、コピーと参照先寿命の規則が必要 |
| 汎用ジェネリクス | 後続。まずモデルに対する限定した契約を実装し、特殊化基盤を再利用する |

## 2. 推奨する書き方

```text
using io

interface Shape;
	readonly double area();

model Circle implements Shape;
	private double radius

	bool init(double radius);
		if(not(radius > 0.0));
			return false
		this.radius = radius
		return true

	readonly double area() = 3.141592653589793 * this.radius^2

model Rectangle implements Shape;
	private double width
	private double height

	bool init(double width, double height);
		if(not(width > 0.0 and height > 0.0));
			return false
		this.width = width
		this.height = height
		return true

	readonly double area() = this.width * this.height

double get_area(ref readonly Shape shape) = shape.area()

void print_area(ref readonly Shape shape);
	io.print(get_area(ref shape))

int main();
	Circle circle = {radius=2.0}
	Rectangle rectangle = {width=3.0, height=4.0}
	print_area(ref circle)
	print_area(ref rectangle)
	return 0
```

`Circle` と `Rectangle` は引き続き別の値型。`print_area` は Shape の操作だけを利用する。
生成は従来の `{名前=値}`、宣言側の配列次元は従来の `型 名前[][]` を維持する。

変更を伴う契約も定義できる。

```text
interface Movable;
	void move(double dx, double dy);

void shift_right(ref Movable target);
	target.move(1.0, 0.0)
```

## 3. 宣言と名前解決

`interface 名前;` をトップレベルに定義し、インデントした本体にメソッドのシグネチャを書く。
戻り値型・引数・配列の後置次元・readonly は、既存のメソッドと同じ記法を使う。
interface 内の末尾 `;` は本体を持たないメソッド宣言の終わりになる。

- フィールド、メソッド本体、式形式、既定実装、static、init、オーバーロードは認めない。
- メソッドは全て public。明示的な `public` は許可し、`private` はエラーにする。
- 通常の関数と同様、引数の既定値は認めない。
- 戻り値・メソッドの引数型は具体的な既存型に限定する。契約メソッド自身の引数に別の interface を使う形式は初版の対象外。
- 引数名の重複と、`this` などの既存の予約名を検査する。初版では空の interface を認めない。
- interface の継承は導入しない。

`interface` / `implements` は新しい予約語とする。従来その識別子を使っていたコードは変更が必要になる。
interface 名はモデル・関数・定数と同じモジュールの公開名の衝突規則に従う。
別モジュールの同名 interface は別の契約であり、メソッド構成が同じでも同一型にはしない。

```text
import graphics.contracts
using graphics.contracts as contracts

model Icon implements contracts.Shape;
	readonly double area() = 1.0

void inspect(ref readonly contracts.Shape value);
	io.print(value.area())
```

`using graphics.contracts.Shape` と全体 using も対応する。
interface だけの import をモデル側の import で代用することはできず、既存のモジュール探索・循環検査を維持する。

## 4. モデルによる実装

`model 名前 implements Interface1, Interface2;` と明示する。実装は同じモデル宣言の本体に記述する。
後から別ファイルで既存モデルに実装を付け足す形式や、自動的な構造一致は導入しない。

実装の宣言は、利用されていないモデルでも全件検査する。

| 検査対象 | 一致条件 |
| --- | --- |
| メソッド名 | 同じ名前の public メソッドが必要。init は対象外 |
| 戻り値 | 同じ型・配列次元・上限制約。数値の昇格や共変変換はしない |
| 引数 | 順序・型・次元・上限制約・値/ref/ref readonly の指定が一致 |
| 引数名 | 一致不要。呼び出しは既存どおり位置引数 |
| readonly の要求 | 実装も readonly が必要 |
| readonly でない要求 | 通常メソッドと readonly メソッドの両方を許可 |

`val` は既存の値引数の明示指定として正規化して比較する。
配列の上限は定数評価後の値で比較し、`[]` と `[3]` は区別する。
宣言の上限が一致しても、実引数の参照先の次元・上限検査は既存どおり実施する。
private メソッドは、利用側と同じファイルにあっても実装として認めない。

複数の interface が同名のメソッドを要求する場合、1つの public メソッドが全ての条件を満たす必要がある。
readonly の有無だけが異なれば readonly 実装で両方を満たせる。
戻り値や引数型が両立しなければ、implements の位置で双方の宣言位置を示してエラーにする。
interface ごとの別名メソッドや明示的実装は初版では導入しない。implements の重複はエラーにする。

## 5. interface を使える場所

初版では、通常の関数・通常のモデルメソッドの `ref I name` / `ref readonly I name` 引数だけで使える。
init は既存どおり ref 引数を受け付けないため、interface 引数も受け付けない。

| 記述・用途 | 初版 |
| --- | --- |
| `void show(ref readonly Shape s);` | 許可 |
| `void move(ref Movable m);` | 許可 |
| 既存モデルの変数・フィールド・配列要素を `ref` で渡す | 既存の場所・寿命・借用条件を満たす場合に許可 |
| 同じ interface 引数を補助関数へ `ref` で転送 | 権限を弱めない場合に許可 |
| `void show(Shape s);` | エラー。interface の値コピーは未定義 |
| `Shape s = circle` / `ref Shape s = circle.addr` | エラー。ローカルの interface 変数は導入しない |
| モデルフィールド、配列の要素型、const、戻り値 | エラー |
| `{}` / `Shape()` / `none` による interface の生成 | エラー |

interface の ref 引数自体を、別のモデルや別の参照先に再束縛することはできない。
同じ契約の mutable 引数から readonly 引数への転送は許可する。逆方向は拒否する。
異なる interface への転送は、実際のモデルが両方を実装していても認めない。
使う関数のシグネチャから要求が分かるようにし、具体型だけが持つ能力を隠れて利用させない。

実引数には `ref` を必須とし、リテラル・関数の一時戻り値・for の繰り返し変数などの制限は既存の ref と同じ。
const のモデルは `ref readonly` に渡せる。生成可能な const の条件は変えず、init を含む型の const は認めない。
モデルを含む配列そのものを Shape と見なすことはできない。必要なら各モデル要素を渡す。

## 6. interface 引数でできる操作

interface 引数に対して利用できる操作を、契約メソッドの呼び出しと、同じ契約の ref 引数への転送に限定する。
具体型が分かる特殊化先でも、この制限を緩めない。

```text
void inspect(ref readonly Shape shape);
	io.print(shape.area())  // 許可
	// shape.radius        // エラー。フィールドは契約にない
	// shape.helper()      // エラー。Shape に宣言されていない
	// io.print(shape)     // エラー。全体の表示は契約に含めない
	// shape.addr          // エラー。具体型の場所として持ち出せない
```

全体コピー・全体代入・比較・キャスト・input・`.addr`・`.value`・`.length` は許可しない。
メソッドが返す具体型の値に対しては、その型の通常の操作を行える。
表示や比較が必要な場合は、`readonly str describe();` などを契約に明示する。

`ref readonly I` では readonly の契約メソッドだけを呼べる。
実装が偶然 readonly でも、契約で readonly と宣言していないメソッドは呼べない。
readonly は純粋関数を意味せず、既存どおり独立したローカル値や別の mutable 引数の変更は許可する。

private のアクセス権は利用側の所属モデルに従うが、interface 引数にフィールドを公開する理由にはならない。
具体型のメソッド本体に入った後は、その定義元のモデルに対する通常の private 規則を適用する。
初期化途中の this を interface として渡す場合も、既存の this 全体の完了検査を先に行う。

## 7. コピー・寿命・別名・実行順序

interface 引数を渡してもモデルのコピー・init・再生成は発生しない。
呼び出し期間中、元のモデルを既存の ref として借用する。
interface のためのヒープ上のラッパーや実行時の型タグ・仮想関数テーブルは作らない。
これは初版の実装方式であり、将来の実行時 interface 値まで同じ表現と定めるものではない。

フィールド・配列要素を参照している間の所有元の置き換え、祖先配列の追加・削除・再代入は、
従来の借用検査で拒否する。参照先をたどる添字や副作用を持つ式は1回だけ評価する。
特殊化のために実引数を再評価せず、メソッド呼び出しでも既存の受け手・引数の評価順を維持する。

同じモデルを複数の ref 引数へ渡す現在の規則は維持する。
readonly の参照があっても、別の mutable な参照からの更新は見える。
interface を、排他的借用や参照先の不変性の保証として扱わない。

## 8. file の明示 close と副作用

file を含むモデルも実装可能とする。interface に合わせてハンドルをコピーしたり、close の責任を解除したりしない。
メソッド名が `close` であることを根拠に、ハンドルが閉じたと判断しない。

```text
interface Closable;
	void close();

model Reader implements Closable;
	private file handle

	void init(str path);
		this.handle = io.file.open(path)

	void close();
		io.file.close(this.handle)

void finish(ref Closable resource);
	resource.close()

int main();
	Reader reader = {path="input.txt"}
	finish(ref reader)
	return 0
```

この finish は Reader 用に解決され、Reader.close の本体の要約を使うため、明示 close として認識できる。
close の実装が何もしなかったり、通常 return の一部でしか閉じなかったりすれば、
実際のハンドルの状態に基づいて呼び出し元の close 漏れを診断する。
契約の型の一致と、リソース操作が正しいことの証明は分ける。

具体型の別名関係・共有ハンドル・メソッドから返る所有責任は、特殊化後の既存解析で追跡する。
契約メソッドの引数や戻り値に、具体型として file や file を含むモデルを使うことも許可する。
readonly メソッドの共有ハンドルの持ち出し禁止は既存どおり維持する。
新しく取得した独立した file を返す処理は、通常の所有責任の移動規則を適用する。

file を含む再帰呼び出し・動的添字など、現在の解析で安全を証明できないケースは引き続きエラーにする。
interface の導入に合わせて、既存の解析が証明できる範囲を広げたと仮定しない。

## 9. コンパイル時の特殊化

interface 引数のある関数をテンプレートとして保持し、呼び出しの具体型の組み合わせごとに内部関数を生成する。
ソース上で型引数を書く必要はない。

例の `print_area(ref circle)` と `print_area(ref rectangle)` では、概念上、
print_area の Circle 用と Rectangle 用を生成する。内部の get_area への転送も同じ具体型で解決する。
ユーザーが書いた定義は1つのまま、最終的には既存モデルへの直接のメソッド呼び出しになる。

- キャッシュのキーは元の関数の識別子と、interface 引数ごとの具体モデル型の順序付き一覧。
- 同名の別モジュールの型を区別する。普通の引数の値・配列長をキーにせず、同じ型の組み合わせは再利用する。
- interface 引数のない元のモデルメソッドを、実装する interface ごとに複製する必要はない。
- 複数の interface 引数と、interface 引数を取る通常のモデルメソッドにも同じ仕組みを使う。
- 再帰・相互再帰では、同じキーの作成中エントリへ戻す。宣言しただけの無限再帰をコンパイラの無限展開にしない。
- インスタンス化した内部関数はソースの公開名に加えず、import / using / 補完の対象にしない。

重要なのは、具体型へ置き換えてから自由に型検査する実装にしないこと。
元の関数本体を契約に対して型検査し、契約外メンバー・whole-value 操作・readonly 違反を先に拒否する。
未使用関数や到達不能分岐も、この段階で検査する。
その後、契約呼び出しだけを選択済みの public 実装へ解決する。
特殊化によって private の権限や実装専用のメンバーを利用側へ追加しない。

既存コード生成は具体型だけを受け取る。GC・スタック・借用・ファイル解析も解決後の具体型を使う。
診断は元のソース位置を保持し、必要なら呼び出し元と具体型の組み合わせを補足する。
資源解析のエラーが未使用テンプレートの宣言だけで必要になるのではなく、具体的な使用に対して発生する一方、
通常の型・契約エラーは未使用でも発生する。

特殊化の総数はプログラム全体で4096件までとする。作成中のキーも1件に数える。
超過時は通常のコンパイル診断で止め、展開元と具体型一覧を示す。
既存の型・参照パスの深さの上限は維持する。

## 10. 診断と VS Code

診断では以下を区別する。

- implements 先が interface でない、または対象モデルが契約の実装を明示していない。
- 必要な public メソッドがない。private の候補がある場合はその宣言位置も示す。
- 戻り値・引数・配列制約・readonly の不一致。
- interface が使用できない場所での宣言・生成・コピー・型変換。
- 契約外のメンバー呼び出し、readonly の権限を強める転送。
- 特殊化後に検出した close 漏れ・参照の問題。呼び出し位置と具体的な実装位置を示す。

VS Code では interface 名と implements をハイライトし、シグネチャ・アウトライン・定義参照を追加する。
interface 引数の `value.` 補完は、具体型を推測して混ぜず、契約にあるメソッドだけを出す。
readonly の引数では readonly の契約メソッドだけを候補にする。
ホバーには宣言された契約と readonly を表示し、定義参照は契約の宣言へ移動する。
implements の型名からは interface 宣言へ移動する。実装メソッド一覧へ移動する新機能は初版の必須条件にしない。
既存のモデルの生成キー・private のフィールド補完を壊さないようにする。

## 11. 実装の分割

1. `qyc.h` / `types.c` / `parse.c`: モデルとは別に interface と契約メソッドの定義を保持し、implements と本体なしのシグネチャを解析する。
2. `modules.c` / 型解決: interface を公開型名として解決し、同名・using 衝突、利用可能な型位置、前方参照を検査する。
3. 型検査: 全ての implements の対応表を確定し、引数の契約・readonly・whole-value 操作を検査する。契約呼び出しは契約のシグネチャを参照する呼び出しノードとして保持する。
4. 特殊化: 元のASTを変更せず内部関数を複製する。元のモジュール・所属モデル・using・ソース位置を維持し、変数・場所・ノードの対応を再構築する。
5. 特殊化の作業キューを空になるまで処理し、呼び出しを具体的な関数・メソッドに解決する。コード生成対象の全ての通常関数から具体化を列挙し、未使用の通常関数にも未解決呼び出しを残さない。interface 引数を持つ未使用テンプレートは具体化しない。
6. `type.c` / `file_close_analysis.inc`: 未完了フィールド検査、借用・readonly 検査と close 要約を具体化済みASTに適用する。抽象 interface を具体的なモデルのフィールド配置として解析しない。
7. 既存の Linux / Windows のコード生成へ具体型だけを渡す。ユーザーの実行順序と作成位置の情報を維持する。
8. VS Code の解析と文書を更新し、実装バージョンに対応した VSIX を作成する。

型検査中のメソッド呼び出しの変換、型検査済みASTのキャッシュ、file 要約のキャッシュに、
特殊化ごとの別の具体型を上書きしないこと。キャッシュと再帰の管理には元の関数と特殊化済み関数を区別して使う。
必要な具体化を全て揃えてから close 解析とコード生成を行う。

## 12. 受け入れ条件

専用テストは `tests/interfaces_test.py` と `make test-interfaces`。
以下を確認し、最終状態で `make test` を1回実行する。ZIP 作成後の再実行はしない。

- Circle / Rectangle の共通処理、複数 interface、public の明示・省略、前方参照。
- 未使用の不正な implements、private 実装、署名・readonly・配列制約の不一致を拒否する。
- 名前の異なる引数と、上限制約の同値な定数式を受け付ける。
- readonly 実装による mutable 契約の充足と、契約に基づく readonly 呼び出し制限を区別する。
- interface の値・配列・フィールド・戻り値・init 引数・ローカルref・キャストを拒否する。
- 同じ契約への ref 転送、複数引数、モデルメソッド経由の転送、再帰・相互再帰を処理できる。
- 別契約への転送と、具体型でだけ可能なフィールド・メソッドの使用を、未使用関数・到達不能分岐でも拒否する。
- 値コピーや init の再実行がなく、mutable メソッドの結果が元のモデルに反映される。
- const / readonly、複数の別名、入れ子・配列要素、添字の1回評価と所有元の借用検査を維持する。
- interface を経由しても、未完了の this を外部へ渡せない。
- file を含む実装の close が認識され、何もしない close・部分 close・早期 return の漏れが拒否される。
- 特殊化した複数の実装の file 要約が混ざらず、共有ハンドル・引数・戻り値の責任が維持される。
- 別モジュール・別名・選択 using・同名の別契約を解決し、private の権限を利用側へ漏らさない。
- 同じ具体型の再使用で特殊化を再利用し、別の具体型の順序を区別し、4096件の上限を診断できる。
- GC 多発時、Linux / Windows、アセンブリ出力でも既存モデルと同じ結果になる。
- VS Code の契約メソッド補完・readonly・定義参照・引数表示・アウトラインを確認する。

## 13. 後続の拡張

この初版で共通の操作と契約の書き方を確立し、その後に必要性に応じて機能を広げる。

- 異種モデルを `Shape` 変数や `Shape items[]` に保持する実行時多態性。
- 値を所有する interface と借用する interface の区別、コピー、寿命、file の所有責任。
- 共通実装、interface の継承、汎用ジェネリクス。
- 基底モデルを使う継承。初期化順序・値コピー・基底部分へのref・公開範囲の設計を別途行う。

将来の実行時 interface 値も今回の契約宣言を再利用する方針とするが、
初版の参照引数を、暗黙の所有値への変換や異種配列への格納と解釈しない。
