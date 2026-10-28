# 独立した列挙型と関連機能（新段階10、0.27.1追加改訂）

状態: 0.27.1の追加改訂としてenum・matchを実装済み。第10節の回復可能I/Oは後続案。
バイナリI/Oの0.27.1は実装済みであり、ユーザー指定により今回も0.27.1の追加改訂として扱う。
CLI・拡張・VSIXのバージョンはユーザー指定どおり0.27.1を維持する。

## 1. 次の実装の優先順位

次段階はenumと、それを安全に使うmatchを中心とする。
ユーザーの訂正に従い、メソッドの括弧省略は追加していない。定義・呼び出しとも既存の()を使う。
その後に回復可能なI/Oを追加すると、エラーコードを文字列や生の整数にせずenumで扱える。

| 範囲 | 内容 |
| --- | --- |
| 新段階10の実装 | 独立したenum型、型に基づくメンバーの省略、表示/変換/入力/配列/モジュール対応 |
| 同時に整備 | enum用matchの網羅性検査、エディター対応 |
| 次段階の設計方向 | io.ErrorCode・io.Error・io.file.try_openによる回復可能I/O |
| 今回含めない | フラグenum、可変の基底型、データ付き列挙、継承、汎用例外/Result、FFI |

## 2. enumはmodelと独立した名前付きの値型

```text
enum sex;
	undetermined
	male
	female
```

この宣言だけでsexを型名として使用でき、typedefや `enum sex value` は不要。
`sex gender`、`sex genders[]`、引数・戻り値・modelのフィールドで使用できる。
enum自体にフィールド・メソッド・init・implementsは持たせず、modelへ暗黙変換もしない。
宣言場所はファイルのトップレベル。model内や関数内への入れ子の宣言は初版では許可しない。
インデント・コメント・行区切りは既存の型宣言と同じ。

型の同一性は宣言そのものによる。同じ数値・同じメンバー名でも、異なるenumは別の型。
型名は既存の関数・定数・型・名前空間と衝突させず、メンバー名はenum内で一意とする。
メンバーには既存の識別子規則を適用し、予約語を使用しない。

```text
enum State;
	none_state
	ready

// sex.maleとState.readyはどちらも内部値1だが、別の型。
```

## 3. 数値・既定値・有効な値

初版の内部値は固定のsigned int_32。lib.sizeは32を返す。
実際のABIスロットは従来どおりであり、配列を32bit詰めに変更する意味ではない。

- 最初の値を省略すると0。以後、省略した値は直前の値＋1。
- `female = 3` のように明示でき、飛び番・負の値も許可する。
- 明示値はint_32の範囲に収まる整数の定数式。0b/0o/0d/0xも既存の規則で利用できる。
- 同じ数値を複数のメンバーへ割り当てる別名は初版では禁止。表示名を一意にするため。
- 値0のメンバーを必須とし、その値を既定値にする。0は先頭以外に明示してもよい。
- 空enum、重複名、重複値、0の欠落、範囲外の明示値、連番のオーバーフローはコンパイルエラー。
- 宣言されていない数値はenumの値として保持しない。飛び番の穴も無効。

元の例の `undetermined / male / female` は **0 / 1 / 2** になる。
`gender = 3` でもfemaleにしたい場合は、次のように明示する。

```text
enum sex;
	undetermined       // 0。既定値
	male               // 1
	female = 3         // 3。2は未定義
```

`sex gender`、`sex genders[3]` の初期値・不足要素、`genders.add()` は全て値0のメンバー。
modelの省略フィールドも同じ。未初期化や「名前のない0」は作らない。

明示値から既存constや先に定義したメンバーを参照する場合も、通常の定数式と型規則を適用する。
例えば `int(male) + 2` は整数式だが、`male + 2` はenumの算術として拒否する。
enum内の後続メンバーへの前方参照は初版では許可せず、constとの依存循環も診断する。

## 4. メンバー名の解決と省略

基本形は `sex.female`。メンバーを通常のグローバル変数名として全て展開しない。
代入先などの期待型が1つのenumに確定する場合、未解決の名前をそのenumのメンバーとして解決する。

```text
sex gender = female
gender = male
user.gender = female
users[1].gender = female

void set_gender(ref sex target, sex value);
	target = value

// valueの型からfemaleをsex.femaleと判断できる。
set_gender(ref gender, female)
```

適用先は宣言・代入・model初期化子・配列要素/リテラル・型付き引数・戻り値・比較・matchのcase。
比較では型が分かっている側から他方へ期待型を伝え、`gender == female` と `female == gender` を許可する。
暗黙のグローバル探索は行わず、複数enumから同名メンバーを推測しない。
通常の変数・定数・using等の名前解決を優先し、見つからない単一の名前にだけ期待型の省略規則を使う。
既存の変数femaleがある場合、その変数を無視してメンバーへ読み替えない。

```text
auto a = sex.female   // sexを推論
b = sex.male         // 未宣言bもsexを推論
c = female           // 期待型も通常の束縛もなければエラー
int n = female       // intはenumの期待型ではないため、暗黙にsexを探さない
```

enumメンバーを値として導入する選択usingも許可する。
`using profile.sex` は型sexの導入、`using profile.sex.female` は値femaleの導入。
型のusingによってメンバー全体は展開しない。asは引き続きモジュールの別名だけに使用する。
同じ値の重複usingは許可し、異なる型の同名メンバーを同じ裸名で導入する場合は衝突エラー。

## 5. 数値代入・明示変換・演算

| 操作 | 規則 |
| --- | --- |
| 同じenumの代入・引数・戻り値 | 許可 |
| 整数定数からenum | 期待型があり、宣言された有効値と一致する場合だけ暗黙変換 |
| 動的な整数からenum | `sex(number)` の明示変換。範囲とメンバーの存在を実行時検査 |
| enumから整数 | `int(gender)` 等の明示変換。変換先の整数型の範囲も検査 |
| 他のenumからenum | 暗黙・直接の明示変換とも拒否。必要なら整数を経由して意図を明示 |
| bool・小数・複素数からenum | 拒否。数値変換を必要とする場合は先に整数へ明示変換 |
| strからenum | `sex("female")` 等。第6節の名前・数値の解析規則を適用 |
| == / != | 同じenum同士。有効な整数定数や省略メンバーにも期待型を適用 |
| < / <= / > / >= | 初版では拒否。順序が必要なら整数へ変換 |
| + / - / ++ / -- / 複合代入 / ビット・シフト / 論理演算 | enumのままでは拒否 |

```text
sex gender = 3          // female=3と宣言した場合は可
gender = 2             // この宣言では2が存在しないためコンパイルエラー

int code = io.input()
gender = sex(code)     // 有効でなければ70。既存のgenderは更新しない
int saved = int(gender)
```

列挙型コンストラクターは位置引数1個を受け付け、名前付き引数は導入しない。
既存のint / strのbase・prefix引数もenumの変換には指定せず、必要なら先に整数へ変換する。
定数で分かる不正値はコンパイルエラー。動的な不正値・文字列は実行時エラー70。
int_128等からの変換でも、先に32bitへ切り詰めて別の有効値へ変えることはしない。
refは同じenum型の変更可能な場所だけを受け付け、整数や別enumのrefとは交換できない。
既存の `.value` はenumとしての値、`.addr` は既存の場所の規則を維持し、整数への抜け道を作らない。

## 6. 表示・文字列・入力・lib

| API | enumの扱い |
| --- | --- |
| io.print / 文字列補間 / io.file.write | 宣言時のメンバー名だけを表示。例: female |
| `str(gender)` | 同じメンバー名をstrで返す |
| `int(gender)` | 内部数値。通常のprintや基数書式で表示できる |
| `lib.size(gender)` | 32 |
| `lib.size(enum配列)` | 現在の要素数に応じて32bitずつ合計 |
| lib.length(enum値) | 型エラー。数値の桁数ならintへ、名前の長さならstrへ変換 |
| lib.length(enum配列) / 配列.length | 外側の要素数。既存どおり |
| io.file.write_bytes(enum配列) | 型エラー。バイト配列へ暗黙変換しない |

```text
print(gender)                    // female
print("gender: {gender}")        // gender: female
print(int(gender))               // 3
print("code: {int(gender):0x}")  // code: 0x3
```

enumそのものの `{gender:0x}` のような数値基数書式は初版では拒否し、intへの変換を明示する。
model・配列の自動表示でも、要素/フィールドのenumは同じ名前で表示する。

入力と `sex(text)` は前後のASCII空白を除き、大文字小文字を区別する裸のメンバー名、
または既存の整数読み取りと同じ10進/0b/0o/0d/0xの数値表記を受け付ける。
`"female"` や `"3"` は有効、`"Female"`・`"sex.female"`・未定義値は無効。
符号や基数の書式が正しくても、int_32の範囲外や未定義値は拒否する。

`io.input(gender)`、パターン内のenum入力先、`sex gender = io.input()` に対応する。
boolで受け取る入力の失敗はfalseで全入力先を保持し、文としての入力失敗は既存どおり65。
値を返す文脈型入力の失敗も65。明示コンストラクターの不正引数70とは呼び出しの意味で区別する。
複数入力では全enumの妥当性を確認してから反映し、部分的な更新を行わない。

## 7. model・関数・モジュールへの統合

enumは通常の値型として、全次元の配列、const、auto、modelフィールド、初期化子、
値/ref/readonly引数、戻り値、ジェネリック関数に対応する。
型推論でもenumの同一性を保持し、内部値が同じという理由でintや別enumへ統合しない。

`public enum` / `private enum` をトップレベルで許可する。省略時はpublic。
メンバーの公開範囲は型に従い、メンバー個別のprivateは許可しない。
公開APIへprivate enum型を含めることは、既存のprivate model/interfaceと同様に拒否する。
import先は `profile.sex.female`、モジュール別名なら `p.sex.female` と記述できる。
ファイル単位のimportとusingの規則、名前の衝突・非公開の診断を維持する。

## 8. matchを同時に整備する

初版はenum専用の文として導入し、enumの全メンバーを処理しているか検査する。
式としてのmatch、範囲パターン、分解・束縛、条件付きcaseは今回は追加しない。

```text
match(user.gender);
	case undetermined;
		print("未設定")
	case male, female;
		print("設定済み")
```

- 対象式は1回だけ評価し、caseは対象のenum型に確定した定数だけを許可。
- caseは1個またはカンマ区切り。修飾名・省略名・有効な整数定数を同じ型規則で扱う。
- 同じメンバーを別の表記で重複させてもエラー。異なるenumのメンバーも拒否する。
- 全メンバーをcaseで列挙するか、最後に `else;` を置く。どちらもなければ列挙漏れをコンパイルエラー。
- elseは1回だけ、最後に記述する。全件列挙に加えてelseを書くことは許可する。
- 各caseは自動的に終了し、次のcaseへフォールスルーしない。caseを終えるためのbreakは不要。
- matchはループではない。中のbreak / continueは、存在すれば最も近い外側のループへ作用する。
- 各caseは独立したブロック。return・初期化・readonly・closeを既存の分岐と同様に検査する。
- 全caseでreturnした場合は関数の戻り値経路を満たす。closeも全ての到達可能な出口で検証する。

新しいメンバーを追加したとき、網羅したつもりのコードの不足を検出できる。
未知の将来メンバーをまとめて扱う方針ならelseを使用する。

## 9. ユーザー例に対応する全体像

female=3を明示するため、メンバー名と数値3のどちらでも同じ値を指定できる。
実行例は `examples/enums/main.qy` に収録する。

```text
using io

enum sex;
	undetermined
	male
	female = 3

model User;
	str name
	int age
	sex gender

	readonly void user_info();
		print("=== USER INFO ===")
		print("{this.name}'s age: {this.age}")
		print("{this.name}'s gender: {this.gender}")

int main();
	User users[2]
	users[0].name = "Ken"
	users[0].age = 23

	users[1].name = "Allice"
	users[1].age = 20
	users[1].gender = female  // sex.female または3でも可

	users[0].user_info()
	users[1].user_info()
	return 0
```

出力:

```text
=== USER INFO ===
Ken's age: 23
Ken's gender: undetermined
=== USER INFO ===
Allice's age: 20
Allice's gender: female
```

## 10. 続く回復可能I/Oの方向

enum導入後は、次にio.file.try_openを実装するのが有用。
例えば標準の `io.ErrorCode` をno_error=0 / not_found / permission_denied / already_exists /
invalid_path / not_regular_file / io_errorなどのenumとする。
`io.Error` はcode・message・path・os_code等を持つ独立したmodelにする。
「エラーの分類」はenum、「詳細な情報」はmodelと役割を分ける。

候補APIは `try_open(path, ref destination, ref error, mode="r") -> bool`。
destinationは未オープンを要求し、成功時だけ新しいclose責任を作る。
失敗時はdestinationを未オープンのままにしてerrorを返し、成功時はerrorをno_error状態へリセットする。
利用者が所有する既存ハンドルを黙って閉じたり上書きしたりしない。
成功/失敗の分岐とclose解析を対応させる必要があるため、enumと同時には実装せず別段階にする。
try_read / try_writeは部分転送量を含むエラー契約が必要であり、try_openの後に設計する。

この方向を具体化した0.27.2の[回復可能I/Oとtry制御文の仕様](recoverable-io-design.md)では、
try/case/finally・再送出・回復経路のclose検査も含めて実装した。

## 11. 実装順と検証観点

1. enum宣言・型ID・メンバー表・定数検査・既定値とモジュール公開制御。
2. 期待型による名前解決、代入/比較/引数/配列、明示変換と無効値の拒否。
3. 定数評価と実行時の表示・入力・lib・ref・model・ジェネリックを同じ規則に揃える。
4. matchと網羅性、戻り値・初期化・close解析を統合する。
5. VS Codeのenum/メンバー補完、型に基づく候補、色分け、定義参照、case候補、診断。

テストでは連番/飛び番/負値/境界/重複/0欠落、数値代入、型の取り違え、名前の衝突、
既定値・add・不足要素・多次元配列、値/ref/readonly、変換失敗時の未更新、入力の全体反映、
表示・基数・lib、private型の漏れ、matchの網羅性/重複/評価回数/制御フロー、
通常のメソッド呼び出し・private/readonlyとの統合を確認する。
Linux / Windows・アセンブリ・VS Codeを検証し、必要な個別確認後にmake testを1回成功させる。
実装セットの完了後にdevへ1回pushし、VSIXとZIPを作成して次の指示を待つ。

## 検証と配布物

`make test-enums` の45メソッドで、ユーザー例・値域・型・入力の全体反映・配列・model・モジュール・matchを検証する。
VS Codeではenum/メンバー/期待型/caseの補完・定義参照・アウトライン・構文ハイライト・コンパイラ診断に対応する。
同じ0.27.1の追加改訂なので、VSIXは既存の0.27.1_VSCode_option.vsixを更新する。
