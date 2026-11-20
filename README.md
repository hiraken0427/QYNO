# QYNO 0.29.8

著作権およびライセンスに関する情報は、本ファイルの末尾に記載している。

[概要](#概要)

[ビルド手順](#ビルド手順)

[ソース構成](#ソース構成)

[新機能](#新機能)

[使用方法](#使用方法)

[言語仕様](#言語仕様)

[標準モジュール](#標準モジュール)

[開発状況](#開発状況)

[ライセンス情報](#ライセンス情報)

## 概要

QYNOは、静的型付けのコンパイル言語である。UTF-8で記述したソースコードから、Linux x64およびWindows x64の実行ファイルを生成する。

コンパイラはC言語で実装している。拡張子は.qyである。数式に近い範囲指定や型推論を備え、整数、小数、複素数、配列を扱うことができる。

## ビルド手順

ビルドにはGCCとMakeが必要である。GCCの128bit数値拡張を使用している。既定ではコンパイラ本体に-O2を適用してビルドする。

Linux x64では、ソースコードを取得したディレクトリで次のコマンドを実行する。

```sh
make
```

生成されるコンパイラはbin/qycである。Windowsではbin/qyc.exeとなる。中間生成物はbuildに置く。タスク実行基盤や非同期ファイルのワーカーを変更した場合は、Linux x64でmake async-blobを実行してgenerated内の同梱ヘッダーを更新する。make async-file-blobでも同じヘッダーを更新できる。再生成にはGCC、binutils、Python 3を使用する。ビルド時の生成物は次のコマンドで削除できる。

```sh
make clean
```

## ソース構成

コンパイラの実装はsrcに置いている。coreは起動処理、syntaxは字句解析と構文解析、analysisは型検査、optimizerは最適化、backendはコード生成を担当する。個別機能の型検査と実行時処理はfeatures内にまとめている。

modulesはモジュールの登録と名前解決、i18nは英日表示、utilsは共通処理である。共有ヘッダーはinclude、生成済みのコードとデータはgenerated、Visual Studio Code拡張機能はvscodeに置く。binとbuildはビルド時に作成する。

## 新機能

0.29.8では、配列の条件処理をlibに追加した。filterは条件に合う要素の配列、countは条件に合う要素数をint_64で返す。anyは1つでも条件に合えばtrue、allはすべての要素が条件に合えばtrueとなる。入力配列は変更しない。

```text
using io
bool positive(x)=x>0
int main();
	int values[]=[-2,0,3,1]
	auto selected=lib.filter(values,positive)
	print(selected)
	print(lib.count(values,positive))
	print(lib.any(values,positive))
	print(lib.all(values,positive))
	print(values)
	return 0
```

述語は要素を1つ値渡しで受け取り、boolを返す同期関数値である。要素型から関数型を決めるため、上のpositiveのように引数型を省略した関数も渡せる。多次元配列では最上位の要素を対象とする。

filterは入力順を維持した独立の配列を返す。filterとcountは全要素を評価する。anyは最初のtrue、allは最初のfalseで処理を止める。空配列ではfilterが空配列、countが0、anyがfalse、allがtrueとなり、述語を呼び出さない。

述語のI/Oエラーはtryとcaseへ伝播する。filterの途中結果は返さない。これらは同期処理であり、自動的にはイベントループへ制御を譲らない。型を推論した配列引数の関数型も整備し、非公開のmodelを用いる条件処理にも対応した。

0.29.7では、名前付きの同期関数を値として扱えるようにした。関数名を代入し、変数から呼び出すことができる。引数、戻り値、配列、modelのフィールドにも使用できる。型を明示する場合はfuncを用いる。

```text
using io
int square(int value) = value ^ 2
int cube(int value) = value ^ 3
int apply(int value, func int(int) operation);
	return operation(value)
func int(int) select(bool use_cube);
	if(use_cube);
		return cube
	return square
int main();
	auto operation = select(true)
	print(apply(3, operation))
	operation = square
	print(operation(3))
	return 0
```

func int(int)はintを1つ受け取り、intを返す関数型である。引数名は省略できる。refとref readonly、配列の条件も型に含まれる。関数値の型は宣言後に固定される。型未確定の引数を持つ関数は、期待するfuncの型から特殊化する。

関数型の初期値はnoneである。未設定のまま呼び出すとエラーとなる。型が同じ関数値とnoneには等価比較を使用できる。lib.sizeは関数値1つにつき64を返す。関数値の表示や数値への変換は行えない。

関数値を経由したI/Oエラーにもtryとcaseを使用できる。同期の関数値はasync関数へ渡せるが、呼び出し自体は同期処理となる。async関数やmodelメソッドの取り出し、匿名関数とクロージャは未対応である。組み込み関数を値として使う場合は通常の関数でラップする。関数型にfile、addr、interface、タスク、グループを含めることはできない。funcを予約語に追加した。

0.29.6では、バイナリファイルの非同期読み書きを追加した。read_bytes_asyncはファイル全体をunsigned int_8の配列として読み込む。write_bytes_asyncは上書き、append_bytes_asyncは追記を行い、書き込んだバイト数をint_64で返す。パス指定で開閉まで行うため、利用者によるcloseは不要である。

```text
using io

async int main();
	unsigned int_8 data[] = await io.file.read_bytes_async("input.bin")
	int_64 written = await io.file.write_bytes_async("copy.bin", data)
	int_64 appended = await io.file.append_bytes_async("copy.bin", [0x00, 0xff])
	print("{written + appended} bytes")
	return 0
```

read_bytes_asyncの第2引数は読み取り上限であり、既定値は67108864である。上限を超えた場合はout_of_rangeとなり、途中までの配列は返さない。空ファイルは空配列となる。UTF-8の検査やBOMの除去、改行の変換は行わない。

write_bytes_asyncとappend_bytes_asyncはunsigned int_8の1次元配列を受け取り、呼び出し時に内容をコピーする。空配列でもファイルを開閉する。相対パスの基準、キャンセル、タイムアウト、エラーの扱いは既存の非同期ファイルAPIに従う。書き込み済みの内容は巻き戻さない。既存のfileハンドルを用いるread_bytesとwrite_bytesの動作は維持する。

現行の配列は内部で1要素に8バイトを使用する。読み取り上限はファイル内容のバイト数であり、メモリ使用量の上限ではない。

0.29.5では、非同期の追記と行単位の読み書きを追加した。いずれもasync関数内で呼び出し、awaitで結果を取得する。

```text
async int main();
	str lines[] = await io.file.read_lines_async("input.txt")
	int_64 written = await io.file.write_lines_async("output.txt", lines)
	int_64 appended = await io.file.append_text_async("output.txt", "追記\n")
	io.print("{written + appended} bytes")
	return 0
```

read_lines_asyncはstr配列を返す。第2引数のmax_bytesは既定で67108864、第3引数のmax_linesは1048576である。LF、CR、CRLFを改行として読み取り、末尾の改行による余分な空行は作らない。

write_lines_asyncはパスとstr配列を受け取る。第3引数のnewlineは既定でLF、第4引数のfinal_newlineはtrueである。改行にはLF、CR、CRLFを指定できる。各要素に改行を含めることはできない。配列は呼び出し時にコピーする。append_text_asyncとともに、書き込んだバイト数をint_64で返す。並行した追記のまとまりや順序は保証しない。

qyc --checkを追加した。mainのないモジュールも構文と型を検査でき、実行ファイルやアセンブリは出力しない。通常のコンパイルと-Sでは引き続きmainが必要である。Visual Studio Code拡張機能もこの検査を使用する。編集中のmain未定義は警告せず、構文や型の誤り、未宣言の名前は従来どおり表示する。

```sh
qyc modules/geometry.qy --check
```

0.29.4では、io.file.read_text_asyncとio.file.write_text_asyncを追加した。async関数内で呼び出し、awaitで文字列または書き込みバイト数を取得する。引数とUTF-8の扱いは同期版に合わせる。read_text_asyncのmax_bytesは省略でき、既定は64MiBである。

```text
using io

async int main();
	auto first = io.file.read_text_async("first.txt")
	auto second = io.file.read_text_async("second.txt")
	auto texts = await task.with_timeout(task.all(first, second), 2.0)
	int_64 written = await io.file.write_text_async("merged.txt", texts[0] + texts[1])
	print("{written} bytes")
	return 0
```

ファイル操作は4本の専用ワーカーで行う。実行待ち列は64件とし、超過分は受付待ちのタスクとなる。ワーカーは最初の非同期ファイル要求で作成する。通常の同期処理やタイマーだけのプログラムでは作成しない。

相対パスの基準と書き込む文字列は呼び出し時に確定する。その後のio.dir.changeや変数への代入は開始済みの要求へ影響しない。ファイル内容やメタデータのスナップショットは保証しない。

キャンセルと期限切れでは、実行中のOS処理の復帰とcloseの結果を待つ。OS処理が戻らない場合は期限を超えて待つことがある。書き込み済みの内容は巻き戻さない。原子的な置換やディスクへの永続化は保証しない。同じパスの処理順が必要な場合は、前の処理をawaitしてから次を開始する。

通常のI/O失敗はio.Errorで通知する。task.allと所有スコープでの集約は既存の規則に従う。不正なUTF-8、パス内のNUL、不正な上限値は同期版と同じ契約違反である。fileハンドルは返さず、API内部で開閉する。

0.29.3では、ソースコードを機能ごとのフォルダへ整理した。コンパイラの出力先はbin、拡張機能の配置先はvscodeへ変更した。言語の構文と動作は0.29.2を維持する。

0.29.2では、タスクグループ、キャンセル、複数タスクの待機、タイムアウトを追加した。withを予約語に追加する。既存のasync 型名とautoによる宣言、呼び出した時点で開始する方式は維持する。

```text
using io

async int work(int value);
	await time.sleep_async(0.1)
	return value * 2

async int main();
	try;
		async with task.group();
			auto first = work(3)
			auto second = work(4)
			auto results = await task.with_timeout(task.all(first, second), 1.0)
			print(results)
	case(task.ErrorCode.timed_out);
		print("時間切れ")
	case(task.ErrorCode.group_failed) as error;
		print(error.message)
	return 0
```

async with task.group()は、その中で作成したタスクを管理する。名前が必要ならas groupを付ける。ブロック末尾、return、外側へ出るbreakやcontinueでは、全員の終了と後始末を待つ。エラー時は残りへキャンセルを要求する。内側で作ったタスクを外側の変数や配列へ保存することはできない。

job.cancel()とgroup.cancel()は停止を要求し、最初の有効な要求でtrueを返す。終了の確認にはawaitを用いる。グループへの要求は子を対象とし、本文を実行する親自身をキャンセルしない。自分へのキャンセル要求は、caseで捕捉しても次の確認点で再通知する。

task.allは同じ完了型のタスクをまとめ、入力順の配列を返すタスクを作る。引数を並べる形とタスク配列を渡す形を使用できる。失敗すると残りへキャンセルを要求し、回収後にgroup_failedを通知する。原因はerror.errorsに格納する。同じタスクを複数回指定した場合、結果は各位置へコピーし、原因の収集は1回とする。

task.with_timeoutは秒数を指定し、元の完了型を維持したタスクを返す。期限切れでは対象をキャンセルし、後始末を待ってtimed_outを通知する。後始末の時間も復帰までの時間に含まれる。対象が完了済みなら0秒でもその結果を採用する。期限はAPIを呼び出した時点から数え、引数のタスクが最初の中断まで実行する同期部分は含めない。

子を明示的にawaitした場合は、従来どおり元のio.Errorなどを受け取る。awaitせずに所有スコープの終了へ持ち越した通常の失敗は、1件でもgroup_failedへ集約する。0.29.1で元のI/Oエラーとして終了していた場合、未処理の終了コードは74から70へ変わる。

async mainは引数なし、void指定、str配列の引数に対応する。async voidも待機でき、結果は()である。task.yield()とtime.sleep_async(0)は少なくとも一度イベントループへ制御を譲る。job.doneとjob.stateで状態を確認できる。同じタスクを複数回awaitでき、配列とmodelの結果は取得ごとにコピーする。

タスクの配列にはautoを使用する。配列を返すタスクを明示する場合はasync int job[]と記述する。タスクを式文で捨てられるのは、明示的なグループの本文内に限る。

実行は1本のイベントループで行い、キャンセルはawaitなどの確認点で通知する。同期の計算ループやtime.sleepはイベントループを待たせる。長い計算で制御を譲る場合はawait task.yield()を記述する。case()はキャンセルを捕捉せず、case(task.ErrorCode.cancelled)を明示する。

asyncメソッド、finally内の非同期処理、永続的な参照ローカル変数は未対応である。ref、addr、file、interfaceを別タスクへ渡すことは許可しない。fileはasync関数内のローカルとして使用でき、明示的なcloseが必要である。finallyはキャンセル時も実行し、同期のcloseとcancelを使用できる。

各ユーザータスクは1MiBのスタック領域を使用し、中断中の変数、結果、エラー、グループと待機情報をGCが保持する。all、期限付きタスク、非同期ファイル要求にはユーザー関数用のスタックを追加確保しない。CPU並列処理は今後追加する。

## 使用方法

次のコードをmain.qyとして保存する。インデントにはタブを使用する。

```text
using io

int main();
	print("Hello, World!")
	return 0
```

コンパイルして実行する。

```sh
./bin/qyc main.qy -o main
./main
```

Windows向けの実行ファイルは、出力先を指定して生成する。

```sh
./bin/qyc main.qy --target windows -o main.exe
```

出力先は--targetで指定する。指定しない場合は、コンパイラを実行しているOSに合わせる。-Sはアセンブリの出力、-Iはモジュールの探索先の追加に使用する。--helpで使い方を確認できる。

表示は英語を既定とする。--jpまたは--japaneseを付けると、ヘルプ、診断、完了メッセージを日本語で表示する。--JPなどの大文字も使用できる。実行時エラーの言語はコンパイル時の指定に従う。--versionの英語表示は従来と同じである。

```sh
./bin/qyc main.qy --japanese
./bin/qyc --help --jp
```

エラーにはファイルの絶対パス、行と列、該当するソースと下線を表示する。コンパイラの診断は端末で自動的に着色する。--color=neverまたは環境変数NO_COLORで色を無効にできる。--color=alwaysは出力先によらず着色する。--quietはコンパイル完了の表示を省く。ツールからの利用には、位置情報を1行で出力する--diagnostics=plainを用いる。

Visual Studio Code用の拡張機能は[vscode](vscode/README.md)に置いている。ソースコードと現行バージョンのVSIXを含む。

mainで実行時引数を受け取る場合は、str配列を宣言する。実行ファイル名は引数に含まれない。

```text
int main(str args[]);
	for(0 <= i < args.length, i++);
		io.print(args[i])
	return 0
```

## 言語仕様

関数や制御文の本体は、ヘッダー末尾のセミコロンとインデントで記述する。条件分岐にはif、繰り返しにはforとwhileを使用する。

```text
int main();
	int values[] = [0, 1, 2, 3]
	for(values.length > i >= 0, i--);
		io.print(values[i])
	return 0
```

整数は8bitから128bitまでを扱い、intは32bitである。unsignedで符号なしを指定できる。小数はhalf、float、double、quadrupleに対応する。複素数はcomplex、complex_64、complex_128で扱う。

charはUnicodeの1書記素、strは書記素の列である。boolは真偽値、voidは()だけを値に持つ型である。これらの型は配列でも使用できる。

autoは初期値から型を推論する。未宣言の名前への代入でも型を推論し、宣言後の型は固定される。関数の引数や戻り値の型も省略できる。constは定数、refは参照渡しに使用する。

modelではフィールドとメソッドを定義する。生成時の値は型名を繰り返さず、Point p = {x=3.0, y=4.0}のように記述できる。publicとprivateで公開範囲を指定し、readonlyで変更を制限する。共通の操作はinterfaceで定義できる。

enumはmodelから独立した列挙型である。列挙値による分岐にはmatchを使用する。

importでモジュールを読み込み、usingで名前の省略や別名を指定する。探索先はエントリファイルのディレクトリを起点とし、-Iで追加できる。

```text
import geometry.circle
using geometry.circle as circle
```

and、or、xorは、boolでは論理演算、整数ではビット演算として動作する。シフトには<<と>>を使用する。整数は2進数、8進数、10進数、16進数で記述でき、接頭辞は0b、0o、0d、0xである。接頭辞を省略した場合は10進数となる。

## 標準モジュール

io、math、lib、time、taskはimportなしで使用できる。名前を省略する場合はusingが必要である。using ioを記述した場合はprintとinputを使用でき、記述しない場合はio.printとio.inputを使用する。

ioはコンソールとファイルの入出力を扱う。io.pathはパスの操作、io.dirはディレクトリの取得、移動、作成、一覧取得、削除に使用する。パスはstrで扱う。

ファイルはio.file.openで開き、io.file.closeで明示的に閉じる。closeを確認できない経路はコンパイルエラーとなる。テキストとバイナリの読み書きに対応し、位置、サイズ、更新日時を取得できる。I/Oエラーはtry、case、finallyで処理する。パス指定の一括読み書きでは、ファイルの開閉をライブラリが行う。

io.file.removeは通常ファイルの削除、copyはコピー、moveは移動と名前変更を行う。コピーと移動は既存の出力先を上書きせず、親ディレクトリを作成しない。コピーは一定量ずつ処理するが、失敗時に途中の出力が残る場合がある。別のファイルシステムへの移動はunsupported_operationとなる。

io.file.read_textとread_linesは、UTF-8のファイルを文字列または行の配列として読み込む。既定の上限は64MiBであり、read_linesの行数上限は1048576である。write_textは上書き、append_textは追記、write_linesは行の配列の書き込みを行う。

io.dir.existsは存在確認、createは1階層の作成、listは直下の名前一覧、removeは空のディレクトリの削除を行う。listはUTF-8のバイト順で名前を返す。上限は既定で1048576件、名前全体で64MiBである。ディレクトリへのリンクはexistsとlistで参照できるが、removeでは削除できない。空でない場合はdirectory_not_emptyとなる。

caseにasを付けると、そのcase内でエラーを参照できる。io.ErrorCodeのcaseではio.Error、task.ErrorCodeのcaseではtask.Error、複数の種類を扱うcaseやcase()ではlib.Errorとなる。従来のtryに付けるasも使用できる。

```text
using io

int main();
	try;
		print(file.read_text("sample.txt"))
	case(ErrorCode.not_found) as error;
		print(error.path)
	case() as error;
		print("{error.domain}: {error.code}")
	return 0
```

lib.ErrorCodeは名前空間付きのエラーコードから作成し、整数への変換や入力先への指定は許可しない。task.ErrorCodeにはcancelled、timed_out、group_failed、invalid_stateを用意する。cancelledの捕捉にはcaseでの明示が必要であり、case()には含めない。

libとtaskのError、ErrorCodeは、名前空間を付けるか、対象の型をusingで個別に指定して使用する。using libやusing taskではこれらの型を展開しない。

mathは数学関数を提供する。lib.sizeは値のビットサイズ、lib.lengthは文字列や配列の長さ、数値の整数部分の桁数を返す。

lib.randomは型名と不等式で乱数を生成する。lib.random.intは整数、lib.random.doubleは小数、lib.random.boolは真偽値を返す。シードを指定した生成と、独立した生成器にも対応する。

timeは時刻の取得、経過時間の計測、待機に使用する。Stopwatchによる計測と、日時の変換にも対応する。

io.path.infoは、パスから種別、通常ファイルのサイズ、更新日時を取得する。戻り値はio.path.PathInfoである。

```text
using io.path as path

int main();
	try;
		auto entry = path.info("data.txt")
		io.print(entry.kind)
		if(entry.has_size);
			io.print("{entry.size_bytes} bytes")
		io.print(time.iso_format(entry.modified_at))
	case(io.ErrorCode.not_found);
		io.print("ファイルが見つかりません")
	return 0
```

kindはio.path.PathKindであり、other、regular_file、directory、linkの順に0から3の値を持つ。通常ファイルだけがhas_size=trueとなり、size_bytesにバイト数を返す。空ファイルのサイズは0である。通常ファイル以外ではhas_size=false、size_bytes=0となる。

modified_atはquadrupleのUNIX時刻である。io.file.modified_atと同じ秒単位で扱い、負の時刻も許可する。0は1970年1月1日を表す。

既定では最後のリンク自体を調べる。第2引数をtrueにするとリンク先を調べる。切れたリンクは既定ではlinkとして取得できるが、リンク先を指定した場合はnot_foundとなる。Windowsではシンボリックリンクとジャンクションをlinkとし、分類できないreparse pointはotherとする。

相対パスは実行時の作業ディレクトリを基準とする。ルート、.、..も取得できる。末尾の区切りはルート以外で取り除くため、file/はfileと同じ対象を調べる。途中の階層にあるリンクは通常のパス解決に従う。

結果は取得時点の値である。フィールドを書き換えてもファイルには反映されない。必要な情報を取得できない場合はio.Errorとなり、途中の結果は返さない。内部の開閉はライブラリが行う。

## 開発状況

現在のQYNOは開発中のプロトタイプである。仕様は予告なく変更される場合がある。

動作、互換性および特定の用途への適合性を保証しない。仕様、実装および本ソフトウェアの利用により生じた損害について、法令で認められる範囲で作者は責任を負わない。

開発はdevブランチで行い、確認した変更をmainブランチへ反映する。

## ライセンス情報

QYNO本体はBSD 2-Clause Licenseで公開する。著作権はhiraken0427に帰属する。

以下はQYNO本体のライセンス全文である。

```text
BSD 2-Clause License

Copyright (c) 2026, hiraken0427
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

UnicodeのデータにはUnicode License V3が適用される。著作権表記とライセンス全文は[grapheme_data.h](generated/grapheme_data.h)に収録している。

GCC実行時コードにはGPLv3以降とGCC Runtime Library Exception 3.1が適用される。著作権表記、ライセンス全文および配布元は[wide_blob.h](generated/wide_blob.h)に収録している。
