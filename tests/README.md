# テストの実行

リポジトリのルートで実行する。

```bash
make test
```

Linux x86_64、GCC、Make、Bash、Python 3 の標準ライブラリを使用する。
追加の Python パッケージは不要。`make test` は必要なら `qyc` をビルドし、
既存の `test.sh`、行編集、統合プログラムと各機能の専用スイートを実行する。
`test.sh` は実行権限に依存しないよう Bash で起動する。
Python を指定する場合は `make test PYTHON=/path/to/python3` とする。

| コマンド | 主な検証内容 |
| --- | --- |
| `make test-auto` | ローカル auto・const auto、型固定、配列・モデルのコピー、入力、readonly・interface・明示 close の制約 |
| `make test-implicit-declarations` | 型名省略の宣言と既存変数への代入、ブロック・同名宣言、名前衝突、未使用警告・候補名、コピー・所有権 |
| `make test-global-const-inference` | ファイル先頭の推論付き定数、前方・モジュール参照、循環・依存深さ、配列上限・モデル既定値・readonly |
| `make test-return-inference` | 関数・メソッドの戻り値推論、複数 return の統一、前方参照・再帰・深さ、interface 契約・コピー・所有権 |
| `make test-generic-functions` | 引数の型ごとの具体化、値・ref・readonly、演算子・一時変数の独立性、再帰・上限、file・private・警告 |
| `make test-rounding` | Decimal を正解値とした丸め、正負の中間値、桁指定、整数精度・範囲外 |
| `make test-semantics` | 短絡評価、比較連鎖の評価回数、スコープ、再帰、値渡し・参照、診断 |
| `make test-text` | Unicode スカラー値、NUL、char、コピー、文字置換、不正 UTF-8、添字エラー |
| `make test-arrays` | 上限・初期化、伸長・削除、深いコピー、多次元、値渡し・参照、添字の副作用 |
| `make test-modules` | 複数ファイルの import・using・別名、探索ルート、衝突・循環、診断、出力形式 |
| `make test-io` | print / input の互換性、io 修飾名、using・別名、型の文脈、形式指定入力 |
| `make test-for` | 範囲の開閉、評価回数、整数の範囲外、小数の有限性・停滞、bool の拒否 |
| `make test-input-refs` | 全スカラー型の直接入力・要素参照、評価順、参照中の変更、引数なしの add、GC・出力形式 |
| `make test-recoverable-io` | ErrorCode / Error / try_open、try/case/finally/throw、伝播・close・OS失敗注入・部分書き込み |
| `make test-file-position` | 論理位置、seek、先読み、4GB超のサイズ、更新日時、OS失敗、readonly |
| `make test-time` | 時間値・時計・待機・単位・日時36メソッド、注入時計のStopwatch3メソッド |
| `make test-file` | ファイルの各モード、UTF-8・改行・EOF、共有ハンドル、書式、明示 close の関数間解析、OS失敗 |
| `make test-lineedit` | 擬似端末でのカーソル移動・編集、Unicode、EOF・割り込み、端末設定の復元 |
| `bash test.sh` | 既存チェック、行編集、`all_features.qy`、CLI の出力指定・アセンブリ出力 |

専用スイートは `python3 tests/arrays_test.py /absolute/path/to/qyc` のように、
検証するコンパイラを指定して直接実行できる。省略時はこのリポジトリの `qyc` を使う。
`support.py` は各テストの一時ディレクトリにソースと実行ファイルを作成し、終了時に削除する。
コンパイラと生成プログラムの実行にはタイムアウトを設けている。
`test.sh` はルートの `tmp*` を使うため、同じチェックアウトで複数同時に実行しない。

コンパイル成功だけでは合格にしない。標準出力・終了コードを比較し、
エラーケースではコンパイル時の終了コード 1、または想定した実行時終了コードと診断を確認する。
丸めはコンパイル時定数と実行時変数をそれぞれ独立した期待値で検証する。
行編集は入力プロンプトと raw モードを確認してからキーを送り、固定時間の sleep に依存しない。

## 再作成の経緯と検証範囲

初期コミット `007ee5f` では Makefile と test.sh が参照する 6 ファイルが未収録で、
完全な Git 履歴にも復元元がなかった。そのため以下を新規作成した。

- `rounding_test.py`、`semantics_test.py`、`text_test.py`、`arrays_test.py`
- `lineedit_test.py`、`all_features.qy`

初回確認では `test.sh` の 281 チェック（行編集 18、統合プログラムの成功確認 1 を含む）と、
専用スイートの 50 テストメソッド（6 / 14 / 14 / 16）が成功した。
丸めのメソッド内には 255 組の値・演算・桁数の検証があり、それぞれ定数評価と実行時評価を確認する。
統合プログラム内では 26 項目を自己検証する。これらの件数は集計単位が異なるため、単純に足して扱わない。

モジュール実装後はさらに 68 テストメソッドを追加した。
当時は既存 50 と合わせて専用スイートは 118 メソッドだった。
モジュールのテストは一時ディレクトリにプロジェクト一式を生成するため、
探索ルートや依存ファイルの状態をテスト間で共有しない。
アセンブリ出力の検証にはビルドに使用する GCC のアセンブラを使う。

標準モジュール io の整備ではモジュールの検証を 78 メソッドへ増やし、
入出力専用の 33 メソッドを追加した。専用スイートは合計 161 メソッド。
行編集も従来形・io 修飾名・別名の各 18 メソッドを実行するため 54 メソッドとなり、
`test.sh` のチェック数は 317 になる。using の有無・表記変更で入出力の期待値は変えない。

0.25.1 では範囲 for の 32 メソッドを追加し、専用スイートは合計 193 メソッド。
整数の範囲と増減幅は Python の range、小数の正確な 0.25 刻みは独立した期待値と比較する。
浮動小数点の丸め、NaN・無限大、停滞、整数最小値を含む増分、境界式の副作用も確認する。
Linux のアセンブリ出力をアセンブルし、Windows は PE ヘッダーと出力形式を確認する。

直接入力・配列要素の ref 渡し・引数なしの add() に38メソッドを追加し、
専用スイートは合計231メソッド。メソッド内では各要素型や変更操作をそれぞれ検証する。
配列全体の引数宣言は従来の `ref int matrix[][]` を維持する。
変数を表示用プロンプトに使うテストは `input(prompt=message)` に移行する。

0.25.2 では io.file と明示 close の検査に52メソッドを追加し、専用スイートは合計283メソッド。
ファイル内容の実バイト、バッファ境界、部分書き込み、割り込みと close 失敗も検証する。
Linux の fault injection には strace があれば利用し、なければ該当ケースだけ skip する。
Windows 用のファイルテストは、Wine の初期設定を済ませてから次のように実行できる。

```bash
QYNO_FILE_TARGET=windows QYNO_FILE_WINE=/absolute/path/to/wine64 python3 tests/file_test.py ./qyc
```

必要な個別検証の後、最終の全体テストは `make test` を 1 回だけ実行する。
ZIP 展開後の再ビルド・再テストは行わない。

Python の失敗・例外・タイムアウトはコマンドの失敗として返される。
シェルの出力比較でも生成プログラムの終了コードを検証し、
コンパイルエラーの検証ではクラッシュを正常なエラーとして数えない。

全体テストは Linux x86_64 で実行する。Windows 向けのファイル専用スイートは Wine でも実行し、
実際に書いたバイト列と戻り値・終了コードを確認する。Windows 実機での実行は別途必要。
行編集の専用テストは Linux 以外では明示的に skip する。

## 0.26.0 のモデル検証

`make test-model` はモデルの専用スイート（37メソッド）を実行する。
名前付き初期化子・既定値・書いた順の評価、コピー・ref・戻り値、
メソッド・一時値・配列・入れ子・モジュールの型解決を検証する。
フィールドと配列要素の参照中に所有元を変更する実行時エラー、
モデル内の共有 file の close 責任、比較、UTF-8入出力、Windows PE と GCC でのアセンブルも確認する。
従来の専用スイート283メソッドと合わせて320メソッドとなる。
モデルのサンプルは `examples/models/main.qy`。全体テストは `make test` を1回実行する。

## 0.26.1 の公開制御検証

`make test-visibility` は公開制御の専用スイート（28メソッド）を実行する。
同型内部・別モデル・モジュール別名、読み書き・ref・入力・初期化・既定値の再利用、
自動表示・コピー・比較、private file の close 責任と Windows / アセンブリ出力を検証する。
`make test` にも含む。実行例は `examples/models/visibility.qy`。

## 0.26.2 の const / readonly 検証

`make test-readonly` は専用スイート（32メソッド）を実行する。
定数モデルの評価・前方参照・配列上限、読み取り専用メソッドと ref 引数、
入れ子・コピー・別名・GC下の実体化、file の共有と明示 close、モジュール、
Windows / アセンブリ出力を検証する。`make test` にも含む。
実行例は `examples/models/readonly.qy`。

## 0.26.3 のカスタム初期化検証

`make test-initialization` は専用スイート（39メソッド）を実行する。
名前付き引数と既定値の評価順序、値のスナップショット、コピーとの分離、
必須引数・未完了フィールド・分岐・配列、生成循環・拒否時の診断、file の所有責任・別名を検証する。
モジュール・GC多発時の生成・再帰・Windows / アセンブリ出力も対象。`make test` に含む。
実行例は `examples/models/initialization.qy`。

## 0.26.4 のインターフェース検証

`make test-interfaces` は interface / implements、コンパイル時の特殊化と参照権限を検証する。
40件で複数モデル・再帰・型と配列制約・別モジュール・未使用の契約違反・未完了の this・file の close 責任を確認する。
特殊化4096件の成功・再利用と4097件の診断も検証する。Windows 出力とアセンブリ出力を含む。
全体テストにも含める。実行例は [examples/models/interfaces.qy](../examples/models/interfaces.qy)。

## 0.26.5 のビット演算・整数基数の検証

`make test-bitwise` の19件と `make test-integer-bases` の33件を全体テストに含める。
bool/intの共有演算子、短絡と両辺評価、64ビットのシフト・不正量、4基数の境界、相互変換、
print/end=/file.writeの書式、inputの自動/固定基数・失敗時の全体保持、参照・GC・interfaceを検証する。
ビット演算はPythonの任意精度整数を使う64ビット参照計算と照合する。
旧来の「intへstrは渡せない」テストは、整数文字列の変換が成功する検証へ更新している。
実行例は [bitwise.qy](../examples/numbers/bitwise.qy) と [bases.qy](../examples/numbers/bases.qy)。

## 0.26.6 のローカル型推論

`make test-auto` は26メソッドで、auto / const auto の型確定と型固定、
既存のスカラー型・多次元配列・モデル・関数戻り値・input の推論を確認する。
コピーの独立性、初期値の評価回数、init 内と interface 特殊化内の宣言も実行して検証する。
空配列・型文脈のないモデル初期化子・自己参照・未導入の構文はコンパイルエラーとし、
readonly / interface の所有制限と、file・file 配列の明示 close の検査を維持する。

## 0.26.7 の型名省略とスコープ

`make test-implicit-declarations` の33メソッドで、型名を省略した宣言・既存変数への代入、
初期値の評価回数、型固定、ブロックとパラメータの同名宣言、型付き for の同名カウンターを検証する。
名前空間・定数・関数・型との衝突、コピー・ref の寿命・readonly・file の close 責任も確認する。
未使用警告ではコンパイル成功と副作用の保持、似た名前の候補、ソース位置、具体化時の重複防止を検証する。
旧テストの「未宣言の単純代入」「内側の明示的な同名宣言」は、新仕様の成功例へ更新し、
未宣言の複合代入と同一スコープでの二重宣言は引き続きエラーとして検証する。

## 0.26.8 の推論付き定数

`make test-global-const-inference` は32メソッドで、ファイル先頭のconst autoとconstの型名省略、
従来の型付き定数と混在した前方参照、import / using / 別名とdiamond依存を検証する。
配列・モデルの定数、readonlyの借用、コピーの独立性、配列上限やモデル既定値からの早期参照も確認する。
自己・相互参照とモデル既定値を含む循環、深すぎる依存、実行時操作・変更操作・名前衝突を診断し、
依存モジュールのエラーではそのファイルの位置を検証する。未導入の関数の型推論は含めない。

## 0.26.9 の戻り値型推論

`make test-return-inference` は34メソッドで、autoと型名省略の関数・メソッド、
複数returnの型統一と順序独立性、void・配列・モデル・file、前方・モジュール参照を確認する。
型注釈で解決できる相互再帰、推論の循環・依存深さ、全経路のreturn、interface契約との一致も検証する。
呼び出し側の評価順、一時変数の所属、initの評価回数、戻り値のコピーとclose責任、
readonly・private・interfaceの所有制限を維持し、未導入の引数の型推論はエラーにする。

## 0.26.10 の引数型推論（新段階1、旧段階5）

`make test-generic-functions` は36メソッドで、型省略と明示auto、固定戻り値の制約、
複数型での演算子変換・ローカル定数・入力用一時変数の独立性、値渡し・ref・readonlyを検証する。
モデルの定義位置のprivate権限、interfaceとの組み合わせ、import / using、再帰、具体化上限4096件、
fileの共有とclose責任、未使用テンプレートの扱いと警告の重複防止も確認する。
残りの実装計画は新6段階へ再編し、今回の実装範囲は新段階1のみ。

## 0.26.11 の整数型（新段階2、旧段階6・7）

`make test-integer-types` / `tests/integer_types_test.py` の30メソッドを全体テストに含む。
8/16/32/64bitのsigned/unsignedの上下限と暗黙・明示変換、Pythonを参照した演算、
算術・論理シフト、符号なし除算・丸め・基数変換、二重丸めを避けるfloat変換、
配列・モデル・定数・ref・readonly・import・interface・ジェネリック・return推論を検証する。
入力失敗時の一括保持、ファイル出力、for境界と開区間、NaN/無限大/0除算の診断も含む。
`integer_bases_test.py` は新たに許可したunsignedの10進リテラルと従来の不正表記を区別する。

## 0.26.12 の既定整数型（新段階3、旧段階8）

`make test-default-integer` / `tests/default_integer_test.py` の28メソッドを全体テストに含む。
int / int_32、unsigned int / unsigned int_32のref一致とint_64との区別、リテラルの幅の推論、
符号付き最小値、32bit演算・シフト・除算・変換・丸めの境界を確認する。
配列・ジェネリック・複数returnの幅、入力の一括保持、length / writeのint_64戻り値、
開始値からのforカウンター型の決定と明示した幅も検証する。
既存の64bitビット演算・基数変換・入力・丸め・forの検証はint_64を明示して維持する。

## 0.26.13 の複素数（新段階4、旧段階9・10）

`make test-complex` / `tests/complex_test.py` の36メソッドを全体テストに含む。
3種類の構築構文・名前付き引数の評価順・iの識別子との区別、成分精度、
Python complexを参照した四則演算、定数と実行時の一致、主平方根・絶対値、
0除算・整数乗の定義域、NaN・inf・符号付き0を確認する。
配列・モデル・値コピー・ref / readonly・interface・ジェネリック・import、
入力の前置／後置i・指数・範囲・一括保持、file.writeとGCも検証する。

## 0.26.14 の広い数値型（新段階5、旧段階11・12・13）

`make test-wide-types` / `tests/wide_types_test.py` の38メソッドを全体テストに含む。
128bit境界・折り返し・符号・基数・シフト・変換、halfとquadrupleの精度と直接の10進解析、
有理数による独立したbinary128丸めの参照結果、四則演算・平方根・極端な桁丸めを検証する。
配列・モデル・定数・ref / readonly / addr・ジェネリック・interface・import・for・GC・ファイル・
入力失敗時の全入力先の保持を確認する。64bit上限を前提としていた旧テストは128bit境界へ拡大した。
`1.5e3`は今回から有効な指数表記なので、旧字句エラーテストは`1.5e3x`へ移行した。
新しい予約語halfを関数名に使っていた既存テストはhalveへ変更し、引数の型変換の検証を維持した。

## 0.26.15 の書記素クラスタとvoid（新段階6）

`make test-grapheme-unit` の20メソッドを追加。Unicode 17の公式766ケースを定数と実行時で検証する。
結合文字・ZWJ絵文字・国旗・インド系文字・ハングル・CRLF、入力の原子性、Unitの型・配列・ref・
auto・ジェネリック・モデル・interface・定数・file.write・明示close・GCを検証する。
端末編集にも結合文字のBackspace、ZWJ列のDelete、国旗の左右移動を追加した。
以前のスカラー値単位と戻り値なしvoidに依存していた期待値は、新しい仕様の検証へ移行している。

## 0.26.16 のモジュール公開制御（追加の新段階7）

`make test-module-visibility` の22メソッドを追加した。モジュール内のprivate参照、外部の修飾・別名・
選択usingの拒否、using-allの非公開名除外と衝突、公開APIのprivate型漏れ、推論・ジェネリック・
配列・モデル・interface・mainを検証する。以前はトップレベルで拒否していたpublic / privateの
テストは、新しい成功ケースへ移行し、修飾子の重複・無効な配置・予約語の検査を維持する。

## 標準ライブラリ（0.27.0）

`make test-standard-library` はusing必須化、lib.size/length、io.pathの基本9関数を検証する。
整数128bit・全小数型・Unicode書記素・多次元/不規則配列・model・定数評価・副作用・不正引数を含む。
全体テストにも含む。Windows向け実行時は `QYNO_TEST_WINDOWS=1` を設定して同じケースを確認する。
既存ケースの裸のprint/inputは修飾名、using、別名による記述へ移行した。

## バイナリファイル（0.27.1）

`make test-binary-file` は37メソッドで、全256値・分割転送・巨大な上限/空ファイル・共有位置・返却配列の独立性、
モード表・型/次元/readonly・close・評価順・Unicodeパス・OSの短い転送/中断/失敗・確保失敗を検証する。
Windowsは `QYNO_FILE_TARGET=windows QYNO_FILE_WINE=/path/to/wine python3 tests/binary_file_test.py ./qyc`。
Linux専用の失敗注入・資源制限の8件はWindowsではスキップする。

## 独立enum・match（0.27.1追加改訂）

`make test-enums` は45メソッドで、連番・飛び番・負値・境界・重複・0欠落、期待型による名前解決、
名義型・明示変換・表示・入力の原子性・lib・配列・model・const・ref・readonly・型推論・公開制御を検証する。
matchの網羅性・重複・評価回数・スコープ・return・必須フィールド初期化・ループ制御・closeも含む。

0.27.2の回復可能I/Oは`tests/recoverable_io_test.py`で検証する。WindowsはWineの初期化後、
`QYNO_FILE_TARGET=windows QYNO_FILE_WINDOWS_RUNNER=/absolute/path/to/runner python3 tests/recoverable_io_test.py ./qyc`
で実行できる。Linux固有のsyscall注入・資源上限のケースはWindowsではskipする。

回復可能I/O専用スイートは53メソッド。アセンブリの実行一致・モジュール境界も検証する。
0.27.2の最終make testはshellチェック326件とPythonテストメソッド1,074件が成功した。

0.27.3ではfile位置23、time36、Stopwatch3の計62メソッドを追加する。
Windowsは`QYNO_FILE_TARGET=windows QYNO_FILE_WINDOWS_RUNNER=/absolute/path/to/runner`を指定して
各スイートを実行できる。syscall注入等のLinux専用8件はskipする。
Stopwatchのスイートは通常コンパイラと同じソースからテスト用時計付きコンパイラを一時作成する。

0.27.3の最終`make test`はshellチェック326件とPythonテストメソッド1,136件がすべて成功した。
VS Code単体テスト129件、VS Code 1.85.2の実機チェック170項目も成功した。
新規62メソッドのWindows生成プログラムによる検証では54件が成功し、Linux専用8件をskipした。
WindowsのAPI失敗は生成PEへの呼び出し結果注入でも確認し、LinuxのerrnoとWindowsのエラー番号を区別する。


## 作業ディレクトリ（0.27.4）

`make test-directory`は28メソッド。相対・絶対・Unicode・128バイトを超えるパス、
currentの独立したstr、absoluteの存在しない対象・空文字・..保持、失敗時の作業位置維持、
fileハンドルと相対open、finally、モジュール経由の伝播とclose解析を検証する。
Linux専用4件はsyscall中断・失敗・到達不能なcwd、リンク、無効UTF-8、権限を扱う。
Windows専用3件はAPI結果注入により、絶対パスがcwdを取得しないこと・エラー分類・厳密なUnicode変換を確認する。
Windowsでは`QYNO_FILE_TARGET=windows QYNO_FILE_WINDOWS_RUNNER=/absolute/path/to/runner`を指定する。

0.27.4の最終make testはshellチェック326件、Python 1,164メソッド（Windows専用3件skip）で失敗0。
新規28メソッドはLinuxで25件成功・3件skip、Windows生成プログラムで24件成功・4件skip。
UBSan・GC頻発設定でもLinuxの同じ28メソッドが通過した。
VS Code単体130件、VS Code 1.85.2実機176項目が成功し、VSIXを作成した。


## 実行時引数・voidマーカー（0.27.5）

`make test-arguments`は24メソッド。通常・空・Unicode・長い/多数の引数、
実行ファイル名の除外、配列の変更・コピー・ref、繰り返し確保、戻り値、-Sを確認する。
名前付きvoid値、メソッドのthis、interface契約、init、無効なmainシグネチャも扱う。
Windows分割の純粋関数は固定ケースとPythonのWindows引用符処理による120ケースで照合する。
出力バッファの境界と不対サロゲートも検査する。
Linux専用1件は無効UTF-8とno-args形式による未使用引数の無視。
Windows専用3件はno-argsでのGetCommandLineW未使用、実際のmsvcrt.dllとの比較、
生成PEへ無効UTF-16を注入したmain前の診断を検証する。
Windowsは既存の`QYNO_FILE_TARGET=windows QYNO_FILE_WINDOWS_RUNNER=/absolute/path/to/runner`を使用する。

0.27.5の最終make testはshellチェック326件、Python1,188メソッド（Windows専用6件skip）で失敗0。
新規24メソッドはLinuxで21件成功・3件skip、Windowsで23件成功・1件skip。
UBSan・GC頻発設定でも両ターゲットの検証が通過した。VS Code単体132件・実機182項目が成功した。
