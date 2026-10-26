"""UTF-8 scalar indexing, character constraints, replacement and errors."""

from support import QynoTestCase, program, run_suite


class TextTests(QynoTestCase):
    def test_scalar_length_and_index_constant_and_runtime(self):
        text = "A日😀e\u0301"
        for index, scalar in enumerate(text):
            with self.subTest(index=index, scalar=scalar):
                self.check_program(program(
                    f'const str S = "{text}"', f"const str C = S[{index}]",
                    "int main();", f'\tstr s = "{text}"',
                    '\tprint("{S.length} {s.length}")', "\tprint(C)", f"\tprint(s[{index}])",
                ), f"5 5\n{scalar}\n{scalar}\n")

    def test_empty_string_and_embedded_nul(self):
        self.check_program(program(
            "int main();", '\tstr empty = ""', '\tstr s = "a\\0b"',
            '\tprint("{empty.length} {s.length}")', "\tprint(s)",
        ), "0 3\na\0b\n")

    def test_character_conversion_and_default(self):
        self.check_program(program(
            'const char C = char("😀")', "int main();", "\tchar zero",
            '\tchar c = "日"', "\tstr s = str(c)", '\tprint("{C} {c} {s} {zero.length}")',
        ), "😀 日 日 1\n")

    def test_replacement_changes_byte_width_without_aliasing(self):
        self.check_program(program(
            "int main();", '\tstr a = "a日😀"', "\tstr b = a",
            '\ta[0] = "🚀"', '\ta[1] = "b"', '\ta[2] = "界"',
            '\tprint("{a} {a.length}")', "\tprint(b)",
        ), "🚀b界 3\na日😀\n")

    def test_text_value_and_reference_parameters(self):
        self.check_program(program(
            "void value(str s);", '\ts[0] = "X"',
            "void reference(ref str s);", '\ts[0] = "日"',
            "int main();", '\tstr s = "abc"', "\tvalue(s)", "\tprint(s)",
            "\treference(ref s)", "\tprint(s)",
        ), "abc\n日bc\n")

    def test_concatenation_and_interpolation(self):
        self.check_program(program(
            "int main();", '\tstr a = "日本" + "語"', '\tprint("[{a}] {a.length} {a[1]}")',
            '\tprint("\\{literal\\} \\\\ \\"quoted\\"")',
        ), '[日本語] 3 本\n{literal} \\ "quoted"\n')

    def test_input_character(self):
        self.check_program(program("int main();", "\tchar c = input()", "\tprint(c)"),
                           "😀\n", stdin="😀\n")

    def test_constant_character_rejects_zero_or_multiple_scalars(self):
        for text in ("", "ab", "e\u0301"):
            with self.subTest(text=text):
                self.compile_source(program("int main();", f'\tchar c = char("{text}")'), error="スカラー")

    def test_runtime_character_rejects_multiple_scalars(self):
        self.check_program(program(
            "int main();", "\tstr s = input()", "\tchar c = char(s)", "\tprint(c)",
        ), "", stdin="ab\n", status=70, error="char")

    def test_constant_index_errors(self):
        for index in (-1, 2):
            with self.subTest(index=index):
                self.compile_source(program("int main();", f'\tprint("日😀"[{index}])'), error="範囲外")

    def test_runtime_index_errors(self):
        for index in (-1, 2):
            with self.subTest(index=index):
                self.check_program(program(
                    "int main();", '\tstr s = "日😀"', "\tint i = input()", "\tprint(s[i])",
                ), "", stdin=f"{index}\n", status=70, error="範囲外")

    def test_invalid_utf8_input_is_rejected_when_inspected(self):
        for invalid in (b"\xff", b"\xc0\xaf", b"\xed\xa0\x80", b"\xf4\x90\x80\x80", b"\xe3\x81"):
            with self.subTest(invalid=invalid):
                self.check_program(program(
                    "int main();", "\tstr s = input()", "\tprint(s.length)",
                ), "", stdin=invalid + b"\n", status=70, error="UTF-8")

    def test_constant_invalid_utf8_is_rejected(self):
        self.compile_source(b'int main();\n\tprint("\xff".length)\n', error="UTF-8")

    def test_readonly_length_and_const_text(self):
        for source, message in (
            (program("int main();", '\tstr s = "abc"', "\ts.length = 2"), "読み取り専用"),
            (program('const str S = "abc"', "int main();", '\tS[0] = "x"'), "const"),
            (program("int main();", '\tstr s = "abc"', '\ts[0] = "xy"'), "スカラー"),
        ):
            with self.subTest(source=source):
                self.compile_source(source, error=message)


if __name__ == "__main__":
    run_suite()
