# モデル型（0.26.3）

`model` は型付きフィールドとメソッドを持つ値型。代入・通常の引数・戻り値では
独立した値を作り、変更したい変数は `ref` で渡す。型名が異なれば別の型であり、
同じフィールド構成でも暗黙に変換しない。

```text
model Point;
	double x = 0.0
	double y = 0.0

	void move(double dx, double dy);
		this.x += dx
		this.y += dy

	double distance();
		return math.sqrt(this.x^2 + this.y^2)

int main();
	Point p = {x=3.0, y=4.0}
	Point copy = p
	copy.move(1.0, 2.0)
	io.print(p)
	io.print(copy)
	return 0
```

インデントはタブ。モデルはファイルのトップレベルに定義する。
公開指定を省略したフィールドとメソッドは public。メンバーへのアクセスは `p.x` / `p.move(...)` と書く。
メソッド内で自身を表す名前は `this`。引数一覧には書かない。
フィールドを直接名前だけで参照せず、`this.x` と明示する。
同じモデル内のフィールド・メソッドの名前は重複できない。メソッドのオーバーロードはない。
`this`・`addr`・`value`・`length` はフィールド名には使用できない。

## 公開制御（0.26.1）

フィールド・メソッドの先頭に `public` または `private` を1つ指定できる。
private は宣言元のモデル型の内部からだけ利用でき、同型の別インスタンスにもアクセスできる。
同一ファイルの通常関数・別モデル、import や using は権限を増やさない。

```text
model Counter;
	private int count = 0
	public void increment();
		this.count++
	public int current() = this.count

int main();
	Counter counter = {}
	counter.increment()
	io.print(counter.current()) // 1
	io.print(counter)           // Counter{}
	return 0
```

読み書き・ref・入力・初期化子の明示的なフィールド指定も同じ規則で検査する。
省略した private フィールドは既定値で初期化する。コピー・比較は private を含む全状態を扱う。
自動表示は、内部から呼んだ場合も public フィールドだけを表示する。
private な file にも明示 close が必要で、public メソッドを介して管理できる。
`public` / `private` は予約語。モデル型自体・通常関数・引数・ローカル変数には指定できない。
詳細は [公開制御の仕様](model-visibility-design.md)、実行例は
[examples/models/visibility.qy](../examples/models/visibility.qy) を参照。

## 初期化

| 記述 | 意味 |
| --- | --- |
| `Point p` | 各フィールドを既定値で初期化 |
| `Point p = {x=3.0, y=4.0}` | 左辺の型を使って新しい値を生成 |
| `Point p = {x=3.0}` | y は既定値 |
| `Point p = {}` | 全フィールドが既定値 |
| `Point p = Point()` | 型名を明示した既定値での生成も可能 |
| `Point copy = p` | 独立したコピー |

フィールドの既定値には、コンパイル時に確定する式・配列・モデル初期化子を指定できる。
通常のユーザー関数呼び出しやファイルの open を既定値には指定できない。
既定値を省いた場合、数値は0、bool は false、str は空文字列、char は U+0000、
file は未オープン。配列は既存の上限・初期化規則に従い、モデルはフィールドを再帰的に初期化する。
各生成で、既定値のモデル・配列も独立した値になる。

初期化子で明示した値の式を、書いた順に左から1回ずつ評価する。
省略したフィールドを既定値で埋める。未知のフィールド・重複指定・型不一致はコンパイルエラー。
初期化子だけでは型を決定できないため、宣言・代入先・引数・戻り値・配列要素などの型の文脈が必要。

```text
Point p = {x=3.0, y=4.0}
p = {x=5.0}      // 新しい値に置き換え。y は既定値の 0.0
p.x = 6.0        // 一部だけ変更。y は保持
```

## コピー・参照・メソッド

モデルをコピーすると、その中のモデルと配列も深くコピーする。
str / char は既存の文字列の値の規則を使い、文字列の変更は元の値に影響しない。
file は共有ハンドルをコピーする。同じハンドルをどの別名から閉じても全てに反映される。

```text
void change_copy(Point point);
	point.x = 100.0

void change_original(ref Point point);
	point.x = 100.0
```

メソッドの対象は暗黙に参照渡しされる。`p.move(...)` は p 自身を変更する。
メソッドにも通常の値引数・ref 引数・配列引数と、値または void の戻り値を定義できる。
別のモデルの同名メソッドや、同名のトップレベル関数と共存できる。
戻り値などの一時モデルにもメソッドを呼べ、その変更は一時値に適用される。

```text
ref Point alias = p.addr
alias.move(1.0, 2.0)
p = {x=20.0}
io.print(alias.x)  // 20.0。alias は p という変数の場所を指す
```

ローカルの ref の型・寿命・none の検査は既存の規則を使う。
`.addr` は変数名の直後に指定する。`p.x.addr` でフィールドの場所を取り出す形式は導入しない。
関数へフィールドを渡す場合は `increase(ref p.x)` と書く。
ref 引数の `.addr` は変更できないが、その引数への値の代入は参照先を更新する。

## 入れ子と配列

```text
model Rectangle;
	Point position
	double width = 0.0
	double height = 0.0

int main();
	Rectangle rect = {
		position={x=1.0, y=2.0},
		width=10.0,
		height=20.0
	}
	Point points[] = [
		{x=1.0, y=2.0},
		{x=3.0, y=4.0}
	]
	points[0].move(1.0, 0.0)
	points.add()  // 既定値の Point を追加
	points.add({x=5.0})
	return 0
```

`[]` と `{}` の中では複数行に記述でき、最後のカンマも許可する。
`Point grid[2][3]` のような多次元配列や、モデル内の配列フィールドにも対応する。
フィールドや配列要素を ref 引数で参照している間、所有元のモデルの再代入や、
祖先配列の追加・削除・再代入を実行時に拒否する。読み書きに使う添字は1回だけ評価する。

## 入出力・比較・ファイル

`io.print(p)` は `Point{x=3.0, y=4.0}` の形式。public フィールドを宣言順に表示する。
private は名前も値も省略し、全て private なら `Point{}`。入れ子・配列も同じ規則で再帰的に表示し、
文字列フィールドは引用符付きで表示する。file フィールドは `<file>` と表示する。
同じモデル型の `==` / `!=` は private を含む全フィールドを値として比較し、配列・入れ子も再帰的に比較する。
file フィールド同士はハンドルの同一性を比較する。

`io.input(p.x)` と `io.input("入力> {p.x}")` で、既存の入力可能な型のフィールドに入力できる。
モデル全体への直接入力や文字列への型変換は導入しない。

モデルやモデル配列内の file にも、正常終了の全経路で明示的な close が必要。
コピー・引数・戻り値・メソッドを通じたハンドルの共有と責任を追跡する。
動的添字などで閉鎖を証明できない場合は、既存と同じくコンパイルエラー。
GC はモデル・配列・文字列を管理し、到達可能な共有ハンドルを維持する。

## モジュールと今回の範囲

モデルもモジュールの公開名になる。次の宣言形式で利用できる。

```text
import geometry
using geometry as geo

int main();
	geo.Point p = {x=3.0, y=4.0}
	io.print(p.distance())
	return 0
```

`geometry.Point`、`using geometry`、`using geometry.Point` にも対応する。
別ファイルの同じ名前のモデルは別の型。モデルと通常の関数の前方参照も可能。

0.26.3 では継承、静的メソッド、
自己参照・相互参照するフィールド型は導入しない。
配列を介するフィールド型の循環も診断する。モデルの入れ子と参照・更新パスは128段まで。

実行例: [examples/models/main.qy](../examples/models/main.qy)

## const と読み取り専用（0.26.2）

モデルとモデル配列も `const Point origin = {x=3.0, y=4.0}` のように定数宣言できる。
従来の const と同じく初期値はコンパイル時に確定する式に限り、関数・メソッド呼び出しは使えない。
フィールドの既定値と他の const を利用でき、public フィールドは定数式として取り出せる。
file を含むモデル型は const の対象から除外する。

`public readonly double distance() = math.sqrt(this.x^2 + this.y^2)` のように、
暗黙の this を読み取り専用にするメソッドを定義できる。数式・ブロック形式のどちらでも使える。
const / readonly の対象からは readonly メソッドだけを呼べ、入れ子・配列・ref・入力・addr を通じた変更も禁止する。
readonly は実行時の参照権限であり、コンパイル時実行や純粋関数を意味しない。

補助関数の引数には `ref readonly Point point`、配列には `ref readonly int items[][]` と書く。
呼び出しは従来どおり `inspect(ref point)`。const も渡せ、変更可能な参照へ権限を戻すことはできない。
独立した値コピー・値引数・戻り値は、コピー先の宣言に従って変更できる。

readonly の file は is_open・比較などの観測だけを許可する。read / readline も現在位置を変えるため禁止し、
file を含むモデル・配列も値コピーや戻り値を介して共有ハンドルを持ち出せない。
明示 close の責任を維持し、変更可能な所有元やメソッドから閉じる。

詳細は [const モデルと読み取り専用の仕様](model-readonly-design.md)、
実行例は [examples/models/readonly.qy](../examples/models/readonly.qy) を参照。

## カスタム初期化（0.26.3）

モデル内に `public void init(...)` または `public bool init(...)` を定義できる。
init のある型の `{名前=値}` は init の名前付き引数になり、private フィールドは本体で設定する。
省略可能な引数には `int n=1` のように既定値を書ける。既定値は通常関数・メソッドには追加しない。

```text
model Counter;
	private int count
	public bool init(int initial=0);
		if(initial < 0);
			return false
		this.count = initial
		return true
	public readonly int current() = this.count

int main();
	Counter counter = {initial=3}
	io.print(counter.current())
	return 0
```

void は通常完了、bool は true で生成に成功する。false は生成位置を示す実行時エラー70で停止する。
init は直接呼べず、通常の代入・引数・戻り値・配列コピーでは再実行しない。
明示引数は記述順、足りない既定引数は宣言順に、生成ごとに1回評価する。

必須引数のある子モデルや配列は、親の init 内で全体代入して完成させられる。
完成前の読み取り・部分更新・ref・this 全体の使用を拒否し、全成功経路での完了を検査する。
空配列や末端0要素の形は、モデル要素の既定生成を要求しない。

init の引数は file を含まない値型に限定する。成功時に保持する file の責任は呼び出し元へ渡す。
false の通常 return では、生成中に取得した file を全て明示 close する必要がある。
init と再帰的にそれを含む型は const として生成できない。init のない型の const は維持する。

詳細は [カスタム初期化の仕様](model-initialization-design.md)、
実行例は [examples/models/initialization.qy](../examples/models/initialization.qy) を参照。
