# モジュールの公開制御（新段階7、0.26.16）

型拡張の新段階1〜6の完了後に追加する、新段階7。
トップレベルの関数・const・model・interfaceにpublic / privateを指定できる。
指定を省略した定義はpublicとする。

```text
// geometry.qy
private const SCALE = 2.0

private double square(double x) = x * x

public double squared_length(double x, double y);
	return (square(x) + square(y)) * SCALE

public model Point;
	double x
	double y

// main.qy
import geometry
using geometry as g

int main();
	io.print(g.squared_length(3.0, 4.0)) // 50.0
	return 0
```

## 名前解決

- privateの定義は、宣言したファイルのモジュール内で使用できる。前方参照、
  privateな補助関数や定数を使った公開関数、ローカルのusing、ジェネリックにも対応する。
- 他のモジュールからの修飾名・モジュール別名・選択usingによるprivate参照はエラー。
- `using geometry` は公開名だけを導入する。非公開名は外部のローカル名や他のusingと衝突しない。
- importしたモジュールやusingで導入した名前を、自動的に再公開する仕組みは追加しない。
  公開する関数や定数を、そのモジュールで定義する。
- モデルのprivateメンバーは引き続き宣言元のモデル内部だけでアクセスできる。
  モジュールのprivateはトップレベルの定義の公開範囲、メンバーのprivateはモデル内の公開範囲である。
- public / privateは宣言の先頭に1個だけ指定する。import・using・ローカル変数・関数引数には付けない。
  mainは公開の `int main();` または `public int main();` とする。

## 公開APIの型

公開した定義から、外部が名前を解決できないprivateなmodel / interface型が漏れないよう検査する。
配列は末端の要素型まで検査する。

- 公開関数・公開メソッド・公開init・公開interfaceの契約の引数と戻り値。
- 公開constの値型、公開modelの公開フィールドの型。
- 推論した戻り値とconst、公開ジェネリック関数の具体化で確定した引数と戻り値。
  privateな型を扱う汎用の補助関数はprivateとして定義する。

公開モデルのprivateフィールドにprivate型を使うことはできる。
privateモデルやprivate関数の型にもprivate型を使用できる。
公開モデルがprivate interfaceを実装し、それを内部の借用契約として使うこともできる。
privateの型を公開APIに指定した場合は、定義の公開範囲をprivateへ変更するか、公開型へ変換する。

```text
private model State;
	int count = 0

public model Counter;
	private State state
	public readonly int current() = this.state.count
```

private定数を使って公開する数値や文字列を計算することは可能。
型が公開可能であれば、その値をpublic constや関数の結果に使用できる。

## エディターと検証

VS Codeではモジュールのprivate定義を外部の補完・定義参照・ホバー・引数ヒントから除外する。
同じファイル内では補完・参照でき、明示した公開指定を表示する。
構文ハイライトと、コンパイラ診断によるprivate参照の赤線表示にも対応する。

`make test-module-visibility` は22メソッドで、全ての宣言種別、using・別名・衝突、
APIの型漏れ、型推論・具体化、privateな実装型、mainや修飾子の制約を確認する。
