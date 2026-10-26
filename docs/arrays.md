# 配列・参照渡し・要素の追加

配列の次元・上限は変数名の後に書く。int / float / double / bool / str / char で同じ構文を使う。

```text
int matrix[2][3]
float measurements[]
double positions[][]
bool flags[]
str names[]
char letters[]
```

関数の引数も `int matrix[][]` と書く。
戻り値は従来どおり `int make()[];` の形で宣言する。

## 配列全体を渡す

```text
int main();
	int matrix[2][3] = [[0, 1, 2], [3, 4, 5]]
	increase_all(ref matrix)
	io.print(matrix)

void increase_all(ref int matrix[][]);
	for(0 <= i < matrix.length, i++);
		for(0 <= j < matrix[i].length, j++);
			matrix[i][j]++
```

呼び出し側・宣言側の両方に ref を付けると、呼び出し元の配列を変更する。
ref のない配列引数は深くコピーされ、関数内で変更しても呼び出し元は変化しない。
配列の要素型と次元数は一致する必要があり、ref 渡しで型変換はしない。

## 1要素を渡す

```text
int main();
	int matrix[2][3] = [[0, 1, 2], [3, 4, 5]]
	increase_element(ref matrix[0][1])
	io.print(matrix)

void increase_element(ref int value);
	value++
```

呼び出し側で要素を選び、受け取り側は通常のスカラー型の ref 引数にする。
渡された要素は int なので、宣言側に配列の次元や添字を書く必要はない。
既存のスカラー変数の ref 渡しと同じ関数を利用できる。
val は値渡しを明示する予約語のため、引数名には value などを使う。

子配列全体も `grow(ref matrix[i])` と `void grow(ref int row[]);` で渡せる。
char 配列の要素は ref 渡しできる。str 内の文字は可変長UTF-8のため ref 渡しの対象にしない。
定数や一時的な値、for のループ変数の ref 渡しは許可しない。

添字は左から順に1回だけ評価し、次の引数を評価する前に範囲検査を行う。
要素の参照中は、その要素を含む配列と祖先配列の add / remove_at / clear / 再代入を検出し、
終了コード70の実行時エラーにする。関数呼び出しが終われば制限は解除される。
他の要素の値の更新、独立した別の行の伸長、配列のコピーは可能。

## 初期値を持つ要素を追加する

```text
int main();
	int matrix[][]
	increase_length_of_matrix(ref matrix)
	increase_length_of_matrix(ref matrix)
	matrix[0].add()
	io.print(matrix)

void increase_length_of_matrix(ref int matrix[][]);
	matrix.add()
```

出力は `[[0], []]`。引数なしの add() は、要素型に応じた初期値を1個追加する。

| 要素型 | 追加する初期値 |
| --- | --- |
| int | 0 |
| float / double | 0.0 |
| bool | false |
| str | 空文字列 |
| char | U+0000 の1文字 |
| 配列 | 宣言された形で初期化した、独立した子配列 |

上限を省略した子配列は空になる。`int matrix[][3]` の add() は `[0, 0, 0]` の行を追加する。
子配列は共有されないので、1行への追加が別の行に反映されることはない。
既存の add(value) も利用でき、配列を追加する場合は深いコピーを作る。

上限は従来どおり適用する。`int matrix[2][3]` は初期状態で2行3列を持つため、
そのまま add() を呼ぶと上限超過となる。行を増やしたい場合は外側の上限を省略する。
add() と add(value) はどちらも値を返さない。
