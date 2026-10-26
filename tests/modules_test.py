"""Black-box multi-file import, using, search-root and diagnostic regressions."""

import subprocess

from support import QynoTestCase, program, run_suite


CIRCLE = 'const double PI = 3.0\ndouble area(double r) = PI * r^2\n'


class ModuleTests(QynoTestCase):
    def compile_project(self, files, *, entry="main.qy", includes=(), flags=(), cwd=None, error=None):
        self.sequence += 1
        root = self.directory / f"project{self.sequence}"
        root.mkdir()
        for name, source in files.items():
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(source.encode() if isinstance(source, str) else source)
        binary = root / "program"
        command = [str(self.compiler), str(root / entry), "-o", str(binary)]
        for include in includes:
            command += ["-I", str(root / include)]
        command += list(flags)
        result = subprocess.run(command, cwd=root if cwd is None else cwd,
                                capture_output=True, text=True, timeout=10)
        self.root = root
        if error is not None:
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertIn("エラー", result.stderr)
            self.assertIn(error, result.stderr)
            return result
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, "")
        return binary

    def run_project(self, files, expected, *, stdin="", status=0, diagnostic=None, **options):
        binary = self.compile_project(files, **options)
        result = subprocess.run([str(binary)], input=stdin, capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, status, result.stderr)
        self.assertEqual(result.stdout, expected)
        if diagnostic is None:
            self.assertEqual(result.stderr, "")
        else:
            self.assertIn(diagnostic, result.stderr)
        return result

    def test_namespace_alias_and_constant(self):
        self.run_project({
            "main.qy": program("import geometry.circle", "using geometry.circle as circle",
                               "int main();", '\tprint("{circle.area(2)} {circle.PI}")'),
            "geometry/circle.qy": CIRCLE,
        }, "12.0 3.0\n")

    def test_fully_qualified_and_same_names_in_three_modules(self):
        self.run_project({
            "main.qy": program("import a, b", "const int N = 3", "int f() = N",
                               "int main();", '\tprint("{a.f()} {b.f()} {f()}")'),
            "a.qy": "const int N = 1\nint f() = N\n",
            "b.qy": "const int N = 2\nint f() = N\n",
        }, "1 2 3\n")

    def test_selective_function_and_constant(self):
        self.run_project({
            "main.qy": program("import geometry.circle", "using geometry.circle.area, geometry.circle.PI",
                               "int main();", '\tprint("{area(2)} {PI}")'),
            "geometry/circle.qy": CIRCLE,
        }, "12.0 3.0\n")

    def test_whole_namespace_using(self):
        self.run_project({
            "main.qy": program("import geometry.circle", "using geometry.circle",
                               "int main();", '\tprint("{area(2)} {PI}")'),
            "geometry/circle.qy": CIRCLE,
        }, "12.0 3.0\n")

    def test_global_import_and_using_are_file_wide(self):
        self.run_project({
            "main.qy": program("int main();", "\tprint(area(2))",
                               "using geometry.circle.area", "import geometry.circle"),
            "geometry/circle.qy": CIRCLE,
        }, "12.0\n")

    def test_block_alias_and_selective_using(self):
        self.run_project({
            "main.qy": program("import geometry.circle", "int main();", "\tif(true);",
                               "\t\tusing geometry.circle as c", "\t\tusing c.area",
                               "\t\tprint(area(2))", "\tprint(geometry.circle.area(3))"),
            "geometry/circle.qy": CIRCLE,
        }, "12.0\n27.0\n")

    def test_block_alias_does_not_leak(self):
        self.compile_project({
            "main.qy": program("import geometry.circle", "int main();", "\tif(true);",
                               "\t\tusing geometry.circle as c", "\t\tprint(c.area(2))", "\tprint(c.area(3))"),
            "geometry/circle.qy": CIRCLE,
        }, error="c.area")

    def test_block_selective_using_does_not_leak(self):
        self.compile_project({
            "main.qy": program("import a", "int main();", "\tif(true);", "\t\tusing a.f",
                               "\t\tprint(f())", "\tprint(f())"),
            "a.qy": "int f() = 1\n",
        }, error="using")

    def test_block_using_not_visible_before_declaration(self):
        self.compile_project({
            "main.qy": program("import a", "int main();", "\tprint(f())", "\tusing a.f"),
            "a.qy": "int f() = 1\n",
        }, error="using")

    def test_using_does_not_leak_between_functions(self):
        self.compile_project({
            "main.qy": program("import a", "int first();", "\tusing a.f", "\treturn f()",
                               "int main();", "\tprint(f())"),
            "a.qy": "int f() = 1\n",
        }, error="using")

    def test_dependency_uses_its_own_imports_and_aliases(self):
        self.run_project({
            "main.qy": program("import services.worker", "int main();", "\tprint(services.worker.run())"),
            "services/worker.qy": "import geometry.circle\nusing geometry.circle as c\ndouble run() = c.area(2)\n",
            "geometry/circle.qy": CIRCLE,
        }, "12.0\n")

    def test_transitive_import_is_not_visible(self):
        self.compile_project({
            "main.qy": "import a\nint main();\n\tprint(b.f())\n",
            "a.qy": "import b\nint run() = b.f()\n", "b.qy": "int f() = 1\n",
        }, error="b.f")

    def test_dependency_using_is_not_exported(self):
        self.compile_project({
            "main.qy": "import a\nint main();\n\tprint(sqrt(4))\n",
            "a.qy": "using math.sqrt\ndouble f() = sqrt(4)\n",
        }, error="using")

    def test_diamond_and_duplicate_imports(self):
        self.run_project({
            "main.qy": "import a, b, common\nimport common\nint main();\n\tprint(a.f() + b.f())\n",
            "a.qy": "import common\nint f() = common.f() + 1\n",
            "b.qy": "import common\nint f() = common.f() + 2\n",
            "common.qy": "int f() = 10\n",
        }, "23\n")

    def test_circular_import_reports_chain_and_import_site(self):
        result = self.compile_project({
            "main.qy": "import a\nint main();\n\treturn 0\n",
            "a.qy": "import b\n", "b.qy": "import a\n",
        }, error="循環")
        self.assertIn("<entry> -> a -> b", result.stderr)
        self.assertIn("b.qy:1:", result.stderr)

    def test_entry_import_cycle(self):
        self.compile_project({"main.qy": "import main\nint main();\n\treturn 0\n"}, error="循環")

    def test_missing_module_reports_roots_and_dependency(self):
        result = self.compile_project({
            "main.qy": "import a\nint main();\n\treturn 0\n", "a.qy": "import missing.widget\n",
        }, error="missing.widget が見つかりません")
        self.assertIn("探索ルート:", result.stderr)
        self.assertIn("a.qy:1:", result.stderr)

    def test_using_does_not_load_a_file(self):
        self.compile_project({
            "main.qy": "using a.f\nint main();\n\tprint(f())\n", "a.qy": "int f() = 1\n",
        }, error="import")

    def test_qualified_name_does_not_load_a_file(self):
        self.compile_project({
            "main.qy": "int main();\n\tprint(a.f())\n", "a.qy": "int f() = 1\n",
        }, error="a.f")

    def test_unknown_export(self):
        for expression in ("a.no_such_function()", "a.NO_SUCH_CONSTANT"):
            with self.subTest(expression=expression):
                self.compile_project({
                    "main.qy": f"import a\nint main();\n\tprint({expression})\n", "a.qy": "int f() = 1\n",
                }, error="no_such" if "no_such" in expression else "NO_SUCH")

    def test_alias_only_accepts_modules(self):
        self.compile_project({
            "main.qy": "import a\nusing a.f as g\nint main();\n\treturn 0\n", "a.qy": "int f() = 1\n",
        }, error="モジュールにだけ")

    def test_wrong_alias_syntax_is_rejected(self):
        for directive in ("using c = a", "using c as a"):
            with self.subTest(directive=directive):
                self.compile_project({
                    "main.qy": f"import a\n{directive}\nint main();\n\treturn 0\n", "a.qy": "int f() = 1\n",
                }, error="エラー")

    def test_ambiguous_selective_using(self):
        result = self.compile_project({
            "main.qy": "import a, b\nusing a.f\nusing b.f\nint main();\n\treturn 0\n",
            "a.qy": "int f() = 1\n", "b.qy": "int f() = 2\n",
        }, error="衝突")
        self.assertIn("a.f", result.stderr)
        self.assertIn("b.f", result.stderr)

    def test_ambiguous_whole_namespace_using(self):
        self.compile_project({
            "main.qy": "import a, b\nusing a, b\nint main();\n\treturn 0\n",
            "a.qy": "const int N = 1\n", "b.qy": "const int N = 2\n",
        }, error="衝突")

    def test_different_aliases_can_coexist(self):
        self.run_project({
            "main.qy": "import a, b\nusing a as left, b as right\nint main();\n\tprint(left.f() + right.f())\n",
            "a.qy": "int f() = 1\n", "b.qy": "int f() = 2\n",
        }, "3\n")

    def test_duplicate_using_is_idempotent(self):
        self.run_project({
            "main.qy": "import a\nusing a.f, a.f\nusing a as c\nusing a as c\nint main();\n\tprint(f() + c.f())\n",
            "a.qy": "int f() = 1\n",
        }, "2\n")

    def test_alias_chain_and_using_through_alias(self):
        self.run_project({
            "main.qy": "import geometry.circle\nusing geometry.circle as c\nusing c as d\nusing d.area\nint main();\n\tprint(area(2))\n",
            "geometry/circle.qy": CIRCLE,
        }, "12.0\n")

    def test_using_alias_collision(self):
        self.compile_project({
            "main.qy": "import a, b\nusing a as c\nusing b as c\nint main();\n\treturn 0\n",
            "a.qy": "", "b.qy": "",
        }, error="衝突")

    def test_alias_conflicts_with_locals_before_or_after_using(self):
        for body in ("\tint c = 1\n\tusing a as c\n", "\tusing a as c\n\tint c = 1\n"):
            with self.subTest(body=body):
                self.compile_project({"main.qy": "import a\nint main();\n" + body, "a.qy": ""}, error="衝突")

    def test_import_root_conflicts_with_local_name(self):
        self.compile_project({
            "main.qy": "import geometry.circle\nint main();\n\tint geometry = 1\n",
            "geometry/circle.qy": CIRCLE,
        }, error="衝突")

    def test_using_conflicts_with_function_or_constant(self):
        for declaration in ("int f() = 2", "const int f = 2"):
            with self.subTest(declaration=declaration):
                self.compile_project({
                    "main.qy": f"import a\nusing a.f\n{declaration}\nint main();\n\treturn 0\n",
                    "a.qy": "int f() = 1\n",
                }, error="衝突")

    def test_parent_directory_root_with_split_and_attached_I(self):
        files = {"app/main.qy": "import geometry.circle\nint main();\n\tprint(geometry.circle.area(2))\n",
                 "modules/geometry/circle.qy": CIRCLE}
        self.run_project(files, "12.0\n", entry="app/main.qy", includes=("modules",))
        self.run_project(files, "12.0\n", entry="app/main.qy", flags=("-Imodules",))

    def test_entry_root_is_independent_of_cwd(self):
        elsewhere = self.directory / "elsewhere"
        elsewhere.mkdir()
        self.run_project({
            "main.qy": "import a\nint main();\n\tprint(a.f())\n", "a.qy": "int f() = 1\n",
        }, "1\n", cwd=elsewhere)

    def test_roots_are_not_relative_to_each_dependency(self):
        self.run_project({
            "main.qy": "import nested.a\nint main();\n\tprint(nested.a.f())\n",
            "nested/a.qy": "import nested.b\nint f() = nested.b.f()\n",
            "nested/b.qy": "int f() = 42\n",
        }, "42\n")

    def test_entry_root_precedes_additional_roots(self):
        self.run_project({
            "main.qy": "import a\nint main();\n\tprint(a.f())\n", "a.qy": "int f() = 1\n",
            "external/a.qy": "int f() = 2\n",
        }, "1\n", includes=("external",))

    def test_additional_roots_keep_command_line_order(self):
        self.run_project({
            "app/main.qy": "import a\nint main();\n\tprint(a.f())\n",
            "first/a.qy": "int f() = 1\n", "second/a.qy": "int f() = 2\n",
        }, "1\n", entry="app/main.qy", includes=("first", "second"))

    def test_missing_include_directory(self):
        self.compile_project({"main.qy": "int main();\n\treturn 0\n"}, includes=("missing",), error="探索ルート")

    def test_missing_I_argument(self):
        result = subprocess.run([str(self.compiler), "-I"], capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 1)
        self.assertIn("探索ルート", result.stderr)

    def test_root_with_spaces(self):
        self.run_project({
            "app/main.qy": "import a\nint main();\n\tprint(a.f())\n",
            "my modules/a.qy": "int f() = 7\n",
        }, "7\n", entry="app/main.qy", includes=("my modules",))

    def test_empty_library_does_not_need_main(self):
        self.run_project({"main.qy": "import empty\nint main();\n\tprint(42)\n", "empty.qy": ""}, "42\n")

    def test_library_cannot_define_main(self):
        self.compile_project({
            "main.qy": "import a\nint main();\n\treturn 0\n", "a.qy": "int main();\n\treturn 0\n",
        }, error="依存モジュールには main")

    def test_entry_still_requires_main(self):
        self.compile_project({"main.qy": "import a\n", "a.qy": "int f() = 1\n"}, error="main 関数がありません")

    def test_library_type_error_reports_its_own_file(self):
        result = self.compile_project({
            "main.qy": "import a\nint main();\n\treturn 0\n",
            "a.qy": 'int broken();\n\treturn "wrong"\n',
        }, error="a.qy:2:")
        self.assertIn('return "wrong"', result.stderr)

    def test_library_lexer_error_reports_its_own_file(self):
        self.compile_project({
            "main.qy": "import a\nint main();\n\treturn 0\n", "a.qy": "int broken();\n    return 1\n",
        }, error="a.qy:2:")

    def test_utf8_bom_and_crlf_in_library(self):
        self.run_project({
            "main.qy": "import a\nint main();\n\tprint(a.f())\n",
            "a.qy": b'\xef\xbb\xbfstr f();\r\n\treturn "\xe6\x97\xa5\xe6\x9c\xac"\r\n',
        }, "日本\n")

    def test_runtime_error_reports_library_file(self):
        result = self.run_project({
            "main.qy": "import a\nint main();\n\tprint(a.divide())\n",
            "a.qy": "int divide();\n\tint n = input()\n\treturn 10 / n\n",
        }, "", stdin="0\n", status=70, diagnostic="a.qy:3:")
        self.assertIn("QYNO 実行時エラー", result.stderr)

    def test_library_input_error_reports_correct_source_line(self):
        result = self.run_project({
            "main.qy": "import a\nint main();\n\ta.read()\n",
            "a.qy": 'void read();\n\tint n\n\tinput("{n}")\n',
        }, "", stdin="invalid\n", status=65, diagnostic="a.qy:3:")
        self.assertIn('input("{n}")', result.stderr)

    def test_array_return_and_reference_parameter_across_modules(self):
        self.run_project({
            "main.qy": "import a\nusing a as items\nint main();\n\tint x[] = items.make()\n\titems.change(ref x)\n\tprint(x[0])\n",
            "a.qy": "int make()[];\n\treturn [1, 2]\nvoid change(ref int values[]);\n\tvalues[0] = 42\n",
        }, "42\n")

    def test_constants_and_text_properties_across_modules(self):
        self.run_project({
            "main.qy": "import a\nusing a.N\nconst int M = N + a.T.length\nint main();\n\tprint(M)\n\tprint(a.V[1])\n",
            "a.qy": 'const int N = LATER\nconst int LATER = 40\nconst str T = "日😀"\nconst int V[] = [7, 8]\n',
        }, "42\n8\n")

    def test_imported_constants_are_readonly(self):
        self.compile_project({
            "main.qy": "import a\nint main();\n\ta.V[0] = 2\n", "a.qy": "const int V[] = [1]\n",
        }, error="const")

    def test_export_names_can_match_value_properties(self):
        self.run_project({
            "main.qy": "import a\nint main();\n\tprint(a.length + a.value + a.addr + a.add())\n",
            "a.qy": "const int length = 1\nconst int value = 2\nconst int addr = 3\nint add() = 4\n",
        }, "10\n")

    def test_builtin_math_alias_and_explicit_import(self):
        self.run_project({
            "main.qy": "import math\nusing math as m\nusing m.sqrt\nint main();\n\tprint(m.round(2.5) + sqrt(4))\n",
        }, "5.0\n")

    def test_library_recursion_and_forward_reference(self):
        self.run_project({
            "main.qy": "import a\nint main();\n\tprint(a.f(6))\n",
            "a.qy": "int f(int n);\n\tif(n == 0);\n\t\treturn 1\n\treturn n * f(n - 1)\n",
        }, "720\n")

    def test_windows_pe_generation_with_modules(self):
        binary = self.compile_project({
            "main.qy": "import a\nint main();\n\treturn a.f()\n", "a.qy": "int f() = 42\n",
        }, flags=("--target", "windows"))
        data = binary.read_bytes()
        self.assertEqual(data[:2], b"MZ")
        offset = int.from_bytes(data[60:64], "little")
        self.assertEqual(data[offset:offset + 4], b"PE\0\0")

    def test_assembly_with_same_function_names_assembles(self):
        assembly = self.compile_project({
            "main.qy": "import a, b\nint main();\n\treturn a.f() + b.f()\n",
            "a.qy": "int f() = 1\n", "b.qy": "int f() = 2\n",
        }, flags=("-S",))
        self.assertIn(".text", assembly.read_text())
        result = subprocess.run(["gcc", "-x", "assembler", "-c", str(assembly), "-o", str(self.directory / "modules.o")],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_same_basename_in_different_directories(self):
        self.run_project({
            "main.qy": "import left.circle, right.circle\nusing left.circle as l, right.circle as r\nint main();\n\tprint(l.f() + r.f())\n",
            "left/circle.qy": "int f() = 10\n", "right/circle.qy": "int f() = 20\n",
        }, "30\n")

    def test_disjoint_blocks_can_reuse_an_alias(self):
        self.run_project({
            "main.qy": program("import a, b", "int main();", "\tif(true);",
                               "\t\tusing a as c", "\t\tprint(c.f())", "\tif(true);",
                               "\t\tusing b as c", "\t\tprint(c.f())"),
            "a.qy": "int f() = 1\n", "b.qy": "int f() = 2\n",
        }, "1\n2\n")

    def test_inner_alias_cannot_shadow_outer_alias(self):
        self.compile_project({
            "main.qy": program("import a, b", "int main();", "\tusing a as c", "\tif(true);", "\t\tusing b as c"),
            "a.qy": "", "b.qy": "",
        }, error="衝突")

    def test_aliases_are_independent_between_modules(self):
        self.run_project({
            "main.qy": "import a, b\nint main();\n\tprint(a.f() + b.f())\n",
            "a.qy": "using math as m\ndouble f() = m.sqrt(4)\n",
            "b.qy": "import other\nusing other as m\ndouble f() = m.sqrt(4)\n",
            "other.qy": "double sqrt(double n) = n + 1\n",
        }, "7.0\n")

    def test_cyclic_using_aliases_fail_without_recursing_forever(self):
        self.compile_project({"main.qy": "using a as b\nusing b as a\nint main();\n\treturn 0\n"}, error="循環")

    def test_parent_directories_are_not_searched_implicitly(self):
        self.compile_project({
            "app/main.qy": "import a\nint main();\n\tprint(a.f())\n", "a.qy": "int f() = 1\n",
        }, entry="app/main.qy", error="見つかりません")

    def test_parameters_cannot_shadow_aliases(self):
        self.compile_project({
            "main.qy": "import a\nusing a as c\nint f(int c) = c\nint main();\n\treturn 0\n", "a.qy": "",
        }, error="衝突")

    def test_call_argument_error_points_to_caller(self):
        self.compile_project({
            "main.qy": 'import a\nint main();\n\tprint(a.f("wrong"))\n', "a.qy": "int f(int n) = n\n",
        }, error="main.qy:3:")

    def test_constant_error_points_to_defining_module(self):
        self.compile_project({
            "main.qy": "import a\nconst int C = a.N\nint main();\n\tprint(C)\n",
            "a.qy": "const int N = 1 / 0\n",
        }, error="a.qy:1:")

    def test_array_runtime_error_points_to_defining_module(self):
        self.run_project({
            "main.qy": "import a\nint main();\n\tprint(a.f())\n",
            "a.qy": "int f();\n\tint items[] = [1]\n\tint index = input()\n\treturn items[index]\n",
        }, "", stdin="3\n", status=70, diagnostic="a.qy:4:")

    def test_syntax_error_in_interpolation_points_to_library(self):
        self.compile_project({
            "main.qy": "import a\nint main();\n\treturn 0\n", "a.qy": 'void f();\n\tprint("{1:.1}")\n',
        }, error="a.qy:2:")

    def test_many_long_function_names_still_have_unique_assembly_labels(self):
        first = "function_" + "x" * 150 + "a"
        second = "function_" + "x" * 150 + "b"
        assembly = self.compile_project({
            "main.qy": f"import a\nint main();\n\treturn a.{first}() + a.{second}()\n",
            "a.qy": f"int {first}() = 1\nint {second}() = 2\n",
        }, flags=("-S",))
        result = subprocess.run(["gcc", "-x", "assembler", "-c", str(assembly), "-o", str(self.directory / "long.o")],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_same_file_under_two_module_names_is_rejected(self):
        root = self.directory / "symlink-project"
        root.mkdir()
        (root / "main.qy").write_text("import a, b\nint main();\n\treturn 0\n")
        (root / "a.qy").write_text("int f() = 1\n")
        (root / "b.qy").symlink_to(root / "a.qy")
        result = subprocess.run([str(self.compiler), str(root / "main.qy")], capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 1)
        self.assertIn("同じファイル", result.stderr)


if __name__ == "__main__":
    run_suite()
