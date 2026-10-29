# 回復可能I/Oとtry制御文（0.27.2）

新段階11として0.27.2で実装。
[ファイル仕様](io-file-design.md)に対する追加仕様である。
0.27.3では[timeとOS操作のエラー統合](time-design.md#10-エラーと既存tryへの統合)を追加した。

## 1. 基本方針

- `io.ErrorCode` は移植可能な失敗分類、`io.Error` は発生時点の詳細情報を表す。
- `io.file.try_open` は、予想している失敗をboolと出力引数で扱う。
- `try` は通常のファイルAPIから伝播したエラーを処理する制御文とする。
- 失敗時の後処理も明示する。`finally`を用意し、回復経路にcloseの抜けを許可しない。
- tryはトランザクションではない。既に完了した代入・出力・ファイル変更を巻き戻さない。

初版はファイルI/Oの回復に限定する。任意の値を投げる汎用例外、ユーザー定義例外型、
`throws`宣言、汎用Result型はこの段階に含めない。

## 2. io.ErrorCode

import不要の標準enum。名前空間の省略には既存と同じusing規則を使う。
`using io`があれば`ErrorCode.not_found`、なければ`io.ErrorCode.not_found`と書く。
列挙子を使うだけでは新しいusing規則を導入しない。

| 列挙子 | 値 | 意味・代表例 |
| --- | --- | --- |
| no_error | 0 | 成功・初期状態。送出しない |
| not_found | 1 | ファイルや親ディレクトリがない |
| permission_denied | 2 | アクセス権限がない |
| already_exists | 3 | 既存対象との衝突。現在のw/aは上書き・追記なので、この理由では失敗しない |
| invalid_path | 4 | OSが拒否するパス。長さ超過、途中の要素がディレクトリでない、リンク循環など |
| not_regular_file | 5 | 対象が通常ファイルではない |
| too_many_open_files | 6 | プロセスまたはシステムのハンドル上限 |
| no_space | 7 | 容量・ディスク割当量の不足 |
| read_only | 8 | 読み取り専用の媒体・ファイルシステム |
| broken_pipe | 9 | OSが通知したパイプの切断。通常ファイルだけの初版では原則発生しない |
| resource_busy | 10 | 共有・ロック等による利用中の競合 |
| io_error | 11 | 上記に分類できないI/O失敗 |

値は公開仕様として固定し、追加コードは末尾へ追加する。
OSコードとは別物であり、既存のプロセス終了コード74とも区別する。
LinuxのerrnoとWindowsのGetLastErrorをこの分類へ変換し、元の値はErrorに残す。
OSが別の原因を同じエラーとして返す場合、両OSで完全に同じ分類になることまでは保証しない。
割り込みによる再試行可能な短い失敗は既存の再試行規則を維持し、直ちに利用者へ送出しない。

## 3. io.Error

標準の公開modelとして、次のフィールドを用意する。

| フィールド | 型 | 意味 |
| --- | --- | --- |
| code | io.ErrorCode | 移植可能な分類 |
| message | str | 人が読む説明。分岐条件に使うことは推奨しない |
| operation | str | `open`、`readline`、`write_bytes`、`close`等の公開API名 |
| path | str | 対象パス。分からない場合は空文字列 |
| os_code | int_64 | 元のOSエラー。OS呼び出し以外で検出した場合は0 |
| source | str | 失敗したAPI呼び出しのソースファイル |
| line | int | ソースの行番号。1始まり、不明なら0 |
| column | int | ソースの列番号。1始まり、不明なら0 |

初期状態と成功時のリセット値はcode=no_error、文字列は空、整数は0。
詳細情報はエラー発生時点のスナップショットであり、次のI/OやOS呼び出しで変化しない。
operationは内部のバッファ補充関数等の名前に置き換えない。
モデルのコピー・引数・ref・配列には既存のmodel規則を適用する。
Errorの値を作成・変更しても、それだけでエラーを送出することはない。

## 4. io.file.try_open

説明用シグネチャ:

```text
bool io.file.try_open(str path, ref file destination, ref io.Error error, str mode = "r")
```

引数は左から右へ1回ずつ評価する。モードは既存と同じr/w/a/rb/wb/ab。

| 結果 | 戻り値 | destination | error | close責任 |
| --- | --- | --- | --- | --- |
| 成功 | true | 新しく開いたハンドル | 全フィールドを成功状態へリセット | 新たに発生 |
| 回復可能な失敗 | false | 呼び出し前の未オープン・閉鎖状態を保持 | 失敗の詳細へ更新 | 発生しない |

destinationは未オープンまたは閉鎖済みでなければならない。
既存の開いたハンドルを黙って閉じたり、上書きしたりしない。
開いたdestination、不正な動的モード、NUL入りパス等のAPI違反は従来どおり実行時エラー70。
型違反や定数の不正モードはコンパイルエラー。
try_openのfalseは後述の送出を発生させず、try内で呼んでもcaseへ移らない。
ただし引数を計算する別の関数が送出したエラーまでfalseへ変換するわけではない。

```qyno
using io

int main();
	file source
	Error error
	if(file.try_open("settings.txt", ref source, ref error));
		file.close(source)
	else;
		print("{error.code}: {error.message}")
	return 0
```

成功・失敗とハンドル状態をclose解析で関連付ける。
直接のif条件、未変更のbool変数、notによる反転、対応関係を保持できる関数の戻り値を扱う。
複雑な別名やboolの上書きで証明できない場合は安全側に拒否する。
結果を使わなくても、後で全経路のcloseが確認できれば許可する。
未オープンへのcloseは既存どおり何もしないので、成功・失敗に共通のcloseも書ける。

falseは「変更をすべて巻き戻した」の意味ではない。
例えばwモードは、OSによる作成・切り詰めの後に後続確認が失敗する可能性がある。
その場合、途中取得したOSハンドルを内部で回収してfalseを返すが、ディスク上の変更は残り得る。

## 5. try / caseの構文

```qyno
try(処理式) as error;
	成功後の処理
case(io.ErrorCode.not_found);
	処理
case(io.ErrorCode.permission_denied, io.ErrorCode.read_only);
	処理
case();
	その他の回復可能エラーの処理
finally;
	必ず実行する後処理
```

try/case/finallyは同じインデントに置く。各本体は1段深くする。
tryは次の4形式を許可する。

```text
try(式);
try(式) as error;
try;
try as error;
```

- ヘッダー式の評価前にハンドラーを有効にし、式を1回評価してから本体へ進む。
- 式は関数呼び出しや代入等。初版では変数宣言をヘッダーに書かず、外側で宣言する。
- 式の結果は捨てる。falseはエラーではなく、通常どおり本体へ進む。
- `try;`はヘッダーなしで本体を保護する。
- caseは0個以上。caseが0個ならfinallyが必須で、tryだけの無意味な構文は拒否する。
- finallyは任意、最後に1個まで置く。

### caseの照合

`case(コード);`はio.ErrorCodeのコンパイル時定数に照合する。
複数コードをカンマで列挙できる。既存enumと同じ期待型による名前解決を行うが、
名前衝突を避けるため`io.ErrorCode.not_found`等の修飾を推奨する。
プロセスの終了コードやos_codeとは照合しない。

caseの重複、no_error、型の異なるコード、非定数、複数のcase()、
case()より後のcaseはコンパイルエラー。
`case();`は残るすべての回復可能エラーを受ける既定ハンドラーで、最後のcaseに置く。
通常成功したときはどのcaseも実行しない。
実行するcaseは1つだけで、フォールスルーはない。

既存のmatchとは別構文とする。
matchの`case value;`は値の分岐、tryの`case(value);`はエラー経路を扱う。
matchと異なり、tryに全コードの網羅は要求しない。該当caseがなければ外側へ伝播する。

### エラー情報の名前

`as error`は任意の識別子を束縛する。固定名の暗黙変数は追加しない。
その識別子は各case本体でのみ参照できるreadonlyなio.Errorとする。
try本体・ヘッダー式・finallyからは参照できない。
必要なら外側で宣言したErrorへコピーして保持する。
各本体のローカル変数は別スコープであり、try本体で宣言した変数をcase/finallyから使わない。

## 6. 送出・伝播・再送出

初版で回復可能とするのは、io.fileのopen/read/readline/write/close/exists/
read_bytes/write_bytesのOS由来の失敗と、通常ファイルではない対象の検出である。
同じ失敗をtry_openで扱う場合と通常openで扱う場合で、ErrorCodeとErrorの生成を共通化する。

| 状況 | 動作 |
| --- | --- |
| 通常のファイルAPIで回復可能エラー | 残りの式・本体を中断し、最も内側から対応するcaseを探す |
| try_openの操作が失敗 | falseとErrorを返す。caseへ移らない |
| readline/read_bytes等のEOF | 既存の正常な終端結果。caseへ移らない |
| 対応するcaseがない | finallyを実行して外側へ伝播 |
| 最外側まで未処理 | 元の発生位置等を診断して従来どおり終了コード74で停止 |
| 構文・型・未宣言・close違反 | コンパイルエラー。caseでは捕捉しない |
| 不正UTF-8等のデータ違反65、API誤用・境界違反・メモリ不足等70 | 従来の実行時エラー。初版のcase対象外 |

初版ではio.print/io.inputの失敗処理は拡張しない。
case()は「捕捉対象となるすべてのファイルI/Oエラー」であり、すべての実行時異常ではない。

関数をまたぐ伝播も行う。利用者がthrowsを書く必要はなく、コンパイラが送出効果を推論する。
失敗した呼び出しの戻り値は使わず、その式の残りも評価しない。
例えば`a = read_number(source)`が送出した場合、aへの代入は完了しない。
ただし、呼び出しの途中で既に行ったref更新や他の副作用は残る。

case本体から出た新しいエラーは、同じtryの他のcaseでは受けず、finally経由で外側へ送る。
同様にfinallyから出たエラーも同じtryでは受けない。
case内の`throw;`は、捕捉した元のErrorを発生位置も含めて再送出する。
caseに属さない場所でのthrowや、`throw 値`は初版では許可しない。
case本体を通常終了すれば、そのエラーは処理済みになる。

## 7. finallyと制御フロー

finallyは次の場合に1回実行する。

1. tryが正常終了したとき。
2. 対応するcaseが終了したとき。
3. 未処理エラー、再送出、case内の新たなエラーを外へ伝播するとき。
4. try/caseからreturn、外側へのbreak/continueを行うとき。

returnは戻り式を評価した後、finallyを通過してから関数を抜ける。
finallyでエラーが出れば保留中のreturn/break/continueは完了しない。
finallyからのreturn、およびfinallyの外へ出るbreak/continueは禁止する。
finally内部のループだけを抜けるbreak/continueは許可する。
型・構文エラー、メモリ不足等の捕捉対象外の異常、プロセス強制終了時の実行は保証しない。

finallyで新たな回復可能エラーが出た場合は、新しいエラーを外側へ伝播する。
保留中だったエラーは置き換わる。未処理で終了する場合の診断には、置き換え前のエラーも表示する。
初版の公開Errorフィールドは現在のエラーのみで、原因チェーンの操作APIは追加しない。

finally内でも、失敗した処理の後の文は実行されない。
複数ハンドルのcloseが必要なら、内側のtry/finallyを使って次のcloseも保証する。
「finallyにcloseを並べたからすべて閉じられる」とは解析しない。

## 8. 明示closeと安全性

### 利用例

```qyno
using io

int main();
	file source
	try(source = file.open("settings.txt", "r")) as error;
		str text = file.read(source)
		print(text)
	case(io.ErrorCode.not_found);
		print("設定ファイルがないため、既定値を使用します")
	case(io.ErrorCode.permission_denied);
		print("設定ファイルを読む権限がありません")
	case();
		print("{error.operation}: {error.message} ({error.os_code})")
	finally;
		file.close(source)
	return 0
```

open失敗ならsourceは未オープンのまま、read失敗ならsourceは開いたままcaseへ進む。
どちらもfinallyのcloseで扱える。close自体の失敗はこのtryのcaseではなく外側へ伝播する。
closeは既存仕様を維持し、OSのcloseが失敗してもハンドルを論理的に閉鎖済みにする。
同じOSハンドル番号が再利用される可能性があるため、closeの再試行は行わない。

### 検査の追加

既存の正常経路のclose検査を維持し、さらに捕捉して実行を再開するエラー経路も検査する。
finallyは隠れた自動closeではなく、利用者が書いた明示closeの共通実行箇所として扱う。
単にtryを書くだけでclose責任を免除しない。

- 同じ関数のcaseへ移る場合、外側で宣言したハンドルは開いた状態で到達し得る。
  case/finally/その後の処理でcloseできればよい。
- スコープから失われるローカル所有ハンドルを開いたまま回復先へ逃がさない。
- 関数境界を越えて捕捉される場合、呼び出し先が取得したローカル所有ハンドルは、
  その関数をエラーで抜ける前に明示closeできることが必要。
- 借用した呼び出し元のハンドルを自動closeしない。呼び出し先で実際にcloseした場合は
  その閉鎖状態をエラー経路の呼び出し元にも反映する。
- 初版では新規ハンドルの所有権移動は正常なreturn/ref出力で行う。
  エラーで関数を抜けながら新規ハンドルのclose責任をref出力へ移す使い方は許可しない。
  ref先が更新済みでも、伝播前に呼び出し先でcloseする必要がある。
- return値としての所有権移動はfinally通過後に確定する。finallyの失敗で戻れなくなる経路も検査する。
- 別名、配列、modelのフィールド、ループの反復、早期returnも対象とし、証明不能なら拒否する。

正常に返すときだけハンドルを渡し、失敗時に閉じる関数は、caseと再送出で書ける。

```qyno
file open_checked(str filename);
	file source
	try(source = io.file.open(filename));
		io.file.read(source)
		return source
	case();
		io.file.close(source)
		throw;
```

この例は読み取り後の位置のまま返す。正常returnでは所有権を呼び出し元へ移し、
失敗時は未オープンまたは取得済みハンドルを閉じてから伝播する。

### 既存プログラムとの関係

try/caseで回復しない従来のプログラムについては、異常終了を正常経路と区別する既存規則を維持する。
すべての既存関数にfinallyの追加を要求する変更にはしない。
一方、従来は異常終了していた関数をtry/caseから呼び、関数をまたいで回復する場合は、
その呼び出し先もエラー経路でcloseを満たす必要がある。
コンパイラは関数のエラー出口に残るローカル資源を要約し、捕捉可能な呼び出し経路で検査する。
再帰・動的ディスパッチ等で確認できなければ、cleanupの追加や関数分割を案内する。
プロセス停止を前提にしたコードを、検査なしに回復可能なコードへ変換しない。

## 9. 途中まで進んだI/O

- read/readline/read_bytesの失敗では、読み取り位置が既に進んでいる可能性がある。
  readlineの出力変数は1行の読み取り成功時にだけ更新し、失敗時は保持する。
- write/write_bytesは一部を書き込んでから失敗する可能性がある。変更は残る。
- openのw/aモードに伴う作成・切り詰めも巻き戻さない。
- read/write失敗後のハンドルは原則開いたまま。成功を保証しないがcloseは必要。
- 同じ操作の無条件な再試行は、重複出力・読み飛ばし等の原因になり得る。

try_read/try_write系は、部分転送量や部分データの契約を確定してから後続段階で追加する。
初版のErrorに「安全に再試行できる」という保証や不確かな転送量は持たせない。

## 10. 実装構成と順序

実装構成は次のとおり。段階11を1セットとして検証・pushする。

1. 標準enum/modelの登録、OSコードの分類、発生位置とErrorの共通生成。
2. try_openと成功条件に連動したclose解析。既存openとの失敗情報の共通化。
3. try/case/as/finally/throwのAST・構文・スコープ・型検査。
4. 式中断、エラー出口、finallyを含むreturn/break/continueの制御フロー。
5. 関数間の送出効果とエラー出口の資源要約、初期化・戻り値・close検査の統合。
6. Linux/Windowsの実行時処理、ネイティブ出力とアセンブリ出力の整合。
7. VS Codeの色分け・補完・定義参照・診断、文書・例・VSIX・ZIP。

### ネイティブコード生成の方針

初版はOS例外やC++ランタイムに依存せず、内部のエラー状態と通常の関数復帰で伝播する。
公開関数シグネチャは変更せず、実行時状態領域に保留中のエラーを保持する。
失敗し得るランタイム補助関数も、各呼び出し後にエラーを確認して自身の復帰処理を通る。
単にエラーフラグを立てて後続処理を続行する実装にはしない。

各フレームは必要に応じて通常継続/return/break/continue/errorの保留状態を保持する。
finallyの実行中は元の保留エラーをフレーム内へ退避し、新しいエラーと区別する。
caseへ入る際には捕捉したErrorを保存し、実行時の保留状態を解除する。

スタックを大域的に戻してcaseへ飛ぶ方式は採用しない。
途中のfinally、明示close、GCの生存参照、エラーの根を飛ばさないためである。
R12/R13のGC状態を過去の値へ戻さず、Error内の文字列・保留中の戻り値・診断履歴もGCの根として保持する。
既存のfile_error_labelから直接終了する経路は、回復対象と従来の致命的エラーに分離する。
エラー情報の確保自体に失敗した場合のメモリ不足は、引き続き捕捉対象外とする。

## 11. 検証観点

- 成功、分類されたOS失敗、未処理エラー74、従来の致命的エラー65/70。
- try_openのtrue/false、古いErrorのリセット、失敗時destination保持、開いた出力先の拒否。
- 引数とtryヘッダーの左から右への1回評価、falseを返す式、式の途中での中断。
- 複数case・複数コード・case()・不一致の伝播・重複・no_error・型違い・matchとの構文区別。
- asのreadonly・スコープ・名前衝突、元の発生位置、関数をまたぐ捕捉、throwの元情報保持。
- try/case/finallyでの新しい失敗、同じtryに再捕捉されないこと、診断履歴。
- 成功/未処理/return/break/continueでのfinallyの実行回数と順序、内部ループの制御。
- open失敗後のclose、read失敗後のclose、close失敗時の論理閉鎖と再試行禁止。
- 複数ファイルのcleanup中の失敗、ネストしたfinally、借用・ref・戻り値・別名・配列・ループ。
- 回復先へ到達する呼び出し先の未閉鎖を拒否し、従来の未捕捉終了プログラムを維持すること。
- 部分読み書き、wモードの副作用、readline出力先の失敗時保持。
- Linux/Windows、GC頻発時のエラー情報・保留戻り値の生存、アセンブリと直接出力の一致。
- VS Codeの構文・標準型/コード補完とコンパイラ診断。

必要な個別検証後、最終make testを1回成功させる。VSIXを作成しdevへ1回pushしたら止まる。
ZIPはコミット済みソースから作成し、展開後の全体テストは繰り返さない。

## 12. 実装上の保守的な判定

- 解析では各ファイル操作の失敗コード集合を広く見積もる。OSによって実際には起きないcaseも検査対象になり得る。
- 初期化メソッドのcaseへ入る際、try本体の途中までのフィールド代入は完了の証明に使わない。
  必須フィールドはcaseでも初期化するか、throwで成功経路から除外する。
- 既存のfile解析の再帰・動的添字・資源数・経路数の上限は維持する。複雑な所有権移動は安全側に拒否する。
- try_openの直接条件・boolへの代入/コピー・not条件・単純なbool戻り値/ref出力の組を追跡する。
  複雑な条件や更新で関係が失われた場合は、未オープンにも安全なcloseを共通経路へ置く。

## 13. 検証結果

- 最終make test: shellチェック326件、Pythonテストメソッド1,074件が成功。
- 回復可能I/O専用: Linux53件、Windows/Wineは53件中48件成功・Linux専用5件skip。
- UBSan付きコンパイラとGC間隔256の設定: 専用53件成功。
- VS Code: 単体125件、VS Code 1.85.2実機161項目が成功。
- VSIX: vscode-extensions/0.27.2_VSCode_option.vsix。
- ZIP展開後の再ビルド・全体テストは実施しない。
