"""Direct input, indexed references, default array growth and their interactions."""

import subprocess

import modules_test as projects
from support import QynoTestCase, program, run_suite


TYPES = (
    ("int", "1", "7", "7"),
    ("float", "1.0", "2.5", "2.5"),
    ("double", "1.0", "2.5", "2.5"),
    ("bool", "false", "true", "true"),
    ("str", '"old"', '"日本語"', "日本語"),
    ("char", '"日"', '"😀"', "😀"),
)


class InputRefTests(QynoTestCase):
    compile_project = projects.ModuleTests.compile_project
    run_project = projects.ModuleTests.run_project

    def test_direct_input_all_scalar_types(self):
        for ty, initial, _, expected in TYPES:
            with self.subTest(type=ty):
                self.check_program(program(
                    "using io", "int main();", f"\t{ty} value = {initial}",
                    "\tinput(value)", "\tprint(value)",
                ), expected + "\n", stdin=expected + "\n")

    def test_direct_input_all_array_element_types(self):
        for ty, initial, _, expected in TYPES:
            with self.subTest(type=ty):
                self.check_program(program(
                    "int main();", f"\t{ty} matrix[][] = [[{initial}]]",
                    "\tio.input(matrix[0][0])", "\tio.print(matrix[0][0])",
                ), expected + "\n", stdin=expected + "\n")

    def test_requested_matrix_input(self):
        self.check_program(program(
            "using io", "int main();", "\tint matrix[2][3]",
            "\tfor(0 <= i < 2, i++);", "\t\tfor(0 <= j < 3, j++);",
            "\t\t\tinput(matrix[i][j])", "\tprint(matrix[0])", "\tprint(matrix[1])",
        ), "[0, 1, 2]\n[3, 4, 5]\n", stdin="0\n1\n2\n3\n4\n5\n")

    def test_formatted_array_input_all_types(self):
        for ty, initial, _, expected in TYPES:
            with self.subTest(type=ty):
                self.check_program(program(
                    "int main();", f"\t{ty} matrix[][] = [[{initial}]]",
                    '\tio.input("入力> {matrix[0][0]}")', "\tio.print(matrix[0][0])",
                ), "入力> " + expected + "\n", stdin=expected + "\n")

    def test_direct_input_returns_bool_and_preserves_failed_target(self):
        for text in ("invalid\n", "", "9223372036854775808\n"):
            with self.subTest(input=text):
                self.check_program(program(
                    "int main();", "\tint a[] = [7]", "\tio.print(io.input(a[0]))",
                    "\tio.print(a[0])",
                ), "false\n7\n", stdin=text)
        self.check_program(program(
            "int main();", "\tint n", "\tio.print(io.input(n))", "\tio.print(n)",
        ), "true\n42\n", stdin="42\n")

    def test_standalone_direct_input_failure(self):
        self.check_program(program(
            "int main();", "\tint a[] = [7]", "\tio.input(a[0])",
        ), "", stdin="invalid\n", status=65, error="入力を読み取れませんでした")

    def test_multi_field_array_failure_is_atomic(self):
        self.check_program(program(
            "int main();", "\tint a[] = [7, 8]",
            '\tio.print(io.input("{a[0]}, {a[1]}"))', "\tio.print(a)",
        ), "false\n[7, 8]\n", stdin="42, invalid\n")

    def test_input_indices_are_evaluated_once(self):
        self.check_program(program(
            "int index(ref int n);", "\tn++", "\treturn 0",
            "int main();", "\tint n = 0", "\tint a[][] = [[7]]",
            "\tio.input(a[index(ref n)][index(ref n)])",
            '\tio.print("{n} {a[0][0]}")',
        ), "2 42\n", stdin="42\n")

    def test_formatted_input_captures_indices_before_assignments(self):
        self.check_program(program(
            "int main();", "\tint i = 0", "\tint a[] = [7, 8]",
            '\tio.input("{i}, {a[i]}")', '\tio.print("{i} {a}")',
        ), "1 [42, 8]\n", stdin="1, 42\n")

    def test_input_revalidates_all_targets_before_commit(self):
        self.check_program(program(
            "int clear(ref int a[]);", "\ta.clear()", "\treturn 0",
            "int main();", "\tint a[] = [7]", "\tint b[] = [8]",
            '\tio.input("{b[0]}, {b[clear(ref b)]}")',
        ), "", stdin="1, 2\n", status=70, error="範囲外")

    def test_invalid_input_targets(self):
        for declarations, call, error in (
            ("const int N = 1", "io.input(N)", "const"),
            ("const int A[] = [1]", "io.input(A[0])", "const"),
            ("", "io.input([1])", "プロンプト"),
            ("", "io.input(prompt=[1])", "プロンプト"),
        ):
            with self.subTest(call=call):
                self.compile_source(program(declarations, "int main();", f"\t{call}"), error=error)
        self.compile_source(program(
            "int main();", "\tint a[] = [1]", "\tio.input(a)",
        ), error="直接入力")

    def test_direct_string_variable_is_target_and_keyword_is_prompt(self):
        self.check_program(program(
            "int main();", '\tstr text = "old"', "\tio.input(text)", "\tio.print(text)",
            '\tstr prompt = "name> "', "\tstr name = io.input(prompt=prompt)",
            "\tio.print(name)",
        ), "new\nname> Alice\n", stdin="new\nAlice\n")

    def test_prompts_use_print_format_for_all_scalar_types(self):
        for ty, initial, _, _ in TYPES:
            prompt = initial.strip('"')
            if ty in ("float", "double"):
                prompt = "1.0"
            with self.subTest(type=ty):
                self.check_program(program(
                    "int main();", f"\t{ty} prompt = {initial}",
                    "\tint n = io.input(prompt=prompt)", "\tio.print(n)",
                ), prompt + "42\n", stdin="42\n")
        self.check_program(program(
            "int main();", "\tint n = io.input(123)", "\tio.print(n)",
        ), "12342\n", stdin="42\n")

    def test_unicode_string_index_input(self):
        self.check_program(program(
            "int main();", '\tstr words[] = ["日本"]',
            "\tio.input(words[0][1])", "\tio.print(words[0])",
        ), "日😀\n", stdin="😀\n")
        self.check_program(program(
            "int main();", '\tstr word = "日本"',
            '\tio.print(io.input("{word[0]}, {word[1]}"))', "\tio.print(word)",
        ), "false\n日本\n", stdin="😀, ab\n")

    def test_input_bounds_and_aliases(self):
        self.check_program(program(
            "using io as console", "int main();", "\tint n = 0", "\tref int alias = n.addr",
            "\tconsole.input(alias)", "\tconsole.print(n)",
        ), "42\n", stdin="42\n")
        self.check_program(program(
            "int main();", "\tint a[] = [7]", "\tint i = 1", "\tio.input(a[i])",
        ), "", stdin="42\n", status=70, error="範囲外")

    def test_element_reference_all_scalar_types(self):
        for ty, initial, replacement, expected in TYPES:
            with self.subTest(type=ty):
                self.check_program(program(
                    f"void change(ref {ty} value);", f"\tvalue = {replacement}",
                    "int main();", f"\t{ty} matrix[][] = [[{initial}]]",
                    "\tchange(ref matrix[0][0])", "\tio.print(matrix[0][0])",
                ), expected + "\n")

    def test_whole_array_reference_all_scalar_types(self):
        for ty, initial, replacement, expected in TYPES:
            with self.subTest(type=ty):
                self.check_program(program(
                    f"void change(ref {ty} matrix[][]);", f"\tmatrix[0][0] = {replacement}",
                    "int main();", f"\t{ty} matrix[][] = [[{initial}]]",
                    "\tchange(ref matrix)", "\tio.print(matrix[0][0])",
                ), expected + "\n")

    def test_requested_element_and_whole_array_increments(self):
        self.check_program(program(
            "using io", "int main();", "\tint matrix[2][3] = [[0, 1, 2], [3, 4, 5]]",
            "\tfor(0 <= i < matrix.length, i++);",
            "\t\tfor(0 <= j < matrix[i].length, j++);",
            "\t\t\tincrease_element(ref matrix[i][j])",
            "\tincrease_all(ref matrix)", "\tprint(matrix)",
            "void increase_element(ref int value);", "\tvalue++",
            "void increase_all(ref int matrix[][]);",
            "\tfor(0 <= i < matrix.length, i++);",
            "\t\tfor(0 <= j < matrix[i].length, j++);", "\t\t\tmatrix[i][j]++",
        ), "[[2, 3, 4], [5, 6, 7]]\n")

    def test_reference_indices_once_and_argument_order(self):
        self.check_program(program(
            "int index(ref int n);", "\tn++", "\treturn 0",
            "int after(ref int n);", "\tio.print(n)", "\treturn 10",
            "void change(ref int value, int increment);", "\tvalue += increment",
            "int main();", "\tint n = 0", "\tint a[][] = [[7]]",
            "\tchange(ref a[index(ref n)][index(ref n)], after(ref n))",
            '\tio.print("{n} {a}")',
        ), "2\n2 [[17]]\n")

    def test_reference_aliases_forwarding_and_release(self):
        self.check_program(program(
            "void twice(ref int a, ref int b);", "\ta++", "\tb++",
            "void forward(ref int a);", "\ttwice(ref a, ref a)",
            "int main();", "\tint a[] = [7]", "\tforward(ref a[0])", "\ta.add(10)",
            "\ta.remove_at(0)", "\tio.print(a)",
        ), "[10]\n")

    def test_nested_call_result_preserves_borrowed_reference(self):
        self.check_program(program(
            "int bump(ref int x);", "\tx++", "\treturn x",
            "int total(int a, int b) = a + b",
            "int main();", "\tint a[] = [1, 2]",
            "\tio.print(total(bump(ref a[0]), bump(ref a[1])))", "\tio.print(a)",
        ), "5\n[2, 3]\n")

    def test_reference_bounds_checked_before_later_arguments(self):
        self.check_program(program(
            "int later();", '\tio.print("unexpected")', "\treturn 0",
            "void f(ref int x, int n);", "\tx++",
            "int main();", "\tint a[] = [7]", "\tint i = 1", "\tf(ref a[i], later())",
        ), "", status=70, error="範囲外")

    def test_invalid_element_references(self):
        for source, error in (
            (program("const int A[] = [1]", "void f(ref int x);", "\tx++",
                     "int main();", "\tf(ref A[0])"), "const"),
            (program("void f(ref double x);", "\tx++", "int main();",
                     "\tint a[] = [1]", "\tf(ref a[0])"), "型"),
            (program("void f(ref int x);", "\tx++", "int main();",
                     "\tint a[] = [1]", "\tf(a[0])"), "呼び出し側"),
            (program("void f(ref str x);", '\tx = "x"', "int main();",
                     '\tstr s = "abc"', "\tf(ref s[0])"), "文字列内"),
            (program("void f(ref int x);", "\tx++", "int main();",
                     "\tf(ref (1 + 2))"), "更新先"),
        ):
            with self.subTest(source=source):
                self.compile_source(source, error=error)

    def test_borrow_prevents_structural_mutation(self):
        for operation in ("a.add(2)", "a.add()", "a.clear()", "a.remove_at(0)", "a = [2]"):
            with self.subTest(operation=operation):
                self.check_program(program(
                    "void f(ref int x, ref int a[]);", f"\t{operation}", "\tx++",
                    "int main();", "\tint a[] = [7]", "\tf(ref a[0], ref a)",
                ), "", status=70, error="ref")

    def test_borrow_prevents_mutation_of_ancestors(self):
        for operation in (
            "a.add()", "a.remove_at(0)", "a.clear()", "a[0].add(2)",
            "a[0].clear()", "a[0] = [2]", "a = [[2]]",
        ):
            with self.subTest(operation=operation):
                self.check_program(program(
                    "void f(ref int x, ref int a[][]);", f"\t{operation}", "\tx++",
                    "int main();", "\tint a[][] = [[7], [8]]", "\tf(ref a[0][0], ref a)",
                ), "", status=70, error="ref")

    def test_borrow_prevents_mutation_in_later_argument(self):
        self.check_program(program(
            "int grow(ref int a[]);", "\ta.add()", "\treturn 1",
            "void f(ref int x, int n);", "\tx++",
            "int main();", "\tint a[] = [7]", "\tf(ref a[0], grow(ref a))",
        ), "", status=70, error="ref")

    def test_other_row_mutation_and_value_copy_are_allowed(self):
        self.check_program(program(
            "void f(ref int x, ref int a[][]);", "\ta[1].add(9)", "\tint copy[][] = a",
            "\tcopy.clear()", "\tx++",
            "int main();", "\tint a[][] = [[7], [8]]", "\tf(ref a[0][0], ref a)",
            "\ta[0].add(10)", "\tio.print(a)",
        ), "[[8, 10], [8, 9]]\n")

    def test_subarray_reference_can_grow(self):
        self.check_program(program(
            "void grow(ref int row[]);", "\trow.add(9)",
            "int main();", "\tint a[][] = [[7]]", "\tgrow(ref a[0])", "\tio.print(a)",
        ), "[[7, 9]]\n")

    def test_nested_assignment_and_calls_have_independent_scratch(self):
        self.check_program(program(
            "int bump(ref int x);", "\tx++", "\treturn x",
            "int index(ref int x);", "\tx++", "\treturn 0",
            "int main();", "\tint a[][] = [[1]]", "\tint b[] = [2]", "\tint n = 0",
            "\ta[index(ref n)][index(ref n)] += bump(ref b[0])",
            '\tio.print("{a} {b} {n}")',
        ), "[[4]] [3] 2\n")

    def test_default_add_all_scalar_types(self):
        for ty, expected in (
            ("int", "0"), ("float", "0.0"), ("double", "0.0"),
            ("bool", "false"), ("str", ""), ("char", "\0"),
        ):
            with self.subTest(type=ty):
                self.check_program(program(
                    "int main();", f"\t{ty} a[]", "\ta.add()", "\ta.add()",
                    '\tio.print("{a.length}|{a[0]}|{a[1]}")',
                ), f"2|{expected}|{expected}\n")

    def test_default_add_independent_nested_arrays(self):
        self.check_program(program(
            "void increase_length_of_matrix(ref int matrix[][]);", "\tmatrix.add()",
            "int main();", "\tint matrix[][]", "\tincrease_length_of_matrix(ref matrix)",
            "\tincrease_length_of_matrix(ref matrix)", "\tmatrix[0].add()",
            "\tio.print(matrix)",
        ), "[[0], []]\n")

    def test_default_add_bounded_children(self):
        self.check_program(program(
            "int main();", "\tint a[][2]", "\ta.add()", "\tio.print(a)",
            "\tchar c[][1]", "\tc.add()", "\tio.print(c[0][0].length)",
        ), "[[0, 0]]\n1\n")

    def test_default_add_limits_and_invalid_arity(self):
        self.check_program(program(
            "int main();", "\tint a[1]", "\ta.add()",
        ), "", status=70, error="最大要素数")
        for statement in ("a.add(1, 2)", "a.remove_at()", "a.clear(1)"):
            with self.subTest(statement=statement):
                self.compile_source(program(
                    "int main();", "\tint a[]", f"\t{statement}",
                ), error="引数")

    def test_default_add_gc_pressure_and_ref_cleanup(self):
        self.check_program(program(
            "int bump(ref int x);", "\tx++", "\treturn x",
            "int main();", "\tint a[][]", "\tfor(0 <= i < 300, i++);",
            "\t\ta.add()", "\t\ta[i].add(i)", "\t\tbump(ref a[i][0])",
            '\tio.print("{a.length} {a[0][0]} {a[299][0]}")',
        ), "300 1 300\n")

    def test_references_input_and_growth_across_modules(self):
        self.run_project({
            "main.qy": program(
                "import values", "int main();", "\tint a[][] = [[7]]",
                "\tvalues.read(ref a[0][0])", "\tvalues.grow(ref a)", "\tio.print(a)",
            ),
            "values.qy": program(
                "using io as console", "void read(ref int value);", "\tconsole.input(value)",
                "void grow(ref int a[][]);", "\ta.add()",
            ),
        }, "[[42], []]\n", stdin="42\n")

    def test_borrow_error_reports_mutation_in_imported_function(self):
        self.run_project({
            "main.qy": program(
                "import values", "int main();", "\tint a[] = [7]",
                "\tvalues.change(ref a[0], ref a)",
            ),
            "values.qy": program(
                "void change(ref int value, ref int a[]);", "\ta.clear()", "\tvalue++",
            ),
        }, "", status=70, diagnostic="values.qy:2:")

    def test_windows_generation(self):
        files = {"main.qy": program(
            "int main();", "\tint a[][]", "\ta.add()", "\ta[0].add()",
            "\tchange(ref a[0][0])", "\tio.input(a[0][0])", "\tio.print(a)",
            "void change(ref int value);", "\tvalue++",
        )}
        binary = self.compile_project(files, flags=("--target", "windows"))
        data = binary.read_bytes()
        self.assertEqual(data[:2], b"MZ")
        offset = int.from_bytes(data[60:64], "little")
        self.assertEqual(data[offset:offset + 4], b"PE\0\0")
        assembly = self.compile_project(files, flags=("-S", "--target", "windows"))
        self.assertIn('.section .rdata,"dr"', assembly.read_text())

    def test_linux_assembly_generation(self):
        assembly = self.compile_project({"main.qy": program(
            "int main();", "\tint a[][]", "\ta.add()", "\ta[0].add()",
            "\tchange(ref a[0][0])", '\tio.input("入力> {a[0][0]}")',
            "void change(ref int value);", "\tvalue++",
        )}, flags=("-S", "--target", "linux"))
        result = subprocess.run([
            "gcc", "-x", "assembler", "-c", str(assembly),
            "-o", str(self.directory / "input-refs.o"),
        ], capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    run_suite()
