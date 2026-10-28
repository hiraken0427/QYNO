# 書記素クラスタとvoid（0.26.15）

新段階6（旧段階14・15）。これで再編した6段階の型拡張を完了した。

## charとstr

charはUnicodeの拡張書記素クラスタ1個、strはその列を表す。
Unicode 17.0.0のUAX #29（GB3〜GB13、インド系文字のGB9cを含む）に従う。
char変換・長さ・添字・置換ではUTF-8の正当性を検査し、結合文字、絵文字の修飾子・ZWJ列、国旗、ハングル、CRLFを扱う。

```text
const char LETTER = "é"

int main();
	str text = "A🇯🇵é👩🏽‍💻"
	io.print(text.length)  // 4
	io.print(text[3])      // 👩🏽‍💻
	text[1] = "👍🏽"
	io.print(text)
	return 0
```

- `.length`、添字、charへの変換、文字列中の要素置換、値・ref・定数・配列・モデル、
  入力先のchar検査を同じ境界規則に揃える。文字列内の要素を直接refで渡す制約は従来どおり。
- charの既定値はU+0000、長さ1。空文字列や2個以上のクラスタはcharとして拒否する。
- 添字とlengthの型はint_64。文字列の格納形式はUTF-8のバイト長＋本体のままで、
  `file.write`の戻り値はバイト数のまま。アドレスやABIのスロットは従来どおり。
- 正規化は行わない。例えば `"é"` と `"é"` は各1クラスタだが、バイト列が違うため等しくない。
  連結や置換の結果、前後のクラスタが結合することがある。以後の添字は変更後の文字列に従う。
- 入力は全ての結果を準備した後に反映する。同じ文字列への複数の要素入力では、記述順に
  一時結果へ置換する。境界の結合で後続の入力先がなくなる場合、全ての入力先を保持して失敗する。
  boolで受け取るinputはfalse、文としてのinputは入力エラー65。
- 端末のBackspace・Delete・左右移動はクラスタ全体を扱う。画面上の幅は引き続き端末の描画に依存する。

公式データ・766件の境界テスト・Unicodeの利用条件を `unicode/17.0.0/` に収録した。
`scripts/build-grapheme-data.py` はネットワークを使わず、固定したデータから範囲表を再生成する。
`grapheme_runtime.c` はコンパイル時・実行時で共通の実装であり、外部のUnicodeライブラリに依存しない。
データや実装の変更後は `scripts/build-wide-runtime.py` も実行して実行時イメージを再生成する。

## 唯一の値を持つvoid

void（別名Void）の唯一の値を `()` とする。論理的な情報量は0bitだが、
変数・配列要素・引数は既存ABIの8バイトスロットを維持する。変更可能な場所には通常どおり別々の場所を割り当てる。

```text
const Void EMPTY = ()

void reset(ref void value);
	value = ()

identity(value) = value

int main();
	void value
	void values[3] = [EMPTY]
	reset(ref values[0])
	io.print(identity(values)) // [(), (), ()]
	return 0
```

- void / Voidは同じ型。変数・定数・多次元配列・モデルのフィールド・引数・戻り値・
  ref / ref readonly / addr・auto・ジェネリックに使用できる。既定値と配列の不足要素は `()`。
- void関数は値なしのreturnと末尾への到達で `()` を返す。`return ()`、
  `void f() = ()`、別のvoid関数の結果を返す記述も可能。通常の関数呼び出しの副作用と評価順は維持する。
- 代入・引数・配列要素はvoid同士だけを許可する。数値・boolへの変換や演算は行わない。
  ==は常にtrue、!=はfalse。比較する式の副作用は左右それぞれ評価する。順序比較は禁止。
- print / 文字列補間 / file.write / strは `()` と表示する。inputも `()` の記述を読み取る。
  入力の範囲検査と、失敗時に他の入力先も保持する規則は同じ。
- closeや配列のadd / remove_at / clearもvoidの結果を値として利用できる。
  明示close、readonly、参照・所有権・配列上限の検査は維持する。
- printは従来どおり文の構文。print自体を値式にする構文変更は行わない。
  mainの戻り値はintのまま、initは従来どおりvoid / boolのブロック形式とする。

回帰検証: `make test-grapheme-unit`（20メソッド、公式766ケースを含む）。
従来の「結合文字は複数文字」「voidをautoや引数へ使えない」というテストは新仕様へ移行した。
