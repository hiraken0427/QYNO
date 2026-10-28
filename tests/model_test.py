"""Models: contextual initialization, value semantics, methods and checked refs."""
from pathlib import Path
import subprocess

from support import QynoTestCase, ROOT, program, run_suite

POINT = program(
    "model Point;", "\tdouble x = 0.0", "\tdouble y = 0.0",
    "\tvoid move(double dx, double dy);", "\t\tthis.x += dx", "\t\tthis.y += dy",
    "\tdouble distance();", "\t\treturn math.sqrt(this.x^2 + this.y^2)",
)
CELL = program("model Cell;", "\tint value_number = 0", "\tvoid increase();", "\t\tthis.value_number++")


class ModelTests(QynoTestCase):
    def test_complete_example(self):
        source = (ROOT / "examples/models/main.qy").read_text()
        self.check_program(source,
            "origin: (0.0, 0.0)\np: (3.0, 4.0)\nhorizontal: (10.0, 0.0)\n"
            "distance: 5.0\np: (3.0, 4.0)\ncopy: (13.0, 4.0)\np: (4.0, 6.0)\n"
            "shared: (20.0, 30.0)\np after value call: (20.0, 30.0)\n"
            "p after ref call: (21.0, 31.0)\npoint: (2.0, 2.0)\npoint: (4.0, 4.0)\n"
            "rectangle position: (6.0, 8.0)\narea: 200.0\np after replacement: (5.0, 0.0)\n"
            "p after field assignment: (8.0, 7.0)\n")

    def test_defaults_named_order_and_replacement(self):
        self.check_program(POINT + program("int main();", "\tPoint p", "\tio.print(p)",
            "\tp = {y=4.0, x=3.0}", "\tio.print(p.distance())", "\tp = {x=2.0}", "\tio.print(p)"),
            "Point{x=0.0, y=0.0}\n5.0\nPoint{x=2.0, y=0.0}\n")

    def test_copy_value_argument_ref_argument_and_return(self):
        self.check_program(CELL + program("void local(Cell c);", "\tc.increase()",
            "void shared(ref Cell c);", "\tc.increase()", "Cell create(int n);", "\treturn {value_number=n}",
            "int main();", "\tCell a = create(2)", "\tCell b = a", "\tlocal(a)", "\tshared(ref b)",
            "\tio.print(a)", "\tio.print(b)", "\tio.print(create(5).value_number)"),
            "Cell{value_number=2}\nCell{value_number=3}\n5\n")

    def test_model_return_is_independent_of_ref_argument(self):
        self.check_program(CELL + program("Cell snapshot(ref Cell c);", "\treturn c", "int main();",
            "\tCell a = {value_number=2}", "\tCell b = snapshot(ref a)", "\tb.increase()", "\tio.print(a)", "\tio.print(b)"),
            "Cell{value_number=2}\nCell{value_number=3}\n")

    def test_local_reference_tracks_replacement_and_rebinding(self):
        self.check_program(CELL + program("int main();", "\tCell a", "\tCell b = {value_number=10}",
            "\tref Cell alias = a.addr", "\talias.increase()", "\ta = {value_number=4}", "\tio.print(alias)",
            "\talias.addr = b.addr", "\talias.increase()", "\tio.print(a)", "\tio.print(b)"),
            "Cell{value_number=4}\nCell{value_number=4}\nCell{value_number=11}\n")

    def test_array_defaults_add_copy_remove_and_clear(self):
        self.check_program(CELL + program("int main();", "\tCell cells[2]", "\tcells[0].increase()",
            "\tCell copy[] = cells", "\tcopy[1].increase()", "\tcopy.add()", "\tcopy.add({value_number=9})",
            "\tcopy.remove_at(0)", "\tio.print(cells)", "\tio.print(copy)", "\tcopy.clear()", "\tio.print(copy)"),
            "[Cell{value_number=1}, Cell{value_number=0}]\n"
            "[Cell{value_number=1}, Cell{value_number=0}, Cell{value_number=9}]\n[]\n")

    def test_multidimensional_model_arrays(self):
        self.check_program(CELL + program("int main();", "\tCell grid[2][2]",
            "\tgrid[0][0].increase()", "\tCell other[][] = grid", "\tother[0][0].increase()",
            "\tio.print(grid[0])", "\tio.print(other[0])"),
            "[Cell{value_number=1}, Cell{value_number=0}]\n[Cell{value_number=2}, Cell{value_number=0}]\n")

    def test_nested_models_and_array_fields_deep_copy(self):
        self.check_program(CELL + program("model Box;", "\tCell cell", "\tint numbers[] = [1, 2]",
            "\tCell cells[] = [{value_number=7}]", "int main();", "\tBox a", "\tBox b = a",
            "\tb.cell.increase()", "\tb.numbers.add(3)", "\tb.cells[0].increase()",
            "\tio.print(a)", "\tio.print(b)"),
            "Box{cell=Cell{value_number=0}, numbers=[1, 2], cells=[Cell{value_number=7}]}\n"
            "Box{cell=Cell{value_number=1}, numbers=[1, 2, 3], cells=[Cell{value_number=8}]}\n")

    def test_field_ref_and_readline(self):
        path = str(self.directory / "input.txt")
        Path(path).write_text("日本\n")
        self.check_program(program("model Data;", "\tint count", "\tstr text",
            "void increase(ref int n);", "\tn++", "int main();", "\tData d",
            "\tincrease(ref d.count)", f'\tfile source = io.file.open("{path}")',
            "\tio.file.readline(source, ref d.text)", "\tio.file.close(source)", "\tio.print(d)"),
            'Data{count=1, text="日本"}\n')

    def test_direct_and_formatted_input_to_fields(self):
        self.check_program(program("model Data;", "\tint count", "\tstr text", "int main();", "\tData d",
            "\tio.input(d.count)", '\tio.input("name> {d.text}")', "\tio.print(d)"),
            'name> Data{count=7, text="Alice"}\n', stdin="7\nAlice\n")

    def test_method_with_user_ref_parameter_and_chaining(self):
        self.check_program(program("model Counter;", "\tint n", "\tvoid update(ref int total);",
            "\t\tthis.n++", "\t\ttotal += this.n", "\tCounter copy();", "\t\treturn this",
            "int main();", "\tCounter c", "\tint total = 10", "\tc.update(ref total)",
            "\tio.print(total)", "\tio.print(c.copy().n)"), "11\n1\n")

    def test_methods_on_temporary_values(self):
        self.check_program(POINT + program("Point create();", "\treturn {x=3.0, y=4.0}",
            "int main();", "\tio.print(create().distance())", "\tio.print(Point().distance())", "\tcreate().move(1.0, 1.0)"),
            "5.0\n0.0\n")

    def test_method_names_are_scoped_by_model(self):
        self.check_program(program("model A;", "\tint n", "\tint get() = 1", "model B;", "\tint n",
            "\tint get() = 2", "int get() = 3", "int main();", "\tA a", "\tB b",
            '\tio.print("{a.get()} {b.get()} {get()}")'), "1 2 3\n")

    def test_forward_type_definitions(self):
        self.check_program(program("Container create();", "\treturn {item={value_number=6}}", "model Container;",
            "\tCell item", "int main();", "\tContainer c = create()", "\tio.print(c.item.value_number)") + CELL, "6\n")

    def test_module_type_names_using_alias_and_selected_using(self):
        (self.directory / "geometry.qy").write_text(POINT)
        for directive, typename in [("", "geometry.Point"), ("using geometry as geo", "geo.Point"),
                                     ("using geometry", "Point"), ("using geometry.Point", "Point")]:
            with self.subTest(directive=directive):
                self.check_program(program("import geometry", directive, "int main();", f"\t{typename} p = {{x=3.0, y=4.0}}", "\tio.print(p.distance())"), "5.0\n")

    def test_model_equality_including_arrays_and_text(self):
        self.check_program(program("model Data;", "\tfloat f = 1.5", "\tbool flag = true", '\tchar letter = "日"',
            '\tstr text = "abc"', "\tint items[] = [1, 2]", "int main();", "\tData a", "\tData b = a",
            "\tio.print(a == b)", "\tio.print({} == a)", "\tData aa[] = [a]", "\tData bb[] = [b]", "\tio.print(aa == bb)",
            "\tb.items.add(3)", "\tio.print(a != b)"), "true\ntrue\ntrue\ntrue\n")

    def test_side_effects_in_initializers_follow_written_order(self):
        self.check_program(program("model Pair;", "\tint a", "\tint b", "int next(ref int n);", "\tn++", "\treturn n",
            "int main();", "\tint n = 0", "\tPair p = {b=next(ref n), a=next(ref n)}", "\tio.print(p)"),
            "Pair{a=2, b=1}\n")

    def test_field_update_indices_evaluated_once(self):
        self.check_program(CELL + program("int index(ref int n);", "\tn++", "\treturn 0",
            "int main();", "\tCell cells[1]", "\tint n = 0", "\tcells[index(ref n)].value_number += 7",
            "\tcells[index(ref n)].increase()", '\tio.print("{n} {cells[0].value_number}")'), "2 8\n")

    def test_active_field_ref_prevents_model_replacement(self):
        self.check_program(CELL + program("void replace(ref Cell c, ref int n);", "\tc = {}", "\tn++", "int main();",
            "\tCell c", "\treplace(ref c, ref c.value_number)"), "", status=70, error="ref")

    def test_active_model_element_ref_prevents_array_growth(self):
        self.check_program(CELL + program("void grow(ref Cell c, ref Cell cells[]);", "\tcells.add()", "int main();",
            "\tCell cells[1]", "\tgrow(ref cells[0], ref cells)"), "", status=70, error="ref")

    def test_active_nested_field_ref_prevents_nested_model_replacement(self):
        self.check_program(CELL + program("model Box;", "\tCell c", "void replace(ref Box b, ref int n);",
            "\tb.c = {}", "\tn++", "int main();", "\tBox b", "\treplace(ref b, ref b.c.value_number)"),
            "", status=70, error="ref")

    def test_file_handle_in_model_copy_is_shared_and_close_is_tracked(self):
        path = self.directory / "file.txt"
        source = program("model Resource;", "\tfile handle", "\tvoid close();", "\t\tio.file.close(this.handle)",
            "int main();", f'\tResource r = {{handle=io.file.open("{path}", "w")}}', "\tResource copy = r",
            '\tio.file.write(copy.handle, "hello")', "\tcopy.close()", "\tio.print(io.file.is_open(r.handle))")
        self.check_program(source, "false\n"); self.assertEqual(path.read_text(), "hello")

    def test_missing_close_in_model_is_compile_error(self):
        self.compile_source(program("model Resource;", "\tfile handle", "int main();",
            '\tResource r = {handle=io.file.open("unused.txt", "w")}'), error="close")

    def test_file_model_return_transfers_close_obligation(self):
        path = self.directory / "returned.txt"
        self.check_program(program("model Resource;", "\tfile handle", "Resource open();",
            f'\treturn {{handle=io.file.open("{path}", "w")}}', "int main();", "\tResource r = open()",
            "\tio.file.close(r.handle)"), "")
        self.compile_source(program("model Resource;", "\tfile handle", "Resource open();",
            '\treturn {handle=io.file.open("unused.txt", "w")}', "int main();", "\tResource r = open()"), error="close")

    def test_file_array_of_models_close_paths(self):
        path = self.directory / "array.txt"
        self.check_program(program("model Resource;", "\tfile handle", "int main();",
            f'\tResource items[] = [{{handle=io.file.open("{path}", "w")}}]', "\tio.file.close(items[0].handle)"), "")

    def test_default_expressions_and_array_limits(self):
        self.check_program(program("const int N = 2", "model Data;", "\tint n = N^3", "\tint values[N] = [1]",
            "int main();", "\tData d", "\tio.print(d)"), "Data{n=8, values=[1, 0]}\n")
        self.compile_source(program("model Data;", "\tint values[1]", "int main();", "\tData d = {values=[1, 2]}"), error="上限")

    def test_invalid_definitions_and_initializers(self):
        cases = [
            ("model A;\n\tint n\n\tint n\nint main();\n\treturn 0\n", "重複"),
            ("model A;\n\tint n\nmodel A;\n\tint x\nint main();\n\treturn 0\n", "重複"),
            ("model A;\n\tA child\nint main();\n\treturn 0\n", "循環"),
            ("model A;\n\tB child\nmodel B;\n\tA child\nint main();\n\treturn 0\n", "循環"),
            ("model A;\n\tint n\nint main();\n\tA a = {missing=1}\n", "フィールド"),
            ("model A;\n\tint n\nint main();\n\tA a = {n=1, n=2}\n", "重複"),
            ("model A;\n\tint n\nint main();\n\tA a = {n=true}\n", "変換"),
            ("int main();\n\tint n = {x=1}\n", "モデル型"),
            ("model A;\n\tfile f = io.file.open(\"unused\")\nint main();\n\treturn 0\n", "既定値"),
            ("model A;\n\tint n = f()\nint f() = 1\nint main();\n\treturn 0\n", "既定値"),
            ("model A;\n\tint length\nint main();\n\treturn 0\n", "フィールド名"),
            ("model A;\n\tint get() = 1\n\tint get() = 2\nint main();\n\treturn 0\n", "定義"),
        ]
        for source, error in cases:
            with self.subTest(source=source): self.compile_source(source, error=error)

    def test_nominal_types_and_unknown_members(self):
        prefix = "model A;\n\tint n\nmodel B;\n\tint n\n"
        for source, error in [("\tA a\n\tB b = a", "変換"), ("\tA a\n\tio.print(a.missing)", "フィールド"),
                              ("\tA a\n\ta.missing()", "メソッド"), ("\tA a\n\tB b\n\tio.print(a == b)", "同じ型")]:
            with self.subTest(source=source): self.compile_source(prefix + "int main();\n" + source + "\n", error=error)

    def test_wrong_method_arguments_and_receiver_type(self):
        for call, error in [("p.move(1.0)", "引数"), ("p.move(true, 0.0)", "変換")]:
            with self.subTest(call=call): self.compile_source(POINT + program("int main();", "\tPoint p", "\t"+call), error=error)
        self.compile_source(program("int main();", "\tint n", "\tn.move()"), error="メソッド")

    def test_missing_ref_and_ref_type_mismatch(self):
        self.compile_source(CELL + program("void f(ref Cell c);", "\tc.increase()", "int main();", "\tCell c", "\tf(c)"), error="ref")
        self.compile_source(CELL + program("void f(ref int n);", "\tn++", "int main();", "\tCell c", "\tf(ref c)"), error="型")

    def test_null_and_escaping_local_references_are_rejected(self):
        self.check_program(CELL + program("int main();", "\tref Cell c", "\tc.increase()"), "", status=70, error="指していません")
        self.compile_source(CELL + program("int main();", "\tref Cell c", "\tif(true);", "\t\tCell local", "\t\tc.addr = local.addr"), error="内側")

    def test_comparisons_capture_values_before_later_side_effects(self):
        self.check_program(CELL + program("Cell mutate(ref Cell c);", "\tc.increase()", "\treturn c",
            "int main();", "\tCell c", "\tio.print(c == mutate(ref c))", "\tio.print(c.value_number)"), "false\n1\n")

    def test_temporary_file_model_method_closes_returned_handle(self):
        path = self.directory / "temporary.txt"
        self.check_program(program("model Resource;", "\tfile handle", "\tvoid close();", "\t\tio.file.close(this.handle)",
            "Resource open();", f'\treturn {{handle=io.file.open("{path}", "w")}}',
            "int main();", "\topen().close()"), "")

    def test_default_initializations_are_independent(self):
        self.check_program(CELL + program("model Box;", "\tCell child = {value_number=4}", "\tint items[] = [1]",
            "int main();", "\tBox a", "\tBox b", "\ta.child.increase()", "\ta.items[0] = 8", "\tio.print(b)"),
            "Box{child=Cell{value_number=4}, items=[1]}\n")

    def test_model_fields_cannot_share_method_names(self):
        self.compile_source(program("model Data;", "\tint n", "\tint n() = 1", "int main();", "\treturn 0"), error="重複")

    def test_branching_default_models_do_not_expand_compiler_ast(self):
        definitions = ["model Level0;\n\tint n = 7\n"]
        for depth in range(1, 20):
            definitions.append(f"model Level{depth};\n\tLevel{depth-1} left\n\tLevel{depth-1} right\n")
        source = "".join(definitions)
        self.check_program(source + program("int main();", "\tLevel4 value", "\tio.print(value.left.right.left.right.n)"), "7\n")

    def test_windows_and_assembly_generation(self):
        source = self.directory / "windows.qy"; source.write_text(POINT + program("model Box;", "\tPoint point", "\tvoid move();", "\t\tthis.point.move(1.0, 0.0)", "int main();", "\tPoint p = {x=3.0}", "\tp.move(1.0, 2.0)", "\tio.print(p)"))
        output = source.with_suffix(".exe")
        result = subprocess.run([str(self.compiler), str(source), "--target", "windows", "-o", str(output)], capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr.decode()); self.assertEqual(output.read_bytes()[:2], b"MZ")
        assembly = source.with_suffix(".s")
        result = subprocess.run([str(self.compiler), str(source), "-S", "-o", str(assembly)], capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr.decode()); self.assertIn(".L.model", assembly.read_text())
        result = subprocess.run(["gcc", "-x", "assembler", "-c", str(assembly), "-o", str(source.with_suffix(".o"))], capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr.decode())


if __name__ == "__main__":
    run_suite()
