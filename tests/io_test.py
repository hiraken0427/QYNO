"""Standard io preserves the existing print/input syntax and runtime behavior."""

from support import QynoTestCase, program, run_suite


FORMS = (
    ("legacy", "", "print", "input"),
    ("qualified", "", "io.print", "io.input"),
    ("using", "using io\n", "print", "input"),
    ("selective", "using io.print, io.input\n", "print", "input"),
    ("alias", "using io as console\n", "console.print", "console.input"),
)


class IOTests(QynoTestCase):
    def check_forms(self, lines, expected, *, stdin="", status=0, error=None):
        for label, prefix, output, input_call in FORMS:
            with self.subTest(form=label):
                source = prefix + program("int main();", *lines)
                source = source.replace("$out", output).replace("$in", input_call)
                self.check_program(source, expected, stdin=stdin, status=status, error=error)

    def test_requested_greeting_example(self):
        self.check_forms((
            "\tstr name", '\t$out("Hello, World!")',
            '\t$in("What your name? > {name}")', '\t$out("Hello, {name}!")', "\treturn 0",
        ), "Hello, World!\nWhat your name? > Hello, 山田 太郎!\n", stdin="山田 太郎\n")

    def test_interpolation_and_expression(self):
        self.check_forms((
            "\tint n = 7", '\t$out("{n} {n^2} {1 < n < 10}")', "\t$out(n + 1)",
        ), "7 49 true\n8\n")

    def test_end_argument_and_empty_output(self):
        self.check_forms((
            "\t$out()", '\t$out("a", end="")', '\t$out(2, end="!")',
            '\t$out(end="{3}\\n")',
        ), "\na2!3\n")

    def test_escapes_unicode_and_embedded_nul(self):
        self.check_forms((
            '\t$out("\\{literal\\} 日本😀\\t\\0", end="")',
        ), "{literal} 日本😀\t\0")

    def test_typed_input(self):
        self.check_forms((
            "\tint n = $in()", "\tfloat f = $in()", "\tdouble d = $in()",
            "\tbool b = $in()", "\tstr text = $in()", "\tchar c = $in()",
            '\t$out("{n} {f} {d} {b} [{text}] {c}")',
        ), "-42 1.5 2.25 true [  日本 語  ] 😀\n", stdin="-42\n1.5\n2.25\ntrue\n  日本 語  \n😀\n")

    def test_assignment_and_function_return_context(self):
        self.check_forms((
            "\tint n", "\tn = $in()", "\t$out(n)",
        ), "42\n", stdin="42\n")
        self.check_program(program(
            "int read_number();", "\treturn io.input()", "int main();", "\tio.print(read_number())",
        ), "42\n", stdin="42\n")

    def test_input_in_function_argument(self):
        self.check_program(program(
            "int twice(int n) = n * 2", "int main();", "\tio.print(twice(io.input()))",
        ), "42\n", stdin="21\n")

    def test_pattern_multiple_fields_and_crlf(self):
        self.check_forms((
            "\tint id", "\tstr name", '\t$in("id, name> {id}, {name}")',
            '\t$out("{id} [{name}]")',
        ), "id, name> 42 [Mary Ann]\n", stdin="42, Mary Ann\r\n")

    def test_framed_pattern(self):
        self.check_forms((
            "\tint age", '\t$in("age> {age} years")', '\t$out("{age}")',
        ), "age> 20\n", stdin="20\n")

    def test_pattern_in_condition(self):
        self.check_forms((
            "\tint n", '\tif($in("{n}"));', '\t\t$out("ok {n}")',
            "\telse;", '\t\t$out("bad")',
        ), "ok 42\n", stdin="42\n")

    def test_pattern_failure_is_atomic(self):
        self.check_forms((
            "\tint a = 7", "\tint b = 8", '\t$out($in("{a}, {b}"))',
            '\t$out("{a} {b}")',
        ), "false\n7 8\n", stdin="1, invalid\n")

    def test_condition_pattern_eof(self):
        self.check_forms(("\tint n = 7", '\t$out($in("{n}"))', "\t$out(n)"), "false\n7\n")

    def test_standalone_pattern_failure(self):
        self.check_forms(("\tint n", '\t$in("{n}")'), "", stdin="invalid\n", status=65, error="入力を読み取れませんでした")

    def test_value_input_failure_and_eof(self):
        self.check_forms(("\tint n = $in()", "\t$out(n)"), "", stdin="invalid\n", status=65, error="入力を読み取れませんでした")
        self.check_forms(("\tstr text = $in()", "\t$out(text)"), "", status=65, error="入力が終わって")

    def test_standalone_value_input_discards_line(self):
        self.check_forms(("\t$in()", "\tstr text = $in()", "\t$out(text)"), "second\n", stdin="first\nsecond\n")

    def test_prompt_expression(self):
        self.check_forms((
            '\tstr prompt = "name> "', "\tstr name = $in(prompt)", "\t$out(name)",
        ), "name> Alice\n", stdin="Alice\n")

    def test_numeric_printing_is_unchanged(self):
        self.check_forms((
            "\tfloat f = 3.14", "\tdouble z = 0.0",
            '\t$out("{f} {0.1 + 0.2} {1.0 / 3} {1.0 / z} {z / z}")',
        ), "3.14 0.3 0.333333 inf nan\n")

    def test_reference_input_target(self):
        self.check_forms((
            "\tint n = 1", "\tref int p = n.addr", '\t$in("{p.value}")', "\t$out(n)",
        ), "42\n", stdin="42\n")

    def test_print_input_can_be_mixed(self):
        self.check_program(program(
            "using io", "int main();", '\tprint("first", end=" ")',
            '\tstr name = io.input("name> ")', '\tio.print("Hello, {name}!")',
        ), "first name> Hello, Alice!\n", stdin="Alice\n")

    def test_global_alias_is_file_wide(self):
        self.check_program(program(
            "int main();", '\tstr name = terminal.input()', '\tterminal.print("Hello, {name}!")',
            "using io as terminal",
        ), "Hello, Alice!\n", stdin="Alice\n")

    def test_local_alias_and_nested_scope(self):
        self.check_program(program(
            "int main();", "\tusing io as terminal", "\tif(true);",
            '\t\tstr name = terminal.input()', '\t\tterminal.print("Hello, {name}!")',
        ), "Hello, Alice!\n", stdin="Alice\n")

    def test_alias_cannot_escape_its_block(self):
        for call in ('terminal.print("x")', "terminal.input()"):
            with self.subTest(call=call):
                self.compile_source(program(
                    "int main();", "\tif(true);", "\t\tusing io as terminal", '\t\tterminal.print("inside")', f"\t{call}",
                ), error="名前空間")

    def test_alias_is_not_visible_before_its_local_declaration(self):
        self.compile_source(program(
            "int main();", '\tterminal.print("x")', "\tusing io as terminal",
        ), error="名前空間")

    def test_unknown_namespace_or_wrong_standard_module(self):
        for name in ("missing", "math"):
            for call in ("print", "input"):
                with self.subTest(namespace=name, call=call):
                    self.compile_source(program("int main();", f'\t{name}.{call}("x")'), error=f"io.{call}")

    def test_alias_target_is_checked(self):
        for call in ("print", "input"):
            with self.subTest(call=call):
                self.compile_source(program(
                    "using math as terminal", "int main();", f'\tterminal.{call}("x")',
                ), error=f"io.{call}")

    def test_print_is_not_an_expression(self):
        self.compile_source(program("int main();", '\tint n = io.print("x")'), error="文として")

    def test_invalid_arguments(self):
        for statement, message in (
            ('io.print("a", "b")', "end="),
            ('io.input("a", "b")', "引数は1つ"),
            ("io.input(42)", "文字列"),
            ('io.input("{1 + 2}")', "変数"),
            ('io.input("{a}{b}")', "区切り"),
        ):
            with self.subTest(statement=statement):
                self.compile_source(program("int main();", "\tint a", "\tint b", f"\t{statement}"), error=message)

    def test_const_input_target_is_rejected(self):
        self.compile_source(program('const int N = 1', "int main();", '\tio.input("{N}")'), error="定数")

    def test_io_names_are_not_first_class_values(self):
        for name in ("io.print", "io.input"):
            with self.subTest(name=name):
                self.compile_source(program("int main();", f"\tstr value = {name}"), error="変数または定数")

    def test_escaped_braces_in_prompt(self):
        self.check_forms((
            '\tstr text = $in("\\{name\\}> ")', "\t$out(text)",
        ), "{name}> Alice\n", stdin="Alice\n")

    def test_character_input_validation_is_unchanged(self):
        self.check_forms(("\tchar c = $in()", "\t$out(c)"), "", stdin="ab\n", status=65, error="char")

    def test_using_io_does_not_expose_math_functions(self):
        self.compile_source(program("using io", "int main();", "\tprint(sqrt(9))"), error="using math.sqrt")

    def test_selective_using_stays_in_its_standard_module(self):
        for directive in ("using io.sqrt", "using math.print", "using math.input"):
            with self.subTest(directive=directive):
                self.compile_source(program(directive, "int main();", "\treturn 0"), error="using の対象")


if __name__ == "__main__":
    run_suite()
