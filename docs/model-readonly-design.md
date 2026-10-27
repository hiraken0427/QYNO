# const モデルと読み取り専用メソッド

状態: QYNO 0.26.2 で実装済み。

対象は const のモデル値・モデル配列、readonly メソッド、読み取り専用の ref 引数。
public / private、値コピー、配列の後置次元、明示 close の規則を維持する。

## 1. 基本方針

| 指定 | 決めること |
| --- | --- |
| public / private | どの位置からメンバーを利用できるか |
| const | コンパイル時に確定した値を定数として保持すること |
| readonly メソッド | 暗黙の this を通じて変更する権限を持たないこと |
| ref readonly 引数 | 渡された場所を読み取り専用で借りること |

アクセス権と変更権限の両方を満たす操作だけを許可する。
private を操作できるメソッドでも、readonly の this には書き込めない。

既存の const は、関数内でもコンパイル時定数である。この意味をモデルにも適用する。
readonly は参照経路の権限であり、const のように値の計算時点を指定しない。
実行時に計算した値を固定する変数宣言は、別の実装単位として設計する。

## 2. 推奨記法と全体例

```text
model Point;
	double x = 0.0
	double y = 0.0

	public void move(double dx, double dy);
		this.x += dx
		this.y += dy

	private readonly double squared_length() = this.x^2 + this.y^2

	public readonly double distance();
		return math.sqrt(this.squared_length())

	public readonly Point shifted(double dx, double dy);
		Point copy = this
		copy.move(dx, dy)
		return copy

double measure(ref readonly Point point) = point.distance()

int main();
	const Point origin = {x=3.0, y=4.0}
	io.print(origin.distance())           // 5.0
	io.print(measure(ref origin))        // 5.0

	Point moved = origin.shifted(1.0, 0.0)
	io.print(moved)                      // Point{x=4.0, y=4.0}
	io.print(origin)                     // Point{x=3.0, y=4.0}

	const Point points[] = [{x=0.0, y=0.0}, {x=3.0, y=4.0}]
	io.print(points[1].distance())       // 5.0
	return 0
```

メソッドは `[public | private] [readonly] 戻り値型 名前(引数)` の順とする。
readonly は省略時に付かない。読み取りしか行わない既存メソッドも、const から呼ぶには明示する。
数式形式・ブロック形式・void のメソッドに適用できる。void の数式形式は既存どおり不可。
公開指定を省略した readonly メソッドは public。

引数は `ref readonly 型 名前[次元]`、呼び出しは従来どおり `ref 対象`。
読み取り専用かどうかは宣言側の契約で決まり、呼び出しで readonly を繰り返さない。

readonly は予約語にする。重複指定・順序違いは宣言エラー。
今回 readonly を指定できる位置はモデルメソッドと ref 引数だけ。
通常関数自体、モデル型、フィールド、ローカル変数、値引数、戻り値には指定できない。
`ref const`、`const ref`、`val readonly`、`readonly ref` は導入しない。
同名メソッドを readonly の有無で多重定義することもできない。

## 3. const モデルの初期化とコンパイル時計算

const はトップレベルと関数内で使え、従来どおり初期値を必須とする。
モデル・モデル配列・モデルを内包する配列も対象とする。

```text
const Point ORIGIN = {}
const Point OFFSET = {x=1.0, y=2.0}
const Point COPY = OFFSET
const Point PAIR[2] = [ORIGIN, OFFSET]
const double X = OFFSET.x
```

許可する初期値は、リテラル・他の const・既存の定数演算・定数評価可能な組み込みと、
モデル初期化子・配列・フィールド既定値の組み合わせ。
`Point()` による既定値生成も、その全状態が定数なら許可する。
private の明示指定は、既存どおり初期化子の字句上の所属モデルで検査する。

通常関数・メソッド呼び出しは、readonly であっても const の初期値には使えない。
例えば `const double D = OFFSET.distance()` と `const Point P = make_point()` はエラー。
一方 `io.print(OFFSET.distance())` は、実行時に読み取り専用メソッドを呼ぶので許可する。
読み取り専用という権限だけでは、入出力や停止性を含むコンパイル時実行の条件を満たさない。

他の const の public フィールドは定数式として参照でき、配列の上限などにも使える。
トップレベル const の前方参照・循環検出、ローカル名の可視範囲、型変換規則を維持する。
モデルの private フィールドは定数評価前にアクセス検査する。

モデルの深いコピー、全フィールドによる比較、public のみの自動表示を維持する。
定数評価でも省略したフィールドを既定値で埋め、配列上限・初期化順序を検査する。
既存の const 配列の100万要素の展開上限に合わせ、モデルのフィールドスロットも数える。
入れ子のモデルと配列を合わせて1つの定数ごとに検査し、過大な展開は診断する。
型の循環禁止と入れ子の上限128段は維持する。

file を直接・モデル・配列経由で含むモデル型は、今回の const モデルの対象から除外する。
型で判定し、file 配列が空でも同じ扱いにする。理由と既存 file 定数との関係は第8節に記す。

## 4. 変更を禁止する範囲

const のモデルと、readonly の this / ref 引数からたどる場所は、全階層を読み取り専用にする。
private、public、配列の添字、`.value` の有無で権限は変わらない。

| 操作 | 読み取り専用の場所 |
| --- | --- |
| フィールド・要素・length の読み取り | 許可 |
| モデル全体やフィールドへの代入・複合代入・++ / -- | エラー |
| 入れ子のフィールド・配列要素・文字列内の文字の更新 | エラー |
| 配列の add / remove_at / clear | エラー |
| 通常の変更可能な ref 引数として渡す | エラー |
| io.input の直接・文字列内の入力先、readline の出力先 | エラー |
| readonly メソッドの呼び出し | 許可 |
| readonly でないメソッドの呼び出し | エラー |
| private を含む全状態による比較 | 許可 |
| public フィールドだけの自動表示 | 許可 |

`const Box b = ...` に対する `b.points[0].move(...)` もエラー。
readonly メソッド内の `this = {}` や `this.child.clear()` もエラー。
`.addr` で変更可能な別名を作る抜け道を許可しない。

readonly メソッドは、readonly の入れ子メソッドを呼べる。
非 readonly メソッドは、本体が実際には変更を行わなくても呼べない。
本体を自動推論して readonly 扱いにすることや、非 readonly 呼び出しを勝手にコピーへ適用することはしない。
一時モデルの変更可能なメソッド呼び出しは、既存どおり独立した一時値に適用できる。

## 5. 値コピーと読み取り専用の参照を分ける

readonly はモデルの型名や値そのものには付かず、値を使う参照経路に付く。
`Point` と const の Point に、別々のモデル型の同一性を作らない。

file を含まない値を代入・値引数・戻り値でコピーした後は、コピー先の宣言に従う。

```text
const Point source = {x=3.0, y=4.0}
Point copy = source
copy.move(1.0, 0.0)  // 許可。source は変化しない
```

readonly メソッドのローカルコピーも変更できる。第2節の shifted がその例。
配列・入れ子・str / char も既存の独立した値の規則に従う。
readonly メソッドが返した Point や配列は通常の値で、呼び出し側で変更できる。
readonly メソッドが返した値に、暗黙に const を付けることはしない。

通常の `void change(Point p)` に const の Point を値渡しすることも許可する。
呼び出し先が操作するのはコピーであり、元の const ではない。
file を含む値のコピーには第8節の追加条件を適用する。

## 6. 読み取り専用 ref 引数

```text
double distance_from_origin(ref readonly Point p) = p.distance()

int total(ref readonly int numbers[]);
	int result = 0
	for(0 <= i < numbers.length, i++);
		result += numbers[i]
	return result
```

`ref readonly` は全ての既存の値型と配列に適用する。file は第8節の観測専用規則を適用する。
変更可能な変数・const・別の読み取り専用 ref・readonly の this とそのアクセス可能なフィールドを渡せる。
読み取り専用へ権限を減らす方向だけを許可し、変更可能な ref への変換は拒否する。
readonly の ref をさらに readonly の ref へ渡すことはできる。

| 実引数 | 値引数 | ref readonly 引数 | 通常の ref 引数 |
| --- | --- | --- | --- |
| 変更可能な場所 | 値コピー | 読み取り専用の借用 | 変更可能な借用 |
| const / 読み取り専用の場所 | 値コピー | 読み取り専用の借用 | エラー |

型・配列の次元・上限制約は従来の ref と同じ検査を行い、参照先の暗黙の型変換は行わない。
明示的な ref 引数は変数・変数からたどるモデルフィールド・配列要素を対象とする。
文字列内の1文字・関数の一時戻り値・リテラルは、従来の明示 ref と同じく対象外。
for の繰り返し変数に対する ref の禁止も維持する。
const の名前やそのフィールド・配列要素は、今回の読み取り専用 ref の実引数として追加する。

呼び出しで `ref` は必須。`val` で参照契約を上書きできない。
readonly 引数も、通常の ref 引数と同様に関数の実行中だけ有効で、場所の再束縛はできない。
readonly の this / 引数に `.addr` を使うことは禁止する。
const の `.addr` も従来どおり禁止する。
読み取り専用の場所を、権限の付いていない addr に変えて持ち出すことはできない。
ローカルの `ref readonly` 変数・読み取り専用 addr・参照戻り値は今回の対象に含めない。

借用は通常の変数に対して値のコピーを要求しない。
const は必要に応じて呼び出し中だけ読み取り専用の実体を生成し、GC の到達可能な対象として保護する。
const の実体化はユーザーが利用できる addr の付与にはならない。
フィールド・配列要素の参照では、既存の所有元の置き換え・配列再配置を防ぐ借用検査を継続する。

## 7. readonly の権限と他の別名

readonly が保証するのは、その this / ref 引数を通じて変更権限を使わないこと。
変更可能な変数への読み取り専用借用は、既存の変更可能な別名を無効にはしない。
同じ場所を複数の ref に渡せる、現在の QYNO の規則を維持する。

```text
void observe_and_change(ref readonly Point view, ref Point writer);
	writer.x += 1.0
	io.print(view.x)

int main();
	Point p = {x=3.0}
	observe_and_change(ref p, ref p)  // 許可。view.x は更新後の 4.0
	return 0
```

一方、const は独立した定数値であり、変更可能な別名を作る手段を与えない。
`const Point p = ...` を同じ呼び出しの writer に渡す操作はコンパイルエラーになる。

readonly メソッドも、別の変更可能な引数やローカルコピーには書き込める。
コンソール入出力や、独立した file の操作も、既存の型・close 規則を満たせば行える。
readonly を純粋関数・結果の再利用・呼び出し順序の変更・並列実行の根拠には使わない。
通常メソッド内の同型の別インスタンスは、その引数が readonly と宣言されていなければ変更可能。
private のアクセス権が同型全体に及ぶことと、this の変更権限を分けて判定する。

## 8. file の共有と読み取り専用の境界

file のコピーは同じハンドルを共有する。読み取りでも read / readline は現在位置を進める。
そのため、読み取り専用の場所に由来する file は観測専用として扱う。

| 操作 | 読み取り専用の場所に由来する file |
| --- | --- |
| io.file.is_open | 許可 |
| ハンドルの同一性の比較 | 許可 |
| モデル全体の自動表示・比較 | 許可。表示は既存の public / private 規則を適用 |
| io.file.read / readline | エラー。共有の現在位置が変化するため |
| io.file.write / close | エラー |
| file フィールドへの新しいハンドルの代入 | エラー |
| ref readonly file 引数への受け渡し | 許可。観測専用の権限を引き継ぐ |
| 通常の file 変数・値引数・戻り値へのコピー | エラー。共有状態の変更権限を渡してしまうため |

file を含むモデル・配列も同じ扱いとし、読み取り専用の経路からの値コピー・値引数・戻り値を禁止する。
file を含む型への `ref readonly`、readonly メソッドの呼び出し、file を含まない部分のコピーは許可する。
空の file 配列や未オープンのハンドルも、同じ型に対して一貫した判定をする。
表示・比較でコンパイラが内部的に作る一時値は、変更可能なユーザーの別名を生成しない。
直接の file / file 配列の print を禁止する既存の制約は維持する。

```text
model Reader;
	private file handle

	public readonly bool is_open() = io.file.is_open(this.handle)

	public bool readline(ref str line) = io.file.readline(this.handle, ref line)

	public void close();
		io.file.close(this.handle)
```

この Reader の readline / close は readonly にできない。
`file alias = this.handle`、`return this.handle`、外部の通常の file 値引数への受け渡しも、
readonly メソッド内ではコンパイルエラーとする。
読み取り専用の Reader 全体をローカルコピーして close する方法も拒否する。
一方、メソッド内で新たに作った独立したハンドルや、明示的に変更権限を持つ別引数の操作は許可する。

const モデルでは file を含む型を受け付けない。file の共有状態まで固定する仕組みを作らず、
定数値の初期化・実体化・コピーが共有リソースの変更経路になることを避ける。
従来許可されている「未オープン状態だけを持つ const file 配列」の処理は変更しない。

明示 close の責任は維持する。観測専用の引数・メソッドへ渡しても閉鎖した扱いにはならない。
所有する変更可能な変数、または変更可能なメソッドを介して、正常終了の全経路で close する。
読み取り専用経路から共有ハンドルの変更権限を復元するキャストや補助APIは導入しない。

## 9. モジュール・既定値・診断・VSCode

import / using / 別名は、公開範囲と同様に readonly の契約を変更しない。
別モジュールで定義されたメソッドの readonly 指定と、ref readonly 引数を型解決時に保持する。
同名の別モデルは引き続き別の型。

readonly の本体は、未使用・到達不能な分岐も検査する。
フィールド既定値と const の定数評価では、元の字句上の所属モデルと、既に検査した権限を保持する。
既定値の補完や定数の実体化を口実に、ユーザーが明示した private の指定を許可しない。

診断は問題の操作位置に出し、readonly の由来と、必要な変更権限を説明する。
例: `this.items は readonly のため add を呼べません`、
`const の Point から move は呼べません。move は readonly ではありません`、
`読み取り専用の file を変更可能な値としてコピーできません`。
const に通常関数呼び出しを書いた場合は、書き込み禁止とは区別して定数評価のエラーにする。

VSCode の補完は、public / private と読み取り専用の権限を両方反映する。
const / readonly の対象からは readonly メソッドを候補にし、配列の変更操作は候補に出さない。
フィールドの読み取り候補は残し、ホバー・引数ヒント・アウトラインに const / readonly を表示する。
手入力した非 readonly 呼び出しのホバーや定義参照は可能とする。
軽量解析で判定できない式は既存どおりコンパイラの検査に委ねる。

## 10. 実装方針

1. メソッドの readonly と引数の ref readonly を解析し、宣言に変更権限を保持する。
2. readonly メソッドの暗黙の this を読み取り専用 ref として扱う。
3. 型の同一性と別に、読み取り専用の場所を判定する。フィールド・添字・value・別名をたどっても権限を失わないようにする。
4. 読む場所の検査と、変更可能な場所の検査を分ける。ref readonly の確認に、現在の変更専用 check_place をそのまま使わない。
5. メソッドの暗黙の this、入力、書き込み、ref の引数、addr、配列操作をそれぞれ必要な権限で検査する。フィールドやメソッドを内部形式へ変換する前に判定する。
6. file を含む型を再帰的に判定し、読み取り専用の場所から共有ハンドルを持ち出すコピー・引数・戻り値を拒否する。観測専用の組み込み呼び出しは個別に許可する。
7. 定数値の内部表現と評価にモデルを追加し、全フィールド・配列制約を保持する。モデルフィールドの読み取りを通常の配列添字評価と混同しない。
8. const のモデル・配列を実行時の読み取り専用呼び出しへ渡す実体化を用意する。GC の保護と、既存の参照先寿命・所有元の借用検査を適用する。
9. public / private の検査と、型検査済みAST・既定値のキャッシュを維持する。省略した初期値とユーザーの明示指定を再検査時に混同しない。
10. VSCode の解析・候補選択・表示を更新する。

変更権限は、単にメソッド内のフィールドへ読み取り専用フラグを貼り付ける実装にはしない。
同じ型のフィールドでも this・変更可能な別引数・独立したコピーの権限は異なる。
判定できない呼び出し先に、readonly の契約を自動で仮定することはしない。

## 11. 受け入れ条件

- const のモデル・配列・入れ子、既定値・前方参照・フィールド定数式・配列上限を評価できる。
- private の明示指定・読み取りを、定数評価前に公開範囲の規則で検査する。
- 実行時の関数・readonly メソッド呼び出しを const 初期値として拒否する。
- const / readonly の全体・フィールド・入れ子・配列・文字列・value 経由の変更を拒否する。
- readonly メソッド同士、private readonly、void、数式形式、他モジュールの呼び出しが動作する。
- readonly の this だけを制限し、変更可能な別引数や独立したローカルコピーは操作できる。
- const から通常のメソッド・変更可能 ref・addr・入力先への変換を拒否する。
- ref readonly では const・変更可能な場所・readonly の場所を受け取り、次元・上限・寿命を検査する。
- readonly の引数から、通常の ref や addr へ権限を戻す操作を拒否する。
- 同じ変更可能な場所を readonly と通常 ref へ渡した場合の挙動を、第7節どおり確認する。
- file を含まない値コピー・値引数・戻り値は独立し、通常の変更可能な値として扱える。
- file 由来の readonly は、直接・value・入れ子・配列経由で read / readline / write / close・持ち出しを拒否する。
- readonly file の is_open・比較、readonly モデル全体の表示と比較を許可する。
- file を含むモデルの const を、空配列や private フィールドの場合も拒否する。
- 既存の const file 配列の利用を維持し、close 漏れ・早期 return の検査を継続する。
- 無指定の既存メソッドの動作を維持し、readonly の重複・指定位置・予約語を診断する。
- VSCode の内部・外部・モデル配列・別名・ref 引数の補完と表示を確認する。

実装時は個別検証の後、最終確認として全体テストを1回実行する。
専用テストは `make test-readonly`、実行例は `examples/models/readonly.qy`。

## 12. 後続機能との境界

カスタム初期化では、private の設定と入力検証を生成時に行えるようにする。
const のコンパイル時計算にその処理を使えるかどうかは、別途評価規則を定める。
インターフェースを導入するときは、readonly メソッドの契約を実装側でも満たす必要がある。
読み取り専用フィールド・実行時の固定値宣言・ローカルの読み取り専用参照・継承は後続の設計とする。
