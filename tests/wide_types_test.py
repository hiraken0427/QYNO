"""128-bit integers and actual binary16/binary128 values across the language."""
import random
import math
import struct
from fractions import Fraction
from decimal import Decimal, localcontext
from support import QynoTestCase, program, run_suite

I_MIN = -(1 << 127)
I_MAX = (1 << 127) - 1
U_MAX = (1 << 128) - 1
PRECISE = '1.0000000000000000000000000000000002'
Q_NEXT = '1.00000000000000000000000000000000019'


def binary128(value):
    """Independent rational round-to-nearest-even oracle, including subnormals."""
    sign = value < 0
    value = abs(value)
    if not value:
        return int(sign) << 127
    n, d = value.numerator, value.denominator
    exponent = n.bit_length() - d.bit_length()
    if (n < d << exponent) if exponent >= 0 else (n << -exponent < d):
        exponent -= 1
    scale = 112 - max(exponent, -16382)
    if scale >= 0: n <<= scale
    else: d <<= -scale
    q, r = divmod(n, d)
    q += r*2 > d or r*2 == d and q & 1
    exponent = max(exponent, -16382)
    if q == 1 << 113: q >>= 1; exponent += 1
    if q >= 1 << 112: q = ((exponent+16383) << 112) | (q - (1 << 112))
    return (int(sign) << 127) | q


def exact_quad(bits):
    m = bits & ((1 << 112)-1)
    e = (bits >> 112) & 32767
    if e: m += 1 << 112
    e = (e or 1)-16383-112
    result = Fraction(m << e) if e >= 0 else Fraction(m, 1 << -e)
    return -result if bits >> 127 else result


class WideTypeTests(QynoTestCase):
    def test_boundaries_defaults_and_decimal_inference(self):
        self.check_program(program('int main();', f'\tint_128 a={I_MIN}', f'\tint_128 b={I_MAX}',
            f'\tunsigned int_128 c={U_MAX}', '\tint_128 d', '\tquadruple q', '\thalf h',
            '\tprint("{a} {b} {c} {d} {q} {h}")', '\tprint(18446744073709551616)',
            '\tprint(-9223372036854775809)'), f'{I_MIN} {I_MAX} {U_MAX} 0 0.0 0.0\n18446744073709551616\n-9223372036854775809\n')

    def test_minimum_parentheses_and_radix_patterns(self):
        self.check_program(program('int main();', f'\tprint(-(+{1 << 127}))',
            '\tint_128 a=0xffffffffffffffffffffffffffffffff', '\tunsigned int_128 b=0xffffffffffffffffffffffffffffffff',
            '\tprint("{a} {b} {b:0x}")', '\tprint(0b1'+'0'*127+')', '\tprint(0o2000000000000000000000)',
            '\tprint(0d18446744073709551616)'), f'{I_MIN}\n-1 {U_MAX} 0xffffffffffffffffffffffffffffffff\n{I_MIN}\n18446744073709551616\n18446744073709551616\n')

    def test_literal_overflows_are_diagnosed(self):
        for text in [str(1 << 128), str(I_MIN - 1), '0x1'+'0'*32, '0b102']:
            self.compile_source(program('int main();', f'\tprint({text})'), error='エラー')

    def test_wrapping_arithmetic_and_const_runtime_match(self):
        self.check_program(program(f'const int_128 A=int_128({I_MAX})+1',
            f'const unsigned int_128 B=unsigned int_128({U_MAX})+1',
            'int main();', f'\tint_128 a={I_MAX}', f'\tunsigned int_128 b={U_MAX}',
            '\ta++', '\tb++', '\tprint("{a} {A} {b} {B}")', '\tprint(int_128(2)^127)',
            '\tprint(int_128(2)^128)'), f'{I_MIN} {I_MIN} 0 0\n{I_MIN}\n0\n')

    def test_seeded_integer_arithmetic_uses_all_128_bits(self):
        rng = random.Random(2614)
        lines = ['int main();']; expected = []
        for i in range(28):
            a = rng.randrange(I_MIN, I_MAX + 1); b = rng.randrange(1, 1 << 125)
            if i % 2: b = -b
            lines += [f'\tint_128 a{i}={a}', f'\tint_128 b{i}={b}']
            for op in ['+', '-', '*', '/', '%']:
                q = abs(a)//abs(b) * (-1 if (a < 0) != (b < 0) else 1)
                v = {'+':a+b, '-':a-b, '*':a*b, '/':q, '%':a-q*b}[op]
                v = (v + (1 << 127)) % (1 << 128) - (1 << 127)
                lines.append(f'\tprint(a{i}{op}b{i})'); expected.append(str(v))
        self.check_program(program(*lines), '\n'.join(expected)+'\n')

    def test_division_remainder_and_domain_checks(self):
        self.check_program(program('int main();', f'\tint_128 a={I_MIN}', '\tint_128 b=-1',
            '\tprint(a%b)', '\tprint(int_128(-1)^-3)'), '0\n-1\n')
        self.check_program(program('int main();', f'\tint_128 a={I_MIN}', '\tint_128 b=-1', '\tprint(a/b)'), '', status=70, error='数値')
        self.check_program(program('int main();', '\tint_128 a=5', '\tint_128 b=0', '\tprint(a/b)'), '', status=70, error='数値')
        self.compile_source(program(f'const int_128 A=int_128({I_MIN})/-1', 'int main();', '\tprint(A)'), error='範囲')
        self.compile_source(program('const int_128 A=int_128(2)^-1', 'int main();', '\tprint(A)'), error='負の累乗')

    def test_bit_operations_and_shift_limits(self):
        self.check_program(program('const int_128 X=int_128(1)<<100', 'int main();', '\tint_128 a=X',
            '\tint_128 b=7', '\tprint(a or b)', '\tprint(a xor b)', '\tprint(a and b)',
            '\tprint(bnot(a))', '\tprint(int_128(-8)>>2)', '\tunsigned int_128 u=0xffffffffffffffffffffffffffffffff',
            '\tprint(u>>127)'), f'{(1<<100)+7}\n{(1<<100)+7}\n0\n{-1-(1<<100)}\n-2\n1\n')
        self.compile_source(program('int main();', '\tprint(int_128(1)<<128)'), error='127')
        self.check_program(program('int main();', '\tint n=128', '\tprint(int_128(1)<<n)'), '', status=70, error='数値')
        self.compile_source(program('int main();', '\tprint(int_128(1) and true)'), error='エラー')

    def test_safe_common_type_and_checked_narrowing(self):
        self.check_program(program('int main();', '\tint a=-1', '\tunsigned int_64 b=18446744073709551615',
            '\tprint(a+b)', '\tint_128 c=42', '\tprint(int(c))', '\tprint(unsigned int_128(c))'), '18446744073709551614\n42\n42\n')
        self.compile_source(program('int main();', '\tint_128 a=1', '\tint b=a'), error='変換できません')
        self.compile_source(program('int main();', '\tint_128 a=-1', '\tprint(unsigned int_128(-1))'), error='範囲')
        self.check_program(program('int main();', f'\tint_128 a={I_MAX}', '\tprint(int_64(a))'), '', status=70, error='数値')
        self.compile_source(program('int main();', '\tint_128 a=1', '\tunsigned int_128 b=1', '\tprint(a+b)'), error='安全に統一')

    def test_integer_radix_text_conversions(self):
        self.check_program(program(f'const str C=str(unsigned int_128({U_MAX}),base=16,prefix=true)',
            'int main();', '\tstr text="0x10000000000000000000000000"', '\tint_128 a=int_128(text)',
            '\tprint("{a} {a:0x} {C}")', '\tprint(str(int_128(-1),base=2))',
            '\tprint(int_128("-0d170141183460469231731687303715884105728"))',
            '\tint_128 base=16', '\tprint(str(a,base=base,prefix=true))'),
            f'{1<<100} 0x10000000000000000000000000 0xffffffffffffffffffffffffffffffff\n'+ '1'*128+f'\n{I_MIN}\n0x10000000000000000000000000\n')

    def test_half_precision_rounding_and_arithmetic(self):
        self.check_program(program('const half A=1.00048828125', 'int main();', '\thalf a=1.00048828125',
            '\thalf b=1.00146484375', '\thalf c=2048', '\tprint("{A} {a} {b} {c+1} {c+2}")',
            '\tprint(half(65504))', '\tprint(half("0.000000059604644775390625"))',
            '\tprint(half(1.5)^2)', '\tprint(half(2)^-2)'), '1.0 1.0 1.002 2048.0 2050.0\n65504.0\n5.9605e-8\n2.25\n0.25\n')

    def test_quadruple_literal_context_never_rounds_through_double(self):
        self.check_program(program(f'const quadruple C={PRECISE}', 'int main();', f'\tquadruple q={PRECISE}',
            f'\tquadruple r={PRECISE}+0.0', f'\tquadruple s=quadruple({PRECISE})',
            '\tprint("{q} {r} {s} {C}")', '\tprint(q>quadruple(1))',
            '\tprint(q==C and q==r and q==s)'), f'{Q_NEXT} {Q_NEXT} {Q_NEXT} {Q_NEXT}\ntrue\ntrue\n')

    def test_quadruple_large_small_and_subnormal_values(self):
        self.check_program(program('int main();', '\tquadruple a=1e4000', '\tquadruple b=1e-4000',
            '\tprint(a>quadruple(1))', '\tprint(b>quadruple(0))', '\tprint(a*b>quadruple(0.99))',
            '\tquadruple sub=quadruple("1e-4965")', '\tprint(sub>quadruple(0))',
            '\tprint(quadruple(str(sub))==sub)', '\tprint(quadruple(str(a))==a)'), 'true\ntrue\ntrue\ntrue\ntrue\ntrue\n')

    def test_real_promotions_and_explicit_narrowing(self):
        self.check_program(program('int main();', '\thalf h=1.5', '\tfloat f=h', '\tdouble d=f', '\tquadruple q=d',
            '\tprint("{h} {f} {d} {q}")', '\tprint(double(q))', '\tprint(half(q))', '\tprint(half(1.5)+double(2))'),
            '1.5 1.5 1.5 1.5\n1.5\n1.5\n3.5\n')
        for decl in ['double d=1.0\n\thalf h=d', 'quadruple q=1.0\n\tdouble d=q', 'float f=1.0\n\thalf h=f']:
            self.compile_source(program('int main();','\t'+decl), error='変換できません')

    def test_real_integer_conversions_check_wide_boundaries(self):
        self.check_program(program('int main();', '\tquadruple q=123456789012345678901234567890',
            '\tprint(int_128(q))', '\tprint(int(half(3.75)))', '\tunsigned int_128 n=18446744073709551616',
            '\tprint(quadruple(n))'), '123456789012345678901234567890\n3\n18446744073709551616.0\n')
        self.compile_source(program('int main();', '\tprint(int_128(quadruple("1e40")))'), error='範囲')
        self.check_program(program('int main();', '\tquadruple q=1e40', '\tprint(int_128(q))'), '', status=70, error='数値')

    def test_square_root_abs_and_const_runtime_agree(self):
        self.check_program(program('const quadruple C=math.sqrt(quadruple(2))', 'int main();', '\tquadruple q=2',
            '\tprint(math.sqrt(q))', '\tprint(math.sqrt(q)==C)', '\tprint(math.sqrt(int_128(16)))',
            '\tprint(math.sqrt(half(4)))', '\tprint(|quadruple(-3.5)|)', '\tprint(|int_128(-18446744073709551616)|)'),
            '1.41421356237309504880168872420969798\ntrue\n4.0\n2.0\n3.5\n18446744073709551616\n')

    def test_integer_rounding_preserves_128_bit_magnitude(self):
        n=12345678901234567890123456789
        self.check_program(program(f'const int_128 C=math.round(int_128({n}),-1)', 'int main();', f'\tint_128 a={n}',
            '\tprint(math.round(a,-1))', '\tprint(C)', '\tprint(math.rounddown(a,-2))', '\tprint(math.roundup(a,-2))',
            '\tprint(math.round(a,-100))'), f'{n+1}\n{n+1}\n{n//100*100}\n{(n//100+1)*100}\n0\n')
        self.check_program(program('int main();', f'\tint_128 a={I_MAX}', '\tprint(math.roundup(a,-1))'), '', status=70, error='数値')

    def test_real_rounding_and_nan_infinity_signed_zero(self):
        self.check_program(program('const quadruple C=math.round(quadruple(1.25),1)', 'int main();',
            '\tquadruple a=1.25', '\tprint(math.round(a,1)==C)', '\tprint(math.rounddown(a))', '\tprint(math.roundup(a))',
            '\thalf h=math.round(half(-1.5))', '\tprint(h)', '\tquadruple z=0', '\tquadruple nan=z/z',
            '\tprint("{nan==nan} {nan!=nan} {nan<z}")', '\tprint(z/(-z))', '\tprint(quadruple(1)/z)',
            '\tprint(-quadruple(0))'), 'true\n1.0\n2.0\n-2.0\nfalse true false\nnan\ninf\n-0.0\n')

    def test_arrays_models_and_default_values(self):
        self.check_program(program('model Values;', '\tint_128 n', '\tquadruple q', '\thalf h', 'int main();',
            '\tValues v={}', '\tprint(v)', '\tValues copy=v', '\tcopy.n=18446744073709551616', '\tprint(v==copy)',
            '\tquadruple a[2][2]=[[1.5],[2.5]]', '\tprint(a)', '\tprint(a==[[1.5,0.0],[2.5,0.0]])',
            '\thalf hs[]=[1.5]', '\ths.add(half(2.5))', '\tprint(hs)'),
            'Values{n=0, q=0.0, h=0.0}\nfalse\n[[1.5, 0.0], [2.5, 0.0]]\ntrue\n[1.5, 2.5]\n')

    def test_element_and_whole_array_ref_copy_and_readonly(self):
        self.check_program(program('increase(ref int_128 value);', '\tvalue++',
            'increase_all(ref quadruple values[]);', '\tvalues[0]+=1.0',
            'first(ref readonly half values[]) = values[0]', 'int main();', '\tint_128 a[2]=[18446744073709551616,1]',
            '\tint_128 b[]=a', '\tincrease(ref a[0])', '\tprint(a)', '\tprint(b)', '\tquadruple q[]=[1.5]',
            '\tincrease_all(ref q)', '\tprint(q)', '\tconst half h[]=[2.5]', '\tprint(first(ref h))'),
            '[18446744073709551617, 1]\n[18446744073709551616, 1]\n[2.5]\n2.5\n')

    def test_generic_arguments_returns_and_array_inference(self):
        self.check_program(program('square(x)=x*x', 'choose(bool b);', '\tif(b);', '\t\treturn half(1.5)', '\treturn quadruple(2.5)',
            'int main();', '\tprint(square(int_128(18446744073709551616)))', '\tprint(square(half(1.5)))',
            '\tprint(square(quadruple(1.5)))', '\tprint(choose(false))', '\ta=[half(1.5),quadruple(2.5)]', '\tprint(a)'),
            '0\n2.25\n2.25\n2.5\n[1.5, 2.5]\n')

    def test_interface_contract_and_custom_initialization(self):
        self.check_program(program('interface Measure;', '\treadonly quadruple get();', 'model Value implements Measure;',
            '\tquadruple number', '\tvoid init(quadruple n);', '\t\tthis.number=n', '\treadonly quadruple get()=this.number',
            'read(ref readonly Measure m)=m.get()', 'int main();', '\tValue v={n=1.5}', '\tprint(read(ref v))'), '1.5\n')

    def test_imported_constants_functions_and_inference(self):
        (self.directory/'numbers.qy').write_text(f'const int_128 LIMIT={I_MAX}\nquadruple next()={PRECISE}\n')
        self.check_program(program('import numbers','using numbers as n','int main();','\tprint(n.LIMIT)','\tprint(n.next())'),f'{I_MAX}\n{Q_NEXT}\n')

    def test_direct_and_interpolated_wide_integer_input(self):
        self.check_program(program('int main();', '\tint_128 a', '\tunsigned int_128 b', '\tbool ok=input("{a}, {b:0x}")',
            '\tprint("{ok} {a} {b}")', '\tinput(a)', '\tprint(a)'), f'true {I_MIN} {U_MAX}\n18446744073709551616\n',
            stdin=f'{I_MIN}, 0xffffffffffffffffffffffffffffffff\n18446744073709551616\n')

    def test_input_failure_is_atomic_for_every_new_type(self):
        for bad in [f'2.5, 3.5, {U_MAX+1}', '2.5, 1e5000, 4', '70000, 3.5, 4', '2.5, broken, 4']:
            self.check_program(program('int main();', '\thalf h=1.5', '\tquadruple q=1.5', '\tunsigned int_128 n=3',
                '\tbool ok=input("{h}, {q}, {n}")', '\tprint("{ok} {h} {q} {n}")'), 'false 1.5 1.5 3\n', stdin=bad+'\n')

    def test_typed_value_input_and_half_subnormal(self):
        self.check_program(program('int main();', '\tquadruple q=input()', '\thalf h=input()', '\tint_128 n=input()',
            '\tprint("{q} {h} {n}")'), f'{Q_NEXT} 5.9605e-8 {I_MIN}\n', stdin=f'{PRECISE}\n0.000000059604644775390625\n{I_MIN}\n')

    def test_input_rejects_nan_infinity_underflow_and_wrong_radix(self):
        for ty,text in [('half','1e-20'),('quadruple','1e-6000'),('quadruple','inf'),('quadruple','nan'),('int_128','0b102')]:
            self.check_program(program('int main();', f'\t{ty} n=1', '\tbool ok=input(n)', '\tprint(ok)'), 'false\n', stdin=text+'\n')
        self.compile_source(program('int main();', '\tquadruple q=1.5', '\tprint("{q:0x}")'), error='int')

    def test_for_integer_open_closed_boundaries_and_overflow(self):
        self.check_program(program('int main();', f'\tfor({I_MAX-1} <= int_128 i <= {I_MAX},i++);', '\t\tprint(i)',
            f'\tfor({I_MIN+1} >= int_128 j >= {I_MIN},j--);','\t\tprint(j)',
            f'\tfor({U_MAX} >= unsigned int_128 k >= {U_MAX-1},k--);', '\t\tprint(k)',
            '\tfor(18446744073709551616 < i <= 18446744073709551618,i++);', '\t\tprint(i)'),
            f'{I_MAX-1}\n{I_MAX}\n{I_MIN+1}\n{I_MIN}\n{U_MAX}\n{U_MAX-1}\n18446744073709551617\n18446744073709551618\n')

    def test_for_real_counters_progress_and_precision(self):
        self.check_program(program('int main();','\tfor(0.0 <= half h < 1.0,h+=0.25);','\t\tprint(h)',
            '\tfor(0.0 < quadruple q <= 1.0,q+=0.25);','\t\tprint(q)'), '0.0\n0.25\n0.5\n0.75\n0.25\n0.5\n0.75\n1.0\n')
        self.compile_source(program('int main();','\tfor(2048.0 < half h < 2050.0,h++);','\t\tprint(h)'),error='前進')

    def test_array_bounds_and_indices_use_checked_internal_int64(self):
        self.check_program(program('const int_128 N=2','int main();','\tint a[N]=[3,4]', '\tint_128 i=1','\tprint(a[i])'), '4\n')
        self.compile_source(program(f'const int_128 N={1<<100}','int main();','\tint a[N]','\tprint(a)'),error='範囲')
        self.check_program(program('int main();','\tint a[2]','\tint_128 i=18446744073709551616','\tprint(a[i])'),'',status=70,error='数値')

    def test_file_write_matches_print_format_and_byte_count(self):
        destination=str(self.directory/'wide.txt')
        self.check_program(program('int main();',f'\tfile f=io.file.open("{destination}","w")',
            '\tint_128 a=18446744073709551616','\tquadruple q=1.5','\thalf h=2.5',
            '\tprint(io.file.write(f,"{a}, {q}, {h}"))','\tio.file.close(f)'), '30\n')
        self.assertEqual((self.directory/'wide.txt').read_text(),'18446744073709551616, 1.5, 2.5')

    def test_gc_preserves_boxed_values_and_independent_updates(self):
        self.check_program(program('int main();','\tint_128 saved=18446744073709551616','\tint_128 values[]=[saved]',
            '\tquadruple q=1.5','\tfor(0 <= i < 1500,i++);','\t\tsaved++','\t\tq+=quadruple(0.5)',
            '\tprint(values)','\tprint(saved)','\tprint(q)'), '[18446744073709551616]\n18446744073709553116\n751.5\n')

    def test_seeded_quadruple_arithmetic_against_decimal(self):
        rng=random.Random(128)
        lines=['int main();']; exact=[]
        with localcontext() as ctx:
            ctx.prec=100
            for i in range(14):
                a=Decimal(rng.randrange(10**28,10**30))/Decimal(10**20)
                b=Decimal(rng.randrange(10**28,10**30))/Decimal(10**20)
                lines += [f'\tquadruple a{i}={a}',f'\tquadruple b{i}={b}']
                for op,result in [('+',a+b),('-',a-b),('*',a*b),('/',a/b)]:
                    lines.append(f'\tprint(a{i}{op}b{i})');exact.append(result)
            binary=self.compile_source(program(*lines))
            import subprocess
            output=subprocess.run([str(binary)],capture_output=True,timeout=5,check=True).stdout.decode().splitlines()
            self.assertEqual(len(output),len(exact))
            for text,value in zip(output,exact):
                self.assertLess(abs(Decimal(text)-value),max(abs(value),Decimal(1))*Decimal('1e-32'))

    def test_binary128_decimal_conversion_matches_exact_rational_rounding(self):
        rng=random.Random(113)
        spellings=['1.00000000000000000000000000000000009629649721936179265279889712924636592690508241076940976083278656005859375',
                   '1e-4965', '6.47517511943802511092443895822764655e-4966', '1e4932']
        spellings += [f'{rng.randrange(1,10**36)}e{rng.randrange(-4900,4800)}' for _ in range(80)]
        source=program('int main();',*[f'\tprint(quadruple("{s}"))' for s in spellings])
        import subprocess
        output=subprocess.run([str(self.compile_source(source))],capture_output=True,check=True,timeout=5).stdout.decode().splitlines()
        for text,spelling in zip(output,spellings):
            self.assertEqual(binary128(Fraction(text)),binary128(Fraction(spelling)),spelling)

    def test_half_decimal_rounding_matches_independent_binary16_format(self):
        rng=random.Random(16)
        spellings=[f'{rng.uniform(-65000,65000):.14f}' for _ in range(200)]
        source=program('int main();',*[f'\thalf h{i}={s}\n\tprint(h{i})' for i,s in enumerate(spellings)])
        import subprocess
        output=subprocess.run([str(self.compile_source(source))],capture_output=True,check=True,timeout=5).stdout.decode().splitlines()
        for text,spelling in zip(output,spellings):
            self.assertEqual(struct.pack('e',float(text)),struct.pack('e',float(spelling)))

    def test_binary128_operations_and_sqrt_match_exact_rational_rounding(self):
        rng=random.Random(226)
        lines=['int main();']; expected=[]
        for i in range(20):
            sa=f'{rng.randrange(1,10**36)}e-30';sb=f'{rng.randrange(1,10**36)}e-30'
            a=exact_quad(binary128(Fraction(sa)));b=exact_quad(binary128(Fraction(sb)))
            lines += [f'\tquadruple a{i}=quadruple("{sa}")',f'\tquadruple b{i}=quadruple("{sb}")']
            for op,value in [('+',a+b),('-',a-b),('*',a*b),('/',a/b)]:
                lines.append(f'\tprint(a{i}{op}b{i})');expected.append(binary128(value))
            # sqrt's correctly rounded significand follows from an integer square root.
            e=a.numerator.bit_length()-a.denominator.bit_length()
            if (a.numerator < a.denominator<<e) if e>=0 else (a.numerator<<-e < a.denominator): e-=1
            root_e=e//2;scale=112-root_e;n=a.numerator;d=a.denominator
            if scale>=0:n <<= scale*2
            else:d <<= -scale*2
            q=math.isqrt(n//d);threshold=(2*q+1)**2*d
            q += n*4>threshold or n*4==threshold and q&1
            expected.append(((root_e+16383)<<112)|(q-(1<<112)))
            lines.append(f'\tprint(math.sqrt(a{i}))')
        import subprocess
        output=subprocess.run([str(self.compile_source(program(*lines)))],capture_output=True,check=True,timeout=5).stdout.decode().splitlines()
        self.assertEqual([binary128(Fraction(s)) for s in output],expected)

    def test_extreme_decimal_rounding_and_negative_powers_keep_subnormals(self):
        self.check_program(program('int main();','\tquadruple q=quadruple("1.2345e-4000")',
            '\tprint(math.round(q,4001)==quadruple("1.2e-4000"))',
            '\tprint(math.round(quadruple(1),-5000))','\tprint(math.roundup(quadruple(1),-5000))',
            '\tprint(math.rounddown(quadruple(-1),-5000))',
            '\tprint(quadruple("1e2470")^-2>quadruple(0))',
            '\tprint(math.round(int_128(5),-9223372036854775808))'), 'true\n0.0\ninf\n-inf\ntrue\n0\n')

    def test_long_decimal_spelling_and_typed_function_arguments(self):
        tiny='1.'+'0'*450+'e-4000'
        self.check_program(program('quadruple echo(quadruple x)=x','int main();',f'\ta={tiny}',
            '\tprint(a>quadruple(0))',f'\tprint(echo({PRECISE}))'),f'true\n{Q_NEXT}\n')

    def test_wide_values_preserve_const_readonly_and_addr_rules(self):
        self.check_program(program('int main();','\tint_128 a=18446744073709551616','\tref int_128 p=a.addr',
            '\tp.value++','\tprint(a)'), '18446744073709551617\n')
        self.compile_source(program('f(ref readonly quadruple q);','\tq+=1.0','int main();','\tquadruple q=1.5','\tf(ref q)'),error='readonly')
        self.compile_source(program('int main();','\tconst int_128 x=1','\tx++'),error='定数')


if __name__ == '__main__':
    run_suite()
