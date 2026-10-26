"""Evaluation order, scopes, control flow, references and diagnostics."""

from support import QynoTestCase, program, run_suite


class SemanticsTests(QynoTestCase):
    def test_short_circuit_does_not_call_rhs(self):
        self.check_program(program(
            "bool bump(ref int n);", "\tn++", "\treturn true",
            "int main();", "\tint n = 0",
            "\tprint(false and bump(ref n))", "\tprint(true or bump(ref n))",
            "\tprint(n)", "\tprint(true and bump(ref n))", "\tprint(n)",
        ), "false\ntrue\n0\ntrue\n1\n")

    def test_comparison_chain_evaluates_middle_once(self):
        self.check_program(program(
            "int next(ref int n);", "\tn++", "\treturn n",
            "int main();", "\tint n = 0", "\tprint(0 < next(ref n) < 2)", "\tprint(n)",
            "\tprint(2 < 1 < next(ref n))", "\tprint(n)",
        ), "true\n1\nfalse\n1\n")

    def test_const_forward_reference(self):
        self.check_program(program(
            "const int A = B + 1", "const int B = 41", "int main();", "\tprint(A)",
        ), "42\n")

    def test_recursion_and_forward_function(self):
        self.check_program(program(
            "int main();", "\tprint(fact(6))", "int fact(int n);",
            "\tif(n <= 1);", "\t\treturn 1", "\treturn n * fact(n - 1)",
        ), "720\n")

    def test_value_and_reference_parameters(self):
        self.check_program(program(
            "void value(val int n);", "\tn += 10",
            "void reference(ref int n);", "\tn += 10",
            "int main();", "\tint n = 5", "\tvalue(val n)", "\tprint(n)",
            "\treference(ref n)", "\tprint(n)",
        ), "5\n15\n")

    def test_reference_alias_and_rebinding(self):
        self.check_program(program(
            "int main();", "\tint a = 1", "\tint b = 2", "\tref int p = a.addr",
            "\tp++", "\tp.addr = b.addr", "\tp += 10",
            '\tprint("{a} {b} {p.value}")',
        ), "2 12 12\n")

    def test_null_reference_runtime_error(self):
        self.check_program(program("int main();", "\tref int p", "\tprint(p)"),
                           "", status=70, error="指して")

    def test_nested_break_and_continue(self):
        self.check_program(program(
            "int main();", "\tint total = 0", "\tfor(1 <= i <= 4, i++);",
            "\t\tif(i == 2);", "\t\t\tcontinue",
            "\t\tfor(1 <= j <= 4, j++);", "\t\t\tif(j == 3);", "\t\t\t\tbreak",
            "\t\t\ttotal += i * 10 + j", "\tprint(total)",
        ), "169\n")

    def test_descending_range_and_while(self):
        self.check_program(program(
            "int main();", "\tint total = 0", "\tfor(5 >= i > 0, i -= 2);",
            "\t\ttotal += i", "\twhile(total < 12);", "\t\ttotal++", "\tprint(total)",
        ), "12\n")

    def test_using_global_and_block_scope(self):
        self.check_program(program(
            "using math.sqrt", "int main();", "\tprint(sqrt(9))", "\tif(true);",
            "\t\tusing math.round", "\t\tprint(round(2.5))",
        ), "3.0\n3.0\n")
        self.compile_source(program(
            "int main();", "\tif(true);", "\t\tusing math.sqrt", "\t\tprint(sqrt(4))",
            "\tprint(sqrt(4))",
        ), error="using")

    def test_numeric_promotion_and_precedence(self):
        self.check_program(program(
            "int main();", "\tfloat f = 1.5", "\tdouble d = f + 2",
            '\tprint("{d} {2^3^2} {-2^2} {(-2)^2} {-7 / 3} {-7 % 3}")',
        ), "3.5 512 -4 4 -2 -1\n")

    def test_runtime_diagnostic_location_and_status(self):
        result = self.check_program(program(
            "int main();", "\tint divisor = input()", "\tprint(10 / divisor)",
        ), "", stdin="0\n", status=70, error="0")
        self.assertIn(b":3:", result.stderr)

    def test_input_failure_is_atomic(self):
        self.check_program(program(
            "int main();", "\tint a = 7", "\tint b = 8",
            '\tprint(input("{a}, {b}"))', '\tprint("{a} {b}")',
        ), "false\n7 8\n", stdin="1, wrong\n")

    def test_invalid_programs(self):
        cases = (
            ("const int A = B\nconst int B = A\nint main();\n\tprint(A)\n", "循環"),
            ("int main();\n\tif(true);\n\t\tint x = 1\n\tprint(x)\n", "変数"),
            ("int main();\n\tint n = 1\n\tif(true);\n\t\tint n = 2\n", "すでに"),
            ("int main();\n\tif(1);\n\t\treturn 0\n", "bool"),
            ("int main();\n\tbreak\n", "break"),
            ("int main();\n\tcontinue\n", "continue"),
            ("int f(int n);\n\tif(n > 0);\n\t\treturn n\nint main();\n\treturn 0\n", "return"),
            ("int main();\n\tdouble d = 1.5\n\tfloat f = d\n", "自動変換"),
            ("int main();\n\tref int p\n\tif(true);\n\t\tint x = 1\n\t\tp.addr = x.addr\n", "ブロック"),
        )
        for source, message in cases:
            with self.subTest(source=source):
                self.compile_source(source, error=message)


if __name__ == "__main__":
    run_suite()
