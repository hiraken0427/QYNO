"""Monomorphized inferred parameters keep types, scopes and resource rules."""
from support import QynoTestCase, program, run_suite


class GenericFunctionTests(QynoTestCase):
    def test_square_omitted_and_explicit_auto_parameters(self):
        self.check_program(program('square(x) = x^2', 'auto twice(auto x) = x * 2', 'int main();',
            '\tio.print("{square(3)} {square(1.5)} {twice(4)} {twice(2.5)}")'), '9 2.25 8 5.0\n')

    def test_fixed_return_type_is_a_constraint_not_an_argument_type(self):
        self.check_program(program('int square(x) = x^2', 'int main();', '\tio.print(square(3))'), '9\n')
        self.compile_source(program('int square(x) = x^2', 'int main();', '\tio.print(square(1.5))'), error='変換できません')
        self.check_program(program('int square(x) = int(x^2)', 'int main();', '\tio.print(square(1.5))'), '2\n')

    def test_mixed_explicit_and_inferred_parameters_keep_typed_context(self):
        self.check_program(program('model Point;', '\tint n', 'add(Point point, x, double extra) = point.n + x + extra',
            'int main();', '\tio.print(add({n=3}, 4, 1))', '\tio.print(add({n=3}, 0.5, 1.0))'), '8.0\n4.5\n')

    def test_operator_lowering_is_independent_for_bool_and_int_instances(self):
        for first, second in [('true, false', '3, 5'), ('3, 5', 'true, false')]:
            expected = 'false\n1\n' if first.startswith('true') else '1\nfalse\n'
            self.check_program(program('combine(x, y) = x and y', 'int main();',
                '\tio.print(combine(' + first + '))', '\tio.print(combine(' + second + '))'), expected)

    def test_bool_short_circuit_and_integer_eagerness_are_preserved(self):
        self.check_program(program('probe(x);', '\tio.print("probe")', '\treturn x', 'combine(x) = x and probe(x)',
            'int main();', '\tio.print(combine(false))', '\tio.print(combine(0))',
            '\tio.print(combine(true))', '\tio.print(combine(3))'), 'false\nprobe\n0\nprobe\ntrue\nprobe\n3\n')

    def test_local_inference_and_const_state_are_not_shared_between_instances(self):
        self.check_program(program('identity(x);', '\tconst N = 2', '\tint scratch[N]', '\tscratch[0] = 1',
            '\ty = x', '\treturn y', 'int main();', '\tio.print(identity(3))', '\tio.print(identity("hello"))',
            '\tio.print(identity(2.5))', '\tio.print(identity(4))'), '3\nhello\n2.5\n4\n')

    def test_char_and_float_types_are_preserved(self):
        self.check_program(program('identity(x) = x', 'void character(ref char c);', '\tc = "B"',
            'void single(ref float value);', '\tvalue += 0.5', 'int main();', '\tc = identity(char("A"))',
            '\tf = identity(float(1.5))', '\tcharacter(ref c)', '\tsingle(ref f)', '\tio.print("{c} {f}")'), 'B 2.0\n')

    def test_array_element_types_and_ranks_are_inferred(self):
        self.check_program(program('first(x) = x[0]', 'int main();', '\tio.print(first([1, 2]))',
            '\tio.print(first([1.5, 2.5]))', '\tio.print(first(["a", "b"]))', '\tio.print(first([[3], [4]]))'), '1\n1.5\na\n[3]\n')

    def test_value_arrays_and_models_remain_independent_copies(self):
        self.check_program(program('model Point;', '\tint n', 'change(x);', '\tx[0] = x[0] + 1', '\treturn x',
            'change_point(x);', '\tx.n++', '\treturn x', 'int main();', '\tvalues = [1, 2]',
            '\tPoint p = {n=3}', '\tio.print(change(values))', '\tio.print(values)',
            '\tio.print(change_point(p).n)', '\tio.print(p.n)'), '[2, 2]\n[1, 2]\n4\n3\n')

    def test_ref_elements_and_whole_arrays(self):
        self.check_program(program('increase(ref x);', '\tx++', 'append(ref auto values, x);', '\tvalues.add(x)',
            'int main();', '\tvalues = [1, 2]', '\tincrease(ref values[0])', '\tappend(ref values, 3)',
            '\td = 1.5', '\tincrease(ref d)', '\tio.print(values)', '\tio.print(d)'), '[2, 2, 3]\n2.5\n')

    def test_readonly_ref_accepts_constant_arrays_and_models(self):
        self.check_program(program('const VALUES = [1, 2]', 'first(ref readonly x) = x[0]',
            'int main();', '\tio.print(first(ref VALUES))', '\tstr values[] = ["a", "b"]', '\tio.print(first(ref values))'), '1\na\n')

    def test_ref_modes_and_readonly_are_checked_for_each_instance(self):
        for call in ['increase(1)', 'increase(ref 1)']:
            self.compile_source(program('increase(ref x);', '\tx++', 'int main();', '\t' + call), error='エラー')
        self.compile_source(program('identity(x) = x', 'int main();', '\tint n = 1', '\tidentity(ref n)'), error='ref')
        self.compile_source(program('change(ref readonly x);', '\tx[0] = 3', 'int main();',
            '\tint values[] = [1]', '\tchange(ref values)'), error='const')

    def test_array_capacity_and_loop_counter_borrows_keep_existing_checks(self):
        self.check_program(program('append(ref x);', '\tx.add(2)', 'int main();', '\tint values[1] = [1]',
            '\tappend(ref values)'), '', status=70, error='最大要素数')
        self.compile_source(program('increase(ref x);', '\tx++', 'int main();', '\tfor(0 <= i < 2, i++);',
            '\t\tincrease(ref i)'), error='繰り返しの変数')

    def test_readonly_owned_arrays_can_be_copied(self):
        self.check_program(program('copy(ref readonly x);', '\tresult = x', '\tresult[0] = 9', '\treturn result',
            'int main();', '\tint values[] = [1, 2]', '\tio.print(copy(ref values))', '\tio.print(values)'), '[9, 2]\n[1, 2]\n')

    def test_inferred_parameter_dimensions_must_be_omitted(self):
        for header in ['f(x[])', 'f(auto x[])', 'f(ref x[][])']:
            self.compile_source(program(header + ' = x', 'int main();', '\treturn 0'), error='次元も推論')

    def test_model_methods_keep_lexical_private_access(self):
        self.check_program(program('model Counter;', '\tprivate int bias = 3',
            '\treadonly add(x) = this.bias + x', 'int main();', '\tCounter c',
            '\tio.print("{c.add(4)} {c.add(0.5)}")'), '7 3.5\n')
        self.compile_source(program('leak(x) = x.secret', 'model Point;', '\tprivate int secret = 3',
            '\tread() = leak(this)', 'int main();', '\tPoint p', '\tio.print(p.read())'), error='private')

    def test_explicit_interface_borrows_can_mix_with_generic_values(self):
        self.check_program(program('interface Value;', '\treadonly int get();', 'model Counter implements Value;',
            '\treadonly int get() = 3', 'const VALUES = [4]', 'first(ref readonly x) = x[0]',
            'read(ref readonly Value value, x) = first(ref VALUES) + value.get() + x',
            'int main();', '\tCounter c', '\tio.print(read(ref c, 4))', '\tio.print(read(ref c, 0.5))'), '11\n7.5\n')

    def test_interface_contracts_and_initializers_keep_explicit_parameter_types(self):
        for definition in ['interface Value;\n\tint get(x);',
                           'model Point;\n\tint n\n\tvoid init(x);\n\t\tthis.n = x',
                           'interface Value;\n\tint get(int n);\nmodel Counter implements Value;\n\tint get(x) = x']:
            self.compile_source(definition + '\nint main();\n\treturn 0\n', error='型を明示')

    def test_typed_recursive_generic_function(self):
        self.check_program(program('int fact(n);', '\tif(n <= 1);', '\t\treturn 1', '\treturn n * fact(n - 1)',
            'int main();', '\tio.print(fact(5))'), '120\n')

    def test_inferred_recursive_return_still_requires_an_annotation(self):
        self.compile_source(program('fact(n);', '\tif(n <= 1);', '\t\treturn 1', '\treturn n * fact(n - 1)',
            'int main();', '\tio.print(fact(3))'), error='戻り値型を明示')

    def test_polymorphic_recursion_is_bounded(self):
        self.compile_source(program('int grow(x) = grow([x])', 'int main();', '\tio.print(grow(1))'), error='次元数')

    def test_all_branches_are_checked_for_the_selected_types(self):
        self.compile_source(program('bad(x);', '\tif(true);', '\t\treturn x', '\treturn x + true',
            'int main();', '\tio.print(bad(1))'), error='エラー')

    def test_argument_evaluation_order_and_single_evaluation(self):
        self.check_program(program('next(ref int count);', '\tcount++', '\treturn count', 'pair(a, b) = a * 10 + b',
            'int main();', '\tcount = 0', '\tio.print(pair(next(ref count), next(ref count)))', '\tio.print(count)'), '12\n2\n')

    def test_print_interpolation_and_input_temporaries_are_instance_local(self):
        self.check_program(program('show(x);', '\tio.print("value: {x}")', 'read(ref x);', '\tio.input(x)',
            'read_pattern(ref x);', '\tio.input("{x}")',
            'int main();', '\tint n', '\tstr text', '\tread(ref n)', '\tread(ref text)',
            '\tshow(n)', '\tshow(text)', '\tread_pattern(ref n)', '\tread_pattern(ref text)',
            '\tshow(n)', '\tshow(text)'), 'value: 7\nvalue: hello\nvalue: 8\nvalue: world\n', stdin='7\nhello\n8\nworld\n')

    def test_builtin_overloads_do_not_share_lowered_nodes_between_types(self):
        self.check_program(program('text(x) = str(x)', 'int main();', '\tio.print(text(15))',
            '\tio.print(text(char("A")))', '\tio.print(text("QYNO"))', '\tio.print(text(16))'), '15\nA\nQYNO\n16\n')

    def test_format_constraints_are_checked_on_each_instance(self):
        self.check_program(program('show(x);', '\tio.print("{x:0x}")', 'int main();', '\tshow(15)'), '0xf\n')
        self.compile_source(program('show(x);', '\tio.print("{x:0x}")', 'int main();', '\tshow(1.5)'), error='int')

    def test_file_values_keep_shared_handles_and_close_responsibility(self):
        path = (self.directory / 'generic.txt').as_posix()
        source = program('open_file(path) = io.file.open(path, "w")', 'close_file(source);', '\tio.file.close(source)',
            'int main();', f'\tf = open_file("{path}")', '\tio.file.write(f, "hello")', '\tclose_file(f)')
        self.check_program(source, '')
        self.assertEqual((self.directory / 'generic.txt').read_text(), 'hello')
        self.compile_source(source.replace('\tclose_file(f)\n', ''), error='close')

    def test_readonly_file_cannot_escape_or_be_closed(self):
        for operation in ['return source', 'io.file.close(source)']:
            self.compile_source(program('bad(ref readonly source);', '\t' + operation, 'int main();',
                '\tfile f', '\tbad(ref f)'), error='readonly')

    def test_pointer_argument_types_are_not_introduced(self):
        for value in ['n.addr', '[n.addr]']:
            self.compile_source(program('void noop();', '\treturn', 'identity(x) = x', 'int main();',
                '\tint n = 1', '\tidentity(' + value + ')'), error='エラー')

    def test_contextless_literals_need_explicit_argument_types(self):
        for value in ['[]', '{}', '{n=3}']:
            self.compile_source(program('identity(x) = x', 'int main();', '\tidentity(' + value + ')'), error='エラー')

    def test_ordinary_functions_are_not_constexpr(self):
        self.compile_source(program('identity(x) = x', 'const N = identity(3)', 'int main();', '\treturn 0'), error='const')

    def test_imported_templates_and_using_are_resolved_in_declaration_scope(self):
        (self.directory / 'helper.qy').write_text(program('using math.sqrt', 'const BIAS = 1',
            'square(x) = x^2 + BIAS', 'root(x) = sqrt(x)'))
        self.check_program(program('import helper', 'using helper as h', 'int main();',
            '\tio.print(h.square(3))', '\tio.print(h.square(1.5))', '\tio.print(h.root(9))'), '10\n3.25\n3.0\n')

    def test_imported_instance_errors_report_source_file(self):
        (self.directory / 'helper.qy').write_text('int square(x) = x^2\n')
        self.compile_source(program('import helper', 'int main();', '\tio.print(helper.square(1.5))'),
            error='変換できません', error_source='helper.qy')

    def test_unused_templates_are_not_emitted_or_instantiated(self):
        self.check_program(program('unused(x) = x.missing()', 'int main();', '\treturn 0'), '')
        self.compile_source(program('unused(x) = x +', 'int main();', '\treturn 0'), error='エラー')

    def test_unused_local_warnings_are_per_source_template(self):
        self.check_program(program('identity(x);', '\tunused = x', '\treturn x', 'int main();',
            '\tio.print(identity(1))', '\tio.print(identity("x"))'), '1\nx\n', warnings=('変数 unused は未使用',))

    def test_specialization_budget_is_enforced(self):
        names = ', '.join(f'x{i}' for i in range(13))
        calls = ['\ttouch(' + ', '.join('true' if value & (1 << bit) else '0' for bit in range(13)) + ')'
                 for value in range(4097)]
        self.compile_source(program('int touch(' + names + ') = 0', 'int main();', *calls), error='具体化はプログラム全体で4096件')


if __name__ == '__main__':
    run_suite()
