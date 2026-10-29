"""Typed random APIs, range syntax, ownership and automatic OS seeding."""
import os, shutil, struct, subprocess, unittest
from support import program,run_suite
from time_support import PlatformCase,WINDOWS,inject_windows_api_results
from random_engine_test import seeded,word,draw

class RandomTests(PlatformCase):
    def main(self,*lines,prefix='using lib.random as random\n'):
        return super().main(*lines,prefix=prefix)

    def test_integer_types_have_matching_widths_and_reference_values(self):
        names=[('int_8',-8),('int_16',-16),('int',-32),('int_32',-32),('int_64',-64),('int_128',-128),
            ('unsigned_int_8',8),('unsigned_int_16',16),('unsigned_int',32),('unsigned_int_32',32),('unsigned_int_64',64),('unsigned_int_128',128)]
        for name,ty in names:
            expected=draw(seeded(0),abs(ty))
            if ty<0 and expected>>(abs(ty)-1):expected-=1<<abs(ty)
            self.check_program(self.main('random.seed(0)',f'auto value=random.{name}()',
                'io.print(value)','io.print(lib.size(value))'),f'{expected}\n{abs(ty)}\n')

    def test_four_boundary_forms_and_reversed_ranges(self):
        for expression,low,high in [('0 < i < 5',1,4),('0 <= i < 5',0,4),('0 < i <= 5',1,5),('0 <= i <= 5',0,5),('5 > i >= 0',0,4),('5 >= i > 0',1,5)]:
            self.check_program(self.main('random.seed(42)',f'int value=random.int({expression})',
                f'io.print({low} <= value <= {high})'),'true\n')

    def test_integer_bounds_intersect_return_type_without_narrowing(self):
        self.check_program(self.main('random.seed(42)','int_8 small=random.int_8(-200 <= n <= 200)',
            'unsigned int_8 value=random.unsigned_int_8(-1 < n <= 500)',
            'io.print(lib.size(small))','io.print(0 <= value <= 255)'), '8\ntrue\n')
        self.check_program(self.main('unsigned int_128 upper=340282366920938463463374607431768211455',
            'io.print(random.int_8(-128 <= x <= upper)>=-128)'), 'true\n')

    def test_placeholder_is_not_a_read_or_write_of_outer_variable(self):
        self.check_program(self.main('int i=99','int value=random.int(5 <= i <= 5)','io.print("{i} {value}")'),'99 5\n')
        self.compile_source(self.main('int value=random.int(0 < i < 10)','io.print(i)'),error='宣言')
        self.compile_source(self.main('int i=1','int value=random.int(i < i < 10)'),error='仮の名前')

    def test_bounds_evaluate_once_in_source_order_including_reverse(self):
        prefix='using lib.random as random\nint bound(int value);\n\tio.print(value)\n\treturn value\n'
        self.check_program(self.main('random.seed(42)','io.print(random.int(bound(10) > i >= bound(0)) < 10)',prefix=prefix),'10\n0\ntrue\n')

    def test_boolean_and_all_real_return_types_obey_open_endpoints(self):
        for name,width in [('half',16),('float',32),('double',64),('quadruple',128)]:
            self.check_program(self.main('random.seed(42)',f'auto value=random.{name}(0 < x < 1)',
                'io.print(0 < value < 1)','io.print(lib.size(value))'),f'true\n{width}\n')
        self.check_program(self.main('random.seed(42)','io.print(lib.size(random.bool()))'),'1\n')

    def test_real_reversed_ranges_singletons_and_large_endpoints(self):
        self.check_program(self.main('random.seed(42)','double value=random.double(1 > x >= 0)',
            'io.print(0 <= value < 1)','io.print(random.quadruple(0.125 <= x <= 0.125))',
            'quadruple huge=random.quadruple(-1e4900 < x < 1e4900)','io.print(-1e4900 < huge < 1e4900)'),
            'true\n0.125\ntrue\n')

    def test_default_model_seed_and_value_copy_are_independent(self):
        expected=word(seeded(0))
        self.check_program(self.main('random.Random first','random.Random copy=first',
            'io.print(first.unsigned_int_64())','io.print(copy.unsigned_int_64())'),f'{expected}\n{expected}\n')

    def test_model_seed_ref_and_value_parameters(self):
        expected=word(seeded(42))
        prefix='using lib.random as random\nunsigned int_64 by_value(random.Random r)=r.unsigned_int_64()\nunsigned int_64 by_ref(ref random.Random r)=r.unsigned_int_64()\n'
        self.check_program(self.main('random.Random r={seed=42}','io.print(by_value(r))','io.print(by_ref(ref r))',
            'r.seed(42)','io.print(r.unsigned_int_64())',prefix=prefix),f'{expected}\n{expected}\n{expected}\n')

    def test_model_arrays_fields_and_temporary_results(self):
        prefix='using lib.random as random\nmodel Holder;\n\trandom.Random generator\nrandom.Random make();\n\treturn {seed=42}\n'
        self.check_program(self.main('random.Random values[2]','Holder holder',
            'io.print(values[0].int(1 <= n <= 6))','io.print(values[1].int(1 <= n <= 6))',
            'io.print(holder.generator.int(1 <= n <= 6))','io.print(make().int(1 <= n <= 6))',prefix=prefix),
            '5\n5\n5\n1\n')

    def test_readonly_generator_and_private_state_are_protected(self):
        source=program('using lib.random as random','void inspect(ref readonly random.Random r);',
            '\tio.print(r.int())','int main();','\trandom.Random r','\tinspect(ref r)','\treturn 0')
        self.compile_source(source,error='readonly')
        self.compile_source(self.main('random.Random r','io.print(r.a)'),error='private')
        self.compile_source(self.main('random.Random r','lib.random._seed(ref r,42)'),error='内部関数')

    def test_character_graphemes_and_ascii_strings(self):
        self.check_program(self.main('random.seed(42)','char letter=random.char("👩🏽‍💻")',
            'io.print(letter)','io.print(letter.length)','str text=random.str(24,"AB012")',
            'io.print(text.length)','io.print(random.str(0))'), '👩🏽‍💻\n1\n24\n\n')
        self.check_program(self.main('io.print(random.str(6,"X"))','io.print(random.char("é"))'),'XXXXXX\né\n')

    def test_default_character_pool_has_printable_ascii_only(self):
        self.check_program(self.main('random.seed(42)','str text=random.str(100)','io.print(text.length)',
            'char letter=random.char()','io.print(letter.length)'), '100\n1\n')

    def test_shared_and_independent_generators_do_not_share_state(self):
        value=word(seeded(42))
        self.check_program(self.main('random.seed(42)','random.Random local={seed=42}',
            'io.print(random.unsigned_int_64())','io.print(local.unsigned_int_64())'),f'{value}\n{value}\n')

    def test_singletons_and_empty_strings_do_not_consume_values(self):
        value=word(seeded(42))
        self.check_program(self.main('random.seed(42)','random.int(5 <= i <= 5)',
            'random.double(0 <= x <= 0)','random.str(0)','random.char("X")','random.str(3,"X")',
            'io.print(random.unsigned_int_64())'),f'{value}\n')

    def test_import_free_alias_using_lib_and_selective_seed(self):
        for prefix,call in [('', 'lib.random.int'),('using lib\n','random.int'),('using lib.random as rng\n','rng.int')]:
            self.check_program(self.main(f'io.print({call}(1 <= n <= 1))',prefix=prefix),'1\n')
        self.check_program(self.main('seed(42)','io.print(lib.random.int(1 <= n <= 6))',prefix='using lib.random.seed\n'),'1\n')
        self.compile_source(self.main('return 0',prefix='using lib.random\n'),error='モジュール別名')
        self.compile_source(self.main('return 0',prefix='using lib.random.int\n'),error='モジュール別名')

    def test_plain_casts_and_old_logical_comparisons_are_unchanged(self):
        self.check_program(self.main('int i=3','io.print(0 < i < 5)','io.print(int(4.8))',
            'io.print(str(42))','io.print(char("A"))'), 'true\n4\n42\nA\n')

    def test_invalid_ranges_types_and_old_api_names_are_rejected(self):
        for expression in ['random.int(0 < i < 1)','random.int(5 <= i <= 4)','random.double()',
                'random.int(1,6)','random.int(true)','random.int(0 < i > 5)',
                'random.int(0 < i < 5 < 10)','random.int(0 < i[0] < 10)',
                'random.int(0.5 < i < 10)','random.bool(1)','random.str(-1)',
                'random.char("")','random.str(0,"")','random.str(4,"あ")']:
            self.compile_source(self.main('io.print('+expression+')'),error='乱数')
        for name in ['integer','real','real_quad','boolean','bits']:
            self.compile_source(self.main(f'io.print(random.{name}())'),error='定義')

    def test_dynamic_invalid_ranges_and_candidates_are_runtime_contract_errors(self):
        for body in [('int min=3','int max=2','random.int(min <= n <= max)'),
                ('int count=-1','random.str(count)'),('str pool=""','random.char(pool)'),
                ('double min=2.0','double max=1.0','random.double(min < n < max)')]:
            self.check_program(self.main(*body),'',status=70,error='乱数')

    def test_random_calls_are_not_const_expressions(self):
        self.compile_source(program('const int VALUE=lib.random.int(1 <= i <= 1)','int main();','\treturn 0'),error='const')

    def test_assembly_and_direct_execution_match(self):
        source=self.main('random.seed(42)','io.print(random.int(1 <= i <= 6))',
            'io.print(random.quadruple(0 < x < 1))','io.print(random.str(8,"AB01"))')
        binary=self.compile_source(source);expected=subprocess.check_output([str(binary)] if not WINDOWS else [os.environ['QYNO_FILE_WINDOWS_RUNNER'],str(binary)])
        path=self.directory/'asm.qy';path.write_text(source);asm=self.directory/'asm.s';exe=self.directory/'asm'
        subprocess.run([str(self.compiler),str(path),'-S','--target','linux','-o',str(asm)],check=True,capture_output=True)
        subprocess.run(['gcc','-nostdlib','-no-pie',str(asm),'-o',str(exe)],check=True,capture_output=True)
        self.assertEqual(subprocess.check_output([str(exe)]),expected)

    def test_windows_import_descriptors_have_separate_dll_terminators(self):
        path=self.directory/'pe.qy';path.write_text(self.main('random.seed(42)'))
        binary=self.directory/'pe.exe'
        subprocess.run([str(self.compiler),str(path),'--target','windows','-o',str(binary)],check=True,capture_output=True)
        data=binary.read_bytes();u16=lambda p:struct.unpack_from('<H',data,p)[0];u32=lambda p:struct.unpack_from('<I',data,p)[0]
        pe=u32(0x3c);optional=pe+24;sections=[]
        for i in range(u16(pe+6)):
            section=optional+u16(pe+20)+i*40;sections.append((u32(section+12),u32(section+20),u32(section+16)))
        def offset(rva):
            for base,raw,size in sections:
                if base<=rva<base+size:return raw+rva-base
            self.fail('unknown PE RVA')
        def string(rva):
            start=offset(rva);return data[start:data.index(0,start)].decode()
        descriptor=offset(u32(optional+120));libraries={}
        while u32(descriptor):
            names=[];index=0
            while True:
                entry=struct.unpack_from('<Q',data,offset(u32(descriptor)+index*8))[0]
                self.assertEqual(entry,struct.unpack_from('<Q',data,offset(u32(descriptor+16)+index*8))[0])
                if not entry:break
                names.append(string(entry+2));index+=1
            libraries[string(u32(descriptor+12)).lower()]=names;descriptor+=20
        self.assertEqual(set(libraries),{'kernel32.dll','bcrypt.dll','ntdll.dll'})
        self.assertEqual(libraries['bcrypt.dll'],['BCryptGenRandom'])
        self.assertEqual(libraries['ntdll.dll'],['RtlNtStatusToDosError'])

    def test_auto_seed_generates_valid_values(self):
        self.check_program(self.main('io.print(lib.size(random.bool()))','io.print(0 <= random.int(0 <= i < 10) < 10)'), '1\ntrue\n')

    @unittest.skipIf(WINDOWS,'Linux entropy syscall injection')
    def test_explicit_and_singleton_values_do_not_request_entropy(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        binary=self.compile_source(self.main('random.int(1 <= i <= 1)','random.double(0 <= x <= 0)',
            'random.str(0)','random.char("X")','try(random.bool()) as e;','\tio.print("wrong")',
            'case(io.ErrorCode.unsupported_operation);','\tio.print(e.operation)',
            'random.seed(42)','io.print(random.int(1 <= i <= 6))'))
        r=subprocess.run(['strace','-qq','-o',str(self.directory/'trace'),'--inject=getrandom:error=ENOSYS',str(binary)],capture_output=True,timeout=5)
        self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,b'bool\n1\n')

    @unittest.skipIf(WINDOWS,'Linux entropy syscall injection')
    def test_entropy_failure_is_retryable_and_reports_public_source(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        binary=self.compile_source(self.main('for(0 <= i < 2,i++);','\ttry(random.int()) as e;',
            '\t\tio.print("wrong")','\tcase(io.ErrorCode.system_error);','\t\tio.print("{e.operation} {e.path.length} {e.os_code}")'))
        r=subprocess.run(['strace','-qq','-o',str(self.directory/'trace'),'--inject=getrandom:error=EIO',str(binary)],capture_output=True,timeout=5)
        self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,b'int 0 5\nint 0 5\n')

    @unittest.skipIf(WINDOWS,'Linux entropy syscall injection')
    def test_entropy_interrupt_and_short_read_are_retried(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        for fault in ['error=EINTR','retval=4']:
            binary=self.compile_source(self.main('io.print(lib.size(random.bool()))'))
            r=subprocess.run(['strace','-qq','-o',str(self.directory/'trace'),f'--inject=getrandom:{fault}:when=1',str(binary)],capture_output=True,timeout=5)
            self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,b'1\n')

    @unittest.skipUnless(WINDOWS,'Windows BCrypt failure injection')
    def test_windows_entropy_status_mapping_and_explicit_seed(self):
        for os_code,name in [(50,'unsupported_operation'),(5,'system_error')]:
            source=self.main('try(random.int()) as e;','\tio.print("wrong")',f'case(io.ErrorCode.{name});',
                '\tio.print("{e.code} {e.operation} {e.path.length}")','random.seed(42)','io.print(random.int(1 <= i <= 6))')
            binary=self.compile_source(source);inject_windows_api_results(binary,{'BCryptGenRandom':0xc0000001,'RtlNtStatusToDosError':os_code})
            r=subprocess.run([os.environ['QYNO_FILE_WINDOWS_RUNNER'],str(binary)],capture_output=True,timeout=30)
            self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,f'{name} int 0\n1\n'.encode())

    def test_seed_maximum_and_model_array_copy_survive_allocations(self):
        value=word(seeded((1<<64)-1))
        self.check_program(self.main('random.seed(18446744073709551615)',
            'io.print(random.unsigned_int_64())','random.Random values[2]',
            'random.Random copy[]=values','values[0].seed(42)',
            'for(0 <= i < 200,i++);','\tstr text=random.str(100)',
            'io.print(copy[0].int(1 <= n <= 6))','io.print(values[0].int(1 <= n <= 6))'),f'{value}\n5\n1\n')

    def test_model_bounds_borrow_receiver_until_arguments_finish(self):
        prefix='using lib.random as random\nint grow(ref random.Random values[]);\n\tvalues.add()\n\treturn 10\n'
        self.check_program(self.main('random.Random values[1]',
            'values[0].int(0 <= i < grow(ref values))',prefix=prefix),'',status=70,error='参照')

    def test_local_generator_does_not_introduce_an_os_error_path(self):
        path=self.directory/'input';path.write_text('x')
        prefix=program('using lib.random as random','void safe();',f'\tfile f=io.file.open("{path}")',
            '\trandom.Random local','\tio.print(lib.size(local.bool()))','\tio.file.close(f)')
        self.check_program(self.main('try(safe());','\tio.print("ok")','case();','\tio.print("wrong")',prefix=prefix),'1\nok\n')

    def test_temporary_receiver_effects_are_analyzed_for_close(self):
        source=program('using lib.random as random','random.Random unsafe();',
            '\tfile f=io.file.open("input")','\tlib.random.bool()','\tio.file.close(f)',
            '\treturn {}','int main();','\ttry(unsafe().int());','\t\tio.print(0)',
            '\tcase(io.ErrorCode.system_error);','\t\tio.print("caught")','\treturn 0')
        self.compile_source(source,error='close')

    def test_entropy_effects_require_file_cleanup(self):
        source=program('void unsafe();','\tfile f=io.file.open("input")','\tlib.random.bool()',
            '\tio.file.close(f)','int main();','\ttry(unsafe());','\t\tio.print(0)',
            '\tcase(io.ErrorCode.system_error);','\t\tio.print("caught")','\treturn 0')
        self.compile_source(source,error='close')

if __name__=='__main__':run_suite()
