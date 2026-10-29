#!/bin/bash
# 使い方: bash ./test.sh（全スイートは make test）
# qyc が直接書き出した実行ファイルを動かして結果を確かめます（gcc などは使いません）
# Linux 上で wine がある場合は、Windows 向け（.exe）の出力も wine で確認します

pass=0

# 式を main の return に埋め込み、終了コードを期待値と比べる
assert() {
    expected="$1"
    input="$2"

    printf 'int main();\n\treturn %s\n' "$input" > tmp.qy
    ./qyc tmp.qy || exit 1
    ./tmp
    actual="$?"

    if [ "$actual" = "$expected" ]; then
        echo "OK  $input => $actual"
        pass=$((pass + 1))
    else
        echo "NG  $input => 期待値 $expected, 実際 $actual"
        exit 1
    fi
}

# プログラム全体を与え、終了コードを期待値と比べる
assert_program() {
    expected="$1"
    title="$2"
    program="$3"

    printf '%b' "$program" > tmp.qy
    ./qyc tmp.qy || exit 1
    ./tmp
    actual="$?"

    if [ "$actual" = "$expected" ]; then
        echo "OK  $title => $actual"
        pass=$((pass + 1))
    else
        echo "NG  $title => 期待値 $expected, 実際 $actual"
        exit 1
    fi
}

# コンパイルエラーになることを確認する
# 入力を与えて実行し、標準出力と終了コードを比べる
#   assert_input 期待する出力 期待する終了コード 題名 プログラム 入力
assert_input() {
    expected="$1"
    expected_code="$2"
    title="$3"
    program="$4"
    input_text="$5"

    printf '%b' "$program" > tmp.qy
    ./qyc tmp.qy || exit 1
    actual="$(printf '%b' "$input_text" | ./tmp 2> /dev/null; code=$?; printf 'x'; exit $code)"
    code="$?"
    actual="${actual%x}"

    if [ "$actual" = "$expected" ] && [ "$code" = "$expected_code" ]; then
        echo "OK  $title"
        pass=$((pass + 1))
    else
        echo "NG  $title（終了コード $code）"
        echo "    期待値: $(printf '%s' "$expected" | od -c | head -3)"
        echo "    実際  : $(printf '%s' "$actual" | od -c | head -3)"
        exit 1
    fi
}

assert_error() {
    title="$1"
    program="$2"

    printf '%b' "$program" > tmp.qy
    ./qyc tmp.qy 2> /dev/null
    code="$?"
    if [ "$code" != "1" ]; then
        echo "NG  $title => コンパイルエラーの終了コード 1 を期待しました（実際 $code）"
        exit 1
    fi
    echo "OK  $title => コンパイルエラー"
    pass=$((pass + 1))
}

# 標準出力を期待値と比べる（$1：期待する出力、$2：テスト名、$3：プログラム）
assert_output() {
    expected="$1"
    title="$2"
    program="$3"

    printf '%b' "$program" > tmp.qy
    ./qyc tmp.qy || exit 1
    actual="$(./tmp; code=$?; printf x; exit "$code")"
    code="$?"
    actual="${actual%x}"

    if [ "$actual" = "$expected" ] && [ "$code" = "0" ]; then
        echo "OK  $title"
        pass=$((pass + 1))
    else
        echo "NG  $title（終了コード $code）"
        echo "    期待値: $(printf '%s' "$expected" | od -c | head -3)"
        echo "    実際  : $(printf '%s' "$actual" | od -c | head -3)"
        exit 1
    fi
}

echo "--- 四則演算 ---"
assert 0 '0'
assert 42 '42'
assert 21 '5+20-4'
assert 41 ' 12 + 34 - 5 '
assert 47 '5+6*7'
assert 15 '5*(9-6)'
assert 4 '(3+5)/2'
assert 3 '10/3'
assert 10 '-10+20'
assert 10 '- -10'
assert 10 '- - +10'
assert 75 '10*10-5*5'
assert 7 '1 + 2 * 3 // コメント'
assert 10 '5000000000 - 4999999990'

echo "--- 書式 ---"
assert_program 7 'CRLF改行' 'int main();\r\n\treturn 1+2*3\r\n'
assert_program 9 '空行とコメント行' '// 先頭コメント\n\nint main();\n\n\t// 本体のコメント\n\treturn 4+5\n'
assert_program 3 '末尾に改行なし' 'int main();\n\treturn 3'

echo "--- print ---"
assert_output $'Hello, World!\n' '文字列' 'int main();\n\tio.print("Hello, World!")\n'
assert_output $'single\n' 'シングルクォート' "int main();\n\tio.print('single')\n"
assert_output $'\n' '引数なしは改行のみ' 'int main();\n\tio.print()\n'
assert_output $'\n' '空文字列' 'int main();\n\tio.print("")\n'
assert_output $'1 + 2 * 3 = 7\n' '式の埋め込み' 'int main();\n\tio.print("1 + 2 * 3 = {1 + 2 * 3}")\n'
assert_output $'-42\n' '負の数' 'int main();\n\tio.print("{-42}")\n'
assert_output $'0\n' '0' 'int main();\n\tio.print("{0}")\n'
assert_output $'-9223372036854775808\n' '最小値' 'int main();\n\tio.print("{-9223372036854775807 - 1}")\n'
assert_output $'37\n' '埋め込みの連続' 'int main();\n\tio.print("{10 / 3}{10 - 3}")\n'
assert_output $'{a} "b" \\ \tc\n' 'エスケープ' 'int main();\n\tio.print("\\{a\\} \\"b\\" \\\\ \\tc")\n'
assert_output $'日本語\n' '日本語' 'int main();\n\tio.print("日本語")\n'
assert_output $'a\nb\n' '複数の print' 'int main();\n\tio.print("a")\n\tio.print("b")\n'
assert_output $'x\n' 'return の後は実行しない' 'int main();\n\tio.print("x")\n\treturn 0\n\tio.print("y")\n'

echo "--- 型と変数 ---"
assert_output $'10 21\n' 'int の変数' 'int main();\n\tint a = 10\n\tio.print("{a} {a * 2 + 1}")\n'
assert_output $'3.14 6.28 4.14\n' 'float' 'int main();\n\tfloat f = 3.14\n\tio.print("{f} {f * 2} {f + 1}")\n'
assert_output $'0.3 0.333333 10.0\n' 'double' 'int main();\n\tdouble d = 0.1 + 0.2\n\tio.print("{d} {1.0 / 3} {2.5 * 4}")\n'
assert_output $'2.5 13.14\n' 'int から float・double への昇格' 'int main();\n\tint a = 10\n\tfloat f = 3.14\n\tio.print("{a / 4.0} {a + f}")\n'
assert_output $'314.1592 1234568.0 123.456\n' 'float は有効数字7桁' 'int main();\n\tfloat p = 3.141592\n\tfloat a = 100 * p\n\tfloat b = 1234567.8\n\tfloat c = 123.456\n\tio.print("{a} {b} {c}")\n'
assert_output $'16777216.0\n' 'float の精度' 'int main();\n\tfloat f = 16777217\n\tio.print("{f}")\n'
assert_output $'true false\n' 'bool' 'int main();\n\tbool b = true\n\tio.print("{b} {false}")\n'
assert_output $'Hello, World!\n' 'str の連結と io.print(str)' "int main();\n\tstr a = \"Hello, \"\n\tstr b = 'World!'\n\tio.print(a + b)\n"
assert_output $'[abcdef]\n' 'str の再代入' 'int main();\n\tstr s\n\ts = s + "abc" + "" + "def"\n\tio.print("[{s}]")\n'
assert_output $'0 0.0 false []\n' '初期値の省略' 'int main();\n\tint i\n\tdouble d\n\tbool b\n\tstr s\n\tio.print("{i} {d} {b} [{s}]")\n'
assert_output $'-3.14 -0.3 -0.0\n' '小数の符号反転' 'int main();\n\tfloat f = 3.14\n\tdouble d = 0.3\n\tio.print("{-f} {-d} {-0.0}")\n'
assert_output $'1.234568e+12 1.234e-5 10.0\n' '指数表記と丸め' 'int main();\n\tio.print("{1234567890123.0} {0.00001234} {9.9999999}")\n'
assert_output $'inf -inf nan\n' '無限大と NaN' 'int main();\n\tdouble z = 0.0\n\tdouble inf = 1.0 / z\n\tio.print("{inf} {-inf} {z / z}")\n'
assert 15 '15'
printf 'int main();\n\tint a = 3\n\ta = a * 5\n\treturn a\n' > tmp.qy
./qyc tmp.qy && ./tmp
if [ "$?" = "15" ]; then echo "OK  代入と return"; pass=$((pass + 1)); else echo "NG  代入と return"; exit 1; fi

expected_version=$'QYNO version 0.27.5 (PROTOTYPE)\nPlease note that this product is a prototype. No warranties of any kind exist,\nincluding implied warranties of merchantability or fitness for a particular purpose.'
if [ "$(./qyc --version)" = "$expected_version" ]; then echo "OK  --version"; pass=$((pass + 1)); else echo "NG  --version"; exit 1; fi

echo "--- 比較・論理演算・if・const ---"
assert_output $'true false true true false true false\n' '比較演算子' 'int main();\n\tio.print("{1 < 2} {2 < 1} {3 <= 3} {4 > 3} {4 >= 5} {5 == 5} {5 != 5}")\n'
assert_output $'true false true false\n' '連鎖比較' 'int main();\n\tint x = 5\n\tio.print("{0 <= x < 10} {0 <= x < 5} {1 < 2 < 3 < 4} {1 < 3 < 2}")\n'
assert_output $'true true true true\n' '型の違う比較' 'int main();\n\tint x = 5\n\tfloat f = 0.1\n\tdouble d = 0.1\n\tio.print("{f == 0.1} {d == 0.1} {x > 4.5} {f < d + 1}")\n'
assert_output $'false true false false\n' 'NaN の比較' 'int main();\n\tdouble z = 0.0\n\tdouble n = z / z\n\tio.print("{n == n} {n != n} {n < 1.0} {n >= 1.0}")\n'
assert_output $'true false true\n' '文字列と bool の比較' 'int main();\n\tstr s = "abc"\n\tstr t = "ab" + "c"\n\tio.print("{s == t} {s != t} {true != false}")\n'
assert_output $'false true false true true\n' '論理演算子' 'int main();\n\tio.print("{true and false} {true or false} {not true} {!false} {true and !false or false}")\n'
assert_output $'false true false\n' '短絡評価' 'int main();\n\tint z = 0\n\tio.print("{false and 1 / z == 0} {true or 1 / z == 0} {2 < 1 < 1 / z}")\n'
assert_output $'b\n' 'else if' 'int main();\n\tint x = 5\n\tif(x < 3);\n\t\tio.print("a")\n\telse if(x < 10);\n\t\tio.print("b")\n\telse;\n\t\tio.print("c")\n'
assert_output $'c\n' 'else' 'int main();\n\tint x = 50\n\tif(x < 3);\n\t\tio.print("a")\n\telse if(x < 10);\n\t\tio.print("b")\n\telse;\n\t\tio.print("c")\n'
assert_output $'in\nafter\n' 'else なし' 'int main();\n\tif(1 == 1);\n\t\tio.print("in")\n\tif(1 == 2);\n\t\tio.print("no")\n\tio.print("after")\n'
assert_output $'inner 10\n' 'ネストした if とブロック内の変数' 'int main();\n\tint x = 5\n\tif(x > 0);\n\t\tif(x > 1);\n\t\t\tint y = x * 2\n\t\t\tio.print("inner {y}")\n'
assert_output $'3.141592 20 Hello, QYNO true\n' 'const 定数' 'const float PI = 3.141592\nconst int LIMIT = 10 * 2\nconst str GREETING = "Hello, " + "QYNO"\nconst bool DEBUG = LIMIT > 15 and not false\nint main();\n\tio.print("{PI} {LIMIT} {GREETING} {DEBUG}")\n'
assert_output $'6.28\n' 'main 内の const' 'const float PI = 3.14\nint main();\n\tconst float TAU = PI * 2\n\tio.print("{TAU}")\n'
printf 'int main();\n\tint x = 5\n\tif(0 <= x <= 5);\n\t\treturn 42\n\treturn 0\n' > tmp.qy
./qyc tmp.qy && ./tmp
if [ "$?" = "42" ]; then echo "OK  if の中の return"; pass=$((pass + 1)); else echo "NG  if の中の return"; exit 1; fi

echo "--- for・while ---"
assert_output $'0\n1\n2\n' 'for（< で増やす）' 'int main();\n\tfor(0 <= i < 3, i++);\n\t\tio.print("{i}")\n'
assert_output $'0\n5\n10\n' 'for（<= と += 5）' 'int main();\n\tfor(0 <= i <= 10, i += 5);\n\t\tio.print("{i}")\n'
assert_output $'10\n7\n4\n1\n' 'for（減らす）' 'int main();\n\tfor(10 >= i > 0, i -= 3);\n\t\tio.print("{i}")\n'
assert_output $'3\n2\n1\n' 'for（i--）' 'int main();\n\tfor(3 >= i >= 1, i--);\n\t\tio.print("{i}")\n'
assert_output $'0.0\n0.25\n0.5\n0.75\n' 'for（float のループ変数）' 'int main();\n\tfor(0.0 <= float x < 1.0, x += 0.25);\n\t\tio.print("{x}")\n'
assert_output $'\n' 'for（1回も回らない）' 'int main();\n\tfor(5 <= i < 5, i++);\n\t\tio.print("{i}")\n\tio.print()\n'
assert_output $'count = 5\n' 'for の終了値は毎周評価' 'int main();\n\tint limit = 3\n\tint count = 0\n\tfor(0 <= j < limit, j++);\n\t\tlimit = 5\n\t\tcount++\n\tio.print("count = {count}")\n'
assert_output $'0\n1\n0\n1\n' 'ループ変数名の再利用' 'int main();\n\tfor(0 <= i < 2, i++);\n\t\tio.print("{i}")\n\tfor(0 <= i < 2, i++);\n\t\tio.print("{i}")\n'
assert_output $'sum = 5050\n' 'while' 'int main();\n\tint sum = 0\n\tint n = 1\n\twhile(n <= 100);\n\t\tsum += n\n\t\tn++\n\tio.print("sum = {sum}")\n'
assert_output $'1\n2\n4\n5\n' 'break と continue' 'int main();\n\tint k = 0\n\twhile(true);\n\t\tk++\n\t\tif(k == 3);\n\t\t\tcontinue\n\t\tif(k > 5);\n\t\t\tbreak\n\t\tio.print("{k}")\n'
assert_output $'1,1\n1,3\n2,1\n2,3\n' '入れ子のループ' 'int main();\n\tfor(1 <= a <= 3, a++);\n\t\tfor(1 <= b <= 3, b++);\n\t\t\tif(b == 2);\n\t\t\t\tcontinue\n\t\t\tif(a == 3);\n\t\t\t\tbreak\n\t\t\tio.print("{a},{b}")\n'
assert_output $'ababab\n' '+= による文字列の連結' 'int main();\n\tstr s = ""\n\tfor(0 <= c < 3, c++);\n\t\ts += "ab"\n\tio.print(s)\n'
assert_output $'6 4 2.5\n' '++ -- += -=' 'int main();\n\tint a = 5\n\ta++\n\tint b = 5\n\tb--\n\tdouble d = 1.0\n\td += 2\n\td -= 0.5\n\tio.print("{a} {b} {d}")\n'
assert_output $'2 4 6 8\n' 'const の増分' 'const int STEP = 2\nint main();\n\tstr s = ""\n\tfor(2 <= i <= 8, i += STEP);\n\t\tif(i > 2);\n\t\t\ts += " "\n\t\tif(i == 2);\n\t\t\ts += "2"\n\t\telse if(i == 4);\n\t\t\ts += "4"\n\t\telse if(i == 6);\n\t\t\ts += "6"\n\t\telse;\n\t\t\ts += "8"\n\tio.print(s)\n'

echo "--- % と ^ ---"
assert_output $'1 -1 1 -1 0 0\n' '% の余り（符号は割られる数と同じ）' 'int main();\n\tio.print("{7 % 3} {-7 % 3} {7 % -3} {-7 % -3} {10 % 5} {0 % 7}")\n'
assert_output $'2 5 7\n' '% の優先順位' 'int main();\n\tio.print("{2 * 3 % 4} {2 + 3 % 4} {10 - 2^3 % 5}")\n'
assert_output $'100 1024 1 1\n' '^ の基本' 'int main();\n\tint a = 10^2\n\tio.print("{a} {2^10} {3^0} {0^0}")\n'
assert_output $'512 -4 4 -8 12\n' '^ の右結合と単項マイナス' 'int main();\n\tio.print("{2^3^2} {-2^2} {(-2)^2} {(-2)^3} {2^2 * 3}")\n'
assert_output $'1 -1 1\n' 'int の負の指数' 'int main();\n\tio.print("{1^-5} {(-1)^-3} {(-1)^-4}")\n'
assert_output $'4611686018427387904 -9223372036854775808 0\n' 'int_64 の累乗の桁あふれ' 'int main();\n\tio.print("{int_64(2)^62} {int_64(2)^63} {int_64(2)^64}")\n'
assert_output $'1024.0 6.25 0.25 0.001\n' 'double の累乗' 'int main();\n\tio.print("{2.0^10} {2.5^2} {2.0^-2} {10.0^-3}")\n'
assert_output $'2.25 0.666667\n' 'float の累乗' 'int main();\n\tfloat f = 1.5\n\tio.print("{f^2} {f^-1}")\n'
assert_output $'144 0.5 2\n' 'const の % と ^' 'const int SQ = 12^2\nconst double HALF = 2.0^-1\nconst int R = 17 % 5\nint main();\n\tio.print("{SQ} {HALF} {R}")\n'
assert_output $'1\n2\nFizz\n4\nBuzz\nFizz\n7\n8\nFizz\nBuzz\n11\nFizz\n13\n14\nFizzBuzz\n' '% を使った FizzBuzz' 'int main();\n\tfor(1 <= i <= 15, i++);\n\t\tif(i % 15 == 0);\n\t\t\tio.print("FizzBuzz")\n\t\telse if(i % 3 == 0);\n\t\t\tio.print("Fizz")\n\t\telse if(i % 5 == 0);\n\t\t\tio.print("Buzz")\n\t\telse;\n\t\t\tio.print("{i}")\n'

echo "--- 関数 ---"
assert_output $'10.0\n25.0\n' '数式形式の関数' 'double f(double x) = x^2 + 1\ndouble h(double x, double y) = x^2 + y^2\nint main();\n\tio.print(f(3))\n\tio.print(h(3.0, 4.0))\n'
assert_output $'3628800\n6765\n' 'ブロック形式と再帰' 'int fact(int n);\n\tif(n <= 1);\n\t\treturn 1\n\treturn n * fact(n - 1)\nint fib(int n);\n\tif(n < 2);\n\t\treturn n\n\telse;\n\t\treturn fib(n - 1) + fib(n - 2)\nint main();\n\tio.print(fact(10))\n\tio.print(fib(20))\n'
assert_output $'9\n' 'main より後ろの関数' 'int main();\n\tio.print(max3(3, 9, 5))\nint max3(int a, int b, int c);\n\tint m = a\n\tif(b > m);\n\t\tm = b\n\tif(c > m);\n\t\tm = c\n\treturn m\n'
assert_output $'hi QYNO\nend\n' 'void の関数と値のない return' 'void greet(str name);\n\tio.print("hi {name}")\n\tif(name == "QYNO");\n\t\treturn\n\tio.print("not reached")\nint main();\n\tgreet("QYNO")\n\tio.print("end")\n'
assert_output $'314.1592\n' '関数から const を参照' 'float area(int r) = r * r * PI\nconst float PI = 3.141592\nint main();\n\tio.print(area(10))\n'
assert_output $'2.5\n' '引数の型変換（int から double）' 'double halve(double x) = x / 2\nint main();\n\tio.print(halve(5))\n'
assert_output $'abab\n' 'str を返す関数' 'str twice(str s) = s + s\nint main();\n\tio.print(twice("ab"))\n'
assert_output $'7\n' '引数への代入は呼び出し側に影響しない' 'int inc(int x);\n\tx += 1\n\treturn x\nint main();\n\tint a = 6\n\tio.print(inc(a))\n\ta = a\n'
assert_output $'inside\n' 'while(true) の中の return' 'int f();\n\twhile(true);\n\t\treturn 1\nint main();\n\tif(f() == 1);\n\t\tio.print("inside")\n'
printf 'int sq(int x) = x * x\nint main();\n\treturn sq(9) - 70\n' > tmp.qy
./qyc tmp.qy && ./tmp
if [ "$?" = "11" ]; then echo "OK  関数の結果を return"; pass=$((pass + 1)); else echo "NG  関数の結果を return"; exit 1; fi

echo "--- print の新しい書き方 ---"
assert_output $'42\n38\n3.14\ntrue\n' 'io.print(式)' 'const int num = 38\nint main();\n\tio.print(num + 4)\n\tio.print(num)\n\tio.print(3.14)\n\tio.print(1 < 2)\n'
assert_output $'12 3.5100true\n' 'end=' 'int main();\n\tio.print(1, end="")\n\tio.print(2, end=" ")\n\tio.print(3.5, end=100)\n\tio.print(true, end="\\n")\n'
assert_output $'a2\n' 'end= に埋め込み' 'int main();\n\tio.print("a", end="{1 + 1}\\n")\n'
assert_output $'<end>\n' 'end= だけ' 'int main();\n\tio.print(end="<end>")\n\tio.print()\n'
assert_output $'x = 5, y = 6\n' 'print の {式} も従来どおり' 'int main();\n\tint x = 5\n\tio.print("x = {x}, y = {x + 1}")\n'

echo "--- リテラルだけの式を float に ---"
assert_output $'314.0 2.0\n' 'float a = 100 * 3.14' 'int main();\n\tfloat a = 100 * 3.14\n\tfloat b = 2.0 ^ 3 / 4\n\tio.print("{a} {b}")\n'

echo "--- 数学関数・型変換・絶対値・using ---"
assert_output $'4.5\n4.5\n12\n2.5\n1.5\n' '絶対値 |x|' 'int main();\n\tdouble x = 3.0\n\tdouble y = 7.5\n\tio.print(|x - y|)\n\tio.print(||x| - |y||)\n\tio.print(|-3| * |4|)\n\tio.print(|-2.5|)\n\tfloat f = -1.5\n\tio.print(|f|)\n'
assert_output $'1.414214 4.0 1.414214\n' 'math.sqrt' 'int main();\n\tio.print("{math.sqrt(2.0)} {math.sqrt(16)} ", end="")\n\tfloat f = 2.0\n\tio.print(math.sqrt(f))\n'
assert_output $'3.0 2.0 -3.0 -2.0\n' 'math.round（絶対値で四捨五入）' 'int main();\n\tio.print("{math.round(2.5)} {math.round(2.3)} {math.round(-2.5)} {math.round(-2.3)}")\n'
assert_output $'3.0 3.0 -2.0 -2.0\n' 'math.roundup（天井関数）' 'int main();\n\tio.print("{math.roundup(2.5)} {math.roundup(2.3)} {math.roundup(-2.5)} {math.roundup(-2.3)}")\n'
assert_output $'2.0 2.0 -3.0 -3.0\n' 'math.rounddown（床関数）' 'int main();\n\tio.print("{math.rounddown(2.5)} {math.rounddown(2.3)} {math.rounddown(-2.5)} {math.rounddown(-2.3)}")\n'
assert_output $'3.14 1200.0 1.24 -1.24 1300\n' '桁数の指定' 'int main();\n\tio.print("{math.round(3.14159, 2)} {math.round(1234.5, -2)} {math.roundup(1.231, 2)} {math.rounddown(-1.239, 2)} {math.round(1250, -2)}")\n'
assert_output $'2 2 -2 -2 3 3\n' 'int(x) は 0 の方向へ切り捨て' 'int main();\n\tio.print("{int(2.5)} {int(2.3)} {int(-2.5)} {int(-2.3)} {int(3.7)} {int(math.round(2.5))}")\n'
assert_output $'3.5 2.5\n' 'float(x)・double(x)' 'int main();\n\tio.print("{double(7) / 2} {float(2.5)}")\n'
assert_output $'5 2.5\n' 'math.abs（推奨しない別名）' 'int main();\n\tio.print("{math.abs(-5)} {math.abs(-2.5)}")\n'
assert_output $'1.414214 3.0\n' 'using（関数の外）' 'using math.sqrt, math.round\nint main();\n\tio.print("{sqrt(2.0)} {round(2.5)}")\n'
assert_output $'1.0 5.0\n' 'using math（すべて）と関数の中の using' 'int main();\n\tusing math\n\tio.print("{roundup(0.1)} {hyp(3.0, 4.0)}")\ndouble hyp(double a, double b) = math.sqrt(a^2 + b^2)\n'
assert_output $'5.0\n' '数式形式の関数で数学関数' 'using math.sqrt\ndouble hyp(double a, double b) = sqrt(a^2 + b^2)\nint main();\n\tio.print(hyp(3.0, 4.0))\n'
assert_output $'1.41\n' 'const で数学関数' 'const double R2 = math.round(math.sqrt(2.0), 2)\nint main();\n\tio.print(R2)\n'
printf 'int main();\n\tdouble big = 10.0 ^ 20\n\tio.print("before")\n\tint n = int(big)\n\tio.print("after")\n' > tmp.qy
./qyc tmp.qy || exit 1
out="$(./tmp 2> /dev/null)"
code="$?"
if [ "$out" = "before" ] && [ "$code" = "70" ]; then echo "OK  int() の範囲外は実行時エラー（終了コード 70）"; pass=$((pass + 1)); else echo "NG  int() の範囲外の実行時エラー（$out / $code）"; exit 1; fi

echo "--- キー入力（00.22） ---"
EX_PROG='int main();\n\tstr user_name\n\tint user_id\n\tuser_id = io.input("入力> ")\n\tuser_name = io.input()\n\tio.print("出力> {user_id}, {user_name}")\n\tio.input("id, nameの順で入力> {user_id}, {user_name}")\n\tio.print(user_id)\n\tio.print(user_name)\n'
assert_input $'入力> 出力> 12345, Kenta\nid, nameの順で入力> 54321\nMary\n' 0 '値の形と形に当てはめる形' "$EX_PROG" '12345\nKenta\n54321, Mary\n'
assert_input $'入力> 出力> 12345, Kenta\nid, nameの順で入力> 54321\nMary   Ann\n' 0 '空白・CRLF・区切りの空白なし' "$EX_PROG" '  12345  \r\nKenta\r\n54321,Mary   Ann  \r\n'
assert_input $'-42 3.14 0.1 true [  そのまま  の文字列 ]\n' 0 '代入先の型で読む' 'int main();\n\tint a = io.input()\n\tdouble d = io.input()\n\tfloat f = io.input()\n\tbool b = io.input()\n\tstr s = io.input()\n\tio.print("{a} {d} {f} {b} [{s}]")\n' '-42\n3.14\n0.1\ntrue\n  そのまま  の文字列 \n'
assert_input $'30\n山田 太郎 は 25 歳\n' 0 '空白区切りと日本語の区切り' 'int main();\n\tint x\n\tint y\n\tio.input("{x} {y}")\n\tio.print(x + y)\n\tstr name\n\tint age\n\tio.input("{name}／{age}")\n\tio.print("{name} は {age} 歳")\n' '10   20\n山田 太郎 ／ 25\n'
assert_input $'整数> 整数で入力してください\n整数> 整数で入力してください\n整数> a = 7\n' 0 'while で聞き直す' 'int main();\n\tint a\n\twhile(not io.input("整数> {a}"));\n\t\tio.print("整数で入力してください")\n\tio.print("a = {a}")\n' 'abc\n3.5\n7\n'
assert_input $'ng 7 8\n' 0 '失敗しても変数は変わらない' 'int main();\n\tint x = 7\n\tint y = 8\n\tif(io.input("{x}, {y}"));\n\t\tio.print("ok")\n\telse;\n\t\tio.print("ng {x} {y}")\n' '1, 2, 3\n'
assert_input $'true\n' 0 '読み込んだ小数はリテラルと同じ値' 'int main();\n\tdouble d = io.input()\n\tdouble e = io.input()\n\tio.print(d == 0.1 and e == 22727350723955.130859375)\n' '0.1\n22727350723955.130859375\n'
assert_input $'-9223372036854775808\n' 0 'int_64 の最小値' 'int main();\n\tint_64 a = io.input()\n\tio.print(a)\n' '-9223372036854775808\n'
assert_input '' 65 '形に合わない入力は終了コード 65' 'int main();\n\tint a\n\tstr b\n\tio.input("{a}, {b}")\n' '54321 Mary\n'
assert_input '' 65 'int として読めない入力は終了コード 65' 'int main();\n\tint a = io.input()\n' '12個\n'
assert_input '' 65 '範囲外の入力は終了コード 65' 'int main();\n\tint a = io.input()\n' '99999999999999999999\n'
assert_input $'false\n' 0 '条件の形は入力の終わりで false' 'int main();\n\tint a\n\tio.print(io.input("{a}"))\n' ''
assert_input '' 65 '値の形は入力の終わりで終了コード 65' 'int main();\n\tint a = io.input()\n' ''
printf 'int main();\n\tint a\n\tstr b\n\tio.input("id, nameの順で入力> {a}, {b}")\n' > tmp.qy
./qyc tmp.qy || exit 1
msg="$(printf '54321 Mary\n' | ./tmp 2>&1 > /dev/null)"
expected_msg=$'QYNO 実行時エラー: tmp.qy:4:2: 入力を読み取れませんでした\n\t4 | \tio.input("id, nameの順で入力> {a}, {b}")\n\t入力 | 54321 Mary\n\t            ^ ここに「,」が必要です'
if [ "$msg" = "$expected_msg" ]; then echo "OK  入力の誤りのメッセージ"; pass=$((pass + 1)); else echo "NG  入力の誤りのメッセージ"; printf '%s\n' "$msg"; exit 1; fi
assert_input '' 70 '0 で割ると終了コード 70' 'int main();\n\tint z = io.input()\n\tio.print(10 / z)\n' '0\n'

echo "--- 枠付きの入力（00.23.1） ---"
assert_input $'座標> (1.5 -2.25\n' 0 '枠付きの形（パイプでは右の枠を出さず、内側の形を読む）' 'int main();\n\tdouble p\n\tdouble q\n\tio.input("座標> ({p}, {q})")\n\tio.print("{p} {q}")\n' '1.5, -2.25\n'
assert_input $'年齢> 20 歳\n' 0 '右の枠が単位' 'int main();\n\tint age\n\tio.input("年齢> {age} 歳")\n\tio.print("{age} 歳")\n' '20\n'
assert_input $'名前> 「[山田 太郎]\n' 0 '最後の str の欄と右の枠' 'int main();\n\tstr s\n\tio.input("名前> 「{s}」")\n\tio.print("[{s}]")\n' '山田 太郎\n'
assert_input '座標> (' 65 '内側の形に合わなければ終了コード 65' 'int main();\n\tdouble p\n\tdouble q\n\tio.input("座標> ({p}, {q})")\n' '(1.5, 2)\n'

echo "--- 値渡しと参照（00.24.0） ---"
assert_output $'10 10\nnum=40 p=40\nnum=42\ntrue true\np=99 num=42\nother=100\n' '.value・.addr・ref の変数' 'int main();\n\tint num = 10\n\tint other = 99\n\tio.print("{num.value} {num}")\n\tref int p = num.addr\n\tp = 40\n\tio.print("num={num} p={p.value}")\n\tp.value += 2\n\tio.print("num={num}")\n\tio.print("{p.addr == num.addr} {p.addr != other.addr}")\n\tp.addr = other.addr\n\tio.print("p={p} num={num}")\n\tp++\n\tio.print("other={other}")\n'
assert_output $'true\nother=5\n' 'none と指す先の変更' 'int main();\n\tint other = 1\n\tref int q\n\tio.print(q.addr == none)\n\tq.addr = other.addr\n\tq = 5\n\tio.print("other={other}")\n'
assert_output $'num=5 other=42\n106\n' 'ref の引数（入れ替えと連鎖）' 'int main();\n\tint num = 42\n\tint other = 5\n\tswap(ref num, ref other)\n\tio.print("num={num} other={other}")\n\tinc_all(ref num)\n\tio.print(num)\nvoid swap(ref int a, ref int b);\n\tint t = a\n\ta = b\n\tb = t\nvoid inc_all(ref int x);\n\tx++\n\tbump(ref x)\nvoid bump(ref int y);\n\ty += 100\n'
assert_output $'none\n' '何も指していない場所の表示' 'int main();\n\tref int p\n\tio.print(p.addr)\n'
assert_input $'a 山田\nb 山田\nc 平林\nd 高橋\n' 0 '値渡し・val・ref・ref の変数（input と組み合わせ）' 'int main();\n\tstr n = "山田"\n\tfa(n)\n\tio.print("a {n}")\n\tfb(val n)\n\tio.print("b {n}")\n\tfc(ref n)\n\tio.print("c {n}")\n\tref str p = n.addr\n\tfc(ref p)\n\tio.print("d {n}")\nvoid fa(str s);\n\tio.input("{s}")\nvoid fb(val str s);\n\tio.input("{s}")\nvoid fc(ref str s);\n\tio.input("{s.value}")\n' '佐藤\n鈴木\n平林\n高橋\n'
assert_input '' 70 '何も指していない ref の読み書きは終了コード 70' 'int main();\n\tref int p\n\tp = 3\n' ''

echo "--- 行の編集（00.23.0。Linux で python3 があるときだけ） ---"
if [ "$(uname)" = "Linux" ] && command -v "${PYTHON:-python3}" > /dev/null 2>&1; then
    le_out="$("${PYTHON:-python3}" tests/lineedit_test.py ./qyc)" || { printf '%s\n' "$le_out"; exit 1; }
    printf '%s\n' "$le_out" | grep -v "行の編集のテスト"
    pass=$((pass + $(printf '%s\n' "$le_out" | grep -c '^OK')))
fi

echo "--- 全機能テスト（tests/all_features.qy） ---"
./qyc tests/all_features.qy -o tmp || exit 1
./tmp > /dev/null
result="$?"
if [ "$result" = "0" ]; then
    echo "OK  tests/all_features.qy の自己検証がすべて成功"
    pass=$((pass + 1))
else
    echo "NG  tests/all_features.qy で $result 件失敗"
    ./tmp | grep "NG"
    exit 1
fi

echo "--- 出力ファイル ---"
printf 'int main();\n\treturn 5\n' > tmp.qy
rm -f tmp tmp.exe tmp.s
./qyc tmp.qy || exit 1
if [ ! -e tmp ] && [ ! -e tmp.exe ]; then
    echo "NG  実行ファイルが作られていません"
    exit 1
fi
if [ -e tmp.s ]; then
    echo "NG  不要なアセンブリが残っています"
    exit 1
fi
echo "OK  実行ファイルを直接作成"
pass=$((pass + 1))

./qyc tmp.qy -o tmp_named || exit 1
./tmp_named
if [ "$?" != "5" ]; then
    echo "NG  -o で指定した実行ファイル"
    exit 1
fi
echo "OK  -o で出力名を指定"
pass=$((pass + 1))
rm -f tmp_named tmp_named.exe

./qyc tmp.qy -S || exit 1
if [ ! -e tmp.s ]; then
    echo "NG  -S でアセンブリが出力されていません"
    exit 1
fi
echo "OK  -S でアセンブリのみ出力"
pass=$((pass + 1))

# Linux 上で wine があれば、Windows 向けの .exe も確認する
if [ "$(uname -s)" = "Linux" ] && command -v wine > /dev/null 2>&1; then
    echo "--- Windows 向け（wine で実行） ---"
    for pair in '75:10*10-5*5' '4:(3+5)/2' '10:-10+20'; do
        expected="${pair%%:*}"
        input="${pair#*:}"
        printf 'int main();\n\treturn %s\n' "$input" > tmp.qy
        ./qyc tmp.qy --target windows -o tmp.exe || exit 1
        WINEDEBUG=-all wine tmp.exe > /dev/null 2>&1
        actual="$?"
        if [ "$actual" = "$expected" ]; then
            echo "OK  $input => $actual"
            pass=$((pass + 1))
        else
            echo "NG  $input => 期待値 $expected, 実際 $actual"
            exit 1
        fi
    done
    printf 'int main();\n\tio.print("Hello {1 + 2}")\n\tio.print("日本語")\n\tfloat f = 3.14\n\tdouble d = 0.1 + 0.2\n\tstr s = "a" + "b"\n\tio.print("{f} {d} {true} {s}")\n\tif(0 <= f < 5 and s == "ab");\n\t\tio.print("if OK")\n\tint t = 0\n\tfor(1 <= i <= 10, i++);\n\t\tt += i\n\tio.print("{t}")\n\tio.print("{17 %% 5} {2^10} {1.5^2}")\n\tio.print(sq(12), end="")\n\tio.print("!")\n' > tmp.qy
    printf 'int sq(int x) = x * x\n' >> tmp.qy
    ./qyc tmp.qy --target windows -o tmp.exe || exit 1
    actual="$(WINEDEBUG=-all wine tmp.exe 2> /dev/null; printf x)"
    actual="${actual%x}"
    if [ "$actual" = $'Hello 3\n日本語\n3.14 0.3 true ab\nif OK\n55\n2 1024 2.25\n144!\n' ]; then
        echo "OK  print"
        pass=$((pass + 1))
    else
        echo "NG  print => $actual"
        exit 1
    fi
    # 入力（パイプから与える。改行は CRLF）
    printf 'int main();\n\tstr user_name\n\tint user_id\n\tuser_id = io.input("入力> ")\n\tuser_name = io.input()\n\tio.input("> {user_id}, {user_name}")\n\tio.print("{user_id} {user_name}")\n\tdouble d = io.input()\n\tio.print(d == 0.1)\n' > tmp.qy
    ./qyc tmp.qy --target windows -o tmp.exe || exit 1
    actual="$(printf '12345\r\nKenta\r\n54321, Mary Ann\r\n0.1\r\n' | WINEDEBUG=-all wine tmp.exe 2> /dev/null; printf x)"
    actual="${actual%x}"
    expected=$'入力> > 54321 Mary Ann\ntrue\n'
    if [ "$actual" = "$expected" ]; then
        echo "OK  input（Windows 向け）"
        pass=$((pass + 1))
    else
        echo "NG  input（Windows 向け）"
        printf '%s' "$actual" | od -c | head -5
        exit 1
    fi
    printf 'int main();\n\tint a = io.input()\n' > tmp.qy
    ./qyc tmp.qy --target windows -o tmp.exe || exit 1
    printf 'abc\r\n' | WINEDEBUG=-all wine tmp.exe > /dev/null 2>&1
    code="$?"
    if [ "$code" = "65" ]; then
        echo "OK  input の誤りは終了コード 65（Windows 向け）"
        pass=$((pass + 1))
    else
        echo "NG  input の誤りの終了コード（Windows 向け）：$code"
        exit 1
    fi
fi

echo "--- コンパイルエラー ---"
assert_error 'スペースでのインデント' 'int main();\n    return 1\n'
assert_error '0で割る' 'int main();\n\treturn 1/0\n'
assert_error '本体のインデントなし' 'int main();\nreturn 1\n'
assert_error 'インデントが深すぎる' 'int main();\n\t\treturn 1\n'
assert_error '; の後に文を続ける' 'int main(); return 1\n'
assert_error '閉じ括弧なし' 'int main();\n\treturn (1+2\n'
assert_error '使えない文字' 'int main();\n\treturn 1 @ 2\n'

assert_error '閉じていない文字列' 'int main();\n\tio.print("abc)\n'
assert_error '使えないエスケープ' 'int main();\n\tio.print("\\q")\n'
assert_error '閉じていない {' 'int main();\n\tio.print("{1 + 2")\n'
assert_error '空の {}' 'int main();\n\tio.print("{}")\n'
assert_error '書式指定' 'int main();\n\tio.print("{1:.1}")\n'
assert_error '埋め込みの式の誤り' 'int main();\n\tio.print("{1 +}")\n'

assert_error 'double を int に' 'int main();\n\tint a = 1.5\n'
assert_error 'double の変数を float に' 'int main();\n\tdouble d = 2.5\n\tfloat f = d\n'
assert_error 'int を bool に' 'int main();\n\tbool b = 1\n'
assert_error 'str と int の連結' 'int main();\n\tstr s = "a" + 1\n'
assert_error 'bool の計算' 'int main();\n\tint x = true + 1\n'
assert_error 'return に double' 'int main();\n\treturn 1.5\n'
assert_error '変数の二重宣言' 'int main();\n\tint a = 1\n\tint a = 2\n'
assert_error '未宣言の変数の複合代入' 'int main();\n\tb += 1\n'
assert_error '予約語を変数名に' 'int main();\n\tint int = 1\n'
assert_error '式の中の埋め込み' 'int main();\n\tstr s = "x{1}"\n'
assert_error '初期値での自己参照' 'int main();\n\tint a = a\n'
assert_error '小数の後に英字' 'int main();\n\tdouble d = 1.5e3x\n'

assert_error 'if の条件が int' 'int main();\n\tif(1);\n\t\tio.print("x")\n'
assert_error 'int と str の比較' 'int main();\n\tint a = 1\n\tstr s = "a"\n\tbool b = a < s\n'
assert_error 'bool の大小比較' 'int main();\n\tbool b = true < false\n'
assert_error 'str の大小比較' 'int main();\n\tstr a = "a"\n\tbool b = a < a\n'
assert_error 'and に int' 'int main();\n\tbool b = 1 and 2\n'
assert_error 'not に int' 'int main();\n\tbool b = not 1\n'
assert_error 'const への代入' 'const int A = 1\nint main();\n\tA = 2\n'
assert_error 'const の初期値なし' 'int main();\n\tconst int A\n'
assert_error 'const の初期値に変数' 'int main();\n\tint x = 1\n\tconst int A = x\n'
assert_error 'const の計算で 0 除算' 'const int A = 1\nconst int B = 10 / (A - 1)\nint main();\n\treturn 0\n'
assert_error 'ブロックの外から変数' 'int main();\n\tif(true);\n\t\tint y = 1\n\tio.print("{y}")\n'
assert_output $'2\n1\n' '内側の明示的な同名宣言' 'int main();\n\tint x = 1\n\tif(true);\n\t\tint x = 2\n\t\tio.print(x)\n\tio.print(x)\n'
assert_error 'if のない else' 'int main();\n\telse;\n\t\treturn 1\n'
assert_error 'if の括弧なし' 'int main();\n\tif true;\n\t\treturn 1\n'
assert_error 'if の ; なし' 'int main();\n\tif(true)\n\t\treturn 1\n'
assert_error 'ファイル先頭に文' 'io.print("x")\nint main();\n\treturn 0\n'

assert_output $'1\n2\n3\n4\n' 'for の開始側の開区間' 'int main();\n\tfor(0 < i < 5, i++);\n\t\tio.print(i)\n'
assert_error 'for の向きの混在' 'int main();\n\tfor(0 <= i > 5, i++);\n\t\tio.print("x")\n'
assert_error 'for の増分の向き違い' 'int main();\n\tfor(0 <= i < 5, i--);\n\t\tio.print("x")\n'
assert_error 'for の減らす範囲で増やす' 'int main();\n\tfor(10 >= i > 0, i += 1);\n\t\tio.print("x")\n'
assert_error 'for の増分 0' 'int main();\n\tfor(0 <= i < 5, i += 0);\n\t\tio.print("x")\n'
assert_error 'for の増分が変数' 'int main();\n\tint s = 1\n\tfor(0 <= i < 5, i += s);\n\t\tio.print("x")\n'
assert_error 'for の増分が別の変数' 'int main();\n\tint j = 0\n\tfor(0 <= i < 5, j++);\n\t\tio.print("x")\n'
assert_error 'for の外でループ変数' 'int main();\n\tfor(0 <= i < 5, i++);\n\t\tio.print("x")\n\tio.print("{i}")\n'
assert_error 'ループの外の break' 'int main();\n\tbreak\n'
assert_error 'break に括弧（00.21 で廃止）' 'int main();\n\twhile(true);\n\t\tbreak()\n'
assert_error 'while の条件が int' 'int main();\n\twhile(1);\n\t\tbreak\n'
assert_error 'bool の ++' 'int main();\n\tbool b = true\n\tb++\n'
assert_error 'const の +=' 'const int C = 1\nint main();\n\tC += 1\n'

assert_error '% に小数' 'int main();\n\tdouble d = 5.5 % 2\n'
assert_error '^ の指数が小数' 'int main();\n\tint x = 5\n\tdouble d = x ^ 0.5\n'
assert_error '% 0' 'int main();\n\tint x = 5 % 0\n'
assert_error 'const の % で 0' 'const int Z = 0\nconst int A = 5 % Z\nint main();\n\treturn 0\n'
assert_error 'const の 0 の負の累乗' 'const int A = 0 ^ -1\nint main();\n\treturn 0\n'
assert_error '^ の底が str' 'int main();\n\tstr s = "a" ^ 2\n'

assert_error 'main がない' 'int f() = 1\n'
assert_error 'main に引数' 'int main(int x);\n\treturn 0\n'
assert_error '関数の二重定義' 'int f() = 1\nint f() = 2\nint main();\n\treturn 0\n'
assert_error '未定義の関数' 'int main();\n\treturn g(1)\n'
assert_error '引数の数の誤り' 'int f(int x) = x\nint main();\n\treturn f(1, 2)\n'
assert_error '引数の型の誤り' 'int f(int x) = x\nint main();\n\treturn f("a")\n'
assert_error 'void を値として使う' 'void v();\n\tio.print("x")\nint main();\n\tint a = v()\n'
assert_error 'void の関数が値を返す' 'void v();\n\treturn 1\nint main();\n\treturn 0\n'
assert_error 'return のない道筋' 'int f(int x);\n\tif(x > 0);\n\t\treturn 1\nint main();\n\treturn 0\n'
assert_error '値のない return' 'int f(int x);\n\treturn\nint main();\n\treturn 0\n'
assert_error '数式形式の void' 'void v() = 1\nint main();\n\treturn 0\n'
assert_error 'const と同じ名前の関数' 'const int f = 1\nint f() = 2\nint main();\n\treturn 0\n'
assert_error 'const と同じ名前のローカル変数' 'const int N = 1\nint main();\n\tint N = 2\n'
assert_error 'const の循環' 'const int A = B\nconst int B = A\nint main();\n\treturn 0\n'
assert_error 'main の呼び出し' 'int main();\n\treturn main()\n'
assert_error 'print の2つ目が end= でない' 'int main();\n\tio.print(1, 2)\n'
assert_error '引数名の重複' 'int f(int x, int x) = 1\nint main();\n\treturn 0\n'
assert_error 'double の変数を float に' 'int main();\n\tdouble d = 2.5\n\tfloat g = d\n'
assert_error 'const の初期値に関数' 'int f() = 1\nconst int C = f()\nint main();\n\treturn 0\n'
assert_output $'()\n' 'print に void の唯一の値' 'void v();\n\treturn\nint main();\n\tio.print(v())\n'

assert_error '&& は廃止' 'int main();\n\tbool b = true && false\n'
assert_error '|| は廃止' 'int main();\n\tbool b = true || false\n'
assert_error '対応のない |' 'int main();\n\tint x = 1\n\tint y = x | 2\n'
assert_error '閉じていない絶対値' 'int main();\n\tio.print(|3)\n'
assert_error '絶対値の中が str' 'int main();\n\tio.print(|"a"|)\n'
assert_error 'int() に範囲外の定数' 'int main();\n\tint n = int(10.0^20)\n'
assert_error 'int() に NaN の定数' 'const double Z = 0.0\nint main();\n\tint n = int(Z / Z)\n'
assert_error 'math にない関数' 'int main();\n\tio.print(math.cbrt(2.0))\n'
assert_error 'ない名前空間' 'int main();\n\tio.print(maths.sqrt(2.0))\n'
assert_error 'using と同じ名前の関数' 'using math.sqrt\nint main();\n\treturn 0\ndouble sqrt(double x) = x\n'
assert_error 'using math と同じ名前の関数' 'using math\nint main();\n\treturn 0\nint round(int x) = x\n'
assert_error 'using なしの省略' 'int main();\n\tio.print(sqrt(2.0))\n'
assert_error 'using に括弧' 'using math.sqrt(x)\nint main();\n\treturn 0\n'
assert_error '桁数が小数' 'int main();\n\tio.print(math.round(2.5, 1.0))\n'
assert_error '数学関数の引数の数' 'int main();\n\tio.print(math.sqrt(1.0, 2.0))\n'
assert_output $'3\n' 'int() の整数文字列変換' 'int main();\n\tio.print(int("3"))\n'
assert_error 'ブロックの外で using' 'int main();\n\tif(true);\n\t\tusing math.sqrt\n\tio.print(sqrt(2.0))\n'

assert_error 'input の {} が隣り合う' 'int main();\n\tint a\n\tint b\n\tio.input("> {a}{b}")\n'
assert_error 'input の {} に式' 'int main();\n\tint a\n\tio.input("> {a + 1}")\n'
assert_error 'input で定数に読み込む' 'const int C = 1\nint main();\n\tio.input("> {C}")\n'
assert_error 'input のプロンプトが配列' 'int main();\n\tstr s = io.input(prompt=[1])\n'
assert_error 'input の引数が2つ' 'int main();\n\tstr s = io.input("a", "b")\n'
assert_error '形の input の結果を int に' 'int main();\n\tint a\n\tint x = io.input("> {a}")\n'
assert_error '関数の外の input' 'const int K = io.input()\nint main();\n\treturn 0\n'

assert_error 'ref の引数に ref なし' 'int main();\n\tint x = 1\n\tf(x)\nvoid f(ref int a);\n\ta = 2\n'
assert_error '値渡しの引数に ref' 'int main();\n\tint x = 1\n\tf(ref x)\nvoid f(int a);\n\ta = 2\n'
assert_error 'ref にリテラル' 'int main();\n\tf(ref 1)\nvoid f(ref int a);\n\ta = 2\n'
assert_error 'ref に定数' 'const int C = 1\nint main();\n\tf(ref C)\nvoid f(ref int a);\n\ta = 2\n'
assert_error 'ref に繰り返しの変数' 'int main();\n\tfor(0 <= i < 3, i++);\n\t\tf(ref i)\nvoid f(ref int a);\n\ta = 2\n'
assert_error 'ref の型の不一致' 'int main();\n\tdouble d = 1.0\n\tf(ref d)\nvoid f(ref int a);\n\ta = 2\n'
assert_program 12 '同じ変数を2つの ref に' 'int main();\n\tint x = 10\n\tg(ref x, ref x)\n\treturn x\nvoid g(ref int a, ref int b);\n\ta++\n\tb++\n'
assert_error '定数の addr' 'const int C = 1\nint main();\n\tio.print(C.addr)\n'
assert_error '普通の変数の addr への代入' 'int main();\n\tint n = 1\n\tint m = 2\n\tn.addr = m.addr\n'
assert_error 'ref の引数の指す先の変更' 'int main();\n\tint n = 1\n\tf(ref n)\nvoid f(ref int a);\n\tint z = 0\n\ta.addr = z.addr\n'
assert_error 'ref の変数に場所でない初期値' 'int main();\n\tref int p = 5\n'
assert_error 'ref の変数に型の違う場所' 'int main();\n\tdouble d = 1.0\n\tref int p = d.addr\n'
assert_error '内側のブロックの変数を指す' 'int main();\n\tref int p\n\tif(true);\n\t\tint b = 1\n\t\tp.addr = b.addr\n'
assert_error '要素を続けて書く' 'int main();\n\tint n = 1\n\tio.print(n.value.addr)\n'
assert_error '場所を int に' 'int main();\n\tint n = 1\n\tint x = n.addr\n'
assert_error '場所の大小の比較' 'int main();\n\tint n = 1\n\tint m = 1\n\tio.print(n.addr < m.addr)\n'

rm -f tmp tmp.exe tmp.qy tmp.s
echo "すべて成功しました（$pass 件）"
