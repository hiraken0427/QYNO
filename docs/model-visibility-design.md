# model の公開制御

状態: QYNO 0.26.1 で実装済み。

モデルメンバーに `public` / `private` を指定できる。
`const` モデル・読み取り専用メソッド、カスタム初期化、インターフェースは後続の実装単位に分ける。
公開指定を省略した 0.26.0 のモデルは、引き続き public として動作する。

## 1. 宣言と既定の公開範囲

フィールドとインスタンスメソッドの先頭に、公開指定を1つだけ書ける。
指定を省略したメンバーは `public` とする。

```text
model Counter;
	private int count = 0

	private void add_positive(int amount);
		if(amount > 0);
			this.count += amount

	public void increment();
		this.add_positive(1)

	public int current() = this.count

	public void copy_from(Counter other);
		this.count = other.count

	public Counter doubled();
		return {count=this.count * 2}

int main();
	Counter counter = {}
	counter.increment()
	counter.increment()
	io.print(counter.current())  // 2

	Counter copy = counter
	copy.increment()
	io.print(counter.current())  // 2
	io.print(copy.current())     // 3

	Counter doubled = counter.doubled()
	io.print(doubled.current())  // 4
	io.print(counter)            // Counter{}
	return 0
```

文法は `member = visibility? (field | method)`、`visibility = public | private` とする。
公開指定は各宣言に書く。`public;` などによるセクション指定は導入しない。
配列フィールドや数式形式のメソッドにも同じ規則を適用する。

`public public`、`private private`、`public private`、`private public` は全て宣言エラー。
モデル型自体、トップレベルの関数・定数、引数・ローカル変数には公開指定を書けない。
`protected`、`internal`、friend、静的メソッド、メソッドのオーバーロードは今回導入しない。

`public` と `private` は新しい予約語にする。従来その名前を識別子として使用しているコードは改名が必要。
公開指定を省略している従来のモデル定義は public として解釈する。

## 2. アクセス権は宣言元のモデルに属する

メンバー名を使用する式の字句上の所属モデルを `A`、対象メンバーの宣言元モデルを `D` とする。
private にアクセスできる条件は `A == D`。
比較には名前の綴りではなく、モジュールを含めて解決したモデル型の同一性を使う。
トップレベルの関数や main の所属モデルは「なし」とする。

| 使用する位置 | public | private |
| --- | --- | --- |
| 宣言元モデルのメソッド | 許可 | 許可 |
| 同じモデル型の別インスタンスを、そのモデル内部から操作 | 許可 | 許可 |
| 同じファイルの通常の関数・main | 許可 | エラー |
| 同じファイルの別モデル | 許可 | エラー |
| 別モジュールの通常の関数・モデル | 許可 | エラー |

`Counter.copy_from` では `other.count` も許可する。
private は「そのインスタンスの this からだけアクセスできる」という規則にはしない。

メソッドを呼び出す権限と、その本体からメンバーを使用する権限は個別に判定する。
外部から public メソッドを呼ぶと、その本体は宣言元モデルの権限で private を操作できる。
public メソッドから private メソッドを呼ぶこともできる。
呼び出し元がその権限を受け取ることはない。

```text
Counter counter
counter.increment()       // 許可
counter.count = 100       // エラー: private フィールドへの書き込み
io.print(counter.count)   // エラー: private フィールドの読み取り
counter.add_positive(10)  // エラー: private メソッドの呼び出し
```

存在確認と公開制御の検査は、コードが実行されるかどうかにかかわらず行う。
`if(false)` の中や、呼ばれていないメソッドの本体にある不正アクセスもコンパイルエラー。

## 3. 全てのメンバー利用に同じ検査を適用する

private フィールドの読み取りだけでなく、次の操作も同じアクセス判定を受ける。

- 代入、複合代入、インクリメント・デクリメント。
- 配列の添字、length、add / remove_at / clear。
- 文字列の添字読み書き。
- `ref` 引数として渡す操作。
- `io.input` の直接入力先・文字列内の入力先として指定する操作。
- `io.print` / `io.file.write` の埋め込み式で指定する操作。
- メソッドの対象となるフィールドの指定。

`ref Counter alias = counter.addr` として全体を参照しても、外部の `alias.count` はエラー。
`counter.value.count`、配列要素、一時値や関数の戻り値からのアクセスも、同じ規則で検査する。
既存の `.addr` の制約は引き続き適用し、フィールドの `.addr` は今回導入しない。

モデルのメソッド内部から `helper(ref this.count)` と外部関数へ渡すことは許可する。
外部関数は、その引数を介して値を操作できる。これはメソッドが明示的に許可した操作として扱う。
private は、既に渡した値・参照・共有 file ハンドルの利用を禁止する仕組みにはしない。

## 4. 入れ子ではメンバーごとに判定する

`outer.inner.secret` では、inner と secret をそれぞれの宣言元モデルについて検査する。
Outer が Inner 型のフィールドを所有していても、Inner の private にアクセスする権限は得ない。

```text
model Inner;
	private int secret = 7
	public int current() = this.secret

model Outer;
	private Inner inner

	public int read() = this.inner.current()  // 許可
	public int invalid() = this.inner.secret // エラー
```

配列や別名を挟んでも判定は変わらない。
`this.items[i].secret` の secret が別モデルの private であれば、所有する側からもアクセスできない。

## 5. 初期化子と既定値

明示的に指定したフィールド名は、初期化子を書いた位置の権限で検査する。
初期化先の型を知っているだけでは、その型の private を指定できない。

```text
Counter a            // 許可: private count は既定値
Counter b = {}       // 許可: 同上
Counter c = Counter() // 許可: 同上
Counter d = {count=5} // エラー: 外部からの明示指定
```

同じ検査を、再代入、関数の引数・戻り値、配列の要素・add の引数、入れ子の初期化子に適用する。
例えば、外部での `Counter items[] = [{count=5}]` もエラー。
一方、Counter のメソッド内の `return {count=5}` は許可する。

外側の初期化子にアクセス権があっても、内側のモデルのアクセス権は得ない。
初期化子内の全ての明示的な指定を、元の字句上の所属モデルで判定する。

コンパイラが省略フィールドを既定値で埋める処理には、private の明示指定の検査を課さない。
既定値式そのものは、フィールド宣言位置の所属モデルの権限で一度検査する。
その式が外部の生成場所で再利用されても、生成側に新しいアクセス権は与えない。
別モデルの private を指定する既定値式は宣言時にエラー。

private フィールドの外部からの初期値設定が必要な場合は、public メソッドを通じて設定する。
独自の生成時処理を定義する構文は、後続のカスタム初期化設計で扱う。

## 6. コピー、値比較、自動表示

### コピー

通常の代入、値引数、戻り値、モデル・配列のコピーでは private を含む全状態をコピーする。
private を既定値に戻したり、コピーから除外したりしない。
`target = source` や `target = {}` による全体の置き換えも許可する。
既存の深いコピー、file の共有、参照中の所有元の変更検査を適用する。

### 比較

同じモデル型の `==` / `!=` は、private を含む全フィールドを値として比較する。
フィールド名を外部から指定できるかどうかと、値全体の比較を分けて扱う。
全フィールドが private のモデルでも、異なる状態の値が全て等しいことにはならない。

### 表示

`io.print(model)` と文字列内の `{model}` による自動表示には、public フィールドだけを含める。
private フィールドは名前も値も省略する。全フィールドが private なら `Counter{}` と表示する。
公開フィールドが宣言順に並ぶ形式は維持する。
配列・入れ子も、各モデルの public フィールドだけを再帰的に表示する。

表示の結果は呼び出し位置に依存させない。モデルの内部でも `io.print(this)` は public のみ。
内部で特定の private を表示したい場合は、`io.print(this.count)` と明示する。
public メソッドが private の値を返すことも、モデルの作者による明示的な公開として許可する。

private はアクセス制御であり、暗号化や比較結果からの推測防止を保証するものではない。

## 7. モジュールと公開指定の境界

モデル型は、現在と同じくモジュールの公開名になる。
今回の private はメンバー単位であり、モデル型自体をモジュール外から隠す指定には使わない。

`import`、モジュールの別名、`using module`、`using module.Counter` はアクセス権を増やさない。
同じ綴りのモデル名が複数のモジュールにあっても、それぞれ別のモデルとして判定する。
メソッドをモジュール関数として直接呼び出す構文や、private を指定する using は導入しない。

## 8. file の close 検査

private にした file フィールドにも、現在の明示 close の責任を適用する。
通常の呼び出し元がフィールドへ直接アクセスできない場合は、public メソッドで操作する。

```text
model TextFile;
	private file handle

	public void open(str path);
		io.file.close(this.handle)
		this.handle = io.file.open(path, "r")

	public bool readline(ref str line);
		return io.file.readline(this.handle, ref line)

	public void close();
		io.file.close(this.handle)

int main();
	TextFile reader
	reader.open("input.txt")
	str line
	while(reader.readline(ref line));
		io.print(line)
	reader.close()
	return 0
```

この例の open は、未オープン状態にも許可される close を最初に呼ぶ。
再度 open するときも、それまでのハンドルを明示的に閉じる。
呼び出し側の `reader.close()` を削除すれば、現在と同じく close 漏れのコンパイルエラー。
private や自動表示によって close の責任を免除しない。

## 9. 診断とVSCode

不正なアクセス位置を主診断とし、モデル名・メンバー名・private の宣言位置を示す。
例: `Counter.count は private です。この位置からはアクセスできません`。
未知の名前、重複宣言、公開指定の誤りは、それぞれ別の診断にする。

VSCode では、外部の補完候補から private を除外する。
同じモデル内部では this と同じモデル型の別インスタンスについて private も提示する。
別モデルの private は内部からでも提示しない。
ホバーとアウトラインに公開指定を表示する。手入力された不正なメンバー名のホバーでは、
private であることを説明できる。定義位置の参照はアクセス権の付与にはならない。

## 10. 実装手順

1. メンバー宣言の public / private を解析し、フィールドとメソッドに公開指定・宣言元モデルを保持する。
2. ユーザーが書いたメンバー利用と初期化子に、字句上の所属モデルを保持する。
3. 型と名前を解決した後、フィールド・メソッド・明示的な初期化項目を検査する。
4. 検査に成功したフィールドだけを、現在の添字形式へ変換する。メソッドの this 引数への変換も検査後に行う。
5. 省略した既定値・内部コピーなどの生成処理は、明示的な利用と区別する。ユーザーの式を生成処理扱いにして検査を省略しない。
6. モデル表示ルーチンでは public フィールドだけを選び、公開フィールド数に応じて区切りを出す。コピー・比較・GC・close 解析では全フィールドを保持する。
7. VSCode の解析・候補選択・表示に同じモデル同一性と公開範囲を反映する。

既定値式と型検査済みASTが再利用されるため、現在チェックしている関数だけでアクセス権を推測しない。
元の式の所属モデルを保持し、コピー・別名解決・AST変換でもその情報を失わないようにする。
実行時のメモリ配置、モデルの型同一性、暗黙の this の渡し方は既存のものを使う。

## 11. 受け入れ条件

- 無指定と public のフィールド・メソッドで、既存のモデルサンプルが動く。
- 内部の this・同型の別インスタンスからの private 利用は許可する。
- 同一ファイルの main・通常の関数・別モデル、別モジュールからの private 利用を拒否する。
- private の代入・複合更新・配列操作・文字列操作・ref・入力・埋め込み・メソッド呼び出しを個別に検査する。
- モジュール別名、using、同名異型、モデル配列・戻り値・一時値・value 経由でも規則を維持する。
- 外部の明示的な private 初期化を、直接・引数・戻り値・配列・入れ子の各文脈で拒否する。
- 内部の明示初期化、private の省略、既定値式の宣言位置での検査を区別する。
- 外部からコピーした private 状態が保持され、比較にも反映される。
- 自動表示は外部・内部で同じ結果となり、入れ子・配列にも private の名前と値が出ない。
- public フィールドが0個・一部・全ての場合に、表示の区切りが正しい。
- private file の public メソッド経由の close を認識し、close 漏れと早期 return を検査する。
- アクセス指定の重複、指定できない位置、予約語としての利用を診断する。
- VSCode の補完・ホバー・定義参照・アウトラインを、内部・外部・別モデルで確認する。

実装時は必要な個別検証の後、全体テストを1回実行する。
専用テストは `make test-visibility`、実行例は `examples/models/visibility.qy`。

## 12. 後続機能への接続

- `const` モデル・読み取り専用メソッド: public / private は利用可能な位置、読み取り専用は変更可能な操作を決める。両方の条件を満たす必要がある。
- カスタム初期化: 型名を繰り返さない生成記法を維持し、公開する初期化入力と private の内部状態を区別する。コピー時に初期化処理を再実行するかどうかも独立して決める。
- インターフェース: 外部へ公開する操作と、その具体的な実装を分ける。private メソッドを外部から呼べる契約として扱わない。
- 継承: 値コピー・基底型への変換・ref・多態性の意味を設計した後に検討する。今回の private を同一ファイル・派生型へ広げる規則にはしない。
