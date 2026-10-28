'use strict';
// Reference: QYNO 0.26.13. No runtime dependency on a compiler installation.
const callable = (path, type, params, documentation) => ({
    kind: 'function', name: path.split('.').at(-1), path, type, rank: 0,
    params: params.map(p => typeof p === 'string' ? { name: p, type: 'value', rank: 0 } : { rank: 0, ...p }),
    documentation, builtin: true
});
const INTEGER_TYPES = ['int_8', 'int_16', 'int_32', 'int_64', 'unsigned int', 'unsigned int_8', 'unsigned int_16', 'unsigned int_32', 'unsigned int_64'];
const STANDARD = [
    ...['complex','complex_64','complex_128'].map(type=>callable(type,type,[{name:'real',type:type==='complex_128'?'double':'float',optional:true,defaultText:'0.0'},{name:'imag',type:type==='complex_128'?'double':'float',optional:true,defaultText:'0.0'}], '複素数を構築します。complexはcomplex_64の別名。real / imagは実数、値1個なら実数または複素数から変換できます。i4.0 / 4.0iも使用できます。')),

    ...INTEGER_TYPES.map(type => callable(type, type, [{name:'value',type:'integer / float / double / str'},{name:'base',type:'int',optional:true,defaultText:'0'}], 'int / unsigned intは32bitの別名です。範囲を検査して整数型へ変換します。strの変換にはbase=0/2/8/10/16を指定できます。')),
    callable('math.sqrt', 'double / float / complex / complex_128', ['x'], '平方根を返します。複素数は主平方根、int引数はdoubleに変換します。'),
    callable('math.abs', 'int / float / double', ['x'], '絶対値。複素数は実数の大きさを返します。|x| の表記も使用できます。'),
    callable('math.round', 'int / float / double', ['x', { name: 'digits', type: 'int', optional: true, default: '0' }], '指定桁で四捨五入します。'),
    callable('math.roundup', 'int / float / double', ['x', { name: 'digits', type: 'int', optional: true, default: '0' }], '指定桁で天井関数に従って丸めます。'),
    callable('math.rounddown', 'int / float / double', ['x', { name: 'digits', type: 'int', optional: true, default: '0' }], '指定桁で床関数に従って丸めます。'),
    callable('int','int',[{name:'value',type:'int / float / double / str'},{name:'base',type:'int',optional:true,defaultText:'0'}], '数値を32bit符号付きint（int_32）に範囲検査付きで変換します。64bitはint_64を明示します。strは接頭辞を自動判定し、base=0/2/8/10/16を指定できます。baseはstrの変換にだけ使えます。'),
    ...['float', 'double'].map(type => callable(type, type, ['value'], `数値を ${type} に変換します。`)),
    callable('char', 'char', [{ name: 'text', type: 'str / char' }], 'Unicodeスカラー値を1個含む文字列へ変換します。'),
    callable('str','str',[{name:'value',type:'int / str / char'},{name:'base',type:'int',optional:true,defaultText:'10'},{name:'prefix',type:'bool',optional:true,defaultText:'false'}], '整数を2/8/10/16進に文字列化します。prefix=trueで接頭辞を付けます。base/prefixはintの変換にだけ使えます。str/charの従来の変換も利用できます。'),
    callable('io.print', 'void', [{ name: 'value', type: 'value', optional: true, placeholder: '"text"' }], '値を表示します。既定の終端は改行。文字列リテラルの {式} と end= を使用できます。'),
    callable('io.input', 'context / bool', [{ name: 'target', type: 'value', optional: true }], 'input(target) は直接入力。値を返す形式は input() / input(prompt=message)。入力先を指定した形式は bool を返します。'),
    callable('io.file.open', 'file', [{ name: 'path', type: 'str', placeholder: '"path.txt"' }, { name: 'mode', type: 'str', optional: true, default: '"r"' }], 'UTF-8ファイルを開きます。r は読み取り、w は切り詰め、a は追記。正常終了の全経路で明示的な close が必要です。'),
    callable('io.file.close', 'void', [{ name: 'source', type: 'file' }], '共有ハンドルを閉じます。二重 close と未オープンの close は許可されます。'),
    callable('io.file.is_open', 'bool', [{ name: 'source', type: 'file' }], 'ハンドルが開いているかを返します。'),
    callable('io.file.read', 'str', [{ name: 'source', type: 'file' }], '現在位置から EOF まで UTF-8 で読みます。改行・BOM・NUL を保持します。'),
    callable('io.file.readline', 'bool', [{ name: 'source', type: 'file' }, { name: 'line', type: 'str', ref: true }], '改行を除いた1行を読みます。空行は true と空文字列、EOF は false で出力先を保持します。LF / CRLF / CR に対応します。'),
    callable('io.file.write', 'int_64', [{ name: 'destination', type: 'file' }, { name: 'value', type: 'value', placeholder: '"text"' }], 'print と同じ書式で書き込み、UTF-8のバイト数をint_64で返します。改行は自動追加しません。'),
    callable('io.file.exists', 'bool', [{ name: 'path', type: 'str', placeholder: '"path.txt"' }], '通常ファイルの存在を確認します。相対パスは実行時の作業ディレクトリが基準です。')
];
const KEYWORD_COMPLETIONS = ['complex', 'complex_64', 'complex_128', 'int', 'int_8', 'int_16', 'int_32', 'int_64', 'unsigned', 'float', 'double', 'bool', 'str', 'char', 'file', 'void', 'auto', 'const', 'ref', 'val', 'return', 'if', 'else', 'for', 'while', 'break', 'continue', 'and', 'or', 'xor', 'bnot', 'not', 'true', 'false', 'none', 'import', 'using', 'as', 'model', 'interface', 'implements', 'public', 'private', 'readonly'];
const properties = {
    real: { name:'real',kind:'property',type:'float',readonly:true,documentation:'複素数の実部。読み取り専用です。' },
    imag: { name:'imag',kind:'property',type:'float',readonly:true,documentation:'複素数の虚部。読み取り専用です。' },
    length: { name: 'length', kind: 'property', type: 'int_64', documentation: '配列の要素数、または文字列のUnicodeスカラー値の数。読み取り専用です。' },
    addr: { name: 'addr', kind: 'property', type: 'address', documentation: '変数の場所。ref 変数の場合は現在参照している場所です。' },
    value: { name: 'value', kind: 'property', documentation: '変数の値。ref 変数では参照先の値です。' },
    add: callable('add', 'void', [{ name: 'value', optional: true }], '要素を追加します。add() は型ごとの初期値を1個追加します。配列上限と参照中の変更を検査します。'),
    remove_at: callable('remove_at', 'void', [{ name: 'index', type: 'int' }], '指定した位置の要素を削除します。'),
    clear: callable('clear', 'void', [], '配列を空にします。')
};
module.exports = { STANDARD, KEYWORD_COMPLETIONS, properties };
