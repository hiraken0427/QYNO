"""Model visibility: lexical nominal ownership, all access paths and defaults."""
import subprocess

from support import QynoTestCase, ROOT, program, run_suite


COUNTER = program(
    "model Counter;", "\tprivate int count = 0",
    "\tprivate void add_positive(int amount);", "\t\tif(amount > 0);", "\t\t\tthis.count += amount",
    "\tpublic void increment();", "\t\tthis.add_positive(1)",
    "\tpublic int current() = this.count",
    "\tpublic void copy_from(Counter other);", "\t\tthis.count = other.count",
    "\tpublic Counter doubled();", "\t\treturn {count=this.count * 2}",
)


class VisibilityTests(QynoTestCase):
    def test_complete_visibility_example(self):
        self.check_program((ROOT / "examples/models/visibility.qy").read_text(),
            "2\n2\n3\n4\nCounter{}\n")

    def test_same_type_other_instance_private_methods_and_fields(self):
        source = program("model Counter;", "\tprivate int count = 4",
            "\tprivate void add(int n);", "\t\tthis.count += n",
            "\tpublic void update(Counter other);", "\t\tother.add(3)", "\t\tthis.count = other.count",
            "\tpublic int current() = this.count", "int main();", "\tCounter a", "\tCounter b",
            "\ta.update(b)", "\tio.print(a.current())", "\tio.print(b.current())")
        self.check_program(source, "7\n4\n")

    def test_public_and_default_visibility(self):
        self.check_program(program("model Data;", "\tpublic int first = 1", "\tint second = 2",
            "\tpublic int sum() = this.first + this.second", "\tint get() = this.second",
            "int main();", "\tData d = {first=3, second=4}", "\td.first++", "\td.second++",
            '\tio.print("{d} {d.sum()} {d.get()}")'), "Data{first=4, second=5} 9 5\n")

    def test_all_external_field_operations_are_rejected(self):
        prefix = program("model Data;", "\tprivate int number", "\tprivate int numbers[] = [1]",
            '\tprivate str text = "ab"', "\tprivate bool flag", "\tprivate double decimal",
            '\tprivate char letter = "A"', "\tprivate file handle",
            "void increase(ref int n);", "\tn++", "void grow(ref int a[]);", "\ta.add()")
        operations = ["io.print(d.number)", "d.number = 1", "d.number += 1", "d.number -= 1",
            "d.number++", "d.number--", "increase(ref d.number)", "grow(ref d.numbers)",
            "io.print(d.numbers[0])", "d.numbers[0] = 3", "d.numbers[0]++", "io.print(d.numbers.length)",
            "d.numbers.add()", "d.numbers.remove_at(0)", "d.numbers.clear()", "io.print(d.text[0])",
            'd.text[0] = "z"', "io.print(d.text.length)", "io.input(d.number)",
            'io.input("value> {d.number}")', 'io.print("{d.number}")', "io.input(d.flag)",
            "io.input(d.decimal)", "io.input(d.letter)", "io.file.is_open(d.handle)",
            'file out = io.file.open("unused", "w")\n\tio.file.write(out, "{d.number}")\n\tio.file.close(out)']
        for operation in operations:
            with self.subTest(operation=operation):
                result = self.compile_source(prefix + program("int main();", "\tData d", "\t" + operation), error="private")
                self.assertIn("宣言:", result.stderr.decode())

    def test_external_method_calls_including_array_method_spellings_are_rejected(self):
        for name in ["hidden", "add", "remove_at", "clear"]:
            with self.subTest(name=name):
                source = program("model A;", "\tprivate int n", f"\tprivate void {name}();", "\t\tthis.n++",
                    "int main();", "\tA a", f"\ta.{name}()")
                result = self.compile_source(source, error=f"A.{name} は private")
                self.assertIn("宣言:", result.stderr.decode())

    def test_same_file_and_other_model_do_not_grant_access(self):
        for declaration in ["int other(Counter c) = c.count",
                "model Other;\n\tint read(Counter c) = c.count",
                "model Other;\n\tvoid change(Counter c);\n\t\tc.add_positive(1)"]:
            with self.subTest(declaration=declaration):
                self.compile_source(COUNTER + declaration + program("", "int main();", "\treturn 0"), error="private")

    def test_unreachable_code_and_unused_methods_are_checked(self):
        self.compile_source(COUNTER + program("int main();", "\tCounter c", "\tif(false);", "\t\tc.count++"), error="private")
        self.compile_source(COUNTER + program("model Other;", "\tvoid unused();", "\t\tCounter c", "\t\tio.print(c.count)",
            "int main();", "\treturn 0"), error="private")

    def test_alias_value_array_temporary_and_return_cannot_bypass_access(self):
        prefix = COUNTER + program("Counter create();", "\treturn {}")
        bodies = ["Counter c\n\tref Counter alias = c.addr\n\tio.print(alias.count)",
            "Counter c\n\tio.print(c.value.count)", "Counter items[1]\n\tio.print(items[0].count)",
            "io.print(Counter().count)", "io.print(create().count)", "create().add_positive(1)"]
        for body in bodies:
            with self.subTest(body=body):
                self.compile_source(prefix + program("int main();", "\t" + body), error="private")

    def test_external_explicit_initializers_in_all_contexts_are_rejected(self):
        prefix = COUNTER + program("void consume(Counter c);", "\tio.print(c.current())")
        bodies = ["Counter c = {count=5}", "Counter c\n\tc = {count=5}", "consume({count=5})",
            "Counter items[] = [{count=5}]", "Counter items[]\n\titems.add({count=5})",
            "Counter items[1]\n\titems[0] = {count=5}"]
        for body in bodies:
            with self.subTest(body=body):
                self.compile_source(prefix + program("int main();", "\t" + body), error="Counter.count は private")
        self.compile_source(COUNTER + program("Counter create();", "\treturn {count=5}", "int main();", "\treturn 0"), error="private")

    def test_omitted_defaults_and_internal_named_initializers(self):
        self.check_program(COUNTER + program("int main();", "\tCounter a", "\tCounter b = {}", "\tCounter c = Counter()",
            '\tio.print("{a.current()} {b.current()} {c.current()}")', "\ta.increment()",
            "\tCounter doubled = a.doubled()", "\tio.print(doubled.current())", "\ta = {}", "\tio.print(a.current())"),
            "0 0 0\n2\n0\n")

    def test_inferred_array_rechecks_do_not_treat_defaults_as_explicit_fields(self):
        self.check_program(COUNTER + program("int main();", "\tio.print([Counter(), Counter()])",
            '\tio.print("{[Counter(), Counter()]}")', "\tCounter counters[] = [Counter(), Counter()]",
            "\tio.print(counters)"), "[Counter{}, Counter{}]\n" * 3)

    def test_internal_initializers_in_arrays_calls_assignments_and_returns(self):
        self.check_program(program("model Secret;", "\tprivate int n = 2", "\tint get() = this.n",
            "\tSecret take(Secret other) = other",
            "\tSecret make();", "\t\tSecret local = {n=3}", "\t\tlocal = {n=4}",
            "\t\tSecret items[]", "\t\titems.add({n=5})", "\t\tSecret list[] = [{n=6}]",
            "\t\treturn this.take({n=local.n + items[0].n + list[0].n})",
            "int main();", "\tSecret s", "\tSecret result = s.make()", "\tio.print(result.get())"), "15\n")

    def test_each_nested_member_uses_its_own_owner(self):
        prefix = program("model Inner;", "\tprivate int secret = 7", "\tint get() = this.secret")
        self.check_program(prefix + program("model Outer;", "\tprivate Inner inner", "\tint get() = this.inner.get()",
            "int main();", "\tOuter o", "\tio.print(o.get())"), "7\n")
        for body in ["return this.inner.secret", "Inner x = {secret=9}", "this.inner = {secret=9}"]:
            with self.subTest(body=body):
                self.compile_source(prefix + program("model Outer;", "\tprivate Inner inner", "\tint bad();", "\t\t"+body,
                    "\t\treturn 0", "int main();", "\treturn 0"), error="Inner.secret は private")
        self.compile_source(prefix + program("model Outer;", "\tpublic Inner inner", "int main();",
            "\tOuter o = {inner={secret=9}}"), error="Inner.secret は private")
        self.compile_source(prefix + program("model Outer;", "\tInner children[1]", "\tint bad() = this.children[0].secret",
            "int main();", "\treturn 0"), error="private")

    def test_defaults_keep_declaration_privileges_when_reused_outside(self):
        source = program("model Secret;", "\tprivate int n = 7", "\tint get() = this.n",
            "model Holder;", "\tprivate Secret hidden = {}", "\tpublic Secret visible = {}",
            "\tint get() = this.hidden.get()", "int main();", "\tHolder a", "\tHolder b = {}",
            "\tHolder c = Holder()", '\tio.print("{a.get()} {b.get()} {c.get()}")', "\tio.print(a)")
        self.check_program(source, "7 7 7\nHolder{visible=Secret{}}\n")
        # The nested initializer is written in Holder, so it cannot name Secret.n.
        self.compile_source(program("model Secret;", "\tprivate int n = 7", "model Holder;",
            "\tprivate Secret hidden = {n=8}", "int main();", "\treturn 0"), error="Secret.n は private")

    def test_whole_value_copy_argument_return_and_equality_keep_private_state(self):
        self.check_program(COUNTER + program("Counter identity(Counter c) = c", "Counter snapshot(ref Counter c) = c",
            "int main();", "\tCounter a", "\ta.increment()", "\tCounter b = identity(a)",
            "\tCounter c = snapshot(ref a)", "\tCounter items[] = [a]", "\tCounter copies[] = items",
            "\ta.increment()", '\tio.print("{b.current()} {c.current()} {copies[0].current()}")',
            '\tio.print("{a == b} {b == c} {items == copies}")', "\tb = a", "\tio.print(a == b)",
            "\tb = {}", "\tio.print(b.current())"), "1 1 1\nfalse true true\ntrue\n0\n")

    def test_private_arrays_models_and_text_are_deep_copied(self):
        self.check_program(program("model Cell;", "\tprivate int n = 3", "\tint get() = this.n",
            "\tvoid change();", "\t\tthis.n++", "model Box;", "\tprivate Cell child", "\tprivate int items[] = [2]",
            '\tprivate str text = "abc"', "\tvoid change();", "\t\tthis.child.change()", "\t\tthis.items[0]++",
            "\t\tthis.items.add(9)", "\t\tthis.items.remove_at(1)",
            '\t\tthis.text[0] = "z"', "\tvoid show();", '\t\tio.print("{this.child.get()} {this.items} {this.text}")',
            "int main();", "\tBox a", "\tBox b = a", "\tb.change()", "\ta.show()", "\tb.show()", "\tio.print(a == b)"),
            "3 [2] abc\n4 [3] zbc\nfalse\n")

    def test_automatic_display_omits_private_recursively_and_uses_public_commas(self):
        self.check_program(program("model Empty;", "\tprivate int secret = 123", "\tvoid show();", "\t\tio.print(this)",
            "model Mixed;", "\tprivate int first = 99", "\tpublic int shown = 1", "\tprivate int middle = 88",
            '\tpublic str text = "ok"', "\tprivate int last = 77", "\tvoid show();", "\t\tio.print(this)",
            "model Outer;", "\tprivate int key = 42", "\tEmpty hidden", "\tMixed visible",
            "int main();", "\tEmpty e", "\tMixed m", "\tio.print(e)", "\te.show()", "\tio.print(m)", "\tm.show()",
            "\tOuter o", "\tio.print(o)", "\tMixed array[] = [m]", '\tio.print("{array}")'),
            'Empty{}\nEmpty{}\nMixed{shown=1, text="ok"}\nMixed{shown=1, text="ok"}\n'
            'Outer{hidden=Empty{}, visible=Mixed{shown=1, text="ok"}}\n[Mixed{shown=1, text="ok"}]\n')

    def test_explicit_reference_and_input_inside_model_are_allowed(self):
        self.check_program(program("void increase(ref int n);", "\tn++", "void grow(ref int a[]);", "\ta.add(4)",
            "model Data;", "\tprivate int n", "\tprivate int items[] = [1]", "\tvoid read();", "\t\tio.input(this.n)",
            '\t\tio.input("next> {this.items[0]}")', "\t\tincrease(ref this.n)", "\t\tgrow(ref this.items)",
            '\t\tio.print("{this.n} {this.items}")', "int main();", "\tData d", "\td.read()"), "next> 8 [3, 4]\n", stdin="7\n3\n")

    def test_private_file_methods_preserve_close_analysis(self):
        path = self.directory / "private.txt"
        path.write_text("日本\n")
        prefix = program("model Reader;", "\tprivate file handle", "\tvoid open(str path);",
            "\t\tio.file.close(this.handle)", "\t\tthis.handle = io.file.open(path)",
            "\tbool readline(ref str line) = io.file.readline(this.handle, ref line)",
            "\tvoid close();", "\t\tio.file.close(this.handle)")
        body = program("int main();", "\tReader r", f'\tr.open("{path}")', "\tstr line", "\tr.readline(ref line)", "\tio.print(line)")
        self.check_program(prefix + body + "\tr.close()\n", "日本\n")
        self.compile_source(prefix + body, error="close")
        self.compile_source(prefix + body + "\tif(true);\n\t\treturn 0\n\tr.close()\n", error="close")
        self.check_program(prefix + body + "\tReader copy = r\n\tcopy.close()\n", "日本\n")

    def test_display_does_not_waive_private_file_close(self):
        prefix = program("model Resource;", "\tprivate file handle", "\tvoid open(str path);",
            '\t\tthis.handle = io.file.open(path, "w")', "\tvoid close();", "\t\tio.file.close(this.handle)")
        body = program("int main();", "\tResource r", f'\tr.open("{self.directory / "out.txt"}")', "\tio.print(r)")
        self.compile_source(prefix + body, error="close")
        self.check_program(prefix + body + "\tr.close()\n", "Resource{}\n")

    def test_modifier_positions_and_reserved_names(self):
        for modifiers in ["public public", "private private", "public private", "private public"]:
            for declaration in ["int n", "int get() = 1"]:
                with self.subTest(modifiers=modifiers, declaration=declaration):
                    self.compile_source(program("model A;", f"\t{modifiers} {declaration}", "int main();", "\treturn 0"), error="公開指定")
        for declaration in ["public model A;\n\tint n", "private int f() = 0", "public const int N = 1"]:
            self.check_program(declaration+"\nint main();\n\treturn 0\n", "")
        sources = ["int f(private int n) = n", "int main();\n\tpublic int n", "model A;\n\tpublic;\n\tint n"]
        for source in sources:
            with self.subTest(source=source):
                self.compile_source(source + "\n", error="エラー")
        for name in ["public", "private"]:
            for source in [f"int main();\n\tint {name}", f"int f(int {name}) = 0",
                    f"int {name}() = 0", f"model {name};\n\tint n", f"model A;\n\tint {name}",
                    f"const int {name} = 1"]:
                with self.subTest(source=source): self.compile_source(source + "\n", error="エラー")

    def compile_modules(self, main, files, error=None):
        for name, text in files.items():
            (self.directory / name).write_text(text)
        return self.compile_source(main, error=error)

    def test_modules_aliases_and_using_do_not_grant_private_access(self):
        for using, typename in [("", "library.Counter"), ("using library as alias\n", "alias.Counter"),
                ("using library\n", "Counter"), ("using library.Counter\n", "Counter")]:
            with self.subTest(using=using):
                main = "import library\n" + using + program("int main();", f"\t{typename} c", "\tio.print(c.count)")
                result = self.compile_modules(main, {"library.qy": COUNTER}, error="Counter.count は private")
                self.assertIn(str(self.directory / "library.qy") + ":2:", result.stderr.decode())
        main = program("import library", "using library as alias", "int main();", "\talias.Counter c", "\tc.increment()", "\tio.print(c.current())",
            "\talias.Counter doubled = c.doubled()", "\tio.print(doubled.current())")
        binary = self.compile_modules(main, {"library.qy": COUNTER})
        result = subprocess.run([str(binary)], capture_output=True, timeout=5)
        self.assertEqual((result.returncode, result.stdout, result.stderr), (0, b"1\n2\n", b""))

    def test_same_spelling_in_different_modules_is_not_same_owner(self):
        main = program("import library", "model Counter;", "\tprivate int count", "\tint bad(library.Counter other) = other.count",
            "int main();", "\treturn 0")
        self.compile_modules(main, {"library.qy": COUNTER}, error="private")

    def test_imported_defaults_do_not_leak_private_initializer_privileges(self):
        library = program("model Secret;", "\tprivate int n = 3", "\tint get() = this.n",
            "model Holder;", "\tprivate Secret hidden", "\tint get() = this.hidden.get()")
        main = program("import library", "using library", "int main();", "\tHolder h = {}", "\tio.print(h.get())")
        binary = self.compile_modules(main, {"library.qy": library})
        self.assertEqual(subprocess.check_output([str(binary)], timeout=5), b"3\n")
        self.compile_modules(program("import library", "using library", "int main();", "\tHolder h = {hidden={}}"), {"library.qy": library}, error="private")

    def test_windows_and_assembly_visibility_output(self):
        source = self.directory / "visibility.qy"
        source.write_text((ROOT / "examples/models/visibility.qy").read_text())
        output = source.with_suffix(".exe")
        subprocess.run([str(self.compiler), str(source), "--target", "windows", "-o", str(output)], check=True, capture_output=True, timeout=10)
        self.assertEqual(output.read_bytes()[:2], b"MZ")
        assembly = source.with_suffix(".s")
        subprocess.run([str(self.compiler), str(source), "-S", "-o", str(assembly)], check=True, capture_output=True, timeout=10)
        subprocess.run(["gcc", "-x", "assembler", "-c", str(assembly), "-o", str(source.with_suffix(".o"))], check=True, capture_output=True, timeout=10)


if __name__ == "__main__":
    run_suite()
