# ファイル位置・サイズ・更新日時（新段階12、0.27.3）

状態: 0.27.3で実装済み。
[標準time](time-design.md)と同じ実装セットとして追加した。
既存のfile型、共有ハンドル、バイナリI/O、try、明示closeを拡張する。

## 1. API

すべてimport不要のio.fileへ追加する。usingと別名は既存の規則を使用する。
表のシグネチャは説明用であり、関数定義構文の変更ではない。

| API | 戻り値 | 意味 |
| --- | --- | --- |
| tell(file source) | int_64 | 先頭からの論理位置。単位はバイト |
| seek(file source, int_64 offset, io.file.SeekOrigin origin=begin) | int_64 | 移動後の先頭からの位置 |
| size(file source) | int_64 | 現在のファイル内容のバイト数 |
| modified_at(file source) | quadruple | 最終更新日時。UNIX時刻の秒数 |

sizeとmodified_atは呼び出した時点のOSの情報を取得し、open時の値をキャッシュしない。
別々の呼び出しで取得した位置・サイズ・日時が同一時点の情報であるとは保証しない。
modified_atはtime.nowと同じ数値表現を使い、timeの日時変換・表示に渡せる。

```qyno
using io.file as fs

int main();
	file source
	try(source = fs.open("measurements.bin", "rb"));
		int_64 bytes = fs.size(source)
		if(bytes >= 16);
			fs.seek(source, -16, fs.SeekOrigin.end)
			unsigned int_8 footer[] = fs.read_bytes(source, 16)
			io.print(footer)
		io.print(time.isoformat(fs.modified_at(source)))
	finally;
		fs.close(source)
	return 0
```

SeekOriginは次節の標準enumであり、利用者による定義やimportは不要。

## 2. SeekOriginと位置

標準の独立enumとして登録する。

```text
enum io.file.SeekOrigin;
	begin = 0
	current = 1
	end = 2
```

- begin: 先頭0を基準とする。
- current: 先読み済みのOS位置ではなく、利用者が読み終えた論理位置を基準とする。
- end: seek本体で取得した現在のファイルサイズを基準とする。
- offsetは符号付きint_64。current/endには負のoffsetも指定できる。
- 結果はint_64で表現可能な非負のバイト位置に限る。
- 初版は移動先を取得時点のEOF以下に制限する。EOFそのものへの移動は許可する。
  EOFより後への移動、空隙を伴う拡張・スパースファイルの作成は後続仕様とする。

基準位置とoffsetの加算は、符号付きの折り返しを起こす前に検査する。
ファイル位置を小数・bool・int_128から暗黙に切り詰めない。
必要ならint_64への既存の範囲検査付き変換を明示する。
定数の負のbeginオフセットはコンパイルエラー、動的な範囲違反は実行時エラー70。
end/current基準の有効性は実際の位置・サイズに依存するため実行時に検査する。

他プロセスがファイルを変更する場合、基準サイズの取得と位置変更は原子的ではない。
取得後に縮小されれば、成功した位置が新しいEOFより後になることはある。
ファイル全体のロックや、他プロセスとの整合性保証はこのAPIに含めない。

## 3. モード・readonly・共有

| モード | tell / size / modified_at | seek |
| --- | --- | --- |
| r / w / a | 可 | 不可 |
| rb / wb / ab | 可 | 可 |

テキストのtellはUTF-8バイト位置。書記素数・文字数・行番号ではない。
初版のseekはバイナリ専用として、UTF-8の途中位置をテキスト読み取りの起点にしない。
textでの任意seek、テキスト用の位置しおり等は後続の設計にする。

tell / size / modified_atは論理状態を変更しないため、constのfileやref readonly fileからも利用できる。
外部状態に依存するためconst初期値への呼び出し・コンパイル時評価・呼び出しの除去は不可。
seekはreadonlyのfileから利用できない。
ハンドルを利用者の代わりにcloseしたり、所有権を移したりしない。

すべての別名が同じ位置と読み取りバッファを共有する。
一方の別名でseekした後、もう一方からreadすると移動後の位置から読む。

## 4. 先読みバッファ

現在の実装では、OSから最大4096バイト先読みして1バイトずつ利用者へ渡す。
通常の読み取り状態では次の関係になる。

```text
論理位置 = OSの現在位置 - バッファ内の未消費バイト数
```

read / read_bytes / readlineによる消費・CRの読み戻しを含めてこの位置を正しく取得する。
例えばOSが4096バイト読んでも、利用者が1バイトしか消費していなければtellは1。
readlineでは、利用者に返さない改行コードも実際に消費したバイトとして位置へ反映する。

seekは次の順序で処理する。

1. 引数を左から右へ1回ずつ評価し、ハンドル・モード・originを検査する。
2. 必要な論理位置とサイズを取得し、移動先の範囲を検査する。
3. 算出した絶対位置へOSの位置を変更する。
4. 成功後だけ読み取りバッファを空にする。
5. 移動後の位置を返す。

currentをOSの先読み位置へ直接加算しない。
同じ位置へのseekでも、成功時には古い先読みバッファを破棄する。
引数検査の失敗で位置・バッファを変更しない。OSの位置変更が失敗した場合も、
通常ファイルに対する各OSの失敗時の位置据置を確認して管理状態を保持する。
EOFの永続キャッシュを導入しない。

## 5. 書き込みと追記

wbでは、書き終えた領域へ戻ってヘッダー等を書き直せる。
wbで既存ファイルをopenすると切り詰めるという既存仕様は変更しない。
既存内容を保持する更新モードr+bは、読み書き切り替え時の同期も含めて後続段階で追加する。

open直後の位置はr/rb/w/wbなら0、a/abならその時点のEOFにそろえる。
a/abの初期位置設定に失敗した場合、内部で取得済みハンドルを回収してopen失敗として扱う。
try_openでは同じ失敗をfalseとErrorへ変換する。

abでseekしても、write_bytesは各OSのappend機能によりその時点の末尾へ書く。
追記先をseekの位置で上書きする動作にはしない。
追記成功後のtellは自分の書き込みの直後の位置。別プロセスの追記だけでは自分の位置を進めない。
したがってappendのtellとsizeは常に一致するとは限らない。
writeが途中まで進んだ場合、位置とサイズにもその進捗が残る。

## 6. サイズ・更新日時

- sizeの単位はバイト。lib.size(data)のビット数とは区別する。
- lib.size(file)は引き続き対象外。file.lengthも追加しない。
- sizeはハンドルに結び付くファイルを調べる。pathを再openして別ファイルを参照しない。
- size / modified_atは読み取り位置を動かさず、先読みバッファを消費・破棄しない。
- 書き込みがOSへ受理された状態を観測するが、ディスクへの永続化は保証しない。
- 更新日時の粒度・更新タイミングはOSとファイルシステムに依存する。
  quadrupleで返すことは、記録された元の時刻の精度を増やす意味ではない。
- 作成日時はLinux等で常に取得できるとは限らないため、今回の共通APIに含めない。

## 7. エラーとclose

未オープン・閉鎖済み、textへのseek、不正origin・offset等のAPI違反は従来どおり1/70。
OSの問い合わせ・seek失敗はio.Errorを送出し、既存のtryで捕捉できる。
sizeやOSの位置をint_64で表現できない場合は、外部の状態によるout_of_rangeとして回復可能にする。
回復可能コードの追加は[time設計のエラー節](time-design.md#10-エラーと既存tryへの統合)にまとめる。
Error.operationはtell/seek/size/modified_at、pathと発生位置は既存の規則を維持する。
close責任は成功・失敗とも既存の所有者に残り、回復経路にも明示close検査を適用する。

## 8. 実装・検証

- Linux: lseek、fstatによるサイズ・mtime取得。
- Windows: SetFilePointerEx、GetFileSizeExまたは同等のハンドル情報、最終書き込みFILETIME。
- FILETIMEは整数段階でUNIXエポックとの差を計算し、doubleを経由せずquadrupleの秒へ変換する。
- ネイティブ出力と-S、Linux/Windowsで同じ公開契約にそろえる。
- 小さなファイル・空・EOF・4096境界・論理current・CRLF・別名・追記・外部縮小を検証する。
- 負値・int_64境界・加算オーバーフロー・EOF超過・readonly・未open/close後も検証する。
- サイズ・日時取得が位置とバッファを変えないこと、seek失敗時の状態をOS失敗注入で確認する。
- mtimeとtimeの変換は既知の日時を使って照合し、ファイルシステムの粒度を許容する。
- 明示close、try_open、finally、標準補完・SeekOrigin候補・診断を統合する。
