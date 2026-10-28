"""Typed logic/bit operators, 64-bit shifts, evaluation order and safety."""
import random
import subprocess
from support import QynoTestCase, ROOT, program, run_suite

MASK=(1<<64)-1
MIN=-(1<<63)
MAX=(1<<63)-1

def signed(n):
    n &= MASK
    return n-(1<<64) if n&(1<<63) else n

def literal(n): return 'int_64('+hex(n&MASK)+')'

class BitwiseTests(QynoTestCase):
    def test_integer_truth_tables_and_complements(self):
        self.check_program(program('int main();','\tint a=12','\tint b=10',
            '\tprint("{a and b} {a or b} {a xor b} {bnot a} {bnot 0} {bnot -1}")'), '8 14 6 -13 -1 0\n')

    def test_boolean_truth_tables(self):
        lines=['int main();']; expected=[]
        for a in (False,True):
            for b in (False,True):
                for op,value in [('and',a and b),('or',a or b),('xor',a!=b)]:
                    lines.append(f'\tprint({str(a).lower()} {op} {str(b).lower()})');expected.append(str(value).lower())
        self.check_program(program(*lines),'\n'.join(expected)+'\n')

    def test_bool_short_circuits_while_int_and_xor_are_eager(self):
        self.check_program(program('int integer(ref int count);','\tcount++','\treturn 3','bool boolean(ref int count);','\tcount++','\treturn true',
            'int main();','\tint count','\tprint(false and boolean(ref count))','\tprint(true or boolean(ref count))',
            '\tprint(count)','\tprint(0 and integer(ref count))','\tprint(-1 or integer(ref count))',
            '\tprint(true xor boolean(ref count))','\tprint(count)'), 'false\ntrue\n0\n0\n-1\nfalse\n3\n')

    def test_evaluation_order_and_lhs_snapshot(self):
        self.check_program(program('int change(ref int n);','\tn=8','\tprint("right")','\treturn 1',
            'int main();','\tint n=3','\tprint(n << change(ref n))','\tprint(n)','\tn=3','\tprint(n xor change(ref n))'), 'right\n6\n8\nright\n2\n')

    def test_shift_boundaries_sign_and_discarded_bits(self):
        self.check_program(program('int main();','\tint n=-3','\tprint(n >> 1)','\tprint(n / 2)',
            '\tprint(int_64(1) << 63)','\tprint(0x8000000000000000 << 1)','\tprint(0xffffffffffffffff >> 63)',
            '\tprint(0x7fffffffffffffff >> 63)','\tprint(-1 << 1)','\tprint(n << 0)','\tprint(n >> 0)'),
            '-2\n-1\n-9223372036854775808\n0\n-1\n0\n-2\n-3\n-3\n')

    def test_precedence_and_associativity(self):
        self.check_program(program('int main();','\tprint(1 << 2 + 1)','\tprint(8 >> 1 >> 1)',
            '\tprint(1 or 2 and 4)','\tprint(bnot 2^3)','\tprint(-2^2)','\tprint((12 and 8) != 0)',
            '\tprint(true or false xor true)','\tprint(true xor true xor true)','\tprint(1 == 1 xor 2 == 3)'),
            '8\n2\n1\n-9\n-4\ntrue\ntrue\ntrue\ntrue\n')
        self.compile_source(program('int main();','\tprint(12 and 8 != 0)'),error='混在')

    def test_type_mixing_and_noninteger_operands_rejected(self):
        for expression in ['true and 1','0 or false','true xor 1','1.0 and 2.0','"a" or "b"',
                           'bnot true','not 1','1 << true','true >> 1','1 << 1.0','1.0 >> 1','[1] xor [2]']:
            with self.subTest(expression=expression): self.compile_source(program('int main();','\tprint('+expression+')'),error='エラー')

    def test_invalid_constant_shift_amounts_even_with_dynamic_lhs_or_dead_branch(self):
        for n in [-1,64,65,MIN,MAX]:
            for op in ['<<','>>']:
                self.compile_source(program('int main();','\tint value=1',f'\tprint(value {op} {literal(n)})'),error='シフト量')
        self.compile_source(program('int main();','\tif(false);','\t\tprint(1 << (32+32))'),error='シフト量')
        self.compile_source(program('int main();','\tprint(true or ((1 << 64) == 0))'),error='シフト量')

    def test_dynamic_invalid_amount_and_location(self):
        for n in [-1,64,65,MIN,MAX]:
            for op in ['<<','>>']:
                result=self.check_program(program('int main();','\tint value=1',f'\tint_64 amount={literal(n)}',f'\tprint(value {op} amount)'),'',status=70,error='シフト量')
                self.assertIn(':4:',result.stderr.decode())

    def test_zero_lhs_does_not_hide_dynamic_errors_or_side_effects(self):
        self.check_program(program('int amount();','\tprint("amount")','\treturn 64','int main();','\tprint(0 << amount())'),
            'amount\n',status=70,error='シフト量')
        self.check_program(program('int amount();','\tprint("amount")','\treturn 64','int main();',
            '\tprint(false and ((1 << amount()) != 0))'), 'false\n')

    def test_lhs_failure_prevents_rhs_evaluation(self):
        self.check_program(program('int left();','\tint zero=0','\treturn 1/zero','int right();','\tprint("right")','\treturn 64',
            'int main();','\tprint(left() << right())'),'',status=70,error='0')

    def test_constant_results_match_runtime_and_python(self):
        rng=random.Random(20261027); lines=['int main();']; expected=[]
        for i in range(35):
            a=rng.randrange(MIN,MAX+1);b=rng.randrange(MIN,MAX+1);n=rng.choice([0,1,7,31,62,63])
            lines += [f'\tint_64 a{i}={literal(a)}',f'\tint_64 b{i}={literal(b)}',f'\tint n{i}={n}']
            for op,value in [('and',signed(a&b)),('or',signed(a|b)),('xor',signed(a^b))]:
                lines += [f'\tconst int_64 c{i}_{op}={literal(a)} {op} {literal(b)}',f'\tprint(a{i} {op} b{i})',f'\tprint(c{i}_{op})']; expected += [str(value)]*2
            for op,value in [('<<',signed(a<<n)),('>>',a>>n)]:
                lines.append(f'\tprint(a{i} {op} n{i})');expected.append(str(value))
        self.check_program(program(*lines),'\n'.join(expected)+'\n')

    def test_chain_comparisons_evaluate_shared_middle_once(self):
        self.check_program(program('int next(ref int n);','\tn++','\treturn n','int main();','\tint n',
            '\tprint(0 < (next(ref n) << 1) < 3)','\tprint(n)','\tprint(3 < 2 < (next(ref n) << 1))','\tprint(n)'), 'true\n1\nfalse\n1\n')

    def test_for_bounds_increments_and_arrays(self):
        self.check_program(program('int main();','\tint a[1 << 3]','\tint total','\tfor(0 <= i < 1 << 3, i += 1 << 1);',
            '\t\ta[i]=i','\t\ttotal += i','\tprint(total)','\tfor((1 << 3) > i >= 0, i--);','\t\ttotal++','\tprint(total)'), '12\n20\n')

    def test_models_readonly_initializers_and_interfaces(self):
        self.check_program(program('interface Flags;','\treadonly int current();','model Bits implements Flags;','\tprivate int bits=1 << 2',
            '\tvoid init(int mask=0b11 or 0b1000);','\t\tthis.bits = this.bits or mask',
            '\treadonly int current() = this.bits and 0xFF','int use(ref readonly Flags flags) = flags.current() xor 1',
            'int main();','\tBits b','\tprint(use(ref b))'), '14\n')

    def test_resource_analysis_distinguishes_eager_integer_operations(self):
        prefix=program('int close(file f);','\tio.file.close(f)','\treturn 1')
        for op in ['and','or','xor']:
            self.check_program(prefix+program('int main();',f'\tfile f=io.file.open("{self.directory}/{op}.txt","w")',f'\tprint(0 {op} close(f))'),str(0 if op=='and' else 1)+'\n')
        self.compile_source(program('bool close(file f);','\tio.file.close(f)','\treturn true','int main();',
            f'\tfile f=io.file.open("{self.directory}/leak.txt","w")','\tprint(false and close(f))'),error='close')

    def test_old_identifier_names_and_reserved_new_keywords(self):
        self.check_program(program('int band(int bor, int bxor) = bor+bxor','int main();','\tprint(band(1,2))'),'3\n')
        for name in ['bnot','xor']:
            self.compile_source(program('int main();',f'\tint {name}=1'),error='エラー')

    def test_shifts_are_not_comparisons_or_compound_assignments(self):
        self.check_program(program('int main();','\tprint(8 >> 1 >= 0)','\tprint(1 << 2 < 5)','\tprint(|-3|^2)'), 'true\ntrue\n9\n')
        for statement in ['int n=1\n\tn <<= 2','int n=1\n\tn >>= 2','int n=1\n\tn and=2','print(1 > > 2)','print(1 >>> 2)']:
            self.compile_source(program('int main();','\t'+statement),error='エラー')

    def test_example_and_platform_outputs(self):
        source=(ROOT/'examples/numbers/bitwise.qy').read_text()
        self.check_program(source,'true\n5\n10\n2\n8\n240\nflags=0b101\n')
        path=self.directory/'example.qy';path.write_text(source)
        exe=path.with_suffix('.exe');asm=path.with_suffix('.s')
        subprocess.run([str(self.compiler),str(path),'--target','windows','-o',str(exe)],check=True,capture_output=True,timeout=10)
        self.assertEqual(exe.read_bytes()[:2],b'MZ')
        subprocess.run([str(self.compiler),str(path),'-S','-o',str(asm)],check=True,capture_output=True,timeout=10)
        subprocess.run(['gcc','-x','assembler','-c',str(asm),'-o',str(path.with_suffix('.o'))],check=True,capture_output=True,timeout=10)

if __name__=='__main__': run_suite()
