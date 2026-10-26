"""Array values, shape limits, mutation, evaluation order and bounds."""

from support import QynoTestCase, program, run_suite


class ArrayTests(QynoTestCase):
    def test_bounded_initialization_and_padding(self):
        self.check_program(program(
            "int main();", "\tint a[4] = [7, 8]", "\tint b[2]",
            '\tprint("{a.length} {a[0]} {a[1]} {a[2]} {a[3]} {b[1]}")',
        ), "4 7 8 0 0 0\n")

    def test_unbounded_add_remove_clear(self):
        self.check_program(program(
            "int main();", "\tint a[] = []", "\ta.add(10)", "\ta.add(20)", "\ta.add(30)",
            "\ta.remove_at(1)", '\tprint("{a.length} {a[0]} {a[1]}")', "\ta.clear()",
            "\tprint(a.length)", "\ta.add(42)", "\tprint(a[0])",
        ), "2 10 30\n0\n42\n")

    def test_growth_preserves_values(self):
        self.check_program(program(
            "int main();", "\tint a[]", "\tfor(0 <= i < 200, i++);", "\t\ta.add(i)",
            "\tint total = 0", "\tfor(0 <= i < a.length, i++);", "\t\ttotal += a[i]",
            '\tprint("{a.length} {total}")',
        ), "200 19900\n")

    def test_numeric_and_text_element_types(self):
        self.check_program(program(
            "int main();", "\tdouble a[] = [1, 2.5]", '\tchar letters[] = ["日", "😀"]',
            "\tbool flags[] = [true, false]",
            '\tprint("{a[0]} {a[1]} {letters[1]} {flags[0]}")',
        ), "1.0 2.5 😀 true\n")

    def test_nested_arrays_have_independent_children(self):
        self.check_program(program(
            "int main();", "\tint a[2][2]", "\ta[0][0] = 9",
            '\tprint("{a[0][0]} {a[1][0]} {a[1][1]}")',
        ), "9 0 0\n")

    def test_declaration_and_assignment_copy_deeply(self):
        self.check_program(program(
            "int main();", "\tint a[][] = [[1, 2], [3]]", "\tint b[][] = a",
            "\tb[0][0] = 9", "\tint c[][]", "\tc = a", "\tc[1].add(4)",
            '\tprint("{a[0][0]} {b[0][0]} {a[1].length} {c[1].length}")',
        ), "1 9 1 2\n")

    def test_value_and_reference_array_parameters(self):
        self.check_program(program(
            "void value(int a[]);", "\ta[0] = 10", "\ta.add(20)",
            "void reference(ref int a[]);", "\ta[0] = 30", "\ta.add(40)",
            "int main();", "\tint a[] = [1]", "\tvalue(a)",
            '\tprint("{a[0]} {a.length}")', "\treference(ref a)",
            '\tprint("{a[0]} {a.length} {a[1]}")',
        ), "1 1\n30 2 40\n")

    def test_returned_array(self):
        self.check_program(program(
            "int make(int n)[];", "\treturn [n, n + 1]",
            "int main();", "\tint a[] = make(7)", '\tprint("{a[0]} {a[1]}")',
        ), "7 8\n")

    def test_compound_index_evaluated_once(self):
        self.check_program(program(
            "int index(ref int n);", "\tn++", "\treturn 0",
            "int main();", "\tint a[] = [10]", "\tint n = 0", "\ta[index(ref n)] += 5",
            '\tprint("{a[0]} {n}")',
        ), "15 1\n")

    def test_rhs_can_reallocate_destination(self):
        self.check_program(program(
            "int grow(ref int a[]);", "\tfor(0 <= i < 100, i++);", "\t\ta.add(i)", "\treturn 7",
            "int main();", "\tint a[] = [1]", "\ta[0] += grow(ref a)",
            '\tprint("{a[0]} {a.length} {a[100]}")',
        ), "8 101 99\n")

    def test_nested_array_and_text_updates(self):
        self.check_program(program(
            "int main();", '\tstr words[] = ["abc", "日"]', '\twords[0][1] = "😀"',
            "\tint rows[][] = [[1], [2]]", "\trows[1].add(3)",
            '\tprint("{words[0]} {words[1]} {rows[1][1]}")',
        ), "a😀c 日 3\n")

    def test_const_array_index_and_length(self):
        self.check_program(program(
            "const int A[3] = [4, 5]", "const int N = A.length + A[1]",
            "int main();", '\tprint("{N} {A[2]}")',
        ), "8 0\n")

    def test_runtime_bounds_errors(self):
        for operation in ("print(a[i])", "a[i] = 9", "a.remove_at(i)"):
            for index in (-1, 2):
                with self.subTest(operation=operation, index=index):
                    self.check_program(program(
                        "int main();", "\tint a[] = [1, 2]", "\tint i = input()", f"\t{operation}",
                    ), "", stdin=f"{index}\n", status=70, error="範囲外")

    def test_runtime_capacity_error(self):
        self.check_program(program(
            "int main();", "\tint a[1] = [1]", "\ta.add(2)",
        ), "", status=70, error="最大要素数")

    def test_runtime_assignment_shape_error(self):
        self.check_program(program(
            "int main();", "\tint a[1]", "\tint b[] = [1, 2]", "\ta = b",
        ), "", status=70, error="上限")

    def test_invalid_array_programs(self):
        cases = (
            ("\tint a[-1]", "上限"),
            ("\tint n = 2\n\tint a[n]", "コンパイル時"),
            ("\tint a[1] = [1, 2]", "上限"),
            ('\tint a[] = [1, "x"]', "変換"),
            ("\tint a[] = [1]\n\ta.length = 2", "読み取り専用"),
            ("\tint a[] = [1]\n\ta.add()", "引数"),
            ("\tint a[] = [1]\n\ta.clear(0)", "引数"),
            ("\tint a[] = [1]\n\ta.remove_at(-1)", "範囲外"),
            ("\tint a[0]\n\ta.add(1)", "上限0"),
            ("\tint a[] = [1]\n\tprint(a.add(2))", "値を返しません"),
            ("\tconst int a[] = [1]\n\ta[0] = 2", "const"),
            ("\tconst int a[] = [1]\n\ta.clear()", "const"),
            ("\tint a" + "[]" * 33, "次元数"),
        )
        for body, message in cases:
            with self.subTest(body=body):
                self.compile_source("int main();\n" + body + "\n", error=message)


if __name__ == "__main__":
    run_suite()
