"""Base literals, conversions, interpolation and atomic typed input."""
import random
from support import QynoTestCase, ROOT, program, run_suite
MIN=-(1<<63)
MAX=(1<<63)-1
MASK=(1<<64)-1

def formatted(n,base,prefix):
    if base==10: return ('-' if n<0 else '')+('0d' if prefix else '')+str(abs(n))
    return ({2:'0b',8:'0o',16:'0x'}[base] if prefix else '')+format(n&MASK,{2:'b',8:'o',16:'x'}[base])

class IntegerBasesTests(QynoTestCase):
    def test_four_base_literals_and_default_decimal(self):
        self.check_program(program('int main();','\tprint(15 == 0b1111 and 0o17 == 0d15 and 15 == 0xF)',
            '\tprint(010)','\tprint(0B1111)','\tprint(0O17)','\tprint(0D15)','\tprint(0Xf)'), 'true\n10\n15\n15\n15\n15\n')

    def test_literal_boundaries_and_decimal_minimum(self):
        expressions=['0x7fffffffffffffff','0x8000000000000000','0xffffffffffffffff','0o1000000000000000000000',
            '0o1777777777777777777777','-9223372036854775808','-0d9223372036854775808','-0xff','-0xffffffffffffffff']
        values=[MAX,MIN,-1,MIN,-1,MIN,MIN,-255,1]
        self.check_program(program('int main();',*[f'\tprint({e})' for e in expressions]),''.join(str(v)+'\n' for v in values))

    def test_invalid_literals_and_unsigned_decimal_rejected(self):
        for value in ['0b102','0o8','0d1A','0xGG','0b','0o','0d','0x','0x10000000000000000',
            '18446744073709551615','0d18446744073709551615','9223372036854775808','+9223372036854775808',
            '-9223372036854775809','0b1_0','0x1.2','0o2000000000000000000000']:
            with self.subTest(value=value): self.compile_source(program('int main();',f'\tprint({value})'),error='エラー')

    def test_leading_zeroes_do_not_count_as_extra_bits(self):
        self.check_program(program('int main();','\tprint(0x000000000000000000000000000000000f)','\tprint(0b00000000000000000000000000000000000000000001)',
            '\tprint(0d0000000000015)'), '15\n1\n15\n')

    def test_integer_formatting_all_bases_signs_zero_and_boundaries(self):
        values=[0,1,15,255,MAX,MIN,-1,-255];lines=['int main();'];expected=[]
        for i,n in enumerate(values):
            lines.append(f'\tint n{i}={hex(n&MASK)}')
            for base,code in [(2,'0b'),(8,'0o'),(10,'0d'),(16,'0x')]:
                lines.append(f'\tprint("{{n{i}:{code}}}")');expected.append(formatted(n,base,True))
        self.check_program(program(*lines),'\n'.join(expected)+'\n')

    def test_str_conversion_positional_named_and_default(self):
        self.check_program(program('int main();','\tint n=255','\tprint(str(n))','\tprint(str(n,2))',
            '\tprint(str(n,8,true))','\tprint(str(n,base=16,prefix=true))','\tprint(str(n,prefix=true,base=10))',
            '\tprint(str("hello"))','\tprint(str(char("A")))'), '255\n11111111\n0o377\n0xff\n0d255\nhello\nA\n')

    def test_int_parsing_automatic_and_fixed_base(self):
        lines=['int main();'];values=[]
        for text,base,expected in [('15',None,15),('010',None,10),('0b1111',None,15),('0o17',None,15),('0d15',None,15),
            ('0xF',None,15),('ff',16,255),('0x10',16,16),('10',16,16),('0x0b10',16,2832),(' -0d15\t',None,-15),
            ('-0xffffffffffffffff',None,1),('-0xff',None,-255),('+0XFF',None,255)]:
            literal=text.replace('\t','\\t');suffix='' if base is None else f',base={base}'
            lines += [f'\tstr text{len(values)}="{literal}"',f'\tprint(int(text{len(values)}{suffix}))'];values.append(str(expected))
        self.check_program(program(*lines),'\n'.join(values)+'\n')

    def test_round_trips_all_int_values_samples(self):
        rng=random.Random(525);values=[MIN,MAX,-1,0,1,255]+[rng.randrange(MIN,MAX+1) for _ in range(12)]
        lines=['int main();'];expected=[]
        for i,n in enumerate(values):
            lines.append(f'\tint n{i}={hex(n&MASK)}')
            for base in [2,8,10,16]:
                lines += [f'\tstr s{i}_{base}=str(n{i},{base},true)',f'\tprint(s{i}_{base})',f'\tprint(int(s{i}_{base}) == n{i})',
                    f'\tprint(int(str(n{i},{base}),{base}) == n{i})']
                expected += [formatted(n,base,True),'true','true']
        self.check_program(program(*lines),'\n'.join(expected)+'\n')

    def test_invalid_conversion_arguments_and_types(self):
        expressions=['str()','int()','str(base=2)','int(base=16)','str(1,base=2,base=8)','str(1,prefix=true,false)',
            'int("ff",prefix=true)','str(1,unknown=2)','str(1,2,true,4)','int("1",10,1)',
            'str(1,base=2.0)','str(1,prefix=1)','str(true)','str(1.5)','str("x",base=10)','int(1,base=10)',
            'str(1,base=0)','str(1,base=3)','int("1",base=3)','int("1",base=0xffffffffffffffff)']
        for expression in expressions:
            with self.subTest(expression=expression): self.compile_source(program('int main();','\tprint('+expression+')'),error='エラー')

    def test_dynamic_invalid_base_is_error70_before_parsing(self):
        for expression in ['str(1,base)','int("invalid",base)']:
            self.check_program(program('int main();','\tint base=3','\tprint('+expression+')'),'',status=70,error='base')

    def test_dynamic_invalid_integer_data_is_error65(self):
        for text in ['', '+','0x','0b102','0o8','0d1a','0x10000000000000000','9223372036854775808',
                     '-9223372036854775809','12.0','1e2','1_0','1 2','１２','1\\0']:
            with self.subTest(text=text): self.check_program(program('int main();',f'\tstr text="{text}"','\tprint(int(text))'),'',status=65,error='int')
        self.check_program(program('int main();','\tstr text="0b10"','\tprint(int(text,16))'),'',status=65,error='int')

    def test_invalid_constant_conversion_data_is_compile_error(self):
        for expr in ['int("bad")','int("0b10",16)','int("9223372036854775808")','int("0o8")']:
            self.compile_source(program('int main();','\tprint('+expr+')'),error='読み取れません')

    def test_const_conversion_and_model_defaults(self):
        self.check_program(program('const str HEX=str(255,base=16,prefix=true)','const int N=int(HEX)',
            'model Data;','\tstr text=str(N,2,true)','\tint n=int("0o17")','int main();','\tData data',
            '\tprint(HEX)','\tprint(data.text)','\tprint(data.n)'), '0xff\n0b11111111\n15\n')

    def test_argument_evaluation_order_and_value_snapshot(self):
        self.check_program(program('bool prefix(ref int n);','\tprint("prefix")','\tn=8','\treturn true',
            'int base();','\tprint("base")','\treturn 16','int main();','\tint n=255',
            '\tprint(str(n,prefix=prefix(ref n),base=base()))','\tprint(n)'), 'prefix\nbase\n0xff\n8\n')

    def test_formatting_expressions_and_end_argument(self):
        self.check_program(program('int next(ref int n);','\tn++','\treturn n','int main();','\tint n=15',
            '\tprint("{next(ref n):0x}",end=" {n:0d}\\n")','\tprint("{ 0xF : 0b }")'), '0x10 0d16\n0b1111\n')

    def test_string_expressions_inside_interpolation_do_not_confuse_colons_and_braces(self):
        self.check_program(program('int main();', '\tprint("{int(\'0xff\'):0d}")',
            '\tprint("{str(\'x:y\')}")','\tprint("{str(\'\\{x:y\\}\')}")'), '0d255\nx:y\n{x:y}\n')

    def test_format_type_and_syntax_errors(self):
        for expr in ['"{1:x}"','"{1:16}"','"{1:0X}"','"{1:}"','"{1:0x:0d}"',
            '"{true:0b}"','"{1.0:0d}"','"{[1]:0x}"']:
            self.compile_source(program('int main();','\tprint('+expr+')'),error='エラー')
        self.compile_source(program('int main();','\tstr s="{1:0x}"'),error='print')

    def test_input_base_selection_optional_prefix_and_automatic_detection(self):
        for code,text,value in [('0b','1111',15),('0b','0B1111',15),('0o','17',15),('0o','0o17',15),
            ('0d','15',15),('0d','0D15',15),('0x','ff',255),('0x','0Xff',255),('0x','255',597)]:
            self.check_program(program('int main();','\tint n',f'\tinput("{{n:{code}}}")','\tprint(n)'),str(value)+'\n',stdin=text+'\n')
        for text,value in [('0b1111',15),('0o17',15),('0d15',15),('0xf',15),('010',10),('-0xff',-255),
                           ('0xffffffffffffffff',-1),('-9223372036854775808',MIN)]:
            for call in ['input(n)','input("{n}")','n=input()']:
                self.check_program(program('int main();','\tint n','\t'+call,'\tprint(n)'),str(value)+'\n',stdin=text+'\n')

    def test_fixed_base_mismatch_invalid_digit_overflow_and_eof_preserve_values(self):
        for code,text in [('0x','0b1111'),('0x','gg'),('0b','102'),('0o','8'),('0d','0xf'),('0x','10000000000000000'),
                          ('0d','9223372036854775808'),('0x','')]:
            self.check_program(program('int main();','\tint n=7',f'\tprint(input("{{n:{code}}}"))','\tprint(n)'),
                'false\n7\n',stdin=text+'\n' if text else '')

    def test_multiple_input_fields_are_atomic(self):
        source=program('int main();','\tint a=7','\tint b=8','\tprint(input("{a:0b}, {b:0x}"))','\tprint("{a} {b}")')
        self.check_program(source,'true\n10 255\n',stdin='1010, ff\r\n')
        self.check_program(source,'false\n7 8\n',stdin='1010, gg\n')

    def test_input_model_array_and_ref_places_capture_indices_before_updates(self):
        self.check_program(program('model Data;','\tint n','int main();','\tint i=0','\tint values[]=[7,8]',
            '\tData data','\tref int alias=i.addr','\tinput("{alias.value:0d}, {values[i]:0x}, {data.n:0o}")',
            '\tprint("{i} {values} {data.n}")'), '1 [255, 8] 8\n',stdin='1, ff, 10\n')

    def test_input_format_types_readonly_and_const_rejected(self):
        for decl in ['bool n','str n','double n','const int n=1']:
            self.compile_source(program('int main();','\t'+decl,'\tinput("{n:0x}")'),error='エラー')
        self.compile_source(program('void read(ref readonly int n);','\tinput("{n:0x}")','int main();','\treturn 0'),error='readonly')

    def test_standalone_input_failure_and_value_input_failure_are_error65(self):
        self.check_program(program('int main();','\tint n=7','\tinput("{n:0x}")'),'',stdin='gg\n',status=65,error='入力')
        self.check_program(program('int main();','\tint n=input()'),'',stdin='0o8\n',status=65,error='入力')

    def test_input_string_then_explicit_conversion_and_prompt(self):
        self.check_program(program('int main();','\tint n=int(io.input("hex> "),base=16)','\tprint("{n:0x}, {n}")'),
            'hex> 0xff, 255\n',stdin='ff\n')
        self.check_program(program('int main();','\tint previous=15','\tint n=input(prompt=str(previous,16,true))','\tprint(n)'),
            '0xf16\n',stdin='0x10\n')

    def test_file_write_interpolation_uses_same_formats(self):
        path=self.directory/'result.txt'
        self.check_program(program('int main();',f'\tfile f=io.file.open("{path}","w")','\tint n=-1',
            '\tio.file.write(f,"{n:0x}, {n:0d}\\n")','\tio.file.close(f)'), '')
        self.assertEqual(path.read_text(),'0xffffffffffffffff, -0d1\n')

    def test_aliases_and_selected_io(self):
        self.check_program(program('using io as console','int main();','\tint n','\tconsole.input("{n:0x}")',
            '\tconsole.print("{n:0o}")'), '0o17\n',stdin='f\n')

    def test_interface_specialization_keeps_conversion_options_and_format_metadata(self):
        self.check_program(program('interface Value;','\treadonly int get();','model A implements Value;','\treadonly int get()=255',
            'model B implements Value;','\treadonly int get()=16','void show(ref readonly Value v);',
            '\tprint("{v.get():0x}")','\tstr s=str(v.get(),prefix=true,base=2)','\tprint(int(s))',
            'int main();','\tA a','\tB b','\tshow(ref a)','\tshow(ref b)'), '0xff\n255\n0x10\n16\n')

    def test_frequent_conversions_do_not_lose_gc_roots(self):
        self.check_program(program('int main();','\tint total','\tfor(0 <= i < 600, i++);',
            '\t\tstr text=str(i,16,true)','\t\ttotal += int(text)','\tprint(total)'), '179700\n')

    def test_input_round_trips_negative_and_boundary_formats(self):
        for base,code in [(2,'0b'),(8,'0o'),(10,'0d'),(16,'0x')]:
            values=[MIN,MAX,-1,0,1]
            source=program('int main();','\tint n',*[line for _ in values for line in [f'\tinput("{{n:{code}}}")','\tprint(n)']])
            self.check_program(source,''.join(str(n)+'\n' for n in values),stdin=''.join(formatted(n,base,True)+'\n' for n in values))

    def test_conversion_optional_arguments_preserve_resource_effects(self):
        self.check_program(program('bool release(file f);','\tio.file.close(f)','\treturn true','int base(file f);',
            '\tio.file.close(f)','\treturn 16','int main();',f'\tfile first=io.file.open("{self.directory}/a.txt","w")',
            f'\tfile second=io.file.open("{self.directory}/b.txt","w")',
            '\tprint(str(255,prefix=release(first),base=base(second)))'),'0xff\n')

    def test_generated_default_recipes_are_not_constant_cached(self):
        self.check_program(program('model Seed;','\tint n','\tvoid init();','\t\tprint("seed")','\t\tthis.n=15',
            'model Data;','\tstr text=str(Seed().n,16,true)','\tint bits=Seed().n and 3',
            'int main();','\tData first','\tData second','\tprint(first.text)','\tprint(second.bits)'),
            'seed\nseed\nseed\nseed\n0xf\n3\n')

    def test_formatted_input_revalidates_places_before_atomic_commit(self):
        self.check_program(program('int clear(ref int a[]);','\ta.clear()','\treturn 0','int main();',
            '\tint a[]=[7]','\tinput("{a[0]:0x}, {a[clear(ref a)]:0o}")'),'',stdin='f, 7\n',status=70,error='範囲外')

    def test_complete_example(self):
        self.check_program((ROOT/'examples/numbers/bases.qy').read_text(),
            'number> binary  : 0b1111\noctal   : 0o17\ndecimal : 15\nexplicit: 0d15\nhex     : 0xf\ntrue\n',stdin='0o17\n')

if __name__=='__main__': run_suite()
