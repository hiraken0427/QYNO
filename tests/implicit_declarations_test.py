"""Implicit local declarations, lexical binding, warnings and permissions."""
from support import QynoTestCase, program, run_suite


class ImplicitDeclarationTests(QynoTestCase):
    def test_scalar_literals_and_expression_types(self):
        self.check_program(program('int main();', '\tn = 3', '\td = 0.5',
            '\tf = float(1.5)', '\tc = char("A")', '\ts = "QYNO"', '\tb = true',
            '\tio.print("{n} {d} {f} {c} {s} {b}")'), '3 0.5 1.5 A QYNO true\n')

    def test_function_returns_and_existing_values_determine_type(self):
        self.check_program(program('int twice(int n) = n * 2', 'int main();',
            '\tint original = 3', '\tn = twice(original)', '\ttext = str(n, 16, true)',
            '\tio.print("{n} {text}")'), '6 0x6\n')

    def test_later_assignments_keep_the_original_type(self):
        for first, second in [('3', '"three"'), ('3', '3.5'), ('true', '1'), ('[1]', '[true]')]:
            with self.subTest(first=first, second=second):
                self.compile_source(program('int main();', '\tn = ' + first,
                    '\tn = ' + second, '\tio.print(n)'), error='変換できません')

    def test_existing_outer_binding_is_assigned(self):
        self.check_program(program('int main();', '\tn = 1', '\tif(true);',
            '\t\tn = n + 2', '\t\tif(true);', '\t\t\tn = 9', '\tio.print(n)'), '9\n')

    def test_branch_locals_do_not_escape_and_siblings_are_independent(self):
        self.check_program(program('int main();', '\tif(true);', '\t\tn = 1', '\t\tio.print(n)',
            '\tif(true);', '\t\tn = "two"', '\t\tio.print(n)'), '1\ntwo\n')
        self.compile_source(program('int main();', '\tif(true);', '\t\tn = 1',
            '\t\tio.print(n)', '\tio.print(n)'), error='宣言されていません')
        self.compile_source(program('int main();', '\tif(true);', '\t\tn = 1',
            '\telse;', '\t\tio.print(n)'), error='宣言されていません')

    def test_loop_body_declarations_and_outer_assignments(self):
        self.check_program(program('int main();', '\ttotal = 0', '\tfor(0 <= i < 3, i++);',
            '\t\tterm = i * 2', '\t\ttotal = total + term', '\tio.print(total)',
            '\twhile(total < 8);', '\t\tstep = 1', '\t\ttotal += step', '\tio.print(total)'), '6\n8\n')

    def test_explicit_inner_auto_uses_outer_value_in_initializer(self):
        self.check_program(program('int main();', '\tn = 3', '\tif(true);',
            '\t\tauto n = n + 1', '\t\tn = 5', '\t\tio.print(n)', '\tio.print(n)'), '5\n3\n')

    def test_explicit_inner_typed_and_const_declarations(self):
        self.check_program(program('int main();', '\tint n = 3', '\tif(true);',
            '\t\tstr n = "inner"', '\t\tio.print(n)', '\tif(true);',
            '\t\tconst auto n = 7', '\t\tio.print(n)', '\tio.print(n)'), 'inner\n7\n3\n')

    def test_inner_variable_can_hide_outer_local_const(self):
        self.check_program(program('int main();', '\tconst int N = 3', '\tif(true);',
            '\t\tauto N = N + 1', '\t\tN++', '\t\tio.print(N)', '\tio.print(N)'), '5\n3\n')

    def test_parameter_shadowing_is_allowed_only_in_nested_blocks(self):
        self.check_program(program('int f(int n);', '\tif(true);', '\t\tauto n = str(n)',
            '\t\tio.print(n + "!")', '\treturn n', 'int main();', '\tio.print(f(3))'), '3!\n3\n')
        for declaration in ['int n = 2', 'auto n = 2', 'const int n = 2']:
            self.compile_source(program('int f(int n);', '\t' + declaration, '\treturn n',
                'int main();', '\treturn 0'), error='すでに')

    def test_method_and_initializer_parameter_scopes(self):
        self.check_program(program('model Data;', '\tint n', '\tvoid init(int value);',
            '\t\tcopy = value', '\t\tif(true);', '\t\t\tauto value = copy + 1', '\t\t\tthis.n = value',
            '\tint get(int value);', '\t\tif(true);', '\t\t\tint value = 7',
            '\t\t\tio.print(value)', '\t\treturn value', 'int main();', '\tData d = {value=3}',
            '\tio.print(d.get(d.n))'), '7\n4\n')
        self.compile_source(program('model Data;', '\tint n', '\tint get(int value);',
            '\t\tauto value = 2', '\t\treturn value', 'int main();', '\treturn 0'), error='すでに')
        self.compile_source(program('model Data;', '\tint n', '\tvoid inspect();',
            '\t\tif(true);', '\t\t\tauto this = 3', 'int main();', '\treturn 0'), error='すでに')

    def test_same_block_redeclaration_stays_an_error(self):
        for declaration in ['int n = 2', 'auto n = 2', 'const int n = 2']:
            self.compile_source(program('int main();', '\tn = 1', '\t' + declaration,
                '\tio.print(n)'), error='すでに')

    def test_typed_loop_can_hide_outer_counter(self):
        self.check_program(program('int main();', '\ti = 9', '\tfor(0 <= int i < 2, i++);',
            '\t\tio.print(i)', '\tio.print(i)'), '0\n1\n9\n')
        self.compile_source(program('int main();', '\ti = 9', '\tfor(0 <= i < 2, i++);',
            '\t\tio.print(i)'), error='型を明示')
        self.compile_source(program('int main();', '\tfor(0 <= i < 2, i++);',
            '\t\tint i = 4'), error='すでに')

    def test_self_reference_and_read_before_declaration_are_errors(self):
        for body in [('\tn = n + 1',), ('\tio.print(n)', '\tn = 1')]:
            self.compile_source(program('int main();', *body), error='宣言されていません')

    def test_compound_updates_require_a_declared_name(self):
        for update in ['n += 1', 'n -= 1', 'n++', 'n--', 'n.value = 1', 'n.addr = none']:
            self.compile_source(program('int main();', '\t' + update), error='宣言されていません')

    def test_member_and_index_assignments_do_not_declare_roots(self):
        for update in ['data.n = 1', 'values[0] = 1']:
            self.compile_source(program('int main();', '\t' + update), error='宣言されていません')

    def test_global_constants_functions_types_and_namespaces_cannot_be_replaced(self):
        for declaration, name in [('const int N = 3', 'N'), ('int f() = 3', 'f'),
                                  ('model Data;\n\tint n', 'Data'),
                                  ('interface Value;\n\tint get();', 'Value')]:
            for forward in [False, True]:
                body = program('int main();', f'\t{name} = 1', '\treturn 0')
                source = body + declaration + '\n' if forward else declaration + '\n' + body
                self.compile_source(source, error='既存')
        for name in ['io', 'math']:
            self.compile_source(program('int main();', f'\t{name} = 1'), error='衝突')

    def test_using_names_cannot_be_replaced_and_other_modules_stay_separate(self):
        (self.directory / 'helper.qy').write_text(program('const int N = 3',
            'int read();', '\tvalue = N + 1', '\treturn value'))
        self.check_program(program('import helper', 'int main();', '\tN = helper.read()',
            '\tio.print(N)'), '4\n')
        for prefix, name in [('using helper', 'N'), ('using helper.read', 'read'),
                             ('using helper as h', 'h')]:
            self.compile_source(program('import helper', prefix, 'int main();',
                f'\t{name} = 1'), error='衝突')

    def test_readonly_and_const_assignments_keep_their_restrictions(self):
        self.compile_source(program('int main();', '\tconst int N = 1', '\tif(true);',
            '\t\tN = 2'), error='定数')
        self.compile_source(program('void f(ref readonly int n);', '\tn = 2',
            'int main();', '\treturn 0'), error='readonly')

    def test_initializer_runs_once(self):
        self.check_program(program('int next(ref int count);', '\tcount++', '\treturn count',
            'int main();', '\tcount = 0', '\tvalue = next(ref count)',
            '\tio.print("{count} {value}")'), '1 1\n')

    def test_arrays_models_and_file_values_keep_copy_rules(self):
        path = (self.directory / 'implicit.txt').as_posix()
        self.check_program(program('model Data;', '\tint n', 'Data make() = {n=3}',
            'int main();', '\tmatrix = [[1, 2], [3]]', '\tcopy = matrix',
            '\tcopy[0][0] = 9', '\tpoint = make()', '\tother = point', '\tother.n++',
            f'\tfiles = [io.file.open("{path}", "w")]', '\taliases = files',
            '\tio.file.close(aliases[0])', '\tio.print("{matrix} {copy} {point.n} {other.n}")'),
            '[[1, 2], [3]] [[9, 2], [3]] 3 4\n')

    def test_input_values_and_destinations_use_static_types(self):
        self.check_program(program('int main();', '\ttext = io.input()', '\tn = 0',
            '\tok = io.input(n)', '\tio.print(text + "!")', '\tio.print("{ok} {n}")'),
            '123!\ntrue 7\n', stdin='123\n7\n')

    def test_reference_alias_reads_and_writes_use_nearest_binding(self):
        self.check_program(program('int main();', '\tn = 1', '\tref int alias = n.addr',
            '\tif(true);', '\t\tauto n = 7', '\t\tn++', '\t\talias++',
            '\t\tio.print(n)', '\tio.print(n)'), '8\n2\n')
        self.compile_source(program('int main();', '\tref int alias', '\tif(true);',
            '\t\tn = 1', '\t\talias.addr = n.addr', '\tio.print(alias)'), error='ブロック')

    def test_invalid_or_contextless_initializers_are_rejected(self):
        for value in ['[]', '{}', 'none']:
            self.compile_source(program('void noop();', '\treturn', 'int main();',
                '\tn = ' + value), error='エラー')

    def test_file_close_and_readonly_ownership_cannot_be_bypassed(self):
        path = (self.directory / 'leak.txt').as_posix()
        self.compile_source(program('int main();', f'\tf = io.file.open("{path}", "w")'), error='close')
        self.compile_source(program('void inspect(ref readonly file source);', '\tcopy = source',
            '\tio.file.close(copy)', 'int main();', '\treturn 0'), error='readonly')

    def test_interface_specialization_includes_implicit_locals(self):
        self.check_program(program('interface Value;', '\treadonly int get();',
            'model Counter implements Value;', '\treadonly int get() = 7',
            'int inspect(ref readonly Value value);', '\tresult = value.get()', '\treturn result',
            'int main();', '\tCounter c', '\tanswer = inspect(ref c)', '\tio.print(answer)'), '7\n')

    def test_unused_implicit_variable_warns_but_compiles_and_keeps_side_effects(self):
        self.check_program(program('int effect();', '\tio.print("effect")', '\treturn 1',
            'int main();', '\tunused = effect()', '\tunused = 2'), 'effect\n',
            warnings=('変数 unused は未使用',))

    def test_typo_warning_suggests_visible_nearby_names(self):
        for typo in ['coutn', 'coun', 'counts', 'counx']:
            self.check_program(program('int main();', '\tcount = 3', '\t' + typo + ' = 9',
                '\tio.print(count)'), '3\n', warnings=(f'変数 {typo} は未使用です（代入のみ）。既存の変数 count と名前が似ています',))

    def test_warning_does_not_suggest_invisible_or_dissimilar_names(self):
        self.check_program(program('int main();', '\tif(true);', '\t\tint count = 3',
            '\t\tio.print(count)', '\tcoutn = 4'), '3\n',
            warnings=('変数 coutn は未使用です（代入のみ）\n',))
        self.check_program(program('int main();', '\tint elephant = 3', '\tother = 4'), '',
            warnings=('変数 other は未使用です（代入のみ）\n',))

    def test_explicit_unused_declarations_keep_existing_behavior(self):
        self.check_program(program('int main();', '\tint unused = 3', '\tauto another = 4',
            '\tconst auto LIMIT = 5'), '')

    def test_reads_refs_interpolation_and_compound_updates_count_as_use(self):
        self.check_program(program('void consume(ref int n);', '\tn++', 'int main();',
            '\ta = 1', '\tio.print("{a:0x}")', '\tb = 1', '\tconsume(ref b)',
            '\tc = 1', '\tc++', '\td = 1', '\td += 2'), '0x1\n')

    def test_unused_warnings_are_per_source_declaration_not_specialization(self):
        self.check_program(program('interface Value;', '\treadonly int get();',
            'model A implements Value;', '\treadonly int get() = 1',
            'model B implements Value;', '\treadonly int get() = 2',
            'int inspect(ref readonly Value value);', '\tunused = value.get()', '\treturn value.get()',
            'int main();', '\tA a', '\tB b', '\tio.print(inspect(ref a))', '\tio.print(inspect(ref b))'),
            '1\n2\n', warnings=('変数 unused は未使用',))

    def test_multiple_warnings_and_errors_keep_their_source_positions(self):
        self.check_program(program('int main();', '\talpha = 1', '\tomega = 2'), '',
            warnings=('変数 alpha は未使用', '変数 omega は未使用'))
        result = self.compile_source(program('int main();', '\tunused = 1', '\tint wrong = true'), error='変換できません')
        self.assertNotIn('警告', result.stderr.decode('utf-8'))
        self.assertIn(':3:', result.stderr.decode('utf-8'))


if __name__ == '__main__':
    run_suite()
