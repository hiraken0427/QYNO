"""Rounding expectations come from Decimal, independently of QYNO's routines."""

from decimal import Decimal, ROUND_CEILING, ROUND_FLOOR, ROUND_HALF_UP

from support import QynoTestCase, program, run_suite


MODES = {"round": ROUND_HALF_UP, "roundup": ROUND_CEILING, "rounddown": ROUND_FLOOR}


class RoundingTests(QynoTestCase):
    def test_double_constant_and_runtime_against_decimal(self):
        for operation, mode in MODES.items():
            for value in ("-125.5", "-2.75", "-2.5", "-0.25", "0.0", "0.25", "2.5", "2.75", "125.5"):
                for digits in (-2, -1, 0, 1, 2):
                    with self.subTest(operation=operation, value=value, digits=digits):
                        expected = Decimal(value).quantize(Decimal(1).scaleb(-digits), rounding=mode)
                        literal = format(expected, "f")
                        if "." not in literal:
                            literal += ".0"
                        self.check_program(program(
                            f"const double C = math.{operation}({value}, {digits})",
                            "int main();",
                            f"\tdouble x = {value}",
                            f"\tprint(C == {literal})",
                            f"\tprint(math.{operation}(x, {digits}) == {literal})",
                        ), "true\ntrue\n")

    def test_integer_constant_and_runtime_against_decimal(self):
        for operation, mode in MODES.items():
            for value in (-1250, -125, -15, -1, 0, 1, 15, 125, 1250, 9007199254740993):
                for digits in (-2, -1, 0, 2):
                    with self.subTest(operation=operation, value=value, digits=digits):
                        expected = int(Decimal(value).quantize(Decimal(1).scaleb(-digits), rounding=mode))
                        self.check_program(program(
                            f"const int C = math.{operation}({value}, {digits})",
                            "int main();", f"\tint x = {value}",
                            "\tprint(C)", f"\tprint(math.{operation}(x, {digits}))",
                        ), f"{expected}\n{expected}\n")

    def test_float_and_omitted_digits(self):
        self.check_program(program(
            "int main();", "\tfloat x = -2.5",
            '\tprint("{math.round(x)} {math.roundup(x)} {math.rounddown(x)}")',
            '\tprint("{math.round(2.5)} {math.roundup(2.25)} {math.rounddown(2.75)}")',
        ), "-3.0 -2.0 -3.0\n3.0 3.0 2.0\n")

    def test_extreme_integer_digits(self):
        self.check_program(program(
            "int main();", "\tint x = 42", "\tint y = -42",
            '\tprint("{math.round(x, -20)} {math.rounddown(x, -20)} {math.roundup(y, -20)}")',
            "\tprint(math.round(x, 9223372036854775807))",
            "\tprint(math.round(x, -9223372036854775807 - 1))",
        ), "0 0 0\n42\n0\n")

    def test_integer_overflow_is_compile_error_or_runtime_error(self):
        for operation, value in (("round", "9223372036854775807"),
                                 ("roundup", "9223372036854775807"),
                                 ("rounddown", "-9223372036854775807 - 1")):
            with self.subTest(operation=operation):
                self.compile_source(program(
                    "int main();", f"\tprint(math.{operation}({value}, -1))",
                ), error="範囲")
                self.check_program(program(
                    "int main();", f"\tint x = {value}",
                    f"\tprint(math.{operation}(x, -1))",
                ), "", status=70, error="範囲")

    def test_invalid_arguments(self):
        for expression, message in (("math.round()", "引数"),
                                    ("math.round(1, 2, 3)", "引数"),
                                    ("math.round(1.5, 1.0)", "桁数"),
                                    ('math.round("x")', "数値")):
            with self.subTest(expression=expression):
                self.compile_source(program("int main();", f"\tprint({expression})"), error=message)


if __name__ == "__main__":
    run_suite()
