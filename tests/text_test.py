"""UTF-8 grapheme indexing, character constraints, replacement and errors."""

from support import QynoTestCase, program, run_suite


class TextTests(QynoTestCase):
    def test_scalar_length_and_index_constant_and_runtime(self):
        text = "A日😀e\u0301"
        for index, scalar in enumerate(["A","日","😀","e\u0301"]):
            with self.subTest(index=index, scalar=scalar):
                self.check_program(program(
                    f'const str S = "{text}"', f"const str C = S[{index}]",
                    "int main();", f'\tstr s = "{text}"',
                    '\tio.print("{S.length} {s.length}")', "\tio.print(C)", f"\tio.print(s[{index}])",
                ), f"4 4\n{scalar}\n{scalar}\n")

    def test_empty_string_and_embedded_nul(self):
        self.check_program(program(
            "int main();", '\tstr empty = ""', '\tstr s = "a\\0b"',
            '\tio.print("{empty.length} {s.length}")', "\tio.print(s)",
        ), "0 3\na\0b\n")

    def test_character_conversion_and_default(self):
        self.check_program(program(
            'const char C = char("😀")', "int main();", "\tchar zero",
            '\tchar c = "日"', "\tstr s = str(c)", '\tio.print("{C} {c} {s} {zero.length}")',
        ), "😀 日 日 1\n")

    def test_replacement_changes_byte_width_without_aliasing(self):
        self.check_program(program(
            "int main();", '\tstr a = "a日😀"', "\tstr b = a",
            '\ta[0] = "🚀"', '\ta[1] = "b"', '\ta[2] = "界"',
            '\tio.print("{a} {a.length}")', "\tio.print(b)",
        ), "🚀b界 3\na日😀\n")

    def test_text_value_and_reference_parameters(self):
        self.check_program(program(
            "void value(str s);", '\ts[0] = "X"',
            "void reference(ref str s);", '\ts[0] = "日"',
            "int main();", '\tstr s = "abc"', "\tvalue(s)", "\tio.print(s)",
            "\treference(ref s)", "\tio.print(s)",
        ), "abc\n日bc\n")

    def test_concatenation_and_interpolation(self):
        self.check_program(program(
            "int main();", '\tstr a = "日本" + "語"', '\tio.print("[{a}] {a.length} {a[1]}")',
            '\tio.print("\\{literal\\} \\\\ \\"quoted\\"")',
        ), '[日本語] 3 本\n{literal} \\ "quoted"\n')

    def test_input_character(self):
        self.check_program(program("int main();", "\tchar c = io.input()", "\tio.print(c)"),
                           "😀\n", stdin="😀\n")

    def test_constant_character_rejects_zero_or_multiple_scalars(self):
        for text in ("", "ab", "e\u0301x"):
            with self.subTest(text=text):
                self.compile_source(program("int main();", f'\tchar c = char("{text}")'), error="書記素")

    def test_runtime_character_rejects_multiple_scalars(self):
        self.check_program(program(
            "int main();", "\tstr s = io.input()", "\tchar c = char(s)", "\tio.print(c)",
        ), "", stdin="ab\n", status=70, error="char")

    def test_constant_index_errors(self):
        for index in (-1, 2):
            with self.subTest(index=index):
                self.compile_source(program("int main();", f'\tio.print("日😀"[{index}])'), error="範囲外")

    def test_runtime_index_errors(self):
        for index in (-1, 2):
            with self.subTest(index=index):
                self.check_program(program(
                    "int main();", '\tstr s = "日😀"', "\tint i = io.input()", "\tio.print(s[i])",
                ), "", stdin=f"{index}\n", status=70, error="範囲外")

    def test_invalid_utf8_input_is_rejected_when_inspected(self):
        for invalid in (b"\xff", b"\xc0\xaf", b"\xed\xa0\x80", b"\xf4\x90\x80\x80", b"\xe3\x81"):
            with self.subTest(invalid=invalid):
                self.check_program(program(
                    "int main();", "\tstr s = io.input()", "\tio.print(s.length)",
                ), "", stdin=invalid + b"\n", status=70, error="UTF-8")

    def test_constant_invalid_utf8_is_rejected(self):
        self.compile_source(b'int main();\n\tio.print("\xff".length)\n', error="UTF-8")

    def test_readonly_length_and_const_text(self):
        for source, message in (
            (program("int main();", '\tstr s = "abc"', "\ts.length = 2"), "読み取り専用"),
            (program('const str S = "abc"', "int main();", '\tS[0] = "x"'), "const"),
            (program("int main();", '\tstr s = "abc"', '\ts[0] = "xy"'), "書記素"),
        ):
            with self.subTest(source=source):
                self.compile_source(source, error=message)


if __name__ == "__main__":
    run_suite()
