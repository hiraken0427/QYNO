"""Complex values, literals, precision, ownership and atomic text input."""
import random
from support import QynoTestCase, program, run_suite

class ComplexTests(QynoTestCase):
    def test_three_construction_forms_and_default_alias(self):
        self.check_program(program('void alias(ref complex_64 z);','\tprint(z)','int main();',
            '\tcomplex a=complex(real=3.0,imag=4.0)','\tcomplex b=3.0+i4.0','\tcomplex c=3.0+4.0i',
            '\talias(ref a)','\tprint(b==a and c==a)'), '3.0 + i4.0\ntrue\n')

    def test_i_stays_an_identifier_and_digit_forms_are_reserved(self):
        self.check_program(program('int main();','\tint i=2','\tprint(i)','\tprint(i4)','\tprint(4i)'), '2\n0.0 + i4.0\n0.0 + i4.0\n')
        for source in ['int i4=1','print(i4foo)','print(4ii)','print(i0x10)','print(i4.0i)']:
            self.compile_source(program('int main();','\t'+source),error='エラー')

    def test_constructor_defaults_keywords_and_argument_order(self):
        self.check_program(program('double read(ref int n);','\tn++','\treturn n', 'int main();','\tint n=0',
            '\tprint(complex())','\tprint(complex(3))','\tprint(complex(imag=4))',
            '\tprint(complex(imag=read(ref n),real=read(ref n)))','\tprint(n)'),
            '0.0 + i0.0\n3.0 + i0.0\n0.0 + i4.0\n2.0 + i1.0\n2\n')
        for call in ['complex(1,2,3)','complex(real=1,real=2)','complex(imag=2,1)','complex(x=1)','complex(true)','complex(real=i2)']:
            self.compile_source(program('int main();',f'\tprint({call})'),error='エラー')

    def test_real_and_imag_components_preserve_precision_types(self):
        self.check_program(program('void single(ref float value);','\tprint(value)',
            'void wide(ref double value);','\tprint(value)','int main();','\tcomplex z=3+i4',
            '\ta=z.real','\tb=z.imag','\tsingle(ref a)','\tsingle(ref b)',
            '\tcomplex_128 w=3+i4','\tc=w.real','\twide(ref c)'), '3.0\n4.0\n3.0\n')
        for statement in ['z.real=4','z.imag++','input(z.real)']:
            self.compile_source(program('int main();','\tcomplex z', '\t'+statement),error='読み取り専用')

    def test_precision_widening_and_explicit_narrowing(self):
        self.check_program(program('int main();','\tcomplex_128 wide=complex_128(16777217,4)',
            '\tcomplex narrow=complex(wide)','\tcomplex_128 copy=narrow',
            '\tprint("{wide.real} {narrow.real} {copy==narrow}")'), '16777217.0 16777216.0 true\n')
        self.compile_source(program('int main();','\tcomplex_128 wide=3+i4','\tcomplex narrow=wide','\tprint(narrow)'),error='変換できません')
        self.compile_source(program('int main();','\tdouble value=1.0','\tcomplex narrow=value','\tprint(narrow)'),error='変換できません')

    def test_arithmetic_and_integer_power(self):
        self.check_program(program('int main();','\tcomplex z=3+i4','\tcomplex w=1-i2',
            '\tprint(z+w)','\tprint(z-w)','\tprint(z*w)','\tprint(z/z)',
            '\tprint(z^2)','\tprint(z^-1)','\tprint(z^0)'),
            '4.0 + i2.0\n2.0 + i6.0\n11.0 - i2.0\n1.0 + i0.0\n-7.0 + i24.0\n0.12 - i0.16\n1.0 + i0.0\n')

    def test_mixed_real_arithmetic_promotes_and_keeps_literal_context(self):
        self.check_program(program('void single(ref complex value);','\tprint(value)',
            'void wide(ref complex_128 value);','\tprint(value)','int main();','\tcomplex z=3+i4',
            '\ta=z+2','\tb=2-z','\tc=z+1.5','\tdouble amount=1.5','\td=z+amount',
            '\tsingle(ref a)','\tsingle(ref b)','\tsingle(ref c)','\twide(ref d)'),
            '5.0 + i4.0\n-1.0 - i4.0\n4.5 + i4.0\n4.5 + i4.0\n')

    def test_random_arithmetic_matches_python_complex(self):
        rng=random.Random(2613); lines=['int main();']; expected=[]
        for n in range(20):
            a=complex(rng.randint(-20,20),rng.randint(-20,20)); b=complex(rng.randint(1,20),rng.randint(1,20))
            lines += [f'\tcomplex_128 a{n}=complex_128({a.real},{a.imag})', f'\tcomplex_128 b{n}=complex_128({b.real},{b.imag})']
            for op,result in [('+',a+b),('-',a-b),('*',a*b),('/',a/b)]:
                # Relative tolerances check results independently of formatting.
                lines += [f'\tz{n}=a{n}{op}b{n}',
                    f'\tprint(|z{n}.real-({result.real:.17f})| < 0.00000000001)',
                    f'\tprint(|z{n}.imag-({result.imag:.17f})| < 0.00000000001)']
                expected += ['true','true']
        self.check_program(program(*lines),'\n'.join(expected)+'\n')

    def test_constants_and_runtime_are_equal_for_every_operation(self):
        self.check_program(program('const complex_128 A=3+i4','const complex_128 B=1-i2',
            'const complex_128 SUM=A+B','const complex_128 PRODUCT=A*B','const complex_128 DIV=A/B',
            'const complex_128 POWER=A^-3','const double LENGTH=|A|', 'int main();',
            '\tcomplex_128 a=A','\tcomplex_128 b=B',
            '\tprint(SUM==a+b and PRODUCT==a*b and DIV==a/b and POWER==a^-3)',
            '\tprint(LENGTH==|a|)','\tprint(A.real)','\tprint(A.imag)'), 'true\ntrue\n3.0\n4.0\n')

    def test_zero_defaults_arrays_models_and_const_padding(self):
        self.check_program(program('model Sample;','\tcomplex z','const complex VALUES[2]=[3+i4]',
            'const Sample ZERO={}','int main();','\tcomplex z','\tcomplex values[2]',
            '\tSample sample','\tprint(z)','\tprint(values)','\tprint(VALUES)',
            '\tprint(sample==ZERO)','\tprint(sample)'),
            '0.0 + i0.0\n[0.0 + i0.0, 0.0 + i0.0]\n[3.0 + i4.0, 0.0 + i0.0]\ntrue\nSample{z=0.0 + i0.0}\n')

    def test_value_copy_and_ref_rebinding_are_independent(self):
        self.check_program(program('void change(complex z);','\tz+=1','\tprint(z)',
            'void increase(ref complex z);','\tz++','int main();','\tcomplex z=3+i4','\tcomplex copy=z',
            '\tchange(z)','\tincrease(ref z)','\tprint(z)','\tprint(copy)'),
            '4.0 + i4.0\n4.0 + i4.0\n3.0 + i4.0\n')

    def test_nested_array_copy_and_element_refs(self):
        self.check_program(program('void increase(ref complex z);','\tz++','int main();',
            '\tcomplex matrix[][]=[[3+i4]]','\tcomplex copy[][]=matrix','\tincrease(ref matrix[0][0])',
            '\tmatrix.add([5+i6])','\tprint(matrix)','\tprint(copy)'),
            '[[4.0 + i4.0], [5.0 + i6.0]]\n[[3.0 + i4.0]]\n')

    def test_generic_arguments_arrays_and_return_inference(self):
        self.check_program(program('square(x)=x^2','first(ref readonly values)=values[0]',
            'int main();','\tcomplex z=3+i4','\tprint(square(z))','\tprint(square(3+i4))',
            '\tvalues=[1,i2]','\tprint(first(ref values))','\tprint(values)'),
            '-7.0 + i24.0\n-7.0 + i24.0\n1.0 + i0.0\n[1.0 + i0.0, 0.0 + i2.0]\n')

    def test_interface_readonly_and_model_method_support_complex(self):
        self.check_program(program('interface Signal;','\treadonly complex get();',
            'model Source implements Signal;','\tcomplex z=3+i4','\treadonly complex get()=this.z',
            'complex read(ref readonly Signal value)=value.get()',
            'int main();','\tSource source','\tprint(read(ref source))'), '3.0 + i4.0\n')
        self.compile_source(program('void f(ref readonly complex value);','\tvalue++','int main();','\treturn 0'),error='readonly')

    def test_equality_ordering_and_unsupported_operator_diagnostics(self):
        self.check_program(program('int main();','\tcomplex z=complex(3)','\tprint(z==3 and 3==z)',
            '\tprint(z!=3+i1)','\tprint(complex()==0)'), 'true\ntrue\ntrue\n')
        for expr in ['i3 < i4','i3 % i4','i3 and i4','i3 << 1','not i3','int(i3)','float(i3)', 'i3^2.0','math.round(i3)']:
            self.compile_source(program('int main();',f'\tprint({expr})'),error='エラー')

    def test_zero_division_and_negative_power_are_checked(self):
        for expr in ['i4/complex()','complex()^-1']:
            self.compile_source(program('const complex X='+expr,'int main();','\tprint(X)'),error='0')
        self.check_program(program('int main();','\tcomplex zero','\tprint(i4/zero)'), '',status=70,error='複素数')
        self.check_program(program('int main();','\tcomplex zero','\tprint(zero^-1)'), '',status=70,error='複素数')
        self.check_program(program('int main();','\tcomplex zero','\tprint(zero^0)'), '1.0 + i0.0\n')

    def test_absolute_value_is_real_and_scaled_division_handles_large_finite_values(self):
        self.check_program(program('int main();','\tcomplex_128 z=complex_128(double(10)^300,double(10)^300)',
            '\tprint(z/z)','\tprint(|z|/(double(10)^300)>1.4)','\tprint(math.abs(3+i4))'),
            '1.0 + i0.0\ntrue\n5.0\n')

    def test_input_prefix_suffix_real_only_and_scientific(self):
        source=program('int main();','\tcomplex_128 z','\tinput(z)','\tprint(z)')
        for text,expected in [('3+i4','3.0 + i4.0'),('3+4i','3.0 + i4.0'),
            ('-i4','0.0 - i4.0'),('4i','0.0 + i4.0'),('3 - i4','3.0 - i4.0'),
            ('3','3.0 + i0.0'),('3e2+i4e-1','300.0 + i0.4'),
            ('3+4e2i','3.0 + i400.0'),('-3-i4e+2','-3.0 - i400.0'),('1e2+3i','100.0 + i3.0')]:
            self.check_program(source,expected+'\n',stdin=text+'\n')

    def test_input_value_context_and_mixed_pattern_fields(self):
        self.check_program(program('int main();','\tcomplex z=input()','\tint n',
            '\tinput("{z}, {n}")','\tprint("{z}|{n}")'), '5.0 - i6.0|-2\n',stdin='3+i4\n5-6i, -2\n')
        self.check_program(program('int main();','\tcomplex z','\tint n','\tinput("{z} {n}")',
            '\tprint("{z}|{n}")'), '3.0 + i0.0|-2\n',stdin='3 -2\n')

    def test_input_failure_is_atomic_and_preserves_previous_values(self):
        source=program('int main();','\tcomplex z=3+i4','\tint n=7','\tprint(input("{z}, {n}"))',
            '\tprint("{z}|{n}")')
        self.check_program(source,'false\n3.0 + i4.0|7\n',stdin='5+i6, invalid\n')
        for text in ['bad','3+i','3+4','i-4','4ii','3+i1e400','i4extra']:
            self.check_program(program('int main();','\tcomplex z=3+i4','\tprint(input(z))','\tprint(z)'),
                'false\n3.0 + i4.0\n',stdin=text+'\n')

    def test_complex64_input_range_and_scientific_component_lexer(self):
        self.check_program(program('int main();','\tcomplex z','\tprint(input(z))','\tprint(z)'),
            'false\n0.0 + i0.0\n',stdin='i1e100\n')
        self.check_program(program('int main();','\tprint(i4e2)','\tprint(4e-1i)'), '0.0 + i400.0\n0.0 + i0.4\n')

    def test_file_write_and_print_use_the_same_complex_format(self):
        path=str(self.directory/'value.txt')
        self.check_program(program('int main();','\tcomplex z=3-i4',f'\tfile f=io.file.open("{path}","w")',
            '\tio.file.write(f,"value={z}")','\tio.file.close(f)',
            f'\tf=io.file.open("{path}","r")','\tprint(io.file.read(f))','\tio.file.close(f)'), 'value=3.0 - i4.0\n')

    def test_imported_constants_and_private_model_members(self):
        (self.directory/'signal.qy').write_text(program('const complex_128 Z=3+i4','square(x)=x*x'))
        self.check_program(program('import signal','int main();','\tprint(signal.square(signal.Z))'), '-7.0 + i24.0\n')
        self.compile_source(program('model Secret;','\tprivate complex z=3+i4','int main();',
            '\tSecret value','\tprint(value.z.real)'),error='private')

    def test_many_allocations_keep_complex_values_and_array_elements_alive(self):
        self.check_program(program('int main();','\tcomplex values[]=[3+i4]','\tcomplex saved=values[0]',
            '\tfor(0 <= i < 1000,i++);','\t\tvalues[0]+=1','\tprint(saved)','\tprint(values)',
            '\tprint(values[0]^2)'), '3.0 + i4.0\n[1003.0 + i4.0]\n1005993.0 + i8024.0\n')


    def test_square_root_principal_branch_and_constant_runtime_agreement(self):
        self.check_program(program('const complex_128 R=math.sqrt(3+i4)', 'int main();',
            '\tcomplex_128 z=3+i4','\tprint(math.sqrt(z))','\tprint(R==math.sqrt(z))',
            '\tprint(math.sqrt(complex_128(-4)))','\tprint(math.sqrt(complex_128(3,-4)))',
            '\tprint(math.sqrt(complex()))'),
            '2.0 + i1.0\ntrue\n0.0 + i2.0\n2.0 - i1.0\n0.0 + i0.0\n')

    def test_nan_signed_zero_and_infinity_have_numeric_comparison_rules(self):
        self.check_program(program('int main();','\tdouble zero=0.0','\tcomplex_128 z=complex_128(zero/zero,1)',
            '\tprint(z==z)','\tprint(z!=z)','\tcomplex_128 p=complex_128(0.0,-0.0)',
            '\tprint(p==complex_128())','\tcomplex_128 huge=complex_128(1.0/zero,0)',
            '\tprint(|huge|)','\tprint(math.sqrt(huge))'),
            'false\ntrue\ntrue\ninf\ninf + i0.0\n')


    def test_named_fields_called_real_and_imag_still_work_on_models(self):
        self.check_program(program('model Point;','\tdouble real','\tdouble imag',
            'int main();','\tPoint p={real=3,imag=4}','\tp.real++','\tprint(p.real)'), '4.0\n')
        self.compile_source(program('model Sample;','\tcomplex z','int main();','\tSample s','\ts.z.real++'),error='読み取り専用')

    def test_constructor_defaults_and_value_model_copies_do_not_share_updates(self):
        self.check_program(program('model Sample;','\tcomplex z','\tvoid init(complex initial=3+i4);','\t\tthis.z=initial',
            'int main();','\tSample first','\tSample second=first','\tfirst.z++',
            '\tprint(first.z)','\tprint(second.z)'), '4.0 + i4.0\n3.0 + i4.0\n')

    def test_constant_arrays_can_be_borrowed_readonly_with_capacity_padding(self):
        self.check_program(program('const complex VALUES[2]=[3+i4]',
            'complex_128 last(ref readonly complex values[2])=values[1]',
            'int main();','\tprint(last(ref VALUES))'), '0.0 + i0.0\n')

    def test_reference_precision_must_match_and_conversions_are_explicit(self):
        self.compile_source(program('void bump(ref complex_128 z);','\tz++','int main();',
            '\tcomplex z=3+i4','\tbump(ref z)'),error='型')
        self.check_program(program('int main();','\tdouble n=16777217.0','\tcomplex z=complex(n)',
            '\tprint(z.real)'), '16777216.0\n')

    def test_extreme_negative_exponent_and_zero_exponents_terminate(self):
        self.check_program(program('const complex C=complex(1)^(-9223372036854775808)',
            'int main();','\tcomplex z=1','\tprint(C==z^(-9223372036854775808))',
            '\tprint((i1)^4)','\tprint((i1)^(-4))'), 'true\n1.0 + i0.0\n1.0 + i0.0\n')

    def test_scaled_square_root_handles_large_and_small_finite_components(self):
        self.check_program(program('int main();','\tdouble scale=double(10)^300',
            '\tfor(0 <= i < 2,i++);','\t\tcomplex_128 z=complex_128(scale,scale)',
            '\t\tcomplex_128 root=math.sqrt(z)','\t\tcomplex_128 ratio=(root*root)/z',
            '\t\tprint(|ratio.real-1|<0.000000001 and |ratio.imag|<0.000000001)',
            '\t\tscale=double(10)^(-300)'), 'true\ntrue\n')

    def test_input_into_nested_array_and_model_fields_is_atomic(self):
        self.check_program(program('model Sample;','\tcomplex z=3+i4','int main();','\tSample sample',
            '\tcomplex matrix[][]=[[5+i6]]','\tprint(input("{sample.z}, {matrix[0][0]}"))',
            '\tprint(sample.z)','\tprint(matrix)'), 'false\n3.0 + i4.0\n[[5.0 + i6.0]]\n',stdin='7+i8, invalid\n')
        self.check_program(program('int main();','\tcomplex matrix[1][1]','\tinput(matrix[0][0])',
            '\tprint(matrix)'), '[[3.0 + i4.0]]\n',stdin='3+4i\n')

    def test_scientific_input_extremes_overflow_and_delimiters(self):
        self.check_program(program('int main();','\tcomplex_128 z','\tinput(z)','\tprint(z.real>0 and z.imag<0)'),
            'true\n',stdin='1e300-i1e-300\n')
        self.check_program(program('int main();','\tcomplex_128 z','\tprint(input(z))'), 'false\n',stdin='i1e9999999999999999999999999\n')
        self.check_program(program('int main();','\tcomplex_128 z','\tinput(z)','\tprint(z)'), '0.0 + i0.0\n',stdin='0e999999999999999999999999999999\n')

    def test_prompts_and_inline_constructor_interpolation(self):
        self.check_program(program('int main();','\tprint("z={complex(real=3,imag=4)}")',
            '\tcomplex prompt=3+i4','\tstr name=input(prompt=prompt)','\tprint(name)'),
            'z=3.0 + i4.0\n3.0 + i4.0ok\n',stdin='ok\n')

    def test_complex_values_are_not_loop_counters_or_radix_fields(self):
        self.compile_source(program('int main();','\tfor(0 <= complex z <= 3,z++);','\t\tprint(z)'),error='for')
        for source in ['print("{i4:0x}")','complex z\n\tinput("{z:0x}")']:
            self.compile_source(program('int main();','\t'+source),error='基数')

if __name__=='__main__':run_suite()
