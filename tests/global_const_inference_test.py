"""Inferred constants retain compile-time values and module-local resolution."""
from support import QynoTestCase, program, run_suite


class GlobalConstInferenceTests(QynoTestCase):
    def test_explicit_auto_and_shorthand_for_all_scalar_types(self):
        self.check_program(program('const auto N = 3', 'const RATIO = 0.5',
            'const SINGLE = float(1.5)', 'const TEXT = "QYNO"', 'const LETTER = char("A")',
            'const ENABLED = true', 'int main();',
            '\tprint("{N} {RATIO} {SINGLE} {TEXT} {LETTER} {ENABLED}")'), '3 0.5 1.5 QYNO A true\n')

    def test_forward_dependencies_mixed_with_explicit_types(self):
        self.check_program(program('const A = B + 1', 'const auto B = C * 2',
            'const int C = D', 'const D = 3', 'int main();', '\tprint("{A} {B} {C} {D}")'), '7 6 3 3\n')

    def test_forward_type_inference_precedes_numeric_operations(self):
        self.check_program(program('const ANSWER = BASE + 0.5', 'const BASE = 3',
            'const LABEL = TEXT + "!"', 'const TEXT = "QYNO"', 'int main();',
            '\tprint("{ANSWER} {LABEL}")'), '3.5 QYNO!\n')

    def test_constants_after_functions_are_file_wide(self):
        self.check_program(program('int read() = LIMIT', 'int main();', '\tprint(read())',
            'const auto LIMIT = 7'), '7\n')

    def test_nested_arrays_and_local_copies(self):
        self.check_program(program('const MATRIX = [[1, 2], [3, 4]]', 'const auto COPY = MATRIX',
            'int main();', '\tvalues = COPY', '\tvalues[0][0] = 9',
            '\tprint(MATRIX)', '\tprint(COPY)', '\tprint(values)'),
            '[[1, 2], [3, 4]]\n[[1, 2], [3, 4]]\n[[9, 2], [3, 4]]\n')

    def test_forward_array_type_length_and_index(self):
        self.check_program(program('const N = VALUES.length', 'const FIRST = VALUES[0]',
            'const VALUES = [1, 2, 3]', 'int main();', '\tint values[N]',
            '\tprint("{N} {FIRST} {values.length}")'), '3 1 3\n')

    def test_typed_empty_arrays_supply_context_to_inferred_aliases(self):
        self.check_program(program('const ALIAS = EMPTY', 'const int EMPTY[] = []',
            'int main();', '\tprint(ALIAS)', '\tprint(ALIAS.length)'), '[]\n0\n')

    def test_forward_constants_supply_interface_dimensions(self):
        self.check_program(program('interface Sum;', '\treadonly int sum(ref readonly int values[N]);',
            'model Impl implements Sum;', '\treadonly int sum(ref readonly int values[N]) = values[0] + values[1]',
            'const N = SIZES[0]', 'const SIZES = [2]',
            'int apply(ref readonly Sum op, ref readonly int values[N]) = op.sum(ref values)',
            'int main();', '\tImpl op', '\tint values[N] = [3, 4]', '\tprint(apply(ref op, ref values))'), '7\n')

    def test_model_field_bounds_defaults_and_initializer_defaults(self):
        self.check_program(program('model Data;', '\tint values[N]', '\tint n = INITIAL',
            'model Counter;', '\tint n', '\tvoid init(int initial=INITIAL);', '\t\tthis.n = initial',
            'const auto N = 2', 'const INITIAL = N + 1', 'int main();', '\tData d', '\tCounter c',
            '\tprint("{d.values.length} {d.n} {c.n}")'), '2 3 3\n')

    def test_inferred_model_and_model_array_constants(self):
        self.check_program(program('model Point;', '\tint x = BASE', 'const BASE = 3',
            'const auto ORIGIN = Point()', 'const COPIED = ORIGIN', 'const POINTS = [ORIGIN, COPIED]',
            'int main();', '\tpoint = COPIED', '\tpoint.x++', '\tprint("{ORIGIN.x} {POINTS[1].x} {point.x}")'), '3 3 4\n')

    def test_early_model_constant_is_safe_in_interface_dimension(self):
        self.check_program(program('interface Values;', '\tint sum(ref int values[N]);',
            'const N = DEFAULT.x', 'const DEFAULT = Point()', 'model Point;', '\tint x = BASE',
            'const BASE = 2', 'int main();', '\tint values[N]', '\tprint(values.length)'), '2\n')

    def test_early_model_resolution_does_not_bypass_interface_storage_rules(self):
        self.compile_source(program('interface Value;', '\treadonly int get();',
            'interface Values;', '\tint sum(ref int values[N]);', 'const N = DEFAULT.n',
            'const DEFAULT = Container()', 'model Container;', '\tint n = 1', '\tValue borrowed',
            'int main();', '\treturn 0'), error='interface')

    def test_typed_model_default_and_inferred_alias_have_well_founded_dependencies(self):
        self.check_program(program('model Point;', '\tint x = COPIED.x',
            'const COPIED = ORIGINAL', 'const Point ORIGINAL = {x=3}',
            'int main();', '\tPoint p', '\tprint(p.x)'), '3\n')

    def test_model_default_cycles_are_diagnosed_before_constant_evaluation(self):
        for source in [program('model Point;', '\tint x = Point().x', 'int main();', '\treturn 0'),
                       program('model Point;', '\tint x = DEFAULT.x', 'const DEFAULT = Point()',
                               'int main();', '\treturn 0'),
                       program('interface Values;', '\tint sum(ref int values[N]);',
                               'const N = DEFAULT.x', 'const DEFAULT = Point()', 'model Point;',
                               '\tint x = DEFAULT.x', 'int main();', '\treturn 0')]:
            self.compile_source(source, error='循環')

    def test_model_constant_defaults_and_bounds_are_ready_when_demanded_early(self):
        self.check_program(program('interface Values;', '\tint sum(ref int values[N]);',
            'const N = DEFAULT.values.length', 'const DEFAULT = Point()', 'model Point;',
            '\tint values[SIZE] = INITIAL', 'const SIZE = 3', 'const INITIAL = [1]',
            'int main();', '\tprint(DEFAULT.values)', '\tprint(N)'), '[1, 0, 0]\n3\n')

    def test_cycles_in_inferred_and_mixed_declarations_are_diagnosed(self):
        for definitions in [('const A = A',), ('const A = B', 'const B = A'),
                            ('const int A = B', 'const B = A'),
                            ('const A = [B]', 'const B = [A]'),
                            ('const A = B.length', 'const B = [A]')]:
            with self.subTest(definitions=definitions):
                self.compile_source(program(*definitions, 'int main();', '\treturn 0'), error='循環')

    def test_dependency_depth_has_a_diagnostic_not_a_crash(self):
        accepted = [f'const C{i} = C{i+1} + 1' for i in range(100)] + ['const C100 = 0']
        self.check_program(program(*accepted, 'int main();', '\tprint(C0)'), '100\n')
        excessive = [f'const C{i} = C{i+1} + 1' for i in range(300)] + ['const C300 = 0']
        self.compile_source(program(*excessive, 'int main();', '\tprint(C0)'), error='依存は 256 段以下')

    def test_empty_or_contextless_initializers_are_rejected(self):
        for value in ['[]', '[[]]', '{}', '{x=1}', 'none']:
            self.compile_source(program('const N = ' + value, 'int main();', '\treturn 0'), error='エラー')

    def test_missing_initializers_dimensions_and_reserved_names_are_rejected(self):
        for definition in ['const N', 'const auto N', 'const auto N[] = [1]',
                           'const N[2] = [1, 2]', 'const int = 1', 'const true = 1']:
            self.compile_source(program(definition, 'int main();', '\treturn 0'), error='エラー')

    def test_runtime_functions_and_io_are_rejected(self):
        for value in ['read()', 'input()', 'io.file.open("unused.txt", "w")']:
            self.compile_source(program('int read() = 3', 'const VALUE = ' + value,
                'int main();', '\treturn 0'), error='const')

    def test_initializer_models_cannot_be_constant_even_when_type_is_inferred(self):
        prefix = program('model Counter;', '\tint n', '\tvoid init();', '\t\tthis.n = 3')
        self.compile_source(prefix + program('const VALUE = Counter()', 'int main();', '\treturn 0'), error='init')
        self.compile_source(program('interface Values;', '\tint sum(ref int values[N]);') + prefix +
            program('const N = VALUE.n', 'const VALUE = Counter()', 'int main();', '\treturn 0'), error='init')

    def test_mutation_and_mutable_ref_are_rejected(self):
        for body in ['\tLIMIT = 4', '\tLIMIT++', '\tinput(LIMIT)', '\tupdate(ref LIMIT)', '\tprint(LIMIT.addr)']:
            self.compile_source(program('const LIMIT = 3', 'void update(ref int n);', '\tn++',
                'int main();', body), error='エラー')
        self.compile_source(program('const VALUES = [1, 2]', 'int main();', '\tVALUES[0] = 3'), error='const')

    def test_readonly_ref_materializes_independent_constant_arrays(self):
        self.check_program(program('const VALUES = [1, 2]', 'int sum(ref readonly int values[]) = values[0] + values[1]',
            'int main();', '\tprint(sum(ref VALUES))', '\tprint(VALUES)'), '3\n[1, 2]\n')

    def test_inferred_model_constants_can_be_borrowed_through_interfaces(self):
        self.check_program(program('interface Value;', '\treadonly int get();',
            'model Point implements Value;', '\tint n = 3', '\treadonly int get() = this.n',
            'const POINT = Point()', 'int read(ref readonly Value value) = value.get()',
            'int main();', '\tprint(read(ref POINT))'), '3\n')

    def test_private_fields_do_not_leak_through_inferred_constants(self):
        self.compile_source(program('model Point;', '\tprivate int n = 3',
            'const POINT = Point()', 'const LEAK = POINT.n', 'int main();', '\treturn 0'), error='private')

    def test_name_collisions_remain_errors(self):
        for suffix in ['const auto LIMIT = 2', 'int LIMIT() = 2',
                       'int read(int LIMIT) = LIMIT', 'model LIMIT;\n\tint n']:
            self.compile_source(program('const LIMIT = 1', suffix, 'int main();', '\treturn 0'), error='エラー')
        self.compile_source(program('const LIMIT = 1', 'int main();', '\tint LIMIT = 2'), error='const')

    def test_local_shorthand_matches_local_const_auto(self):
        self.check_program(program('int main();', '\tconst N = 3', '\tconst auto M = N + 1',
            '\tif(true);', '\t\tconst N = 5', '\t\tprint(N)', '\tprint("{N} {M}")'), '5\n3 4\n')
        self.compile_source(program('int main();', '\tint n = 3', '\tconst C = n'), error='const')

    def test_integer_bases_and_boolean_short_circuit_keep_their_semantics(self):
        self.check_program(program('bool effect();', '\tprint("effect")', '\treturn true',
            'const FLAGS = 0xff xor 0x0f', 'const HEX = str(FLAGS, 16, true)',
            'const NUMBER = int(HEX)', 'const YES = true or effect()', 'const NO = false and effect()',
            'int main();', '\tprint("{NUMBER:0b} {HEX} {YES} {NO}")'), '0b11110000 0xf0 true false\n')

    def test_imported_inferred_constants_using_and_aliases(self):
        (self.directory / 'settings.qy').write_text(program('const auto LIMIT = BASE + 1', 'const BASE = 3',
            'const LABEL = "settings"', 'const VALUES = [LIMIT, BASE]'))
        self.check_program(program('import settings', 'using settings as cfg', 'using settings.LIMIT',
            'const TOTAL = LIMIT + cfg.BASE', 'int main();', '\tprint("{TOTAL} {cfg.LABEL} {cfg.VALUES}")'),
            '7 settings [4, 3]\n')

    def test_diamond_imports_keep_one_constant_definition(self):
        (self.directory / 'leaf.qy').write_text('const VALUE = 3\n')
        (self.directory / 'left.qy').write_text('import leaf\nconst VALUE = leaf.VALUE + 1\n')
        (self.directory / 'right.qy').write_text('import leaf\nconst auto VALUE = leaf.VALUE + 2\n')
        self.check_program(program('import left, right', 'const TOTAL = left.VALUE + right.VALUE',
            'int main();', '\tprint(TOTAL)'), '9\n')

    def test_imported_cycles_and_undefined_names_report_dependency_source(self):
        for body, message in [('const A = B\nconst B = A\n', '循環'), ('const A = MISSING\n', '宣言されていません')]:
            (self.directory / 'broken.qy').write_text(body)
            result = self.compile_source(program('import broken', 'const RESULT = broken.A',
                'int main();', '\tprint(RESULT)'), error=message, error_source='broken.qy')
            self.assertIn('broken.qy:', result.stderr.decode('utf-8'))

    def test_ambiguous_using_and_module_name_collisions_are_rejected(self):
        for name in ['left', 'right']:
            (self.directory / (name + '.qy')).write_text('const LIMIT = 3\n')
        self.compile_source(program('import left, right', 'using left, right', 'const VALUE = LIMIT',
            'int main();', '\treturn 0'), error='衝突')
        self.compile_source(program('import left', 'const left = 3', 'int main();', '\treturn 0'), error='衝突')


if __name__ == '__main__':
    run_suite()
