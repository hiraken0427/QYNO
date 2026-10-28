"""0.27.0: explicit standard names, payload sizes, digit counts and paths."""
import os
from support import QynoTestCase, program, run_suite

WINDOWS = os.environ.get('QYNO_TEST_WINDOWS') == '1'


class StandardLibraryTests(QynoTestCase):
    def main(self, *lines, prefix=''):
        return prefix + program('int main();', *('\t' + line for line in lines), '\treturn 0')

    def test_bare_standard_names_require_using(self):
        for call, hint in [('print(42)', 'using io'), ('input()', 'using io'),
                           ('sqrt(9)', 'using math.sqrt'), ('size(42)', 'using lib.size'),
                           ('length(42)', 'using lib.length'), ('basename("x")', 'using io.path.basename')]:
            with self.subTest(call=call):
                self.compile_source(self.main(call), error=hint)

    def test_import_does_not_introduce_short_names(self):
        self.compile_source(self.main('print(1)', prefix='import io\n'), error='using io')
        self.compile_source(self.main('size(1)', prefix='import lib\n'), error='using lib.size')

    def test_qualified_using_selective_and_aliases(self):
        forms=[('', 'io.print', 'lib.size', 'lib.length'),
               ('using io, lib\n','print','size','length'),
               ('using io.print, lib.size, lib.length\n','print','size','length'),
               ('using io as out, lib as info\n','out.print','info.size','info.length')]
        for prefix,out,size,length in forms:
            with self.subTest(prefix=prefix):
                self.check_program(self.main(f'{out}({size}(42))',f'{out}({length}(42))',prefix=prefix),'32\n2\n')

    def test_input_only_works_with_visible_using(self):
        self.check_program(self.main('int n','input(n)','print(n)',prefix='using io\n'),'42\n',stdin='42\n')
        self.compile_source(self.main('int n','input(n)',prefix='using io.print\n'),error='using io')
        self.compile_source(self.main('if(true);','\tusing io','\tprint(1)','print(2)'),error='using io')

    def test_using_child_namespaces_is_not_recursive(self):
        self.check_program(self.main('print(path.basename("data/a.txt"))',prefix='using io\n'),'a.txt\n')
        self.compile_source(self.main('join("a", "b")',prefix='using io\n'),error='using io.path.join')
        self.check_program(self.main('io.print(join("a", "b"))',prefix='using io.path\n'),'a/b\n')

    def test_using_duplicate_and_collision(self):
        self.check_program(self.main('io.print(size(1))',prefix='using lib, lib.size\n'),'32\n')
        self.compile_source(self.main('io.print(size(1))',prefix='int size(int x) = x\nusing lib.size\n'),error='衝突')

    def test_type_conversions_and_absolute_value_remain_language_features(self):
        self.check_program(self.main('io.print(int("42"))','io.print(|-3|)','io.print(str(4))'),'42\n3\n4\n')

    def test_integer_sizes_all_widths_and_signedness(self):
        for width in [8,16,32,64,128]:
            for sign in ['', 'unsigned ']:
                name=sign+'int_'+str(width)
                with self.subTest(type=name):
                    self.check_program(self.main(f'{name} n = 1','io.print(lib.size(n))','io.print(lib.size(n + n))'),f'{width}\n{width}\n')
        self.check_program(self.main('int n','unsigned int u','io.print(lib.size(n))','io.print(lib.size(u))'),'32\n32\n')

    def test_real_complex_boolean_and_unit_sizes(self):
        decls=[('half',16),('float',32),('double',64),('quadruple',128),('complex',64),('complex_64',64),('complex_128',128),('bool',1),('void',0),('Void',0)]
        for name,bits in decls:
            with self.subTest(type=name):
                self.check_program(self.main(f'{name} n','io.print(lib.size(n))'),f'{bits}\n')

    def test_text_size_and_grapheme_length(self):
        self.check_program(self.main('str a = "é"','str b = "é"','char c = "🇯🇵"',
            'io.print("{lib.size(a)} {lib.size(b)} {lib.size(c)}")',
            'io.print("{lib.length(a)} {lib.length(b)} {lib.length(c)}")',
            'str nul = "A\\0B"','io.print("{lib.size(nul)} {lib.length(nul)}")',
            'io.print("{lib.size(\"\")} {lib.length(\"\")}")'), '16 24 64\n1 1 1\n24 3\n0 0\n')

    def test_matrix_and_empty_array_sizes(self):
        self.check_program(self.main('int matrix[2][3]','int empty[]',
            'io.print("{lib.size(matrix)} {lib.size(matrix[0])} {lib.size(matrix[0][0])}")',
            'io.print("{lib.length(matrix)} {matrix.length} {lib.length(matrix[0])}")',
            'io.print(lib.size(empty))','empty.add(42)','io.print(lib.size(empty))'),
            '192 96 32\n2 2 3\n0\n32\n')

    def test_ragged_and_variable_length_arrays(self):
        self.check_program(self.main('int rows[][] = [[1,2],[3]]','str names[] = ["A", "あ", ""]',
            'bool flags[] = [true, false, true]','void units[4]',
            'io.print("{lib.size(rows)} {lib.length(rows)} {lib.length(rows[1])}")',
            'io.print("{lib.size(names)} {lib.size(flags)} {lib.size(units)}")'), '96 2 1\n32 3 0\n')

    def test_model_private_fields_and_model_arrays(self):
        prefix='model Item;\n\tprivate int n = 1\n\tstr text = "あ"\n'
        self.check_program(self.main('Item a','Item items[2]','io.print(lib.size(a))','io.print(lib.size(items))',prefix=prefix),'56\n112\n')

    def test_size_ref_and_readonly(self):
        prefix='int_64 measure(ref readonly int values[]) = lib.size(values)\n'
        self.check_program(self.main('int values[] = [1,2,3]','io.print(measure(ref values))',prefix=prefix),'96\n')

    def test_argument_evaluated_once(self):
        prefix='int next(ref int n);\n\tn++\n\treturn 42\n'
        self.check_program(self.main('int n','io.print(lib.size(next(ref n)))','io.print(lib.length(next(ref n)))','io.print(n)',prefix=prefix),'32\n2\n2\n')

    def test_constant_sizes_lengths_and_paths(self):
        prefix='const int VALUES[2][3] = [[1,2,3],[4,5,6]]\nconst int_64 BITS = lib.size(VALUES)\nconst int_64 DIGITS = lib.length(-123)\nconst str NAME = io.path.with_extension("dir/a.txt", ".bin")\n'
        self.check_program(self.main('io.print("{BITS} {DIGITS} {NAME}")',prefix=prefix),'192 3 dir/a.bin\n')

    def test_integer_digits_extremes(self):
        values=[('int_8','-128',3),('int','-2147483648',10),('int_64','-9223372036854775808',19),
                ('int_128','-170141183460469231731687303715884105728',39),
                ('unsigned int_128','340282366920938463463374607431768211455',39)]
        for name,literal,digits in values:
            with self.subTest(type=name):
                self.check_program(self.main(f'{name} n = {literal}','io.print(lib.length(n))'),f'{digits}\n')
        self.check_program(self.main('io.print(lib.length(0))','io.print(lib.length(-42))'),'1\n2\n')

    def test_real_digits_zero_fractions_and_exponents(self):
        for name in ['half','float','double','quadruple']:
            with self.subTest(type=name):
                self.check_program(self.main(f'{name} a = -0.0',f'{name} b = 0.125',f'{name} c = -123.75',f'{name} d = 1000.0',
                    'io.print("{lib.length(a)} {lib.length(b)} {lib.length(c)} {lib.length(d)}")'),'1 1 3 4\n')
        self.check_program(self.main('double large = 1e308','quadruple huge = 1e4932','quadruple small = 1e-4900',
            'io.print("{lib.length(large)} {lib.length(huge)} {lib.length(small)}")'),'309 4933 1\n')

    def test_real_digits_near_exact_power(self):
        self.check_program(self.main('double a = 999.9999999999999','double b = 1000.0','double c = 1000.0000000000001',
            'quadruple q = 999.9999999999999999999999999999999',
            'io.print("{lib.length(a)} {lib.length(b)} {lib.length(c)} {lib.length(q)}")'),'3 4 4 3\n')

    def test_nonfinite_length_is_error(self):
        for expr in ['1.0 / zero','-1.0 / zero','zero / zero']:
            self.check_program(self.main('double zero = 0.0',f'double n = {expr}','io.print(lib.length(n))'),'',status=70,error='NaN・無限大')
        self.compile_source(self.main('io.print(lib.length(1.0 / 0.0))'),error='NaN・無限大')

    def test_unsupported_types_and_nested_types(self):
        for declaration,expr in [('bool b','b'),('complex z','z'),('void v','v')]:
            self.compile_source(self.main(declaration,f'io.print(lib.length({expr}))'),error='lib.length')
        for declaration,expr in [('file f','f'),('int n','n.addr'),('file files[]','files')]:
            self.compile_source(self.main(declaration,f'io.print(lib.size({expr}))'),error='lib.size')
        self.compile_source(self.main('Box b','io.print(lib.size(b))',prefix='model Box;\n\tprivate file f\n'),error='lib.size')

    def test_argument_counts_and_types(self):
        for expr in ['lib.size()','lib.size(1,2)','lib.length()','io.path.join("x")','io.path.stem("a","b")']:
            self.compile_source(self.main(expr),error='引数')
        self.compile_source(self.main('io.path.join("a", 42)'),error='str・char')
        self.compile_source(self.main('int n','lib.size(ref n)'),error='ref')

    def test_path_basic_functions(self):
        self.check_program(self.main('str p = "dir/archive.tar.gz"','io.print(io.path.join("dir", "a.txt"))',
            'io.print(io.path.basename(p))','io.print(io.path.dirname(p))','io.print(io.path.extension(p))',
            'io.print(io.path.stem(p))','io.print(io.path.with_extension(p, ".bak"))',
            'io.print(io.path.with_extension(p, ""))'), 'dir/a.txt\narchive.tar.gz\ndir\n.gz\narchive.tar\ndir/archive.tar.bak\ndir/archive.tar\n')

    def test_path_empty_root_and_trailing_separators(self):
        self.check_program(self.main('str empty = ""','str root = "/"','str dir = "a/b/"',
            'io.print("[{io.path.basename(empty)}] [{io.path.dirname(empty)}]")',
            'io.print("[{io.path.basename(root)}] [{io.path.dirname(root)}]")',
            'io.print("[{io.path.basename(dir)}] [{io.path.dirname(dir)}]")',
            'io.print(io.path.join("", "a"))','io.print(io.path.join("a", ""))'), '[] [.]\n[] [/]\n[b] [a]\na\na\n')

    def test_path_hidden_files_and_extension_rules(self):
        for name,stem,ext in [('.env','.env',''),('.config.json','.config','.json'),('file.','file','.'),('.','.',''),('..','..','')]:
            with self.subTest(name=name):
                self.check_program(self.main(f'str p = "{name}"','io.print("[{io.path.stem(p)}] [{io.path.extension(p)}]")'),f'[{stem}] [{ext}]\n')
        self.check_program(self.main('io.print(io.path.with_extension(".env", ".txt"))'),'.env.txt\n')

    def test_path_normalize_preserves_parent_segments(self):
        self.check_program(self.main('str p = "a//./link/../b/"','io.print(io.path.normalize(p))',
            'io.print(io.path.normalize(""))','io.print(io.path.normalize("./."))',
            'io.print(io.path.normalize("/a///b/"))'), 'a/link/../b\n.\n.\n/a/b\n')

    def test_path_native_absolute_rules_and_constant_evaluation(self):
        if WINDOWS:
            prefix='const bool A = io.path.is_absolute("C:/data")\nconst str P = io.path.normalize("C:\\\\a\\\\.\\\\b")\n'
            lines=['str p = "C:/data"','io.print(io.path.join("a", p))','io.print(io.path.is_relative("C:foo"))','io.print(io.path.is_absolute("/foo"))']
            expected='true C:/a/b\nC:/data\ntrue\nfalse\n'
        else:
            prefix='const bool A = io.path.is_absolute("/data")\nconst str P = io.path.normalize("//a/./b")\n'
            lines=['str p = "/data"','io.print(io.path.join("a", p))','io.print(io.path.is_relative("C:foo"))','io.print(io.path.is_absolute("/foo"))']
            expected='true //a/b\n/data\ntrue\ntrue\n'
        self.check_program(self.main('io.print("{A} {P}")',*lines,prefix=prefix),expected)

    def test_path_unicode_and_backslash(self):
        self.check_program(self.main('str p = "資料/日本😀.txt"','io.print(io.path.basename(p))','io.print(io.path.stem(p))'), '日本😀.txt\n日本😀\n')
        self.check_program(self.main('str p = "a\\\\b.txt"','io.print(io.path.basename(p))'),'b.txt\n' if WINDOWS else 'a\\b.txt\n')

    def test_path_bad_extensions_and_nul(self):
        for path,ext in [('a.txt','txt'),('a.txt','.a/b'),('/','.txt'),('','.txt'),('..','.txt')]:
            self.compile_source(self.main(f'io.path.with_extension("{path}", "{ext}")'),error='io.path の引数')
        self.compile_source(self.main('io.path.basename("a\\0b")'),error='io.path の引数')
        self.check_program(self.main('str p = "a\\0b"','io.print(io.path.basename(p))'),'',status=70,error='io.path の引数')
        self.check_program(self.main('str p = "a"','str ext = "bad"','io.print(io.path.with_extension(p, ext))'),'',status=70,error='io.path の引数')

    def test_size_preserves_void_call_side_effects(self):
        self.check_program(self.main('int values[]','io.print(lib.size(values.add(7)))','io.print(values)'), '0\n[7]\n')

    def test_constant_model_size_and_text_array_size(self):
        prefix='model Entry;\n\tprivate int n=1\n\tstr name="あ"\nconst Entry E={}\nconst int_64 BITS=lib.size(E)\nconst str NAMES[]=["A","あ"]\nconst int_64 TOTAL=lib.size(NAMES)\n'
        self.check_program(self.main('io.print("{BITS} {TOTAL}")',prefix=prefix),'56 32\n')

    def test_windows_unc_and_drive_relative_paths(self):
        if not WINDOWS:
            self.check_program(self.main('str p = "C:/a"','io.print(io.path.is_absolute(p))'),'false\n')
            return
        self.check_program(self.main('str p = "//server/share/dir/name.txt"',
            'io.print(io.path.is_absolute(p))','io.print(io.path.dirname(p))',
            'io.print(io.path.dirname("//server/share/"))','io.print(io.path.basename("C:/"))'),
            'true\n//server/share/dir\n//server/share/\n\n')
        for child in ['C:foo','/foo']:
            self.check_program(self.main(f'str child = "{child}"','io.print(io.path.join("base",child))'),'',status=70,error='io.path の引数')
        self.compile_source(self.main('io.path.join("base", "C:foo")'),error='io.path の引数')

    def test_path_invalid_utf8_is_diagnosed(self):
        self.check_program(self.main('str p = io.input()','io.print(io.path.basename(p))'),'',stdin=b'\xff\n',status=65,error='UTF-8')


if __name__ == '__main__':
    run_suite()
