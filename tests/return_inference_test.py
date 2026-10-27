"""Return type inference checks bodies once and keeps typed call boundaries."""
from support import QynoTestCase, program, run_suite


class ReturnInferenceTests(QynoTestCase):
    def test_auto_and_omitted_expression_functions(self):
        self.check_program(program('auto square(int x) = x^2', 'twice(int x) = x * 2',
            'int main();', '\tprint("{square(3)} {twice(4)}")'), '9 8\n')

    def test_forward_calls_and_local_auto_use_the_inferred_type(self):
        self.check_program(program('int main();', '\tresult = calculate(3)', '\tprint(result)',
            'calculate(int x) = later(x) + 0.5', 'auto later(int x) = x * 2'), '6.5\n')

    def test_all_existing_scalar_return_types(self):
        self.check_program(program('integer() = 3', 'single() = float(1.5)', 'decimal() = 2.5',
            'text() = "QYNO"', 'letter() = char("A")', 'flag() = true', 'int main();',
            '\tprint("{integer()} {single()} {decimal()} {text()} {letter()} {flag()}")'), '3 1.5 2.5 QYNO A true\n')

    def test_multiple_numeric_returns_promote_independently_of_source_order(self):
        self.check_program(program('auto first(bool choice);', '\tif(choice);', '\t\treturn 3', '\treturn 2.5',
            'auto second(bool choice);', '\tif(choice);', '\t\treturn 2.5', '\treturn 3',
            'int main();', '\tprint("{first(true)} {first(false)} {second(true)} {second(false)}")'), '3.0 2.5 2.5 3.0\n')

    def test_float_and_double_return_types_are_not_narrowed(self):
        self.check_program(program('choose(bool b);', '\tif(b);', '\t\treturn float(1.5)', '\treturn 2.5',
            'void take(ref double value);', '\tvalue += 1.0', 'int main();', '\tvalue = choose(true)',
            '\ttake(ref value)', '\tprint(value)'), '2.5\n')

    def test_char_and_str_returns_merge_to_str(self):
        self.check_program(program('choose(bool b);', '\tif(b);', '\t\treturn char("A")', '\treturn "long"',
            'int main();', '\tprint(choose(true) + choose(false))'), 'Along\n')

    def test_incompatible_returns_are_rejected(self):
        for first, second in [('true', '1'), ('"x"', '1'), ('[1]', '[true]'), ('[[1]]', '[1]')]:
            self.compile_source(program('choose(bool b);', '\tif(b);', '\t\treturn ' + first,
                '\treturn ' + second, 'int main();', '\treturn 0'), error='統一できません')

    def test_missing_value_return_path_is_rejected(self):
        self.compile_source(program('choose(bool b);', '\tif(b);', '\t\treturn 3',
            'int main();', '\treturn 0'), error='すべての道筋')

    def test_void_from_fallthrough_and_bare_returns(self):
        self.check_program(program('announce();', '\tprint("hello")', 'auto stop(bool b);',
            '\tif(b);', '\t\treturn', '\tprint("continue")', 'int main();',
            '\tannounce()', '\tstop(true)', '\tstop(false)'), 'hello\ncontinue\n')
        self.compile_source(program('announce();', '\tprint("hello")', 'int main();',
            '\tauto result = announce()'), error='void')

    def test_mixed_value_and_bare_returns_are_rejected(self):
        for first, second in [('return 3', 'return'), ('return', 'return 3')]:
            self.compile_source(program('choose(bool b);', '\tif(b);', '\t\t' + first, '\t' + second,
                'int main();', '\treturn 0'), error='混在')

    def test_void_expression_bodies_and_raw_addresses_are_rejected(self):
        for definition in ['auto call() = noop()', 'auto address(ref int n) = n.addr',
                           'auto addresses(ref int n) = [n.addr]']:
            self.compile_source(program('void noop();', '\treturn', definition,
                'int main();', '\treturn 0'), error='エラー')

    def test_arrays_are_inferred_and_copied(self):
        self.check_program(program('make(int n) = [[n, n + 1], [n + 2]]',
            'choose(bool b, int values[][]);', '\tif(b);', '\t\treturn values', '\treturn [[9]]',
            'int main();', '\tfirst = make(1)', '\tsecond = choose(true, first)', '\tsecond[0][0] = 7',
            '\tprint(first)', '\tprint(second)'), '[[1, 2], [3]]\n[[7, 2], [3]]\n')

    def test_contextless_empty_arrays_and_model_literals_are_rejected(self):
        for value in ['[]', '{}', '{x=3}']:
            self.compile_source(program('auto make() = ' + value, 'int main();', '\treturn 0'), error='エラー')
        self.check_program(program('auto empty();', '\tint values[] = []', '\treturn values',
            'int main();', '\tprint(empty())'), '[]\n')

    def test_model_values_keep_nominal_types_and_copies(self):
        self.check_program(program('model Point;', '\tint x', 'make(int x);', '\tPoint p = {x=x}', '\treturn p',
            'int main();', '\tp = make(3)', '\tcopy = p', '\tcopy.x++', '\tprint("{p.x} {copy.x}")'), '3 4\n')
        self.compile_source(program('model A;', '\tint x', 'model B;', '\tint x',
            'choose(bool b);', '\tif(b);', '\t\treturn A()', '\treturn B()',
            'int main();', '\treturn 0'), error='統一できません')

    def test_auto_model_methods_keep_visibility_readonly_and_this(self):
        self.check_program(program('model Counter;', '\tprivate int n = 3',
            '\tprivate auto hidden() = this.n', '\treadonly get() = this.n',
            '\tauto increase(int amount);', '\t\tthis.n += amount', '\t\treturn this.hidden()',
            'int main();', '\tCounter c', '\tprint(c.increase(2))', '\tprint(c.get())'), '5\n5\n')
        self.compile_source(program('model Counter;', '\tint n', '\treadonly auto bad();',
            '\t\tthis.n++', '\t\treturn this.n', 'int main();', '\treturn 0'), error='readonly')

    def test_inferred_methods_implement_explicit_interface_contracts(self):
        self.check_program(program('interface Value;', '\treadonly int get();', '\treadonly int values()[];',
            'model Counter implements Value;', '\treadonly auto get() = 3', '\treadonly values() = [1, 2]',
            'inspect(ref readonly Value value);', '\tauto result = value.get()', '\treturn result + value.values()[0]',
            'int main();', '\tCounter c', '\tprint(inspect(ref c))'), '4\n')

    def test_inferred_interface_implementation_must_match_the_contract(self):
        self.compile_source(program('interface Value;', '\treadonly int get();', 'model Counter implements Value;',
            '\treadonly auto get() = 3.5', 'int main();', '\treturn 0'), error='一致しません')
        self.compile_source(program('interface Value;', '\treadonly int values()[2];', 'model Counter implements Value;',
            '\treadonly auto values() = [1, 2]', 'int main();', '\treturn 0'), error='一致しません')

    def test_signature_only_interfaces_main_init_and_fields_need_explicit_types(self):
        for definition in ['interface Value;\n\tauto get();', 'interface Value;\n\tget();',
                           'auto main() = 0', 'main() = 0',
                           'model Counter;\n\tinit();\n\t\treturn', 'model Counter;\n\tauto n = 3']:
            source = definition + '\n' if 'main()' in definition else definition + '\nint main();\n\treturn 0\n'
            self.compile_source(source, error='エラー')

    def test_array_suffix_is_inferred_not_written_after_auto(self):
        for header in ['auto make()[]', 'make()[]']:
            self.compile_source(program(header + ' = [1]', 'int main();', '\treturn 0'), error='次元も推論')

    def test_argument_type_inference_is_still_deferred(self):
        for header in ['auto f(x)', 'f(x)', 'f(auto x)']:
            self.compile_source(program(header + ' = x^2', 'int main();', '\treturn 0'), error='引数の型')

    def test_unannotated_self_and_mutual_recursion_require_return_annotations(self):
        for definitions in [program('auto f(int n) = f(n - 1)'),
                            program('auto f(int n) = g(n)', 'g(int n) = f(n)'),
                            program('f(int n);', '\tif(n == 0);', '\t\treturn 1', '\treturn f(n - 1)')]:
            self.compile_source(definitions + program('int main();', '\treturn 0'), error='戻り値型を明示')

    def test_explicit_return_annotation_breaks_recursive_inference_cycle(self):
        self.check_program(program('auto even(int n);', '\tif(n == 0);', '\t\treturn true', '\treturn odd(n - 1)',
            'bool odd(int n);', '\tif(n == 0);', '\t\treturn false', '\treturn even(n - 1)',
            'int main();', '\tprint("{even(4)} {odd(4)}")'), 'true false\n')

    def test_deep_acyclic_dependencies_have_a_limit(self):
        accepted = [f'f{i}() = f{i+1}()' for i in range(100)] + ['f100() = 3']
        self.check_program(program(*accepted, 'int main();', '\tprint(f0())'), '3\n')
        excessive = [f'f{i}() = f{i+1}()' for i in range(300)] + ['f300() = 3']
        self.compile_source(program(*excessive, 'int main();', '\treturn 0'), error='依存は 256 段以下')

    def test_function_and_argument_evaluation_order_is_unchanged(self):
        self.check_program(program('next(ref int count);', '\tcount++', '\treturn count',
            'add(int a, int b) = a * 10 + b', 'int main();', '\tcount = 0',
            '\tprint(add(next(ref count), next(ref count)))', '\tprint(count)'), '12\n2\n')

    def test_const_readonly_ref_temporaries_belong_to_the_correct_function(self):
        self.check_program(program('const VALUES = [1, 2]', 'sum(ref readonly int values[]) = values[0] + values[1]',
            'read();', '\tauto n = sum(ref VALUES)', '\treturn n',
            'int main();', '\tresult = read()', '\tprint(result)'), '3\n')

    def test_inferred_functions_do_not_become_compile_time_functions(self):
        self.compile_source(program('auto square(int n) = n^2', 'const RESULT = square(3)',
            'int main();', '\treturn 0'), error='const')
        self.check_program(program('positive(int n) = n > 0', 'const YES = true or positive(3)',
            'int main();', '\tprint(YES)'), 'true\n')

    def test_private_and_readonly_ownership_cannot_escape_through_inferred_returns(self):
        self.compile_source(program('model Point;', '\tprivate int n', 'leak(ref readonly Point point) = point.n',
            'int main();', '\treturn 0'), error='private')
        self.compile_source(program('leak(ref readonly file source) = source',
            'int main();', '\treturn 0'), error='readonly')
        self.compile_source(program('interface Value;', '\treadonly int get();',
            'leak(ref readonly Value value) = value', 'int main();', '\treturn 0'), error='interface')

    def test_file_return_inference_keeps_explicit_close_responsibility(self):
        path = (self.directory / 'returned.txt').as_posix()
        source = program(f'open_file() = io.file.open("{path}", "w")', 'int main();',
            '\tf = open_file()', '\tio.file.write(f, "hello")', '\tio.file.close(f)')
        self.check_program(source, '')
        self.assertEqual((self.directory / 'returned.txt').read_text(), 'hello')
        self.compile_source(source.replace('\tio.file.close(f)\n', ''), error='close')

    def test_file_array_return_inference_transfers_shared_handles(self):
        path = (self.directory / 'array.txt').as_posix()
        source = program(f'open_files() = [io.file.open("{path}", "w")]', 'int main();',
            '\tfiles = open_files()', '\tio.file.close(files[0])')
        self.check_program(source, '')
        self.compile_source(source.replace('\tio.file.close(files[0])\n', ''), error='close')

    def test_returned_model_initializer_runs_once(self):
        self.check_program(program('model Point;', '\tint n', '\tvoid init(int initial);',
            '\t\tprint("init")', '\t\tthis.n = initial', 'make(int n);',
            '\tPoint point = {initial=n}', '\treturn point', 'int main();',
            '\tpoint = make(3)', '\tprint(point.n)'), 'init\n3\n')

    def test_value_input_return_has_a_static_string_type(self):
        self.check_program(program('read_text() = input()', 'read_int() = int(input())',
            'int main();', '\ttext = read_text()', '\tn = read_int()',
            '\tprint(text + "!")', '\tprint(n + 1)'), '123!\n8\n', stdin='123\n7\n')

    def test_imported_inferred_functions_using_and_aliases(self):
        (self.directory / 'helper.qy').write_text(program('auto square(int n) = n^2', 'twice(int n) = n * 2'))
        self.check_program(program('import helper', 'using helper as h', 'using helper.twice',
            'int main();', '\tprint("{h.square(3)} {twice(4)}")'), '9 8\n')

    def test_imported_inference_cycles_report_the_dependency_source(self):
        (self.directory / 'helper.qy').write_text('loop(int n) = loop(n)\n')
        self.compile_source(program('import helper', 'int main();', '\tprint(helper.loop(3))'),
            error='戻り値型を明示', error_source='helper.qy')

    def test_return_inside_loops_and_unreachable_returns_are_checked(self):
        self.check_program(program('find();', '\twhile(true);', '\t\treturn 3',
            'int main();', '\tprint(find())'), '3\n')
        self.compile_source(program('find();', '\tfor(0 <= i < 3, i++);', '\t\treturn i',
            'int main();', '\treturn 0'), error='すべての道筋')
        self.compile_source(program('find();', '\treturn 3', '\treturn true',
            'int main();', '\treturn 0'), error='統一できません')


if __name__ == '__main__':
    run_suite()
