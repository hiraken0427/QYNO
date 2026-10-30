'use strict';
// Reference: QYNO 0.27.9. No runtime dependency on a compiler installation.
const callable = (path, type, params, documentation) => ({
    kind: 'function', name: path.split('.').at(-1), path, type, rank: 0,
    params: params.map(p => typeof p === 'string' ? { name: p, type: 'value', rank: 0 } : { rank: 0, ...p }),
    documentation, builtin: true
});
const INTEGER_TYPES = ['int_8', 'int_16', 'int_32', 'int_64', 'int_128', 'unsigned int', 'unsigned int_8', 'unsigned int_16', 'unsigned int_32', 'unsigned int_64', 'unsigned int_128'];
const IO_CODES = ['no_error','not_found','permission_denied','already_exists','invalid_path','not_regular_file','too_many_open_files','no_space','read_only','broken_pipe','resource_busy','io_error','unsupported_operation','out_of_range','system_error','not_directory','directory_not_empty'];
const IO_ERROR_CODE = {name:'ErrorCode',path:'io.ErrorCode',type:'io.ErrorCode',kind:'enum',builtin:true,members:IO_CODES.map((name,enumValue)=>({name,path:'io.ErrorCode.'+name,type:'io.ErrorCode',kind:'enumMember',readonly:true,builtin:true,enumValue,documentation:'回復可能なファイルI/Oエラー分類'}))};
const IO_ERROR = {name:'Error',path:'io.Error',type:'io.Error',kind:'model',builtin:true,members:[['code','io.ErrorCode'],['message','str'],['operation','str'],['path','str'],['os_code','int_64'],['source','str'],['line','int'],['column','int']].map(([name,type])=>({name,type,kind:'field',rank:0,visibility:'public',builtin:true,documentation:'発生時点のI/Oエラー情報'}))};
const enumType=(path,names)=>({name:path.split('.').at(-1),path,type:path,kind:'enum',builtin:true,members:names.map((name,enumValue)=>({name,path:path+'.'+name,type:path,kind:'enumMember',readonly:true,builtin:true,enumValue}))});
const SEEK_ORIGIN=enumType('io.file.SeekOrigin',['begin','current','end']);
const WEEKDAY=enumType('time.Weekday',['monday','tuesday','wednesday','thursday','friday','saturday','sunday']);
const DATE_TIME={name:'DateTime',path:'time.DateTime',type:'time.DateTime',kind:'model',builtin:true,members:[['year','int','1970'],['month','int','1'],['day','int','1'],['hour','int','0'],['minute','int','0'],['second','quadruple','0.0'],['offset_minutes','int','0']].map(([name,type,defaultText])=>({name,type,defaultText,kind:'field',rank:0,visibility:'public',builtin:true}))};
const STOPWATCH={name:'Stopwatch',path:'time.Stopwatch',type:'time.Stopwatch',kind:'model',builtin:true,members:['start','stop','elapsed','lap','reset','restart','is_running'].map(name=>({...callable(name,name==='is_running'?'bool':['stop','elapsed','lap'].includes(name)?'quadruple':'void',[],'停止期間を除く累積時間を秒数で扱います。'),readonly:['elapsed','is_running'].includes(name),visibility:'public'}))};
const RANDOM_FUNCTIONS = [
    ...['int_8','int_16','int','int_32','int_64','int_128','unsigned_int_8','unsigned_int_16','unsigned_int','unsigned_int_32','unsigned_int_64','unsigned_int_128'].map(name=>callable('lib.random.'+name,name.replace('unsigned_','unsigned '),[{name:'min <= i < max',type:'range',range:true,optional:true,placeholder:'0 <= i < 10'}],'不等式の整数解から一様に生成。省略時は型の全域。中央の名前は宣言不要の仮の名前です。')),
    ...['half','float','double','quadruple'].map(name=>callable('lib.random.'+name,name,[{name:'min < x < max',type:'range',range:true,placeholder:'0 < x < 1'}],'有限の小数範囲を不等式で指定。開いた端点を返しません。範囲省略不可。')),
    callable('lib.random.bool','bool',[],'trueとfalseを各1/2で生成します。'),
    callable('lib.random.char','char',[{name:'candidates',type:'str',optional:true,placeholder:'"ABCDEF"'}],'候補の書記素クラスタから抽選。省略時は印字可能ASCII。'),
    callable('lib.random.str','str',[{name:'count',type:'int_64',placeholder:'8'},{name:'candidates',type:'str',optional:true,placeholder:'"ABCDEF0123456789"'}],'指定長のASCII文字列。候補は印字可能ASCII。暗号用ではありません。'),
    callable('lib.random.seed','void',[{name:'value',type:'unsigned int_64',placeholder:'42'}],'固定シードへ再初期化し、両OSで同じ列を再現します。')
];
const RANDOM_GENERATOR={name:'Random',path:'lib.random.Random',type:'lib.random.Random',kind:'model',builtin:true,
    initializer:{...callable('init','void',[{name:'seed',type:'unsigned int_64',defaultText:'0',optional:true}],'固定シード。省略時0。'),initializer:true,visibility:'public'},
    members:RANDOM_FUNCTIONS.map(fn=>({...fn,path:'lib.random.Random.'+fn.name,visibility:'public',readonly:false}))};
const STANDARD = [
    SEEK_ORIGIN, WEEKDAY, DATE_TIME, STOPWATCH, RANDOM_GENERATOR, ...RANDOM_FUNCTIONS,
    callable('time.now', "quadruple", [], "UNIX時刻の秒数。日時用。OSによる時刻変更の影響を受けます。"),
    callable('time.monotonic', "quadruple", [], "後戻りしない時計の秒数。起点は非公開で、処理時間や期限の計測に使います。"),
    callable('time.cpu', "quadruple", [], "プロセスが使用したuser+kernelのCPU秒数。"),
    callable('time.elapsed', "quadruple", [{"name": "start", "type": "quadruple"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.sleep', "void", [{"name": "seconds", "type": "quadruple"}], "有限・非負の期間だけ待機します。秒単位。0は即時復帰。"),
    callable('time.sleep_until', "void", [{"name": "deadline", "type": "quadruple"}], "monotonicの指定期限まで待ちます。過去の期限なら即時復帰。"),
    callable('time.nanoseconds', "quadruple", [{"name": "value", "type": "quadruple"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.microseconds', "quadruple", [{"name": "value", "type": "quadruple"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.milliseconds', "quadruple", [{"name": "value", "type": "quadruple"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.seconds', "quadruple", [{"name": "value", "type": "quadruple"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.minutes', "quadruple", [{"name": "value", "type": "quadruple"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.hours', "quadruple", [{"name": "value", "type": "quadruple"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.days', "quadruple", [{"name": "value", "type": "quadruple"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.weeks', "quadruple", [{"name": "value", "type": "quadruple"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.in_nanoseconds', "quadruple", [{"name": "value", "type": "quadruple"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.in_microseconds', "quadruple", [{"name": "value", "type": "quadruple"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.in_milliseconds', "quadruple", [{"name": "value", "type": "quadruple"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.in_seconds', "quadruple", [{"name": "value", "type": "quadruple"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.in_minutes', "quadruple", [{"name": "value", "type": "quadruple"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.in_hours', "quadruple", [{"name": "value", "type": "quadruple"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.in_days', "quadruple", [{"name": "value", "type": "quadruple"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.in_weeks', "quadruple", [{"name": "value", "type": "quadruple"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.to_datetime', "time.DateTime", [{"name": "timestamp", "type": "quadruple"}, {"name": "offset_minutes", "type": "int", "optional": true, "default": "0"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.to_timestamp', "quadruple", [{"name": "date", "type": "time.DateTime"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.weekday', "time.Weekday", [{"name": "date", "type": "time.DateTime"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.day_of_year', "int", [{"name": "date", "type": "time.DateTime"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.is_leap_year', "bool", [{"name": "year", "type": "int"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.days_in_month', "int", [{"name": "year", "type": "int"}, {"name": "month", "type": "int"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.iso_format', "str", [{"name": "timestamp", "type": "quadruple"}, {"name": "offset_minutes", "type": "int", "optional": true, "default": "0"}], "日時を小数秒9桁と必須のZ/固定オフセットで表示します。"),
    callable('time.format', "str", [{"name": "timestamp", "type": "quadruple"}, {"name": "pattern", "type": "str", "placeholder": "\"%Y-%m-%d %H:%M:%S\""}, {"name": "offset_minutes", "type": "int", "optional": true, "default": "0"}], "UTC/固定オフセットで日時を表示。%fは6桁、%Nは9桁。"),
    callable('time.parse', "quadruple", [{"name": "text", "type": "str"}], "秒単位のquadrupleを使用するtime標準関数。UTC/固定オフセットに対応します。"),
    callable('time.try_parse', "bool", [{"name": "text", "type": "str"}, {"name": "destination", "type": "quadruple", "ref": true}], "ISO日時を解析。失敗時falseで出力先を保持します。出力先はref quadruple。"),
    callable('time.format_duration', "str", [{"name": "seconds", "type": "quadruple"}, {"name": "digits", "type": "int", "optional": true, "default": "3"}], "[-][Dd ]HH:MM:SSの形式で期間を表示。digitsは0〜9。"),
    ...['NANOSECOND','MICROSECOND','MILLISECOND','SECOND','MINUTE','HOUR','DAY','WEEK'].map(name=>({name,path:'time.'+name,type:'quadruple',kind:'constant',readonly:true,builtin:true,documentation:'秒単位の時間定数'})),
    IO_ERROR_CODE, IO_ERROR,
    ...['complex','complex_64','complex_128'].map(type=>callable(type,type,[{name:'real',type:type==='complex_128'?'double':'float',optional:true,defaultText:'0.0'},{name:'imag',type:type==='complex_128'?'double':'float',optional:true,defaultText:'0.0'}], '複素数を構築します。complexはcomplex_64の別名。real / imagは実数、値1個なら実数または複素数から変換できます。i4.0 / 4.0iも使用できます。')),

    ...INTEGER_TYPES.map(type => callable(type, type, [{name:'value',type:'integer / half / float / double / quadruple / str'},{name:'base',type:'int',optional:true,defaultText:'0'}], 'int / unsigned intは32bitの別名です。範囲を検査して整数型へ変換します。strの変換にはbase=0/2/8/10/16を指定できます。')),
    callable('math.sqrt', 'half / float / double / quadruple / complex / complex_128', ['x'], '平方根を返します。複素数は主平方根、64bit以下のint引数はdouble、int_128引数はquadrupleに変換します。'),
    callable('math.abs', 'integer / half / float / double / quadruple', ['x'], '絶対値。複素数は実数の大きさを返します。|x| の表記も使用できます。'),
    callable('math.round', 'integer / half / float / double / quadruple', ['x', { name: 'digits', type: 'int', optional: true, default: '0' }], '指定桁で四捨五入します。'),
    callable('math.ceil', 'integer / half / float / double / quadruple', ['x', { name: 'digits', type: 'int', optional: true, default: '0' }], '指定桁で天井関数に従って丸めます。'),
    callable('math.floor', 'integer / half / float / double / quadruple', ['x', { name: 'digits', type: 'int', optional: true, default: '0' }], '指定桁で床関数に従って丸めます。'),
    callable('int','int',[{name:'value',type:'int / float / double / str'},{name:'base',type:'int',optional:true,defaultText:'0'}], '数値を32bit符号付きint（int_32）に範囲検査付きで変換します。64bitはint_64を明示します。strは接頭辞を自動判定し、base=0/2/8/10/16を指定できます。baseはstrの変換にだけ使えます。'),
    ...['half', 'float', 'double', 'quadruple'].map(type => callable(type, type, ['value'], `数値を ${type} に変換します。halfはbinary16、quadrupleはbinary128。half / quadrupleは10進文字列からも変換できます。`)),
    callable('char', 'char', [{ name: 'text', type: 'str / char' }], 'Unicode拡張書記素クラスタを1個含む文字列へ変換します。'),
    callable('str','str',[{name:'value',type:'integer / half / quadruple / str / char'},{name:'base',type:'int',optional:true,defaultText:'10'},{name:'prefix',type:'bool',optional:true,defaultText:'false'}], '整数を2/8/10/16進に文字列化します。prefix=trueで接頭辞を付けます。base/prefixはintの変換にだけ使えます。half / quadrupleは10進表示で文字列化できます。str/charの従来の変換も利用できます。'),
    callable('io.print', 'void', [{ name: 'value', type: 'value', optional: true, placeholder: '"text"' }], '値を表示します。既定の終端は改行。文字列リテラルの {式} と end= を使用できます。'),
    callable('io.input', 'context / bool', [{ name: 'target', type: 'value', optional: true }], 'input(target) は直接入力。値を返す形式は input() / input(prompt=message)。入力先を指定した形式は bool を返します。'),
    callable('lib.size', 'int_64', ['value'], '値のビット数。型の論理幅またはUTF-8本体・全要素の合計を返します。実メモリ確保量・予約容量・管理情報は含みません。'),
    callable('lib.length', 'int_64', ['value'], '文字列は書記素数、配列は外側の要素数、数値は符号を除いた整数部分の10進桁数。0は1桁。'),
    ...['join','basename','dirname','extension','stem','is_absolute','is_relative','normalize','with_extension'].map(name=>callable('io.path.'+name, ['is_absolute','is_relative'].includes(name)?'bool':'str',
        name==='join'?[{name:'base',type:'str'},{name:'child',type:'str'}]:name==='with_extension'?[{name:'path',type:'str'},{name:'extension',type:'str'}]:[{name:'path',type:'str'}],
        ({join:'パス2個を結合します。',basename:'末尾の名前を取得します。',dirname:'親パスを取得します。',extension:'最後の拡張子をドット付きで返します。',stem:'拡張子を除いた末尾の名前を返します。',is_absolute:'完全な絶対パスか判定します。',is_relative:'完全な絶対パスでないか判定します。',normalize:'区切りと余分な . を整理します。.. は保持します。',with_extension:'拡張子を変更します。空文字なら削除します。'})[name]+' 生成先OSの規則で処理し、ファイルへはアクセスしません。')),
    callable('io.dir.exists','bool',[{name:'path',type:'str / char'}],'ディレクトリの存在を確認する。'),
    callable('io.dir.create','void',[{name:'path',type:'str / char'}],'ディレクトリを1階層作成する。既存の項目はエラーとなる。'),
    {...callable('io.dir.list','str',[{name:'path',type:'str / char'},{name:'max_entries',type:'int_64',optional:true,default:'1048576',defaultText:'1048576'},{name:'max_name_bytes',type:'int_64',optional:true,default:'67108864',defaultText:'67108864'}],'直下の名前をUTF-8のバイト順で返す。'),rank:1},
    callable('io.dir.remove','void',[{name:'path',type:'str / char'}],'空のディレクトリを削除する。リンクは削除しない。'),
    callable('io.file.read_text','str',[{name:'path',type:'str / char'},{name:'max_bytes',type:'int_64',optional:true,default:'67108864',defaultText:'67108864'}],'UTF-8のファイルを文字列として読み込む。開閉は内部で行う。'),
    {...callable('io.file.read_lines','str',[{name:'path',type:'str / char'},{name:'max_bytes',type:'int_64',optional:true,default:'67108864',defaultText:'67108864'},{name:'max_lines',type:'int_64',optional:true,default:'1048576',defaultText:'1048576'}],'UTF-8のファイルを行の配列として読み込む。'),rank:1},
    ...['write_text','append_text'].map(name=>callable('io.file.'+name,'int_64',[{name:'path',type:'str / char'},{name:'text',type:'str / char'}],name==='write_text'?'文字列を上書きし、バイト数を返す。':'文字列を追記し、バイト数を返す。')),
    callable('io.file.write_lines','int_64',[{name:'path',type:'str / char'},{name:'lines',type:'str',rank:1},{name:'newline',type:'str / char',optional:true,default:'"\\n"',defaultText:'"\\n"'},{name:'final_newline',type:'bool',optional:true,default:'true',defaultText:'true'}],'行の配列を書き込み、バイト数を返す。'),
    callable('io.file.remove','void',[{name:'path',type:'str / char'}],'通常のファイルを削除する。リンクは削除しない。'),
    ...['copy','move'].map(name=>callable('io.file.'+name,'void',[{name:'source',type:'str / char'},{name:'destination',type:'str / char'}],name==='copy'?'ファイルの内容をコピーする。既存の出力先は上書きしない。':'同じファイルシステム内で移動する。既存の出力先は上書きしない。')),
    callable('io.dir.current','str',[],'現在の作業ディレクトリを絶対パスで取得します。実行ファイルの場所とは異なります。'),
    callable('io.dir.change','void',[{name:'path',type:'str'}],'既存ディレクトリへ移動します。絶対・相対パスに対応。プロセス全体の相対パスの基準を変更します。'),
    callable('io.path.absolute','str',[{name:'path',type:'str'}],'現在の作業ディレクトリを基準に絶対パスを生成します。存在を要求せず、.. は保持します。constでは使用不可。'),
    ...['tell','size','modified_at'].map(name=>callable('io.file.'+name,name==='modified_at'?'quadruple':'int_64',[{name:'source',type:'file'}],name==='modified_at'?'最終更新日時をUNIX秒数で取得。':'論理位置または内容のバイト数を取得。位置を変更しません。')),
    callable('io.file.seek','int_64',[{name:'source',type:'file'},{name:'offset',type:'int_64'},{name:'origin',type:'io.file.SeekOrigin',optional:true,default:'io.file.SeekOrigin.begin',defaultText:'io.file.SeekOrigin.begin'}],'バイナリの先頭〜EOFへ移動し、位置を返します。currentは先読みを除いた論理位置。'),
    callable('io.file.try_open','bool',[{name:'path',type:'str'},{name:'destination',type:'file',ref:true},{name:'error',type:'io.Error',ref:true},{name:'mode',type:'str',optional:true,default:'"r"'}],'成功時trueとfile、失敗時falseとErrorを返します。出力先は未オープンが必要。falseではcaseへ移りません。'),
    callable('io.file.open', 'file', [{ name: 'path', type: 'str', placeholder: '"path.txt"' }, { name: 'mode', type: 'str', optional: true, default: '"r"' }], '通常ファイルを開きます。r/w/a はUTF-8、rb/wb/ab はバイナリ。w/wb は切り詰め、a/ab は追記。正常終了の全経路で明示的な close が必要です。'),
    callable('io.file.close', 'void', [{ name: 'source', type: 'file' }], '共有ハンドルを閉じます。二重 close と未オープンの close は許可されます。'),
    callable('io.file.is_open', 'bool', [{ name: 'source', type: 'file' }], 'ハンドルが開いているかを返します。'),
    callable('io.file.read', 'str', [{ name: 'source', type: 'file' }], '現在位置から EOF まで UTF-8 で読みます。改行・BOM・NUL を保持します。'),
    callable('io.file.read_line', 'bool', [{ name: 'source', type: 'file' }, { name: 'line', type: 'str', ref: true }], '改行を除いた1行を読みます。空行は true と空文字列、EOF は false で出力先を保持します。LF / CRLF / CR に対応します。'),
    callable('io.file.write', 'int_64', [{ name: 'destination', type: 'file' }, { name: 'value', type: 'value', placeholder: '"text"' }], 'print と同じ書式で書き込み、UTF-8のバイト数をint_64で返します。改行は自動追加しません。'),
    {...callable('io.file.read_bytes', 'unsigned int_8', [{name:'source',type:'file'},{name:'count',type:'int_64'}], 'rbから最大countバイトを読み、実際に読んだ要素数の配列を返します。countは非負で必須。EOFは空配列。不足分を補完しないよう受け取り先はdata[]を推奨。'),rank:1},
    callable('io.file.write_bytes', 'int_64', [{name:'destination',type:'file'},{name:'data',type:'unsigned int_8',rank:1}], 'wb/abへ全要素を1バイトずつ書き、バイト数を返します。dataは読み取り専用で観測しref不要。UTF-8・改行変換は行いません。'),
    callable('io.file.exists', 'bool', [{ name: 'path', type: 'str', placeholder: '"path.txt"' }], '通常ファイルの存在を確認します。相対パスは実行時の作業ディレクトリが基準です。')
];
const KEYWORD_COMPLETIONS = ['complex', 'complex_64', 'complex_128', 'int', 'int_8', 'int_16', 'int_32', 'int_64', 'int_128', 'half', 'quadruple', 'unsigned', 'float', 'double', 'bool', 'str', 'char', 'file', 'void', 'Void', 'auto', 'const', 'ref', 'val', 'return', 'if', 'else', 'for', 'while', 'break', 'continue', 'and', 'or', 'xor', 'bnot', 'not', 'true', 'false', 'none', 'import', 'using', 'as', 'enum', 'match', 'case', 'try', 'finally', 'throw', 'model', 'interface', 'implements', 'public', 'private', 'readonly'];
const TEXT_METHODS = [
    callable('contains','bool',[{name:'value',type:'str / char'}],'書記素境界で完全一致する部分を含むか。空検索はtrue。'),
    callable('starts_with','bool',[{name:'value',type:'str / char'}],'書記素境界で先頭から完全一致するか。'),
    callable('ends_with','bool',[{name:'value',type:'str / char'}],'書記素境界で末尾まで完全一致するか。'),
    callable('find','int_64',[{name:'value',type:'str / char'}],'最初の一致の書記素番号。未検出は-1、空検索は0。'),
    callable('trim','str',[],'両端のUnicode 17 White_Spaceクラスタを除いた新しい文字列。'),
    {...callable('split','str',[{name:'separator',type:'str / char'}],'完全一致で分割し、空要素を保持。空区切りはエラー。'),rank:1},
    callable('replace','str',[{name:'old',type:'str / char'},{name:'new',type:'str / char'}],'元の文字列の重ならない一致を全置換。空oldはエラー。')
].map(method=>({...method,readonly:true}));
const properties = {
    real: { name:'real',kind:'property',type:'float',readonly:true,documentation:'複素数の実部。読み取り専用です。' },
    imag: { name:'imag',kind:'property',type:'float',readonly:true,documentation:'複素数の虚部。読み取り専用です。' },
    length: { name: 'length', kind: 'property', type: 'int_64', documentation: '配列の要素数、または文字列のUnicode拡張書記素クラスタの数。読み取り専用です。' },
    addr: { name: 'addr', kind: 'property', type: 'address', documentation: '変数の場所。ref 変数の場合は現在参照している場所です。' },
    value: { name: 'value', kind: 'property', documentation: '変数の値。ref 変数では参照先の値です。' },
    append: callable('append', 'void', [{ name: 'value', optional: true }], '要素を追加します。append() は型ごとの初期値を1個追加します。配列上限と参照中の変更を検査します。'),
    remove_at: callable('remove_at', 'void', [{ name: 'index', type: 'int_64' }], '指定した位置の要素を削除します。'),
    clear: callable('clear', 'void', [], '配列を空にします。')
};
module.exports = { STANDARD, KEYWORD_COMPLETIONS, properties, TEXT_METHODS };
