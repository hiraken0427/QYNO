# 128bit整数・half・quadruple（0.26.14）

新段階5（旧段階11・12・13）を実装した。配列、モデルのフィールド、定数、値渡し、
ref / readonly / addr、関数とジェネリック、入出力、for、Linux / Windows出力で使用できる。

| 型 | 数値形式 |
| --- | --- |
| int_128 | -2^127〜2^127-1の符号付き整数 |
| unsigned int_128 | 0〜2^128-1の符号なし整数 |
| half | IEEE binary16、仮数精度11bit、指数5bit |
| quadruple | IEEE binary128、仮数精度113bit、指数15bit |

```text
using io

increase(ref int_128 value);
	value++

int main();
	int_128 n = 18446744073709551616
	unsigned int_128 mask = 0xffffffffffffffffffffffffffffffff
	half small = 1.5
	quadruple precise = 1.0000000000000000000000000000000002
	increase(ref n)
	io.print("{n}, {mask:0x}, {small}, {precise}")
	return 0
```

int_128 / half / quadrupleは型名として予約する。従来同名の変数・関数を使っていた場合は名前を変更する。

## リテラル・推論・変換

- 10進整数は従来のint / int_64 / unsigned int_64の範囲を維持し、その先をint_128、
  さらに2^127以上をunsigned int_128とする。負号付きの最小値と括弧にも対応する。
- 基数表記は0b / 0o / 0d / 0x。64bit以内の既存のビット列解釈は維持し、
  64bitを超える非10進のビット列はint_128とする。128bit幅の全1は-1になる。
  unsignedの文脈と明示変換では、同じビット列をunsigned int_128として解釈できる。
- 通常の小数はdouble。doubleの範囲を超える有限の小数はquadrupleとして推論する。
  e / Eによる10進指数表記にも対応する。虚数の係数は引き続きdoubleの範囲である。
- half / quadrupleの宣言・引数・戻り値・明示変換の文脈では、元の小数リテラルから
  目的の精度に直接変換する。リテラル式もその文脈で演算する。
  既にdoubleに格納した値や、doubleとして計算済みの戻り値の精度を後から回復する機能ではない。
- autoは引き続き静的な型推論で、代入後に型は変わらない。
- 整数の暗黙変換は全範囲を保持する拡大、または範囲に収まる定数のみ。
  符号付き64bitと符号なし64bitを混ぜた場合、安全な共通型としてint_128を使用できる。
  int_128とunsigned int_128には安全な共通型がないため、明示変換が必要。
- 実数の拡大順はhalf → float → double → quadruple。変数の縮小は明示変換が必要。
  例えば `half(q)` / `double(q)`。整数から実数への変換は許可し、その実数の精度で丸める。
- 整数への変換は範囲を検査し、小数部を0方向へ切り捨てる。NaN・無限大・範囲外は拒否する。
- complexの成分型は変更しない。quadrupleを成分へ入れる場合は、
  `complex(real=q)` などの明示的な構築で丸める。quadrupleと複素数の暗黙の混合演算は許可しない。

## 演算と安全性

- 整数の四則演算・余り・累乗・and / or / xor / bnot・シフトに対応する。
  演算は128bitで折り返す。符号付き右シフトは算術、符号なしは論理シフト。
  シフト量は0〜127。指数・シフト量・丸めの桁数は内部のint_64へ範囲検査して変換する。
- 整数の0除算と最小値 / -1はエラー。最小値 % -1は0。
  整数の負の累乗は、整数で表せる1と符号付きの-1だけを許可する。
- half / quadrupleは四則演算、整数乗、比較、abs、sqrt、round / roundup / rounddownに対応する。
  各演算の結果を目的の精度へ最近接・偶数丸めする。sqrtは正しく丸めた実数平方根。
  int_128のsqrtはquadrupleへ拡大して計算する。
- 実数の0除算・オーバーフロー・不正な平方根は従来のfloat / doubleと同様にinf / nanを生じる。
  NaNとの比較は!=を除いてfalse。符号付き0を維持する。0の負の累乗はエラー。
- round系の中間値は、10の累乗を浮動小数点へ変換せず整数比として計算する。
  四捨五入は0から離す。整数の丸め結果が範囲外ならエラー。
- forの整数更新は境界を越えたら終了し、折り返して継続しない。
  half / quadrupleのカウンターも有限値・非0の増分・進行方向・丸め後の停滞を検査する。
- 配列の長さ・添字・バイト数は引き続き内部のint_64。大きすぎるint_128の添字や配列上限を切り詰めない。

## 入出力

- print / file.write / 文字列補間はすべての新しい型に対応する。
  128bit整数の基数指定は型の128bit幅を使用し、10進表示は符号の有無に従う。
- strは128bit整数の基数・prefixを指定できる。half / quadrupleは `str(value)` で10進表示へ変換できる。
  halfは有効数字5桁、quadrupleは36桁までを表示し、末尾の不要な0を省く。
  これらの表示は同じ型へ読み戻せる。大きな指数・小さな指数はe表記を使用する。
- int_128 / unsigned int_128の文字列変換は0b / 0o / 0d / 0xとbaseを受け付ける。
  half / quadrupleの文字列変換は10進・e / E表記を直接解析する。
- input(target)、input("{target}")、型付きの `value = input()` に対応する。
  入力先の精度・整数範囲で検査し、boolで失敗を受け取る場合は全入力先を保持する。
  入力のnan / infと、非0の入力が丸め後に0になるアンダーフローは拒否する。
  10進の数字の入力上限は4096桁。

## 実装とビルド

128bit整数とquadrupleは不変の16バイト領域を参照し、halfは16bitのビット列を保持する。
スカラーのスロットと配列要素は従来の8バイト。型名のbit数が配列の詰めた格納サイズを保証するものではない。
通常のコピーとrefの規則、既存GCの寿命管理を維持する。

`wide_runtime.c`をコンパイラの定数評価と実行時で共有する。GCCの128bit整数・binary128拡張と
libgccの基本演算を使い、10進変換・平方根・桁丸めは整数比で実装する。
生成プログラムはlibcやlibquadmathを必要としない。Linux / Windowsとも同じ、OS呼び出しのない
実行時コードを使用する。GCCの利用条件は [licenses/libgcc](../licenses/libgcc/README.md) に収録した。

通常のmakeは同梱の `wide_blob.h` を使用する。実行時コードを変更した場合は
`python3 scripts/build-wide-runtime.py` で再生成する。再生成にはLinux x86-64のGCCとGNU binutilsが必要。
`qyc -S`もこの実行時コードを含むアセンブリを出力する。

回帰テスト: `make test-wide-types`。VS Code拡張は型名、unsignedの宣言、指数表記、補完・引数ヒント、
0.26.14のコンパイラ診断に対応する。
