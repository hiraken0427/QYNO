'use strict';
// Reference: QYNO 0.27.0. No runtime dependency on a compiler installation.
const callable = (path, type, params, documentation) => ({
    kind: 'function', name: path.split('.').at(-1), path, type, rank: 0,
    params: params.map(p => typeof p === 'string' ? { name: p, type: 'value', rank: 0 } : { rank: 0, ...p }),
    documentation, builtin: true
});
const INTEGER_TYPES = ['int_8', 'int_16', 'int_32', 'int_64', 'int_128', 'unsigned int', 'unsigned int_8', 'unsigned int_16', 'unsigned int_32', 'unsigned int_64', 'unsigned int_128'];
const STANDARD = [
    ...['complex','complex_64','complex_128'].map(type=>callable(type,type,[{name:'real',type:type==='complex_128'?'double':'float',optional:true,defaultText:'0.0'},{name:'imag',type:type==='complex_128'?'double':'float',optional:true,defaultText:'0.0'}], '複素数を構築します。complexはcomplex_64の別名。real / imagは実数、値1個なら実数または複素数から変換できます。i4.0 / 4.0iも使用できます。')),

    ...INTEGER_TYPES.map(type => callable(type, type, [{name:'value',type:'integer / half / float / double / quadruple / str'},{name:'base',type:'int',optional:true,defaultText:'0'}], 'int / unsigned intは32bitの別名です。範囲を検査して整数型へ変換します。strの変換にはbase=0/2/8/10/16を指定できます。')),
    callable('math.sqrt', 'half / float / double / quadruple / complex / complex_128', ['x'], '平方根を返します。複素数は主平方根、64bit以下のint引数はdouble、int_128引数はquadrupleに変換します。'),
    callable('math.abs', 'integer / half / float / double / quadruple', ['x'], '絶対値。複素数は実数の大きさを返します。|x| の表記も使用できます。'),
    callable('math.round', 'integer / half / float / double / quadruple', ['x', { name: 'digits', type: 'int', optional: true, default: '0' }], '指定桁で四捨五入します。'),
    callable('math.roundup', 'integer / half / float / double / quadruple', ['x', { name: 'digits', type: 'int', optional: true, default: '0' }], '指定桁で天井関数に従って丸めます。'),
    callable('math.rounddown', 'integer / half / float / double / quadruple', ['x', { name: 'digits', type: 'int', optional: true, default: '0' }], '指定桁で床関数に従って丸めます。'),
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
    callable('io.file.open', 'file', [{ name: 'path', type: 'str', placeholder: '"path.txt"' }, { name: 'mode', type: 'str', optional: true, default: '"r"' }], 'UTF-8ファイルを開きます。r は読み取り、w は切り詰め、a は追記。正常終了の全経路で明示的な close が必要です。'),
    callable('io.file.close', 'void', [{ name: 'source', type: 'file' }], '共有ハンドルを閉じます。二重 close と未オープンの close は許可されます。'),
    callable('io.file.is_open', 'bool', [{ name: 'source', type: 'file' }], 'ハンドルが開いているかを返します。'),
    callable('io.file.read', 'str', [{ name: 'source', type: 'file' }], '現在位置から EOF まで UTF-8 で読みます。改行・BOM・NUL を保持します。'),
    callable('io.file.readline', 'bool', [{ name: 'source', type: 'file' }, { name: 'line', type: 'str', ref: true }], '改行を除いた1行を読みます。空行は true と空文字列、EOF は false で出力先を保持します。LF / CRLF / CR に対応します。'),
    callable('io.file.write', 'int_64', [{ name: 'destination', type: 'file' }, { name: 'value', type: 'value', placeholder: '"text"' }], 'print と同じ書式で書き込み、UTF-8のバイト数をint_64で返します。改行は自動追加しません。'),
    callable('io.file.exists', 'bool', [{ name: 'path', type: 'str', placeholder: '"path.txt"' }], '通常ファイルの存在を確認します。相対パスは実行時の作業ディレクトリが基準です。')
];
const KEYWORD_COMPLETIONS = ['complex', 'complex_64', 'complex_128', 'int', 'int_8', 'int_16', 'int_32', 'int_64', 'int_128', 'half', 'quadruple', 'unsigned', 'float', 'double', 'bool', 'str', 'char', 'file', 'void', 'Void', 'auto', 'const', 'ref', 'val', 'return', 'if', 'else', 'for', 'while', 'break', 'continue', 'and', 'or', 'xor', 'bnot', 'not', 'true', 'false', 'none', 'import', 'using', 'as', 'model', 'interface', 'implements', 'public', 'private', 'readonly'];
const properties = {
    real: { name:'real',kind:'property',type:'float',readonly:true,documentation:'複素数の実部。読み取り専用です。' },
    imag: { name:'imag',kind:'property',type:'float',readonly:true,documentation:'複素数の虚部。読み取り専用です。' },
    length: { name: 'length', kind: 'property', type: 'int_64', documentation: '配列の要素数、または文字列のUnicode拡張書記素クラスタの数。読み取り専用です。' },
    addr: { name: 'addr', kind: 'property', type: 'address', documentation: '変数の場所。ref 変数の場合は現在参照している場所です。' },
    value: { name: 'value', kind: 'property', documentation: '変数の値。ref 変数では参照先の値です。' },
    add: callable('add', 'void', [{ name: 'value', optional: true }], '要素を追加します。add() は型ごとの初期値を1個追加します。配列上限と参照中の変更を検査します。'),
    remove_at: callable('remove_at', 'void', [{ name: 'index', type: 'int' }], '指定した位置の要素を削除します。'),
    clear: callable('clear', 'void', [], '配列を空にします。')
};
module.exports = { STANDARD, KEYWORD_COMPLETIONS, properties };
