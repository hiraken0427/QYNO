"""Range iteration, evaluation order, checked updates and floating-point safety."""

import itertools
import subprocess

import modules_test as projects
from support import QynoTestCase, program, run_suite


LIMITS = "const int_64 MIN = -9223372036854775807 - 1\nconst int_64 MAX = 9223372036854775807\n"


class ForTests(QynoTestCase):
    # Reuse the existing multi-file fixture without inheriting its test methods.
    compile_project = projects.ModuleTests.compile_project
    run_project = projects.ModuleTests.run_project

    def test_reverse_array_lengths(self):
        for length in (0, 1, 10):
            with self.subTest(length=length):
                values = ", ".join(map(str, range(length)))
                self.check_program(program(
                    "int main();", f"\tint values[] = [{values}]",
                    "\tfor(values.length > i >= 0, i--);", "\t\tio.print(values[i])",
                ), "".join(f"{i}\n" for i in reversed(range(length))))

    def test_integer_boundaries_and_steps_against_range(self):
        for (start, end), step, open_start, inclusive_end in itertools.product(
            ((0, 0), (0, 5), (5, 0), (-3, 3), (3, -3)), (1, 2, -1, -2), (False, True), (False, True),
        ):
            with self.subTest(start=start, end=end, step=step, open_start=open_start, inclusive_end=inclusive_end):
                increasing = step > 0
                left = ("<" if open_start else "<=") if increasing else (">" if open_start else ">=")
                right = ("<=" if inclusive_end else "<") if increasing else (">=" if inclusive_end else ">")
                initial = start + step if open_start else start
                stop = end + (1 if increasing else -1) if inclusive_end else end
                expected = "".join(f"{i}\n" for i in range(initial, stop, step))
                self.check_program(program(
                    "int main();", f"\tint n = {start}", f"\tint m = {end}",
                    f"\tfor(n {left} int i {right} m, i += {step});", "\t\tio.print(i)",
                ), expected)

    def test_open_start_uses_full_step(self):
        self.check_program(program(
            "int main();", "\tfor(10 > i >= 0, i -= 2);", "\t\tio.print(i)",
            "\tfor(0 < i <= 5, i -= -2);", "\t\tio.print(i)",
        ), "8\n6\n4\n2\n0\n2\n4\n")

    def test_start_once_and_endpoint_once_per_check(self):
        self.check_program(program(
            "int begin(ref int calls);", "\tcalls++", "\treturn 0",
            "int finish(ref int calls);", "\tcalls++", "\treturn 3",
            "int main();", "\tint starts = 0", "\tint ends = 0", "\tint count = 0",
            "\tfor(begin(ref starts) < i <= finish(ref ends), i++);", "\t\tcount++",
            '\tio.print("{starts} {ends} {count}")',
        ), "1 4 3\n")

    def test_changed_endpoint_is_rechecked_but_start_is_not(self):
        self.check_program(program(
            "int main();", "\tint n = 2", "\tint m = 4", "\tfor(n < i <= m, i++);",
            "\t\tn = 100", "\t\tif(i == 3);", "\t\t\tm = 5", "\t\tio.print(i)",
        ), "3\n4\n5\n")

    def test_initial_overflow_skips_endpoint(self):
        for header in ("MAX < i <= finish(ref calls), i++", "MIN > i >= finish(ref calls), i--"):
            with self.subTest(header=header):
                self.check_program(LIMITS + program(
                    "int finish(ref int calls);", "\tcalls++", "\treturn 0", "int main();", "\tint calls = 0",
                    f"\tfor({header});", '\t\tio.print("unreachable")', "\tio.print(calls)",
                ), "0\n")

    def test_overflow_after_body_does_not_recheck_endpoint(self):
        self.check_program(LIMITS + program(
            "int_64 finish(ref int calls);", "\tcalls++", "\treturn MAX", "int main();", "\tint calls = 0",
            "\tfor(MAX <= i <= finish(ref calls), i++);", "\t\tio.print(i)", "\tio.print(calls)",
        ), "9223372036854775807\n1\n")

    def test_extreme_values_and_checked_update(self):
        cases = (
            ("MIN >= i >= MIN, i--", [-9223372036854775808]),
            ("MAX <= i <= MAX, i++", [9223372036854775807]),
            ("MIN + 1 > i >= MIN, i--", [-9223372036854775808]),
            ("MAX - 1 < i <= MAX, i++", [9223372036854775807]),
            ("MAX - 1 <= i <= MAX, i += 2", [9223372036854775806]),
            ("MIN + 1 >= i >= MIN, i -= 2", [-9223372036854775807]),
        )
        for header, values in cases:
            with self.subTest(header=header):
                self.check_program(LIMITS + program("int main();", f"\tfor({header});", "\t\tio.print(i)"),
                                   "".join(f"{i}\n" for i in values))

    def test_minimum_integer_step_without_negation(self):
        for header, expected in (
            ("MIN < i <= 0, i -= MIN", "0\n"),
            ("MIN <= i <= 0, i -= MIN", "-9223372036854775808\n0\n"),
            ("0 > int_64 i >= MIN, i += MIN", "-9223372036854775808\n"),
        ):
            with self.subTest(header=header):
                self.check_program(LIMITS + program("int main();", f"\tfor({header});", "\t\tio.print(i)"), expected)

    def test_normal_integer_arithmetic_still_wraps(self):
        self.check_program(LIMITS + program("int main();", "\tint_64 n = MAX", "\tn++", "\tio.print(n)"),
                           "-9223372036854775808\n")

    def test_continue_break_and_nested_scopes(self):
        self.check_program(program(
            "int main();", "\tfor(0 < i <= 4, i++);", "\t\tif(i == 2);", "\t\t\tcontinue",
            "\t\tfor(0.0 < double x <= 1.0, x += 0.5);", '\t\t\tio.print("{i} {x}")',
            "\t\t\tif(x == 1.0);", "\t\t\t\tbreak", "\tfor(0 < i <= 1, i++);", "\t\tio.print(i)",
        ), "1 0.5\n1 1.0\n3 0.5\n3 1.0\n4 0.5\n4 1.0\n1\n")

    def test_array_bounds_remain_checked(self):
        self.check_program(program(
            "int main();", "\tint a[] = [1, 2]", "\tfor(a.length > i >= 0, i--);",
            "\t\ta.clear()", "\t\tio.print(a[i])",
        ), "", status=70, error="範囲外")

    def test_float_quarter_steps(self):
        for ty, direction, open_start, inclusive_end in itertools.product(
            ("float", "double"), (1, -1), (False, True), (False, True),
        ):
            with self.subTest(ty=ty, direction=direction, open_start=open_start, inclusive_end=inclusive_end):
                first, last = (0, 4) if direction > 0 else (4, 0)
                left = ("<" if open_start else "<=") if direction > 0 else (">" if open_start else ">=")
                right = ("<=" if inclusive_end else "<") if direction > 0 else (">=" if inclusive_end else ">")
                indices = range(first + direction if open_start else first,
                                last + direction if inclusive_end else last, direction)
                expected = "".join(f"{i / 4}\n" for i in indices)
                self.check_program(program(
                    "int main();", f"\tfor({first / 4} {left} {ty} x {right} {last / 4}, x += {direction * 0.25});",
                    "\t\tio.print(x)",
                ), expected)

    def test_decimal_rounding_and_existing_comparison_promotion(self):
        self.check_program(program(
            "int main();", "\tfor(0.0 < double x <= 0.3, x += 0.1);", "\t\tio.print(x)",
            "\tfor(0.0 < float x <= 0.3, x += 0.1);", "\t\tio.print(x)",
            "\tdouble limit = 0.3", "\tfor(0.0 < float x <= limit, x += 0.1);", "\t\tio.print(x)",
        ), "0.1\n0.2\n0.1\n0.2\n0.3\n0.1\n0.2\n")

    def test_float_start_and_limit_are_not_repeated(self):
        self.check_program(program(
            "double begin(ref int calls);", "\tcalls++", "\treturn 0.0",
            "double finish(ref int calls);", "\tcalls++", "\treturn 0.5",
            "int main();", "\tint starts = 0", "\tint ends = 0",
            "\tfor(begin(ref starts) < double x <= finish(ref ends), x += 0.25);", "\t\tio.print(x)",
            '\tio.print("{starts} {ends}")',
        ), "0.25\n0.5\n1 3\n")

    def test_zero_and_reversed_float_intervals(self):
        self.check_program(program(
            "int main();", "\tfor(0.0 < double x <= 0.0, x += 0.25);", '\t\tio.print("bad")',
            "\tfor(1.0 < float x <= 0.0, x++);", '\t\tio.print("bad")', '\tio.print("done")',
        ), "done\n")

    def test_invalid_steps_and_types(self):
        cases = (
            ("0 < i <= 3, i--", "向き"), ("3 > i >= 0, i++", "向き"),
            ("0 < i <= 3, i += 0", "0"), ("0.0 < double x <= 1.0, x += -0.0", "0"),
            ("0 < i <= 3.5, i++", "int"), ("0 < i <= 3, i += 0.5", "int"),
            ("false <= bool b <= true, b++", "bool"), ("false < i <= 3, i++", "bool"),
            ("0 < i <= true, i++", "bool"), ("0 < i <= 3, i += true", "bool"),
            ('"a" <= str s <= "z", s++', "str"), ("0 < i >= 3, i++", "向き"),
        )
        for header, error in cases:
            with self.subTest(header=header):
                self.compile_source(program("int main();", f"\tfor({header});", "\t\treturn 0"), error=error)
        self.compile_source(program("int main();", "\tint step = 1", "\tfor(0 < i <= 3, i += step);", "\t\treturn 0"), error="定数")

    def test_nonfinite_constant_headers(self):
        for header in (
            "1.0 / 0.0 <= double x <= 2.0, x++",
            "0.0 < double x <= 1.0 / 0.0, x++",
            "0.0 < double x <= 1.0, x += 1.0 / 0.0",
            "0.0 < double x <= 1.0, x += math.sqrt(-1.0)",
            "0.0 < float x <= 10000000000000000000000000000000000000000.0, x++",
        ):
            with self.subTest(header=header):
                self.compile_source(program("int main();", f"\tfor({header});", "\t\treturn 0"), error="有限")

    def test_step_underflows_after_conversion(self):
        tiny = "0." + "0" * 50 + "1"
        self.compile_source(program("int main();", f"\tfor(0.0 < float x <= 1.0, x += {tiny});", "\t\treturn 0"), error="0")

    def test_statically_known_initial_stagnation(self):
        for ty, start, limit in (("float", "16777216.0", "16777220.0"),
                                 ("double", "9007199254740992.0", "9007199254740996.0")):
            with self.subTest(ty=ty):
                self.compile_source(program("int main();", f"\tfor({start} < {ty} x <= {limit}, x++);", "\t\treturn 0"), error="前進")

    def test_runtime_initial_stagnation(self):
        for ty, start, limit in (("float", "16777216", "16777220.0"),
                                 ("double", "9007199254740992", "9007199254740996.0")):
            with self.subTest(ty=ty):
                self.check_program(program(
                    "int main();", f"\t{ty} start = io.input()", f"\tfor(start < {ty} x <= {limit}, x++);",
                    '\t\tio.print("unreachable")',
                ), "", stdin=start + "\n", status=70, error="前進")

    def test_runtime_later_stagnation(self):
        for ty, start, limit, expected in (
            ("float", "16777215.0", "16777220.0", "16777215\n16777216\n"),
            ("double", "9007199254740991.0", "9007199254740996.0", "9007199254740991\n9007199254740992\n"),
        ):
            with self.subTest(ty=ty):
                self.check_program(program("int main();", f"\tfor({start} <= {ty} x <= {limit}, x++);", "\t\tio.print(int_64(x))"),
                                   expected, status=70, error="前進")

    def test_break_does_not_perform_unnecessary_update(self):
        self.check_program(program("int main();", "\tfor(16777216.0 <= float x <= 16777216.0, x++);", "\t\tbreak", '\tio.print("done")'), "done\n")

    def test_nonfinite_runtime_start_or_endpoint(self):
        for ty, part, expression in itertools.product(("float", "double"), ("start", "end"), ("1.0 / z", "z / z")):
            with self.subTest(ty=ty, part=part, expression=expression):
                start, end = ("bad", "1.0") if part == "start" else ("0.0", "bad")
                self.check_program(program(
                    "int main();", f"\t{ty} z = 0.0", f"\t{ty} bad = {expression}",
                    f"\tfor({start} <= {ty} x <= {end}, x += 0.25);", '\t\tio.print("unreachable")',
                ), "", status=70, error="有限")

    def test_dynamic_endpoint_becomes_nan(self):
        self.check_program(program(
            "double finish(ref int calls);", "\tcalls++", "\tio.print(calls)", "\tif(calls == 1);", "\t\treturn 1.0",
            "\tdouble z = 0.0", "\treturn z / z", "int main();", "\tint calls = 0",
            "\tfor(0.0 <= double x <= finish(ref calls), x += 0.25);", "\t\tio.print(x)",
        ), "1\n0.0\n2\n", status=70, error="終了境界")

    def test_floating_overflow(self):
        self.compile_source(program(
            "int main();", "\tfor(300000000000000000000000000000000000000.0 < float x <= 300000000000000000000000000000000000000.0, x += 200000000000000000000000000000000000000.0);", "\t\treturn 0",
        ), error="有限")
        self.check_program(program(
            "int main();", "\tfloat start = io.input()",
            "\tfor(start < float x <= 300000000000000000000000000000000000000.0, x += 200000000000000000000000000000000000000.0);", '\t\tio.print("bad")',
        ), "", stdin="300000000000000000000000000000000000000\n", status=70, error="有限")

    def test_body_can_update_counter_but_nonfinite_is_diagnosed(self):
        self.check_program(program(
            "int main();", "\tfor(0.0 <= double x <= 1.0, x += 0.25);", '\t\tio.print("before")',
            "\t\tdouble z = 0.0", "\t\tx = z / z", "\t\tcontinue",
        ), "before\n", status=70, error="有限")

    def test_empty_loop_still_checks_body_and_scope(self):
        self.compile_source(program("int main();", "\tfor(0 > i >= 0, i--);", "\t\tio.print(undefined)"), error="宣言")
        self.compile_source(program("int main();", "\tfor(0 < i <= 1, i++);", "\t\tio.print(i)", "\tio.print(i)"), error="宣言")

    def test_bool_array_and_while_keep_existing_behavior(self):
        self.check_program(program(
            "int main();", "\tbool flags[] = [false, true]", "\tfor(0 <= i < flags.length, i++);",
            "\t\tio.print(flags[i])", "\tbool flag = true", "\twhile(flag);", '\t\tio.print("once")', "\t\tflag = false",
        ), "false\ntrue\nonce\n")

    def test_windows_generation(self):
        files = {"main.qy": LIMITS + program(
            "int main();", "\tfor(MAX - 1 < i <= MAX, i++);", "\t\tio.print(i)",
            "\tfor(0.0 < double x <= 1.0, x += 0.25);", "\t\tio.print(x)",
        )}
        binary = self.compile_project(files, flags=("--target", "windows"))
        data = binary.read_bytes()
        self.assertEqual(data[:2], b"MZ")
        offset = int.from_bytes(data[60:64], "little")
        self.assertEqual(data[offset:offset + 4], b"PE\0\0")
        assembly = self.compile_project(files, flags=("-S", "--target", "windows"))
        self.assertIn("jo ", assembly.read_text())
        self.assertIn('.section .rdata,"dr"', assembly.read_text())

    def test_linux_assembly_with_checked_updates(self):
        assembly = self.compile_project({"main.qy": LIMITS + program(
            "int main();", "\tfor(MAX - 1 < i <= MAX, i++);", "\t\tio.print(i)",
            "\tfor(0.0 < double x <= 1.0, x += 0.25);", "\t\tio.print(x)",
        )}, flags=("-S", "--target", "linux"))
        self.assertIn("jo ", assembly.read_text())
        result = subprocess.run(["gcc", "-x", "assembler", "-c", str(assembly), "-o", str(self.directory / "linux.o")],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_error_in_imported_loop_keeps_its_location(self):
        self.run_project({
            "main.qy": "import sample\nint main();\n\tsample.run()\n",
            "sample.qy": "void run();\n\tdouble start = io.input()\n\tfor(start < double x <= 9007199254740996.0, x++);\n\t\tio.print(x)\n",
        }, "", stdin="9007199254740992\n", status=70, diagnostic="sample.qy:3:")


if __name__ == "__main__":
    run_suite()
