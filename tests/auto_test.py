"""Local auto inference keeps concrete types and existing ownership rules."""
from support import QynoTestCase, program, run_suite


class AutoTests(QynoTestCase):
    def test_scalar_literals_and_type_specific_ref_calls(self):
        self.check_program(program(
            'void integer(ref int n);', '\tn++',
            'void single(ref float n);', '\tn += 0.5',
            'void decimal(ref double n);', '\tn += 0.5',
            'void character(ref char n);', '\tn = "B"',
            'int main();', '\tauto n = 3', '\tauto f = float(1.5)',
            '\tauto d = 2.5', '\tauto c = char("A")', '\tauto s = "hello"',
            '\tauto b = true', '\tinteger(ref n)', '\tsingle(ref f)',
            '\tdecimal(ref d)', '\tcharacter(ref c)',
            '\tio.print("{n} {f} {d} {c} {s} {b}")'), '4 2.0 3.0 B hello true\n')

    def test_current_64_bit_integer_semantics_are_preserved(self):
        self.check_program(program('int main();', '\tauto n = 9223372036854775807',
            '\tauto bits = 0xffffffffffffffff', '\tio.print(n)', '\tio.print(bits)'),
            '9223372036854775807\n-1\n')

    def test_expression_and_function_result_inference(self):
        self.check_program(program('int twice(int n) = n * 2', 'int main();',
            '\tauto n = twice(3) + 1', '\tauto d = n + 0.5',
            '\tauto text = str(n, base=16, prefix=true)', '\tauto flag = n > 0',
            '\tio.print("{n} {d} {text} {flag}")'), '7 7.5 0x7 true\n')

    def test_inferred_type_is_fixed_for_later_assignments(self):
        for initial, replacement in [('3', '3.5'), ('3', '"three"'), ('true', '1'),
                                     ('[1]', '[true]')]:
            with self.subTest(initial=initial, replacement=replacement):
                self.compile_source(program('int main();', '\tauto value = ' + initial,
                    '\tvalue = ' + replacement), error='変換できません')
        self.compile_source(program('int main();', '\tdouble d = 2.5',
            '\tauto f = float(1.5)', '\tf = d'), error='自動変換できません')

    def test_local_const_inference_and_array_bounds(self):
        self.check_program(program('int main();', '\tconst auto count = 2 + 1',
            '\tconst auto label = "size"', '\tconst auto items = [1, 2, 3]',
            '\tint values[count]', '\tio.print("{label} {values.length} {items}")'),
            'size 3 [1, 2, 3]\n')
        self.compile_source(program('int main();', '\tconst auto n = 3', '\tn = 4'), error='定数')
        self.compile_source(program('int main();', '\tint n = 3', '\tconst auto c = n'), error='const')

    def test_nested_array_inference_deep_copy_and_growth(self):
        self.check_program(program('int main();', '\tauto original = [[1, 2], [3, 4]]',
            '\tauto copied = original', '\tcopied[0][0] = 9', '\tcopied[0].add(5)',
            '\tcopied.add([6])', '\tio.print(original)', '\tio.print(copied)'),
            '[[1, 2], [3, 4]]\n[[9, 2, 5], [3, 4], [6]]\n')

    def test_array_inference_for_all_existing_scalar_types(self):
        self.check_program(program('int main();', '\tauto a = [float(1), float(2)]',
            '\tauto b = [1, 2.5]', '\tauto c = ["a", "b"]',
            '\tauto d = [char("A"), char("B")]', '\tauto e = [true, false]',
            '\tio.print(a)', '\tio.print(b)', '\tio.print(c)', '\tio.print(d)', '\tio.print(e)'),
            '[1.0, 2.0]\n[1.0, 2.5]\n["a", "b"]\n["A", "B"]\n[true, false]\n')

    def test_arrays_from_calls_remain_independent(self):
        self.check_program(program('int make()[] = [1, 2]', 'int main();',
            '\tauto a = make()', '\tauto b = a', '\tb[0] = 9',
            '\tio.print(a)', '\tio.print(b)'), '[1, 2]\n[9, 2]\n')

    def test_model_inference_preserves_value_copy_and_methods(self):
        self.check_program(program('model Point;', '\tint x',
            '\tvoid move(int dx);', '\t\tthis.x += dx',
            'Point make() = {x=3}', 'int main();', '\tauto a = make()',
            '\tauto b = a', '\tb.move(4)', '\tio.print("{a.x} {b.x}")'), '3 7\n')

    def test_model_arrays_are_copied_deeply(self):
        self.check_program(program('model Item;', '\tint n', 'int main();',
            '\tItem item = {n=1}', '\tauto a = [item]', '\tauto b = a',
            '\tb[0].n = 9', '\tio.print("{item.n} {a[0].n} {b[0].n}")'), '1 1 9\n')

    def test_initializer_is_evaluated_once(self):
        self.check_program(program('int next(ref int count);', '\tcount++', '\treturn count',
            'int main();', '\tint count = 0', '\tauto n = next(ref count)',
            '\tio.print("{n} {count}")'), '1 1\n')

    def test_value_input_defaults_to_string_and_target_input_to_bool(self):
        self.check_program(program('int main();', '\tauto text = io.input()', '\tint n = 0',
            '\tauto ok = io.input(n)', '\tio.print(text + "!")', '\tio.print("{ok} {n}")'),
            '123!\ntrue 7\n', stdin='123\n7\n')

    def test_inferred_input_destination_keeps_type(self):
        self.check_program(program('int main();', '\tauto n = 0', '\tio.input(n)',
            '\tio.print(n + 1)'), '16\n', stdin='0xf\n')

    def test_scope_is_local_and_sibling_blocks_are_independent(self):
        self.check_program(program('int main();', '\tif(true);', '\t\tauto n = 1', '\t\tio.print(n)',
            '\tif(true);', '\t\tauto n = "two"', '\t\tio.print(n)'), '1\ntwo\n')
        self.compile_source(program('int main();', '\tif(true);', '\t\tauto n = 1', '\tio.print(n)'), error='宣言されていません')

    def test_self_reference_and_redeclaration_are_rejected(self):
        self.compile_source(program('int main();', '\tauto n = n'), error='宣言されていません')
        self.compile_source(program('int main();', '\tauto n = 1', '\tauto n = 2'), error='すでに')

    def test_missing_context_for_empty_array_or_model_is_rejected(self):
        for value in ['[]', '[[]]', '{}', '{x=1}']:
            with self.subTest(value=value):
                self.compile_source(program('int main();', '\tauto value = ' + value), error='エラー')

    def test_missing_initializer_explicit_dimensions_and_ref_are_rejected(self):
        for declaration in ['auto n', 'const auto n', 'auto n[] = [1]',
                            'auto n[3] = [1]', 'ref auto n = value.addr']:
            with self.subTest(declaration=declaration):
                self.compile_source(program('int main();', '\tint value = 3', '\t' + declaration), error='auto')

    def test_address_is_not_inferred_and_void_is_storable(self):
        for value in ['n.addr', 'none', 'io.print(1)']:
            with self.subTest(value=value):
                self.compile_source(program('void noop();', '\treturn', 'int main();',
                    '\tint n = 1', '\tauto value = ' + value), error='エラー')

    def test_field_main_and_loop_auto_restrictions(self):
        for source in [program('auto main() = 0'),
                       program('model Point;', '\tauto x = 0', 'int main();', '\treturn 0'),
                       program('int main();', '\tfor(0 <= auto i < 2, i++);', '\t\tio.print(i)')]:
            with self.subTest(source=source): self.compile_source(source, error='エラー')

    def test_implicit_declaration_and_explicit_nested_shadowing(self):
        self.check_program(program('int main();', '\tn = 3', '\tio.print(n)'), '3\n')
        self.check_program(program('int main();', '\tint n = 1', '\tif(true);',
            '\t\tauto n = 2', '\t\tio.print(n)', '\tio.print(n)'), '2\n1\n')

    def test_readonly_owned_values_can_be_copied(self):
        self.check_program(program('void inspect(ref readonly int values[]);', '\tauto copy = values',
            '\tcopy[0] = 9', '\tio.print(copy)', 'int main();', '\tint values[] = [1]',
            '\tinspect(ref values)', '\tio.print(values)'), '[9]\n[1]\n')

    def test_interface_cannot_escape_through_auto(self):
        self.compile_source(program('interface Shape;', '\treadonly int size();',
            'void inspect(ref readonly Shape shape);', '\tauto copy = shape',
            'int main();', '\treturn 0'), error='interface')

    def test_file_inference_keeps_explicit_close_requirement(self):
        path = (self.directory / 'created.txt').as_posix()
        source = program('int main();', f'\tauto f = io.file.open("{path}", "w")',
            '\tio.file.close(f)')
        self.check_program(source, '')
        self.compile_source(source.replace('\tio.file.close(f)\n', ''), error='close')

    def test_readonly_file_cannot_be_copied_through_auto(self):
        self.compile_source(program('void inspect(ref readonly file source);',
            '\tauto copy = source', '\tio.file.close(copy)', 'int main();', '\treturn 0'), error='readonly')

    def test_inference_inside_initializer_and_interface_specialization(self):
        self.check_program(program('interface Value;', '\treadonly int get();',
            'model Counter implements Value;', '\tprivate int n',
            '\tvoid init(int initial);', '\t\tauto copied = initial', '\t\tthis.n = copied',
            '\treadonly int get() = this.n', 'int inspect(ref readonly Value value);',
            '\tauto result = value.get()', '\tauto values = [result]', '\treturn values[0]',
            'int main();', '\tCounter c = {initial=7}', '\tauto copy = c',
            '\tio.print(inspect(ref copy))'), '7\n')

    def test_file_array_inference_keeps_shared_handles_and_close_tracking(self):
        path = (self.directory / 'array.txt').as_posix()
        source = program('int main();', f'\tauto files = [io.file.open("{path}", "w")]',
            '\tauto copy = files', '\tio.file.close(copy[0])')
        self.check_program(source, '')
        self.compile_source(source.replace('\tio.file.close(copy[0])\n', ''), error='close')


if __name__ == '__main__':
    run_suite()
