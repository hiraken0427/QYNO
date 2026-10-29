# 標準モジュールtime（新段階12、0.27.3）

状態: 0.27.3で実装済み。
[ファイルの位置・サイズ・更新日時](file-position-design.md)とまとめて設計する。
この文書で「初版」とした機能を新段階12で実装し、後続機能とは区別する。

## 1. 名前空間

timeをio / math / libと同じimport不要の標準モジュールにする。

| 記述 | 扱い |
| --- | --- |
| time.monotonic() | import・using不要 |
| using timeの後のmonotonic() | 可 |
| using time.monotonicの後のmonotonic() | 可 |
| using time as clockの後のclock.monotonic() | 可 |
| import timeだけでmonotonic() | 不可。importは短い名前を導入しない |
| using timeの後のStopwatch / DateTime / Weekday | 標準の型名として利用可 |

timeを予約語にはせず、他の標準名前空間と同じ衝突検査で保護する。
time関数の呼び出し構文、引数の評価順、モデルの値コピー、メソッドの括弧は既存の規則を使う。

## 2. 秒数と型

公開する経過時間・タイムスタンプは通常のquadruple値とし、専用の数値型や接尾辞を追加しない。
既定の単位をすべて秒に統一し、必要な単位へ明示変換する。

- 戻り値型は固定。代入先がdoubleだからdoubleを返す、といった文脈依存の切り替えは行わない。
- doubleが必要な場合はdouble(value)で明示的に縮小する。
- int系・half・float・doubleの引数は既存のquadrupleへの拡大規則で受ける。bool・複素数は受けない。
- 受け取る秒数は有限値に限定する。負値を許すかは各関数で規定する。
- 小数リテラルはquadruple引数の文脈で直接変換する。既にdoubleにした値の精度は回復しない。
- ファイル位置・バイト数は時間とは異なり、引き続きint_64を使用する。

```qyno
quadruple started = time.monotonic()
time.sleep(time.milliseconds(20))
quadruple elapsed = time.elapsed(started)
double milliseconds = double(time.in_milliseconds(elapsed))
```

doubleは約15〜17桁、quadrupleは約34桁の有効精度を持つ。
2026年付近のUNIX時刻をdoubleの秒数で保存すると、隣接値の間隔は約0.24マイクロ秒になる。
quadrupleは大きな絶対時刻と細かい差の両方を保持しやすい。
OSの整数秒・ナノ秒・カウンターはdoubleを経由せず変換する。
ただし数値の表現精度と、OS・ハードウェアの時計の分解能・正確さは別である。

## 3. 時計・待機

| API | 戻り値 | 契約 |
| --- | --- | --- |
| now() | quadruple | UNIXエポック1970-01-01T00:00:00Zからの秒数 |
| monotonic() | quadruple | 後戻りしない時計の秒数。起点は非公開 |
| cpu() | quadruple | 現在のプロセスが使用したuser+kernelのCPU秒数 |
| elapsed(quadruple start) | quadruple | monotonic()を1回取得してstartを引く |
| sleep(quadruple seconds) | void | 指定した非負の期間だけ待つ |
| sleep_until(quadruple deadline) | void | monotonicの指定時点まで待つ |

nowは日時・記録用。OSの時刻合わせにより前後へ飛び得るため、処理時間の計測にはmonotonicを使う。
monotonicは同一プロセスの比較に使い、日時変換や別プロセスへの永続化には使わない。
サスペンド中の加算はOSの時計の仕様に依存し、初版でOS間の完全統一は保証しない。
elapsedのstartはmonotonic由来を前提とする。数値型だけでは時計の混用を禁止できない。
有限の未来値を渡した場合、elapsedは負の差を返す。値を0へ丸めて誤用を隠さない。
cpuは待機時間を表さない。将来マルチスレッド化した場合、CPU秒数が実経過時間を超えることもある。

sleepは0なら直ちに返り、OSへのyieldを保証しない。
正の値はOSへ変換する際に切り上げ、通常の成功では要求期間より前に戻らない。
スケジューラー等による超過は許容し、正確なタイミングや実時間制御を保証しない。
割り込み時は同じmonotonicの締め切りまで残りを待ち、ビジーループにしない。
sleep_untilの期限が既に過ぎていれば直ちに返る。
OSの相対待機へ分割し、nowを使った時計変更の影響を持ち込まない。

sleepの入力上限はINT64_MAX秒、sleep_untilの残り期間も同じ範囲で検査する。
ナノ秒への一括int_64変換であふれさせず、整数秒と端数へ分けて必要なら複数回待機する。
初版は現在の実行スレッドをブロックする。非同期タスク・キャンセル・コールバックは追加しない。
これらの時計・待機関数はconst初期値に使えず、最適化で除去・まとめて取得しない。

## 4. 単位変換

次の関数はすべてquadrupleを受け取り、quadrupleを返す純粋関数とする。

| 秒へ変換 | 計算 | 秒から変換 |
| --- | --- | --- |
| nanoseconds(x) | x / 1,000,000,000 | in_nanoseconds(seconds) |
| microseconds(x) | x / 1,000,000 | in_microseconds(seconds) |
| milliseconds(x) | x / 1,000 | in_milliseconds(seconds) |
| seconds(x) | x | in_seconds(seconds) |
| minutes(x) | x * 60 | in_minutes(seconds) |
| hours(x) | x * 3,600 | in_hours(seconds) |
| days(x) | x * 86,400 | in_days(seconds) |
| weeks(x) | x * 604,800 | in_weeks(seconds) |

逆変換は右表の名前で計算を逆にする。intへ丸めず、小数部分を保持する。
負の期間も変換できる。入力と結果の非有限値は拒否する。
daysは固定86,400秒であり、夏時間を含む暦上の1日ではない。
月・年を固定秒数へ変換するmonths/yearsは追加しない。

quadrupleの公開定数NANOSECOND / MICROSECOND / MILLISECOND / SECOND / MINUTE / HOUR / DAY / WEEKも用意する。
例えばtime.MILLISECONDSのような別名は設けず、単数の定数と複数の変換関数を区別する。

```qyno
quadruple timeout = 2 * time.SECOND + time.milliseconds(250)
quadruple interval = time.minutes(5)
```

## 5. Stopwatch

標準model time.Stopwatchを追加する。内部フィールドはprivateとし、利用者には秒数で返す。
初期状態は停止・経過0。生成だけではOS時計を取得しない。

| メソッド | 戻り値 | 動作 |
| --- | --- | --- |
| start() | void | 開始・再開。実行中なら何もしない |
| stop() | quadruple | 停止して累積経過秒を返す。停止済みなら現在値を返す |
| readonly elapsed() | quadruple | 停止期間を除いた累積経過秒 |
| lap() | quadruple | 前回lapから増えた経過秒を返し、区切りを更新 |
| reset() | void | 停止し、経過・lapの基準を0へ戻す |
| restart() | void | 経過・lapを0へ戻して開始する |
| readonly is_running() | bool | 実行中か返す |

start/stopを繰り返すと停止中を除いた時間を累積する。
最初のlapは開始からの累積時間、次は前のlapからの増分を返す。
stopはlapの基準を変えない。停止中にlapすると前回以降の残りを返し、その次は0になる。
reset/restartはlapの基準も0にする。
時計の取得に失敗したメソッドは、新しい内部状態を確定せずErrorを伝播する。

モデルの値コピーは独立したストップウォッチになる。
実行中にコピーした場合も、コピー時点で勝手に時計を読み直さず、開始点・累積・lap基準をコピーする。
以降のstart/stop/resetは互いに影響しない。
readonly elapsedは実行中なら時計を読むが、モデルの状態を書き換えない。const評価は不可。
closeやGC finalizerが必要な資源を保持せず、fileのような終了処理は不要。

```qyno
using io
using time

int main();
	Stopwatch watch
	watch.start()
	sleep(milliseconds(20))
	print("lap: {in_milliseconds(watch.lap())} ms")
	sleep(milliseconds(30))
	quadruple total = watch.stop()
	print("total: {format_duration(total)}")
	return 0
```

## 6. 日時とDateTime

UNIX時刻はPOSIXの秒数として扱う。うるう秒を独立したsecond=60として表現しない。
初版のタイムゾーンはUTCと固定オフセットに限定する。
日本標準時の表示はoffset_minutes=540。OS設定・ロケールに依存せず同じ結果を返す。

標準model time.DateTimeは次の公開フィールドを持つ。

| フィールド | 型 | 既定値・範囲 |
| --- | --- | --- |
| year | int | 1970。対応範囲1〜9999 |
| month | int | 1。1〜12 |
| day | int | 1。実在する日 |
| hour | int | 0。0〜23 |
| minute | int | 0。0〜59 |
| second | quadruple | 0.0。0以上60未満。小数秒を含む |
| offset_minutes | int | 0。-840〜840の整数分 |

DateTimeの既定値はUTCのUNIXエポック。
フィールドを直接変更できるため、受け取る各APIで日時全体を検査する。
存在しない日付を勝手に翌月へ繰り越したり、NaN・無限大を受け付けたりしない。
暦は先発グレゴリオ暦。タイムスタンプからの変換では、UTC側と指定オフセットの現地側の両方を
対応年の範囲内に限定する。1970年より前の負のUNIX時刻も扱える。

| API | 戻り値 | 意味 |
| --- | --- | --- |
| to_datetime(quadruple timestamp, int offset_minutes=0) | DateTime | 秒数を日時へ変換 |
| to_timestamp(DateTime date) | quadruple | 日時とオフセットからUNIX時刻へ変換 |
| weekday(DateTime date) | Weekday | オフセット側の日付の曜日 |
| day_of_year(DateTime date) | int | 1〜365/366 |
| is_leap_year(int year) | bool | 指定年がうるう年か |
| days_in_month(int year, int month) | int | 指定月の日数 |

DateTime引数は値を読み取るだけで変更しない。constやreadonlyの値も渡せる。
Weekdayは独立enumでmonday=0、tuesday=1、wednesday=2、thursday=3、friday=4、saturday=5、sunday=6。
week番号・和暦・暦月の加算は初版に含めない。

```qyno
time.DateTime appointment = {year=2026, month=10, day=29, hour=9, minute=30, second=0.125, offset_minutes=540}
quadruple timestamp = time.to_timestamp(appointment)
io.print(time.isoformat(timestamp, 540))
io.print(time.weekday(appointment))
```

## 7. 文字列化・解析

| API | 戻り値 | 意味 |
| --- | --- | --- |
| isoformat(quadruple timestamp, int offset_minutes=0) | str | ISO 8601/RFC 3339の固定形式 |
| format(quadruple timestamp, str pattern, int offset_minutes=0) | str | 下表のパターンで表示 |
| parse(str text) | quadruple | 下記のISO形式をUNIX時刻へ解析 |
| try_parse(str text, ref quadruple destination) | bool | 同じ解析。失敗時はdestination保持 |
| format_duration(quadruple seconds, int digits=3) | str | 経過時間を読みやすく表示 |

isoformatはYYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ、または末尾を+09:00等とする。
小数秒は常に9桁。UTCはZを使用する。
秒数を最も近いナノ秒へ最近接・偶数丸めしてから日付へ分解し、秒・日・年の繰り上がりも反映する。
丸め後に対応年を外れる場合は拒否する。

formatも同じナノ秒への丸めを先に行い、ロケールに依存しない。

| パターン | 表示 |
| --- | --- |
| %Y / %m / %d | 年4桁 / 月2桁 / 日2桁 |
| %H / %M / %S | 時2桁 / 分2桁 / 整数秒2桁 |
| %f | 小数秒の上位6桁（マイクロ秒） |
| %N | 小数秒9桁（ナノ秒） |
| %z / %:z | +0900 / +09:00。UTCでは+0000 / +00:00 |
| %j | 年内の日数3桁 |
| %u | 月曜1〜日曜7 |
| %% | %そのもの |

未知の指定子と末尾だけの%は拒否する。通常のUnicode文字列はそのまま出力する。
%fは9桁へ丸めた後の上位6桁であり、表示だけのためにさらに秒を繰り上げない。

parseは4桁年、T、2桁の時分秒、任意の小数1〜9桁、必須のZまたは±HH:MMだけを受ける。
空白補完・小文字t/z・タイムゾーン省略・秒60・無効日付は受けない。
オフセットは±14:00まで。±14時の場合は分00に限る。
try_parseのfalseは不正な入力形式・対応範囲外を表し、tryのcaseへ移らない。
実行時のparseの同じ失敗は入力データのエラー65とする。const評価中の失敗はコンパイルエラー。
利用者入力にはtry_parseを推奨する。
型違反はコンパイルエラー。解析のメモリ不足等をfalseに隠さない。

数値のquadruple値はナノ秒未満も保持できるため、parse(isoformat(t))は任意のtと完全一致するとは限らない。
ナノ秒へ丸めた時刻としての往復と、解析・再表示による正規化した文字列表現の一致を保証する。
to_datetime/to_timestampは表示のためのナノ秒丸めを行わず、quadrupleの精度で小数秒を保持する。

format_durationの形式は[-][Dd ]HH:MM:SS[.fraction]。
例えば0.125は00:00:00.125、90061.5は1d 01:01:01.500。
digitsは0〜9。最近接・偶数丸めし、負の期間も表示する。丸め結果が0なら負号を出さない。
整数秒部分が符号付きint_64の正の上限以内に収まる期間を対象とする。
端数の丸めで整数秒が上限を超える場合も拒否する。

## 8. 時計領域・演算

通常の数値なのでtime.now() - time.monotonic()も型の上では書けるが、意味のある期間にはならない。
monotonicは計測・期限、UNIX時刻は記録・日時変換に使い分ける。
専用のClockPoint / Duration数値型による混用の静的拒否は今回導入しない。

```qyno
quadruple deadline = time.monotonic() + time.seconds(2)
while(time.monotonic() < deadline);
	io.print("poll")
	time.sleep(time.milliseconds(100))
```

上の例は簡易待機ループであり、イベントループやタイムアウト付きI/Oを追加する仕様ではない。
月・年の加算は固定の秒数で代用しない。

## 9. const・readonly・副作用

- 単位変換、公開単位定数、日時の純粋変換、曜日・月日数、format/parse系はconst評価に対応する。
- OSに依存する時計取得・sleep・Stopwatchの動作はconst評価しない。
- try_parseは出力引数を変更するためconst初期値には使えない。
- Stopwatchのelapsed/is_running以外はreadonly参照から呼べない。
- DateTimeは通常の値モデル。配列・コピー・ref・readonlyは既存の規則に従う。
- 時間値の配列は通常のquadruple values[]、double化した値はdouble values[]で保持する。

## 10. エラーと既存tryへの統合

新しい例外型や異なるcase照合規則は増やさず、OS操作の失敗は既存io.Errorを使用する。
標準io.ErrorCodeへ次の値を末尾追加し、0.27.2の0〜11を変更しない。

| 列挙子 | 値 | 用途 |
| --- | --- | --- |
| unsupported_operation | 12 | OS・対象が要求された時計や操作を提供しない |
| out_of_range | 13 | 外部ファイルのサイズ・OS位置等が公開型の範囲を超える |
| system_error | 14 | 分類できない時計取得・待機等のOS失敗 |

ファイル由来の通常のOS I/O失敗は従来のio_error等を維持する。
timeのError.pathは空文字列、operationはnow/monotonic/sleep等の公開API名。
診断ではtimeの名前空間を表示し、内部で使うOS関数名に置き換えない。
Stopwatchの時計取得失敗は利用者が呼んだStopwatchのメソッド名・ソース位置を保持する。
元のOSコードとメッセージは既存のErrorへ保存し、未処理なら終了コード74。

sleepの負値・非有限値、無効DateTime、無効pattern、呼び出し側が渡した範囲外の日時等は、
定数ならコンパイルエラー、動的ならAPI違反70とする。
不正な解析文字列はparseで65、try_parseでfalse。この分類をcase()へ混在させない。

時計・待機・Stopwatchの時計を読むメソッドにも、関数間の送出効果推論を適用する。
fileを開いた後にtime.sleep等が失敗しても、回復先へ移るまでの明示close責任を失わない。
特にreadonly Stopwatch.elapsedも、実行中はOS取得に失敗し得る点を解析から除外しない。

## 11. 実装構成

| 処理 | Linux | Windows |
| --- | --- | --- |
| now | clock_gettime(CLOCK_REALTIME) | GetSystemTimePreciseAsFileTime等 |
| monotonic | clock_gettime(CLOCK_MONOTONIC) | QueryPerformanceCounter / QueryPerformanceFrequency |
| cpu | clock_gettime(CLOCK_PROCESS_CPUTIME_ID) | GetProcessTimesのuser+kernel |
| sleep | monotonicを基準にしたnanosleep/clock_nanosleep | 相対待機タイマー等＋monotonicで残りを確認 |

APIが利用できない場合はunsupported_operationとし、異なる意味の時計へ黙って置き換えない。
OSが成功を返すだけで戻り値を持たない時計APIも、そのAPIの契約に沿って扱う。
整数カウンターの差を先に求められるStopwatchでは、整数で差を計算してからquadrupleへ変換する。
time elapsedの数値引数は通常のquadruple演算で差を求める。
内部の単位変換ではint_128等を利用して掛け算・エポック差のオーバーフローを防ぐ。
既存の送出効果・close解析はファイル組み込みだけを対象とする部分を見直し、時計や待機を含める。
エラーコード集合の固定ビットマスクも追加コード12〜14を落とさない表現へ共通化する。

日時・単位・文字列処理はOSローカル設定やlibcのstrftime等に依存せず共通の純粋処理へ置く。
既存のwide runtimeに追加し、定数評価とネイティブ実行、Linux/Windows、-Sを同じ規則にそろえる。
日時表示の丸めはmath.roundの四捨五入とは別の仕様として明示し、最近接・偶数で固定する。

## 12. 初版と後続の範囲

初版は時計6関数、単位変換16関数と8定数、Stopwatchの7メソッド、
日時・表示・解析・暦の11関数、DateTimeとWeekday。
以下の構成で実装・検証する。

1. fileの位置・サイズ・更新日時と標準型・エラーコードの追加。
2. timeの3時計、elapsed、待機、単位変換・定数。
3. Stopwatchとエラー経路・readonly・コピーの統合。
4. UTC/固定オフセットの暦・DateTime・表示・解析。
5. サンプル、VS Code、文書、配布物と最終検証。

CLI/VSIXのバージョンは0.27.3。必要な個別検証後に最終make testを1回実行し、
devへの1回のpush・VSIX/ZIP作成を終えたら次の指示を待つ。ZIP展開後の全体テストは行わない。

| 後続機能 | 先に決める必要がある仕様 |
| --- | --- |
| OSローカル時刻・IANA地域名 | タイムゾーンDB、夏時間の欠落時刻・重複時刻、環境依存 |
| add_months / add_years | 月末の丸め方、2月29日、可逆性 |
| 時計の詳細・分解能取得 | カウンターの刻み、OSの公称分解能、観測精度の区別 |
| 定期実行・非同期timer | イベントループ、キャンセル、例外、所有権 |
| I/Oタイムアウト | 待機可能なI/O、途中転送、キャンセル後の状態 |
| スリープ・電源状態をまたぐ統一時計 | OSごとの起動・サスペンド・復帰の契約 |
| r+b・ファイル拡張 | 読み書きの切り替え、バッファ同期、空隙の扱い |

## 13. 検証観点

- usingなしの短い名前を拒否し、修飾・一括/選択using・別名・型名を解決する。
- 返り値のquadruple推論、doubleへの明示変換、int_64ファイル位置と単位の区別。
- 既知の整数tickを直接変換し、doubleを経由して下位桁を失わないことを検証する。
- monotonicの非減少、cpuの非減少、elapsedの1回取得と未来値の負の差。
- sleepは非常に短い期間・0・負値・非有限・割り込み・期限超過を検証する。
  実機の性能値や厳密な上限時間に依存する不安定なテストにしない。
- Stopwatchはテスト用時計を注入して開始/停止/再開/lap/reset/restart、独立コピー、失敗時の未更新を検証する。
- 暦は1900/2000/2100年、月末、負のUNIX時刻、年1/9999、オフセットによる日付変更を検証する。
- 文字列はタイムゾーン必須、無効日付、うるう秒、端数桁、丸めの繰り上がり、未知format、往復を確認する。
- try_parseの失敗時出力保持、OS失敗のcase照合、時間関数を挟むfileのcloseを確認する。
- file.modified_atとtimeの整合、GC頻発、Linux/Windows/-S、VS Codeの標準型・メソッド・引数・診断を検証する。

## 14. ファイル操作と計測を組み合わせる例

以下は0.27.3で実行できる利用例。

```qyno
using io
using io.file as fs
using time

int main();
	Stopwatch watch
	file source
	watch.start()
	try(source = fs.open("measurements.bin", "rb")) as error;
		print("updated: {isoformat(fs.modified_at(source), 540)}")
		if(fs.size(source) >= 16);
			fs.seek(source, -16, fs.SeekOrigin.end)
			unsigned int_8 footer[] = fs.read_bytes(source, 16)
			print(footer)
		print("position: {fs.tell(source)} bytes")
	case(ErrorCode.not_found);
		print("ファイルがありません")
	case();
		print("{error.operation}: {error.message}")
	finally;
		fs.close(source)
	print("elapsed: {in_milliseconds(watch.stop())} ms")
	return 0
```

## 15. 配布と個別検証

- `make test-time`: 時計・単位・日時・解析の36メソッドと、注入時計によるStopwatchの3メソッド。
- `make test-file-position`: バッファ・位置・サイズ・更新日時の23メソッド。
- 定期実行や不安定な性能上限をテストの合格条件にしない。
- `QYNO_TEST_CLOCK_STEP_NS`はテストコンパイラを作るときだけ指定する内部用のビルドマクロで、通常ビルドには含まれない。
- WindowsのnowはGetSystemTimePreciseAsFileTimeを使用するため、生成プログラムはWindows 8以降のAPIを必要とする。
- VS Code: time / DateTime / Stopwatch / Weekday、単位定数、SeekOrigin、位置関数の補完と診断。
- VSIX: vscode-extensions/0.27.3_VSCode_option.vsix。

最終検証: `make test`で既存shellチェック326件・Pythonテスト1,136件が成功。
新規62メソッドはWindows生成プログラムでも54件成功・Linux専用8件skip。
VS Code単体129件・実機170項目が成功した。
