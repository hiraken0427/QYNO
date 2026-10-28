# 標準ライブラリの統一・拡張（0.27.0、新段階8）

状態: 0.27.0で実装済み。後続候補のAPIは未実装。
既存の入出力の動作は維持し、名前解決の互換例外を廃止する。
本書の「0.27.0対象」と「後続候補」は区別する。候補一覧の全機能を1回で実装する計画ではない。

## 1. importとusingを分離する

標準モジュールは `io`・`math`・新設の `lib`。そのサブモジュールもimport不要。
import不要とは修飾名が常に使えることを指し、関数の省略名を自動導入する意味ではない。

| 記述 | 0.27.0の規則 |
| --- | --- |
| `io.print(x)` / `io.input(x)` | import・using不要 |
| `math.sqrt(x)` / `lib.size(x)` | import・using不要 |
| `print(x)` / `input(x)` | `using io` または該当関数の選択usingが必要 |
| `sqrt(x)` | `using math` または `using math.sqrt` が必要 |
| `size(x)` / `length(x)` | `using lib` または該当関数の選択usingが必要 |
| `using io as console` | `console.print(x)`。裸のprintは導入しない |
| `import io` | 許可するが、省略名は導入しない |
| `using io.file as fs` | `fs.open(...)` |
| `using io` | io直下の関数と子名前空間を導入。`file.open(...)`・`path.join(...)` は可、`open(...)`・`join(...)` は不可 |
| `using io.path` | path直下の関数を導入して `join(...)` などを使用 |

usingは既存のファイル・ブロックスコープと同じ。他ファイルから引き継がない。
同一シンボルの重複usingは許可し、異なる関数を同じ名前に導入した場合は衝突エラー。
将来の標準モジュールにも同じ規則を適用し、短い関数名のために特例を作らない。
`lib` も他の標準モジュールと同様、ユーザーのファイルや変数で置き換えない。

型変換 `int(x)`・`double(x)`・`str(x)`・`char(x)`・`complex(...)` 等は言語の型構築でありusing不要。
演算子 `|x|` はmath.absと同じ計算でもusing不要。配列のadd等のメソッド、既存の.lengthも維持する。
printは従来どおり文としての構文、inputは従来どおり文脈型・入力先・入力パターンを扱う。
本変更でユーザー定義print/inputや第一級関数を追加しない。既存の予約名制約も維持する。

```text
using io.print, io.input
using math as m
using lib

int main();
	int value = 42
	print("value={value}, bits={size(value)}, digits={length(value)}")
	print(m.sqrt(9))
	return 0
```

互換オプションや暗黙usingは設けない。裸の標準呼び出しには、例えば
「printを省略して呼ぶにはusing io.printを宣言するか、io.printと記述してください」と診断する。
既存のサンプル・テストはusing追加または修飾名へ移行する。mathは既にusing中心のため、
今回の主な互換変更は裸のprint/inputだが、全標準関数で規則を検証する。

## 2. lib.length(value) -> int_64

値の種類に応じた「長さ」を返す。引数は1個で、式も指定できる。型名だけを渡す構文は導入しない。

| 引数 | 結果 |
| --- | --- |
| char | 1（拡張書記素クラスタ1個） |
| str | Unicode 17.0.0の拡張書記素クラスタ数。既存の.lengthと同一 |
| 配列 | 外側の現在の要素数。既存の配列.lengthと同一 |
| signed / unsigned整数 | 符号を除いた10進桁数。0は1 |
| half / float / double / quadruple | 保存された数値の絶対値の整数部分の10進桁数。整数部分0は1 |
| bool / complex系 / void / model / interface / file / addr | 型エラー |

数値は表示文字列の長さではない。符号、小数点、小数部、指数表記の文字数を数えない。
`length(-42)=2`、`length(3.1415)=1`、`length(-0.25)=1`、`length(0.0)=1`、
`length(1e6)=7`、`length(1e-6)=1`。負のゼロも1。
NaN・正負の無限大は桁数を定義できないため、実行時の引数エラー70とする。
定数評価で分かる場合も同じ不正値としてコンパイル診断する。

整数最小値の絶対値でオーバーフローさせない。128bit整数を64bitへ落とさない。
浮動小数点は表示用の丸めやlog10の境界誤差に依存させず、保存値を正確に比較して桁数を決める。
例えばhalfの最大有限値65504は5、doubleで正確な1000.0は4、その直前の表現可能値は3。
quadrupleの大きい値もint_64への変換を経由せず計算する。
10進リテラルの見た目ではなく型へ丸めた後の値で判定する。

配列では要素数を再帰合計しない。2行3列ならlength(matrix)=2、length(matrix[0])=3。
数値に対する `.length` は追加しない。既存プロパティは文字列・配列のままで、数値の桁数はlibで提供する。

## 3. lib.size(value) -> int_64

値の「型で定まる幅または現在の内容のビット数」を返す。
実メモリ使用量、ABIスロット幅、確保済み容量、ファイルサイズ、表示した文字列の長さではない。
この区別は関数の補完説明・ドキュメント・診断にも明示する。

| 型 | sizeの結果（bit） |
| --- | --- |
| int_8 / unsigned int_8 | 8 |
| int_16 / unsigned int_16 | 16 |
| int / int_32 / unsigned int / unsigned int_32 | 32 |
| int_64 / unsigned int_64 | 64 |
| int_128 / unsigned int_128 | 128 |
| half / float / double / quadruple | 16 / 32 / 64 / 128 |
| complex / complex_64 | 64（float実部32＋float虚部32） |
| complex_128 | 128（double実部64＋double虚部64） |
| bool | 1 |
| void / Void | 0 |
| char / str | 現在のUTF-8本体のバイト数×8 |
| 配列 | 現在存在する全要素のsizeの合計。子配列は再帰計算 |
| model | 現在の全フィールドのsizeの合計。privateフィールドも含む |
| interface / file / addr | 型エラー。指す先や外部リソースのサイズと混同しない |

bool=1、void=0は論理幅であり、現実の変数・配列の格納単位を詰める変更ではない。
intやhalfの変数も現在のABIでは8バイトスロットを持ち、128bit値は別領域を持つがsizeには影響しない。
参照変数は参照先の値について計算する。ポインタ自身の幅を返すAPIではない。
autoは推論後の型、constやreadonlyも通常の値と同じ。

charとstrは終端NUL・長さヘッダー・参照・GC管理情報を含めない。
埋め込みNULは本体の1バイトとして数える。正規化しない。
`"é"` と `"é"` はどちらもlength=1だが、sizeはそれぞれ16と24。
絵文字や結合文字を単純な「1文字=32bit」として数えない。

配列の宣言上限や内部予約容量ではなく現在の要素が対象。
`int values[3]` は初期値を持つ3要素なので96bit、空の `int values[]` は0bit。
`int matrix[2][3]` は192bit、`size(matrix[0])` は96、`size(matrix[0][0])` は32。
`str names[]` や不規則な多次元配列は、それぞれ現在の本体サイズを合計する。
配列間で共有される管理情報を数えないので、コピーしても値が同じならsizeは同じ。

modelは整列パディングやメソッドを数えない。privateフィールドの値や名前を公開せず、合計だけを返す。
これは隠蔽の迂回による値の取得ではないが、privateな可変長データの長さが合計から推測されうることは仕様とする。
file・interface・addrを含むmodelや配列は、空でも静的な要素型・フィールド型の検査で拒否する。
循環した所有データ構造は今回追加しない。将来導入する場合はsizeの再帰規則を再設計する。

### 評価・安全性・計算量

- 両関数は引数を左から通常どおり1回だけ評価する。`size(make_value())` の呼び出しを省略しない。
- 型を見るための暗黙変換をしない。`size(int_8(42))=8`、`size(42)=32`。
- 配列・モデルを値渡しのために深いコピーしない。組み込みの読み取り専用操作として観測する。
- const式では同じ規則で定数評価できる。定数評価と実行時の結果を一致させる。
- 固定幅型のsizeと配列lengthはO(1)。可変長要素を含むsizeは走査が必要。
  文字列lengthは書記素の走査が必要で、数値lengthはその型の最大桁数以内で処理する。
- sizeの合計・UTF-8バイト数×8がint_64最大値を超える場合は、折り返さずエラー70。
- libは値の計測だけを扱い、ファイルシステム・パス・ネットワーク機能を混在させない。

## 4. ioの役割と名前空間

ioはInput / Outputの略。コンソール・ファイル・ディレクトリ・パスを同じツリーで扱う。
基本操作は短く、同じ名前の操作でも対象を子モジュールで区別する。

| 名前空間 | 役割 | 0.27.0の範囲 |
| --- | --- | --- |
| io | コンソールのprint / input | 既存動作維持、usingを厳格化 |
| io.path | パス文字列の分解・結合 | 下記の基本9関数を追加 |
| io.file | ファイル内容・ハンドル・ファイル操作 | 既存7関数を維持。追加APIは後続 |
| io.dir | ディレクトリ操作・列挙 | 後続候補 |
| io.stream | ファイル・コンソール等の共通ストリーム | 後続候補 |
| io.net | ネットワーク | 後続候補。socketやTLSは別途設計 |

`using io` は子モジュールの関数を再帰展開しない。補助名が増えてもprint/inputの短縮に影響させない。
ファイルのサイズは将来の `io.file.size(...)`、値のサイズは `lib.size(...)` と区別する。

## 5. io.pathの基本9関数（0.27.0対象）

全関数はファイルシステムへアクセスせず、UTF-8パス文字列だけを扱う。
対象OSは生成するプログラムのOSであり、クロスコンパイルしたホストのOSではない。

| API | 戻り値 | 意味 |
| --- | --- | --- |
| `join(str base, str child)` | str | パス2個を結合。3個以上は入れ子で結合 |
| `basename(str path)` | str | 末尾のファイル名・ディレクトリ名 |
| `dirname(str path)` | str | 親のパス。親のない相対名なら `"."` |
| `extension(str path)` | str | 最後の拡張子。先頭のドットを含む |
| `stem(str path)` | str | basenameから最後の拡張子を除いた部分 |
| `is_absolute(str path)` | bool | 完全な絶対パスか |
| `is_relative(str path)` | bool | is_absoluteの否定 |
| `normalize(str path)` | str | 区切りの整理・不要な `.` の除去。下記制約あり |
| `with_extension(str path, str extension)` | str | 最後の拡張子を置換。空文字なら削除 |

### パス規則

- Linuxは `/` が区切り。バックスラッシュは通常のファイル名の文字として保持する。
- Windowsは `/` と `\` を区切りとして受け付け、組み立てる結果は `/` を使う。
  ドライブ絶対パス `C:/...` とUNC `//server/share/...` のルートを認識・保持する。
  ドライブ相対 `C:foo` やルート相対 `/foo` はWindowsでは完全な絶対パスではない。
- joinはchildが完全な絶対パスならchildを返す。空の片方は他方を返す。
  間の区切りだけ整理し、内部の `..` や大文字小文字を変更しない。
  Windowsのドライブ相対・ルート相対は単独の分解・判定で扱えるが、joinの非空childには認めずエラー70。
  非空baseがドライブ相対の場合もjoinは拒否する。カレントドライブ依存を暗黙に解決しない。
- normalizeは重複区切りと `.` を整理するが、`..` は消さない。
  `a/link/../b` と `a/b` が同じ実体とは限らず、字句処理で安全を保証できないため。
  相対パスが空になる場合は `"."`。ルート・UNC先頭は維持し、それ以外の末尾区切りは除く。
  Linuxで先頭がちょうど `//` の場合はそのまま保持し、OS固有の解釈を勝手に除去しない。
- basename/dirnameは末尾区切りを除いて処理する。ただしルートは除かない。
  `/` のbasenameは空文字、dirnameは `/`。空パスのbasenameは空文字、dirnameは `"."`。
  Windowsのドライブルート・UNC共有ルートも同様にルートとして保持する。
- extensionはbasenameの最後のドット以降。`.env` は拡張子なし、`a.tar.gz` は `.gz`、
  `.config.json` は `.json`、`file.` は `.`。`.` と `..` は拡張子なし。
- with_extensionは空文字または `.` で始まる拡張子を受け付ける。
  区切り文字を含む拡張子、basenameのないルート・空パス、`.`・`..` の置換は引数エラー70。
  親部分を保持し、拡張子がなければ追加する。`.env` に `.txt` を指定した結果は `.env.txt`。
- Windowsのデバイス名前空間 `//?/`・`//./` は基本パスの規則と異なるため初版では引数エラー70。
- 全てUTF-8を検証。不正UTF-8は65、埋め込みNUL・不正引数は70。
  ファイル名のOS上の全禁止文字や存在は検査しない。パスの安全なアクセス許可判定にも使わない。
- `~`・環境変数・ワイルドカードを展開しない。相対パスの基準を変えず、importの探索とも無関係。
- 定数評価でもターゲットOSの規則を使う。

```text
using io
using io.path as path

int main();
	str target = path.join("data", "settings.json")
	print(path.basename(target))       // settings.json
	print(path.stem(target))           // settings
	print(path.extension(target))      // .json
	print(path.with_extension(target, ".bak")) // data/settings.bak
	return 0
```

## 6. 入出力APIの拡充候補（0.27.0の必須実装ではない）

小さい機能から段階的に追加する。候補を多く用意しても、未定義の名前を予約・実装済み扱いにはしない。

| 分類 | 候補API | 方針・先に必要な設計 |
| --- | --- | --- |
| パス追加 | io.path.parts / join_all / with_name | 配列との相互変換、名前の置換 |
| 実体パス | io.path.absolute / resolve / relative | 作業ディレクトリ、リンク、異なるドライブ、存在しないパスの扱い |
| バイナリ | io.file.read_bytes / write_bytes | unsigned int_8配列、rb/wb/ab、分割読み込み・上限 |
| 位置・サイズ | io.file.tell / seek / size / truncate | 単位はバイト、バッファ同期、appendの規則 |
| 簡易テキスト | io.file.read_text / write_text / append_text | path指定の一括処理。内部のハンドルを成功・失敗の全経路で閉じる |
| 簡易バイナリ | io.file.read_all_bytes / write_all_bytes | path指定、上限付き読み込み |
| 行単位 | io.file.read_lines / write_lines | str配列、改行規則、最後の改行、大容量時の上限 |
| ファイル管理 | io.file.copy / move / remove / replace | 上書きは明示。replaceの原子性はOS・同一ファイルシステムの条件を明記 |
| メタデータ | io.file.info | 通常ファイル・リンク・サイズ・更新時刻。時刻型を先に決める |
| ディレクトリ | io.dir.exists / create / list / remove | listは名前か情報modelの配列。再帰削除は別の明示APIにする |
| ディレクトリ巡回 | io.dir.walk / glob | 列挙の順番、リンク循環、途中失敗、結果数上限 |
| 作業ディレクトリ | io.dir.current / change | changeはプロセス全体に影響。通常は明示パスを推奨 |
| 一時領域 | io.file.create_temp / io.dir.create_temp | 推測できる名前の組立てではなくOSの安全な作成機能を使う |
| コンソール追加 | io.error / flush / is_terminal | stderr出力、バッファ、リダイレクト時の挙動 |
| 回復可能I/O | io.file.try_open等とio.Error | EOF・引数違反・OS失敗を区別。結果と詳細エラーを明示して受け取る |
| 共通ストリーム | io.stream.read / write / copy | 読み書き権限、所有・借用、close責任、部分転送 |
| ネットワーク | io.net.connect / listen / accept | タイムアウト、切断、DNS、TLS、回復可能エラー |

簡易APIでも明示closeの思想は維持する。ユーザーがopenしたハンドルはユーザーがcloseし、
path指定の一括APIが内部でopenしたハンドルはライブラリがcloseする。
既存のreadlineのfalseはEOFの意味を保ち、I/O失敗をfalseに混在させない。
seek・read_bytes等はハンドルの現在位置を変えるためreadonly操作にはしない。
lib.size(file)でファイル内容のサイズを取得する仕様にはしない。

## 7. 0.27.0の実装範囲と順序

新段階8を0.27.0として、以下を1つの検証可能な実装セットにする。

1. 標準関数の裸名互換処理を取り除き、using・修飾名で必ず名前解決する。
   print/inputの特殊構文にも解決結果を持たせ、構文の分岐による迂回を防ぐ。
2. libを標準モジュールに登録し、size/lengthの型検査・定数評価・実行時処理を実装する。
   型パラメータの組み込みとして扱い、汎用関数の呼び出しで配列をコピーしない。
3. io.pathの基本9関数とターゲットOSに従うパス処理を追加する。
4. サンプル・文書・既存テストを新using規則へ移行する。
5. VS Codeの標準カタログ、補完、引数ヒント、診断を同じ公開規則へ更新する。
   現状の裸のio.print/io.inputの自動補完特例を削除し、usingがなければ修飾名を候補にする。
6. CLI・バージョン期待値・拡張の表示名を0.27.0へ同期し、VSIXを作成する。
   個別検証の後にmake testを1回成功させ、devへ1回pushし、ZIPを作成して次の指示を待つ。

関数本体のエラー検査・モデルのprivate・readonly・ref・配列の範囲検査・明示closeは維持する。
後続候補はバイナリI/O、回復可能I/O、位置・サイズ操作の順を基本とし、別の実装指示で確定する。

## 8. 検証基準

- using無しのprint/input/sqrt/size/lengthを拒否し、対応する修飾名・選択using・一括using・別名は成功する。
- importだけでは裸名を許可しない。usingのブロック外・別ファイルへの漏れ、重複、衝突、子モジュールを検査。
- 型変換と演算子はusing不要。既存の入力先・パターン入力・プロンプト・出力書式を維持する。
- sizeは全数値型、最小値・最大値、bool、void、UTF-8、空配列、多次元、不規則配列、model、refで確認。
- sizeの配列上限と現在長の区別、管理領域を含めないこと、未対応型の入れ子拒否、合計の範囲検査を確認。
- lengthは整数128bit、符号、ゼロ、小数、負のゼロ、10の累乗の前後、極小・最大有限値、非有限値で確認。
- 文字列lengthは既存Unicode境界規則と一致。結合文字・絵文字・埋め込みNUL、sizeとの違いを確認。
- 引数の副作用が1回だけ起き、配列の深いコピーを避け、定数評価・通常実行で一致することを確認。
- パスはLinuxとWindowsで区切り・ルート・UNC・ドライブ相対・空文字・ドット名・拡張子・NUL・UTF-8を確認。
- クロスコンパイルの定数評価と生成プログラムの実行がターゲットOSの規則で一致することを確認。
- Linux / Windows出力、アセンブリ出力、VS Codeのusing有無での補完・赤線・引数表示を検証する。
