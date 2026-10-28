"""Default int32 aliases, literal inference and explicit migration to int64."""
from support import QynoTestCase, program, run_suite


class DefaultIntegerTests(QynoTestCase):
    def test_signed_alias_ref_identity_and_wrap(self):
        self.check_program(program('void bump(ref int_32 value);','\tvalue++',
            'int main();','\tint value=2147483647','\tbump(ref value)','\tio.print(value)',
            '\tint_32 other=-2147483648','\tother--','\tio.print(other)'), '-2147483648\n2147483647\n')

    def test_unsigned_alias_ref_identity_and_wrap(self):
        self.check_program(program('void bump(ref unsigned int_32 value);','\tvalue++',
            'int main();','\tunsigned int value=4294967295','\tbump(ref value)','\tio.print(value)',
            '\tunsigned int_32 other=0','\tother--','\tio.print(other)'), '0\n4294967295\n')

    def test_int64_is_distinct_from_default_int_even_through_ref(self):
        for a,b in [('int','int_64'),('int_64','int'),('unsigned int','unsigned int_64')]:
            self.compile_source(program(f'void f(ref {a} value);','\tvalue++','int main();',
                f'\t{b} value=1','\tf(ref value)'),error='型')
        self.compile_source(program('int_64 main();','\treturn 0'),error='main')
        self.check_program(program('int_32 main();','\treturn 0'),'')

    def test_decimal_literal_inference_uses_signed32_then_signed64_then_unsigned64(self):
        self.check_program(program('void s32(ref int value);','\tio.print(value)',
            'void s64(ref int_64 value);','\tio.print(value)','void u64(ref unsigned int_64 value);','\tio.print(value)',
            'int main();','\ta=2147483647','\tb=2147483648','\tc=9223372036854775807',
            '\td=9223372036854775808','\te=18446744073709551615','\ts32(ref a)','\ts64(ref b)',
            '\ts64(ref c)','\tu64(ref d)','\tu64(ref e)'),
            '2147483647\n2147483648\n9223372036854775807\n9223372036854775808\n18446744073709551615\n')

    def test_negative_literal_minima_and_unary_precedence(self):
        self.check_program(program('void s32(ref int value);','\tio.print(value)',
            'void s64(ref int_64 value);','\tio.print(value)','int main();',
            '\ta=-2147483648','\tb=-0d2147483648','\tc=-2147483649','\td=-9223372036854775808',
            '\ts32(ref a)','\ts32(ref b)','\ts64(ref c)','\ts64(ref d)','\tio.print(-2^2)',
            '\te=-(2147483648)','\ts32(ref e)'), '-2147483648\n-2147483648\n-2147483649\n-9223372036854775808\n-4\n-2147483648\n')

    def test_nondecimal_literals_infer_by_value_and_keep_full64_bit_patterns(self):
        self.check_program(program('void s32(ref int value);','\tio.print(value)',
            'void s64(ref int_64 value);','\tio.print(value)','int main();',
            '\ta=0x7fffffff','\tb=0x80000000','\tc=0xffffffff','\td=0xffffffffffffffff',
            '\ts32(ref a)','\ts64(ref b)','\ts64(ref c)','\ts64(ref d)',
            '\tunsigned int_64 max=0xffffffffffffffff','\tio.print(max)'),
            '2147483647\n2147483648\n4294967295\n-1\n18446744073709551615\n')

    def test_prefixed_octal_binary_decimal_literals_have_the_same_numeric_inference(self):
        self.check_program(program('void s32(ref int value);','\tio.print(value)',
            'void s64(ref int_64 value);','\tio.print(value)','int main();',
            '\ta=0b1111111111111111111111111111111','\tb=0o20000000000',
            '\tc=0d2147483648','\ts32(ref a)','\ts64(ref b)','\ts64(ref c)'),
            '2147483647\n2147483648\n2147483648\n')

    def test_large_literals_do_not_retype_declared_variables(self):
        for declaration in ['int value=2147483648','unsigned int value=4294967296',
                            'int value=1\n\tvalue=2147483648']:
            self.compile_source(program('int main();','\t'+declaration,'\tio.print(value)'),error='変換できません')
        self.check_program(program('int main();','\tvalue=2147483648','\tvalue=1','\tio.print(value)'), '1\n')

    def test_int_cast_uses32_and_int64_cast_remains_available(self):
        self.compile_source(program('int main();','\tio.print(int(2147483648))'),error='範囲')
        self.check_program(program('int main();','\tint_64 value=2147483648','\tio.print(int_64(value))'), '2147483648\n')
        self.check_program(program('int main();','\tint_64 value=2147483648','\tio.print(int(value))'), '',status=70,error='範囲外')
        self.check_program(program('int main();','\tunsigned int value=4294967295','\tio.print(value)'), '4294967295\n')

    def test_float_to_int_cast_enforces_the_new_signed_and_unsigned_limits(self):
        for expression in ['int(2147483648.0)','unsigned int(4294967296.0)','int(-2147483649.0)']:
            self.compile_source(program('int main();',f'\tio.print({expression})'),error='範囲')
        self.check_program(program('int main();','\td=2147483647.0','\tio.print(int(d))',
            '\td=4294967295.0','\tio.print(unsigned int(d))'), '2147483647\n4294967295\n')

    def test_default_and_explicit64_arithmetic_use_separate_widths(self):
        self.check_program(program('const int C=2147483647+1','int main();','\tint a=2147483647',
            '\tint_64 b=2147483647','\tio.print("{C} {a+1} {b+1}")','\tio.print(2^32)',
            '\tio.print(int_64(2)^32)','\tio.print(unsigned int(2147483648)>>31)',
            '\tio.print(int(-2147483648)>>31)'), '-2147483648 -2147483648 2147483648\n0\n4294967296\n1\n-1\n')

    def test_bitwise_masks_fit_unsigned32_without_forcing_int64_results(self):
        self.check_program(program('void check(ref unsigned int value);','\tio.print(value)','int main();',
            '\tunsigned int value=4294967295','\tmasked=value and 0xffffffff','\tcheck(ref masked)',
            '\tio.print("{value:0x} {int(-1):0x} {int_64(-1):0x}")'),
            '4294967295\n0xffffffff 0xffffffff 0xffffffffffffffff\n')

    def test_shift_range_tracks_default32_and_explicit64(self):
        self.compile_source(program('int main();','\tio.print(1<<32)'),error='31')
        self.check_program(program('int main();','\tamount=32','\tio.print(1<<amount)'), '',status=70,error='31')
        self.check_program(program('int main();','\tio.print(int_64(1)<<63)'), '-9223372036854775808\n')

    def test_default32_division_minimum_overflow_and_remainder(self):
        self.check_program(program('int main();','\tint a=-2147483648','\tint b=-1','\tio.print(a%b)'), '0\n')
        self.check_program(program('int main();','\tint a=-2147483648','\tint b=-1','\tio.print(a/b)'), '',status=70,error='範囲')
        self.compile_source(program('const int N=-2147483648 / -1','int main();','\tio.print(N)'),error='範囲')

    def test_array_aliases_and_generic_instances_keep_the_actual_width(self):
        self.check_program(program('void bump(ref int_32 values[]);','\tvalues[0]++','identity(x)=x',
            'int main();','\tint values[]=[2147483647]','\tbump(ref values)','\tio.print(identity(values))',
            '\tint_64 wide[]=[2147483647]','\twide[0]++','\tio.print(identity(wide))'),
            '[-2147483648]\n[2147483648]\n')

    def test_mixed_array_and_return_inference_widens_to64_without_truncation(self):
        self.check_program(program('choose(bool flag);','\tif(flag);','\t\treturn 1','\treturn 2147483648',
            'void check(ref int_64 value);','\tio.print(value)','int main();','\tvalues=[1,2147483648]',
            '\tvalue=choose(false)','\tcheck(ref value)','\tio.print(values)'), '2147483648\n[1, 2147483648]\n')

    def test_length_and_file_write_counts_still_return_int64(self):
        path=str(self.directory/'text.txt')
        self.check_program(program('void check(ref int_64 n);','\tio.print(n)','int main();',
            '\tint values[]=[1,2]','\tlength=values.length','\tcheck(ref length)',
            '\tstr text="abc"','\tlength=text.length','\tcheck(ref length)',
            f'\tfile f=io.file.open("{path}","w")','\twritten=io.file.write(f,"text")',
            '\tcheck(ref written)','\tio.file.close(f)'), '2\n3\n4\n')

    def test_input_default_limits_and_atomic_failure(self):
        self.check_program(program('int main();','\tint n=1','\tunsigned int u=2','\tio.input("{n} {u}")',
            '\tio.print("{n} {u}")','\tio.print(io.input("{n} {u}"))','\tio.print("{n} {u}")'),
            '-2147483648 4294967295\nfalse\n-2147483648 4294967295\n',
            stdin='-2147483648 4294967295\n2147483647 4294967296\n')
        self.check_program(program('int main();','\tint n=io.input()','\tio.print(n)'), '',stdin='2147483648\n',status=65,error='範囲')

    def test_explicit64_input_keeps_full_signed_range(self):
        self.check_program(program('int main();','\tint_64 n=io.input()','\tio.print(n)'),
            '-9223372036854775808\n',stdin='-9223372036854775808\n')

    def test_default_base_conversion_reads_and_formats32_bits(self):
        self.check_program(program('const int C=int("0xffffffff")','int main();','\tstr text="0xffffffff"',
            '\tio.print("{C} {int(text)} {int_64(text)}")','\tio.print(str(C,16,true))',
            '\tio.print(unsigned int(text))'), '-1 -1 4294967295\n0xffffffff\n4294967295\n')
        self.compile_source(program('int main();','\tio.print(int("2147483648"))'),error='読み取れません')
        self.check_program(program('int main();','\tstr text="2147483648"','\tio.print(int(text))'), '',status=65,error='読み取れません')

    def test_rounding_checks_the_default32_result(self):
        self.compile_source(program('int main();','\tio.print(math.roundup(2147483647,-1))'),error='範囲')
        self.check_program(program('int main();','\tint value=2147483647','\tio.print(math.roundup(value,-1))'), '',status=70,error='範囲')
        self.check_program(program('int main();','\tint_64 value=2147483647','\tio.print(math.roundup(value,-1))'), '2147483650\n')

    def test_implicit_for_uses_integer_start_at_least32_bits(self):
        self.check_program(program('int main();','\tfor(2147483646 <= i <= 2147483647,i++);','\t\tio.print(i)',
            '\tfor(2147483648 <= i < 2147483650,i++);','\t\tio.print(i)',
            '\tint a[]=[1,2]','\tfor(a.length > i >= 0,i--);','\t\tio.print(a[i])'),
            '2147483646\n2147483647\n2147483648\n2147483649\n2\n1\n')

    def test_explicit_for_stays_at_the_declared_width(self):
        self.check_program(program('int main();','\tfor(2147483647 <= int i <= 2147483648,i++);','\t\tio.print(i)',
            '\tfor(2147483647 <= int_64 i <= 2147483648,i++);','\t\tio.print(i)'),
            '2147483647\n2147483647\n2147483648\n')

    def test_internal_indices_and_exponents_accept_int64(self):
        self.check_program(program('int main();','\tint values[]=[1,2]','\tint_64 index=1',
            '\tio.print(values[index])','\tvalues.remove_at(index)','\tio.print(values)',
            '\tio.print(1^9223372036854775807)','\tio.print(math.round(42,-9223372036854775808))'),
            '2\n[1]\n1\n0\n')

    def test_generic_inferred_for_counter_is_separate_for32_and64_starts(self):
        self.check_program(program('walk(start);','\tfor(start <= i <= start+1,i++);','\t\tio.print(i)',
            'int main();','\twalk(1)','\twalk(2147483648)'), '1\n2\n2147483648\n2147483649\n')


    def test_aliases_match_across_modules_models_and_interface_contracts(self):
        (self.directory/'helper.qy').write_text(program('void bump(ref int_32 value);','\tvalue++'))
        self.check_program(program('import helper','interface Value;','\treadonly int_32 get();',
            'model Sample implements Value;','\tint n=2147483647','\treadonly int get()=this.n',
            'int_32 read(ref readonly Value value)=value.get()', 'int main();','\tSample sample',
            '\thelper.bump(ref sample.n)','\tio.print(read(ref sample))'), '-2147483648\n')

    def test_forward_global_constants_preserve_inferred_integer_widths(self):
        self.check_program(program('const ALIAS=LARGE','const SMALL=2147483647','const LARGE=2147483648',
            'void narrow(ref int value);','\tio.print(value)','void wide(ref int_64 value);','\tio.print(value)',
            'int main();','\ta=SMALL','\tb=ALIAS','\tnarrow(ref a)','\twide(ref b)'),
            '2147483647\n2147483648\n')

    def test_generic_returns_keep_argument_width_and_explicit_int_constraints(self):
        self.check_program(program('twice(x)=x+x','int main();','\tio.print(twice(2147483647))',
            '\tio.print(twice(int_64(2147483647)))'), '-2\n4294967294\n')
        self.compile_source(program('int identity(x)=x','int main();','\tio.print(identity(int_64(1)))'),error='変換できません')
        self.check_program(program('int identity(x)=int(x)','int main();','\tio.print(identity(int_64(1)))'), '1\n')

if __name__=='__main__':run_suite()
