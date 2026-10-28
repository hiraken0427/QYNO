"""Fixed-width integer ranges, conversions, operators and typed storage."""
import random
import operator
from support import QynoTestCase, program, run_suite

class IntegerTypeTests(QynoTestCase):
    def test_declarations_aliases_and_boundaries(self):
        lines=['int main();']; expected=[]
        for b in (8,16,32,64):
            for u in (False,True):
                ty=('unsigned ' if u else '')+f'int_{b}'
                for name,n in [('lo',0 if u else -(1<<(b-1))),('hi',(1<<b)-1 if u else (1<<(b-1))-1)]:
                    lines += [f'\t{ty} v{b}{int(u)}{name}={n}',f'\tprint(v{b}{int(u)}{name})'];expected.append(str(n))
        lines+=['\tunsigned int alias=4294967295','\tprint(alias)']
        self.check_program(program(*lines),'\n'.join(expected+['4294967295'])+'\n')

    def test_implicit_conversion_requires_entire_source_range_or_fitting_constant(self):
        self.check_program(program('const int_64 SMALL=12','int main();','\tint_8 a=SMALL','\tint_16 b=a',
            '\tunsigned int_8 c=250','\tint_16 d=c','\tprint("{a} {b} {c} {d}")'),'12 12 250 250\n')
        for decl in ['int_8 a=128','unsigned int_8 a=-1','int_16 wide=1\n\tint_8 a=wide',
                     'int_8 signed=-1\n\tunsigned int_8 a=signed','unsigned int_64 wide=1\n\tint a=wide']:
            self.compile_source(program('int main();','\t'+decl,'\tprint(a)'),error='変換できません')

    def test_explicit_integer_casts_are_checked(self):
        self.check_program(program('int main();','\tint n=127','\tint_8 a=int_8(n)',
            '\tunsigned int_64 u=18446744073709551615','\tprint("{a} {unsigned int_64(u)}")'),'127 18446744073709551615\n')
        for expr in ['int_8(128)','unsigned int_8(-1)','int(18446744073709551615)','int_32(2147483648)']:
            self.compile_source(program('int main();',f'\tprint({expr})'),error='範囲')
        for ty,n in [('int_8',128),('unsigned int_8',-1),('int',18446744073709551615)]:
            self.check_program(program('int main();',f'\ta={n}',f'\tprint({ty}(a))'),'',status=70,error='範囲外')

    def test_float_conversions_across_unsigned_high_bit(self):
        self.check_program(program('int main();','\tunsigned int_64 u=18446744073709551615',
            '\tprint(double(u)>0)','\tprint(float(u)>0)','\td=9223372036854775808.0',
            '\tprint(unsigned int_64(d))','\td=255.9','\tprint(unsigned int_8(d))'),'true\ntrue\n9223372036854775808\n255\n')
        for expr in ['unsigned int_64(-0.5)','unsigned int_64(18446744073709551616.0)','int_8(128.0)']:
            self.compile_source(program('int main();',f'\tprint({expr})'),error='範囲')

    def test_wrapping_and_bitwise_use_declared_width(self):
        self.check_program(program('const int_8 C=int_8(127)+1','int main();','\tint_8 a=127','\ta++',
            '\tunsigned int_8 u=255','\tu++','\tprint("{C} {a} {u}")','\tprint(bnot int_8(0))',
            '\tprint(bnot unsigned int_8(0))','\tprint(unsigned int_8(128)>>1)','\tprint(int_8(-128)>>1)',
            '\tprint(int_8(64)<<1)','\tprint(unsigned int_8(128)<<1)'),'-128 -128 0\n-1\n255\n64\n-64\n-128\n0\n')

    def test_shift_count_is_limited_by_operand_width(self):
        for ty,b in [('int_8',8),('unsigned int_16',16),('int_32',32),('unsigned int_64',64)]:
            self.compile_source(program('int main();',f'\tprint({ty}(1)<<{b})'),error='シフト量')
            self.check_program(program('int main();',f'\tn={b}',f'\tprint({ty}(1)>>n)'),'',status=70,error='シフト量')

    def test_unsigned_division_remainder_order_and_power(self):
        self.check_program(program('const unsigned int_64 C=18446744073709551615 / unsigned int_64(3)',
            'int main();','\tunsigned int_64 a=18446744073709551615','\tunsigned int_64 b=3',
            '\tprint("{C} {a/b} {a%b} {a>b} {a>=b} {b<a} {b<=a}")',
            '\tprint(unsigned int_8(3)^5)','\tprint(int_8(3)^5)'),
            '6148914691236517205 6148914691236517205 0 true true true true\n243\n-13\n')
        self.check_program(program('int main();','\tint_8 a=-128','\tint_8 b=-1','\tprint(a/b)'),'',status=70,error='範囲')

    def test_const_and_runtime_random_arithmetic_agree_with_python(self):
        rng=random.Random(2611);lines=['int main();'];out=[]
        for b in (8,16,32,64):
            mask=(1<<b)-1
            for u in (False,True):
                ty=('unsigned ' if u else '')+f'int_{b}'
                for k in range(4):
                    a=rng.randrange(1<<b);c=rng.randrange(1,1<<b)
                    if not u:
                        a=a-(1<<b) if a>>(b-1) else a;c=c-(1<<b) if c>>(b-1) else c
                    name=f'v{b}{int(u)}{k}';lines += [f'\t{ty} {name}={a}']
                    for op in ['+','-','*','and','or','xor','/','%']:
                        if op=='/': val=abs(a)//abs(c)*(-1 if (a<0)!=(c<0) else 1)
                        elif op=='%': val=a-(abs(a)//abs(c)*(-1 if (a<0)!=(c<0) else 1))*c
                        else: val={"+":operator.add,"-":operator.sub,"*":operator.mul,"and":operator.and_,"or":operator.or_,"xor":operator.xor}[op](a,c)
                        val &= mask
                        if not u and val>>(b-1):val-=1<<b
                        expr=f'{name} {op} {ty}({c})';const=f'{ty}({a}) {op} {ty}({c})'
                        lines += [f'\tprint({expr})',f'\tconst {ty} c{len(out)}={const}',f'\tprint(c{len(out)})'];out += [str(val),str(val)]
        self.check_program(program(*lines),'\n'.join(out)+'\n')

    def test_arrays_models_ref_and_generic_inference(self):
        self.check_program(program('model Packet;','\tunsigned int_8 tag','\tint_16 number',
            'increase(ref x);','\tx++','identity(x)=x','int main();',
            '\tunsigned int_8 matrix[][]=[[255,2],[3,4]]','\tincrease(ref matrix[0][0])',
            '\tmatrix.add([5,6])','\tprint(identity(matrix))','\tPacket p={tag=255,number=-32768}',
            '\tprint(p)','\tincrease(ref p.tag)','\tprint(p.tag)'),
            '[[0, 2], [3, 4], [5, 6]]\nPacket{tag=255, number=-32768}\n0\n')

    def test_typed_input_and_interpolation_are_atomic(self):
        self.check_program(program('int main();','\tunsigned int_64 u','\tint_8 n',
            '\tinput("{u} {n}")','\tprint("{u} {n}")','\tprint(input(n))','\tprint(n)'),
            '18446744073709551615 -128\nfalse\n-128\n',stdin='18446744073709551615 -128\n128\n')
        self.check_program(program('int main();','\tunsigned int_8 u=input()','\tprint(u)'), '255\n',stdin='255\n')
        self.check_program(program('int main();','\tunsigned int_8 u=10','\tprint(input(u))','\tprint(u)'),
            'false\n10\n',stdin='-1\n')

    def test_base_format_and_parse_preserve_width(self):
        self.check_program(program('const unsigned int_64 MAX=unsigned int_64("0xffffffffffffffff")',
            'int main();','\tint_8 s=-1','\tunsigned int_64 u=MAX',
            '\tprint("{s:0x} {u:0d}")','\tprint(str(s,16,true))',
            '\tprint(int_8("ff",16))','\tstr text="18446744073709551615"',
            '\tprint(unsigned int_64(text))','\tprint(str(u))'),
            '0xff 0d18446744073709551615\n0xff\n-1\n18446744073709551615\n18446744073709551615\n')

    def test_loops_terminate_at_integer_width_boundaries(self):
        self.check_program(program('int main();','\tfor(126 <= int_8 i <= 127,i++);','\t\tprint(i)',
            '\tfor(254 <= unsigned int_8 j <= 255,j++);','\t\tprint(j)',
            '\tfor(1 >= unsigned int_8 k >= 0,k--);','\t\tprint(k)'), '126\n127\n254\n255\n1\n0\n')

    def test_mixed_signed_unsigned_requires_a_safe_common_type(self):
        self.check_program(program('int main();','\tint_8 a=-1','\tunsigned int_8 b=255',
            '\tc=a+b','\tprint(c)','\tprint(a<b)'), '254\ntrue\n')
        self.check_program(program('int main();','\tint a=-1','\tunsigned int_64 b=1','\tprint(a+b)'),'0\n')
        self.compile_source(program('int main();','\tint_128 a=-1','\tunsigned int_128 b=1','\tprint(a+b)'),error='安全に統一')


    def test_rounding_preserves_unsigned_magnitude_and_checks_narrow_result(self):
        self.check_program(program('using math','const unsigned int_64 C=rounddown(unsigned int_64(18446744073709551615),-1)',
            'int main();','\tunsigned int_64 a=18446744073709551615','\tprint(C)',
            '\tprint(rounddown(a,-1))','\tprint(round(a,-18))','\tprint(rounddown(a,-20))',
            '\tunsigned int_64 b=9223372036854775808','\tprint(roundup(b,-19))',
            '\tunsigned int_8 small=254','\tprint(round(small,-1))'),
            '18446744073709551610\n18446744073709551610\n18000000000000000000\n0\n10000000000000000000\n250\n')
        for ty,n in [('unsigned int_8',255),('int_8',127),('unsigned int_64',18446744073709551615)]:
            self.compile_source(program('using math','int main();',f'\tprint(roundup({ty}({n}),-1))'),error='範囲')
            self.check_program(program('using math','int main();',f'\t{ty} n={n}','\tprint(roundup(n,-1))'),'',status=70,error='範囲')

    def test_abs_and_sqrt_accept_all_integer_types(self):
        self.check_program(program('using math','const unsigned int_64 C=abs(18446744073709551615)',
            'int main();','\tunsigned int_64 n=18446744073709551615','\tprint(C)','\tprint(abs(n))',
            '\tprint(sqrt(unsigned int_8(16)))','\tprint(|int_8(-3)|)'),
            '18446744073709551615\n18446744073709551615\n4.0\n3\n')

    def test_unsigned_hexadecimal_maximum_is_contextual_in_both_operand_orders(self):
        self.check_program(program('int main();','\tunsigned int_64 a=0xffffffffffffffff',
            '\tprint(a)','\tprint(a==0xffffffffffffffff)','\tprint(0xffffffffffffffff==a)',
            '\tprint(unsigned int_64(0xffffffffffffffff))'), '18446744073709551615\ntrue\ntrue\n18446744073709551615\n')

    def test_mixed_width_expression_does_not_narrow_explicit_types(self):
        self.check_program(program('int main();','\tx=int_8(127)+int_16(1)',
            '\tconst C=int_8(127)+int_16(1)','\tprint("{x} {C}")'),'128 128\n')

    def test_unsigned_decimal_literals_are_inferred_and_overflow_is_rejected(self):
        self.check_program(program('int main();','\tx=18446744073709551615','\tprint(x)',
            '\tprint(0d9223372036854775808)','\tprint(+9223372036854775808)'),
            '18446744073709551615\n9223372036854775808\n9223372036854775808\n')
        self.check_program(program('int main();','\tprint(18446744073709551616)','\tprint(-9223372036854775809)'),'18446744073709551616\n-9223372036854775809\n')
        for expr in ['340282366920938463463374607431768211456','-170141183460469231731687303715884105729']:
            self.compile_source(program('int main();',f'\tprint({expr})'),error='エラー')

    def test_boolean_and_integer_operands_do_not_mix(self):
        for expr in ['unsigned int_8(1) and true','int_8(1) or false','not unsigned int_8(1)']:
            self.compile_source(program('int main();',f'\tprint({expr})'),error='エラー')

    def test_array_indices_and_remove_at_check_unsigned_high_bit(self):
        self.check_program(program('int main();','\tint_8 numbers[]=[1,2,3]','\tunsigned int_8 index=1',
            '\tprint(numbers[index])','\tnumbers.remove_at(index)','\tprint(numbers)'), '2\n[1, 3]\n')
        self.check_program(program('int main();','\tint numbers[1]','\tunsigned int_64 index=18446744073709551615',
            '\tprint(numbers[index])'),'',status=70,error='範囲外')

    def test_const_arrays_models_and_array_default_values(self):
        self.check_program(program('model Sample;','\tunsigned int_64 n',
            'const Sample S={n=18446744073709551615}','const unsigned int_8 VALUES[]=[255,1]',
            'int main();','\tprint(S)','\tprint(VALUES)','\tunsigned int_16 zeroes[2]',
            '\tzeroes.remove_at(0)','\tzeroes.add()','\tprint(zeroes)','\tprint(S==S)'),
            'Sample{n=18446744073709551615}\n[255, 1]\n[0, 0]\ntrue\n')

    def test_ref_type_identity_and_readonly_are_retained(self):
        self.compile_source(program('void f(ref int_16 n);','\tn++','int main();','\tint_8 n=1','\tf(ref n)'),error='エラー')
        self.compile_source(program('void f(ref readonly unsigned int_8 n);','\tn++','int main();','\tunsigned int_8 n=1','\tf(ref n)'),error='readonly')

    def test_unsigned_negative_power_and_zero_division_fail(self):
        self.compile_source(program('int main();','\tprint(unsigned int_64(18446744073709551615)^(-1))'),error='負の累乗')
        self.check_program(program('int main();','\tunsigned int_64 n=18446744073709551615','\tprint(n^(-1))'),'',status=70,error='負の累乗')
        self.check_program(program('int main();','\tunsigned int_8 zero=0','\tprint(unsigned int_8(1)/zero)'),'',status=70,error='0 で割')

    def test_scalar_array_input_and_base_interpolation(self):
        self.check_program(program('int main();','\tint_8 values[2]','\tinput(values[0])',
            '\tinput("{values[1]:0x}")','\tprint(values)'), '[-128, -1]\n',stdin='-128\nff\n')
        self.check_program(program('int main();','\tunsigned int_64 a=1','\tint_8 b=2',
            '\tprint(input("{a} {b}"))','\tprint("{a} {b}")'), 'false\n1 2\n',stdin='18446744073709551615 128\n')

    def test_file_write_and_text_conversion_match_print(self):
        path=str(self.directory/'values.txt')
        self.check_program(program('int main();','\tunsigned int_64 u=18446744073709551615','\tint_8 n=-1',
            f'\tfile f=io.file.open("{path}","w")','\tio.file.write(f,"{u} {n:0x}")','\tio.file.close(f)',
            f'\tf=io.file.open("{path}","r")','\tprint(io.file.read(f))','\tio.file.close(f)'), '18446744073709551615 0xff\n')

    def test_evaluation_order_in_integer_conversions_and_generic_calls(self):
        self.check_program(program('unsigned int_8 take(ref int counter);','\tcounter++','\treturn 2',
            'combine(x,y)=x+y','int main();','\tint count=0',
            '\tprint(combine(take(ref count),take(ref count)))','\tprint(count)'), '4\n2\n')


    def test_imports_interfaces_and_return_inference_keep_integer_types(self):
        (self.directory/'numbers.qy').write_text(program('const unsigned int_64 MAX=18446744073709551615',
            'unsigned int_8 small() = 255'))
        self.check_program(program('import numbers','interface ByteValue;','\treadonly unsigned int_8 value();',
            'model Counter implements ByteValue;','\tunsigned int_8 n=255',
            '\treadonly unsigned int_8 value() = this.n',
            'unsigned int_8 read(ref readonly ByteValue value) = value.value()',
            'choose(bool flag);','\tif(flag);','\t\treturn int_8(-1)','\treturn unsigned int_8(255)',
            'int main();','\tCounter c','\tprint(read(ref c))','\tprint(numbers.MAX)',
            '\tprint(numbers.small())','\tprint(choose(true))','\tprint(choose(false))'),
            '255\n18446744073709551615\n255\n-1\n255\n')

    def test_integer_to_float_rounding_matches_constants_without_double_rounding(self):
        self.check_program(program('const unsigned int_64 N=9223372586610589697','const float C=float(N)',
            'int main();','\tunsigned int_64 n=N','\tf=float(n)',
            '\tprint(C==f)','\tprint(double(f)==9223373136366403584.0)'), 'true\ntrue\n')

    def test_unsigned_nan_and_infinite_dynamic_casts_fail(self):
        for expression in ['zero/zero','1.0/zero','-1.0/zero']:
            self.check_program(program('int main();','\tdouble zero=0.0',f'\tdouble n={expression}',
                '\tprint(unsigned int_64(n))'),'',status=70,error='範囲外')

    def test_const_steps_and_open_loops_keep_their_integer_width(self):
        self.check_program(program('const STEP=2','int main();','\tfor(250 < unsigned int_8 i <= 255,i+=STEP);',
            '\t\tprint(i)','\tfor(-126 > int_8 j >= -128,j-=STEP);','\t\tprint(j)'), '252\n254\n-128\n')

if __name__=='__main__':run_suite()
