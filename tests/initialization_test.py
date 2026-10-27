"""Construction protocols, default recipes, definite completion and file ownership."""
import subprocess
from support import QynoTestCase, ROOT, program, run_suite

POINT = program("model Point;", "\tprivate int px", "\tprivate int py",
    "\tvoid init(int x=0, int y=0);", "\t\tthis.px = x", "\t\tthis.py = y",
    "\treadonly int x() = this.px", "\treadonly int y() = this.py")
REQUIRED = program("model Required;", "\tprivate int n", "\tvoid init(int n);", "\t\tthis.n = n",
    "\treadonly int get() = this.n")


class InitializationTests(QynoTestCase):
    def test_complete_example(self):
        self.check_program((ROOT / "examples/models/initialization.qy").read_text(), "12.0\n12.0\n")

    def test_named_arguments_defaults_and_zero_argument_type_call(self):
        self.check_program(POINT + program("int main();", "\tPoint a", "\tPoint b = {}", "\tPoint c = Point()",
            "\tPoint d = {y=4, x=3}", "\tPoint e = {x=7}",
            '\tprint("{a.x()} {b.x()} {c.y()} {d.x()} {d.y()} {e.x()} {e.y()}")'), "0 0 0 3 4 7 0\n")

    def test_optional_parameters_before_required_parameters(self):
        self.check_program(program("model Pair;", "\tint n", "\tvoid init(int first=2, int second);",
            "\t\tthis.n = first * 10 + second", "int main();", "\tPair p = {second=3}", "\tprint(p.n)"), "23\n")

    def test_argument_expressions_are_evaluated_in_written_order(self):
        self.check_program(POINT + program("int mark(int n);", "\tprint(n)", "\treturn n", "int main();",
            "\tPoint p = {y=mark(4), x=mark(3)}", '\tprint("{p.x()} {p.y()}")'), "4\n3\n3 4\n")

    def test_argument_model_and_array_snapshots_precede_later_side_effects(self):
        self.check_program(program("model Data;", "\tint n", "model Target;", "\tint n", "\tvoid init(Data first, int values[], int last);",
            "\t\tthis.n = first.n * 100 + values[0] * 10 + last", "int mutate(ref Data d, ref int a[]);",
            "\td.n = 9", "\ta[0] = 8", "\treturn 3", "int main();", "\tData d = {n=1}", "\tint a[] = [2]",
            "\tTarget t = {first=d, values=a, last=mutate(ref d, ref a)}", "\tprint(t.n)"), "123\n")

    def test_default_recipes_run_after_explicit_arguments_in_declaration_order(self):
        self.check_program(program("model Probe;", "\tint n", "\tvoid init(int n=1);", '\t\tprint("probe{n}")', "\t\tthis.n = n",
            "model Target;", "\tint n", "\tvoid init(Probe first={n=2}, int explicit, Probe second={n=3});",
            "\t\tthis.n = first.n + explicit + second.n", "int mark();", '\tprint("explicit")', "\treturn 4",
            "int main();", "\tTarget t = {explicit=mark()}", "\tprint(t.n)"), "explicit\nprobe2\nprobe3\n9\n")

    def test_copy_arguments_returns_and_array_copies_do_not_reinitialize(self):
        self.check_program(program("model Data;", "\tint n", "\tvoid init(int n=1);", '\t\tprint("init{n}")', "\t\tthis.n = n",
            "Data identity(Data d) = d", "int main();", "\tData a = {n=7}", "\tData b = a", "\tData c = identity(a)",
            "\tData items[] = [a]", "\tData copies[] = items", "\tcopies.add(a)", "\tprint(copies)", "\tprint(c.n)"),
            "init7\n[Data{n=7}, Data{n=7}]\n7\n")

    def test_aggregate_without_init_preserves_public_private_and_const_rules(self):
        self.check_program(program("model Data;", "\tprivate int secret = 7", "\tint n = 2", "\treadonly int get() = this.secret",
            "const Data D = {n=3}", "int main();", "\tprint(D)", "\tprint(D.get())"), "Data{n=3}\n7\n")
        self.compile_source(program("model Data;", "\tprivate int secret", "int main();", "\tData d = {secret=1}"), error="private")

    def test_init_parameter_names_do_not_fall_back_to_fields_even_inside_model(self):
        prefix=program("model Data;", "\tint field", "\tvoid init(int given);", "\t\tthis.field = given")
        for source in [program("int main();", "\tData d = {field=2}"),
                program("\tData invalid();", "\t\treturn {field=2}", "int main();", "\treturn 0")]:
            self.compile_source(prefix+source, error="引数")

    def test_required_named_argument_errors_and_type_constraints(self):
        for body, error in [("Required r", "必須引数"), ("Required r = {}", "必須引数"),
                ("Required r = Required()", "必須引数"), ("Required r = {n=1, n=2}", "重複"),
                ("Required r = {missing=1}", "引数"), ("Required r = {n=true}", "変換")]:
            with self.subTest(body=body): self.compile_source(REQUIRED+program("int main();", "\t"+body),error=error)

    def test_field_default_and_parameter_default_recipes_are_not_cached(self):
        self.check_program(program("model Probe;", "\tint n", "\tvoid init(int n=7);", '\t\tprint("init{n}")', "\t\tthis.n = n",
            "model Box;", "\tProbe a = {n=3}", "\tProbe b", "model Consumer;", "\tint n", "\tvoid init(Probe p={n=9});",
            "\t\tthis.n = p.n", "int main();", "\tBox a", "\tBox b", "\tConsumer c", "\tConsumer d", "\tBox copy = a"),
            "init3\ninit7\ninit3\ninit7\ninit9\ninit9\n")

    def test_default_recipes_reject_variables_functions_and_other_parameters(self):
        cases=["model A;\n\tvoid init(int n=input());\n\t\treturn",
            "int make() = 1\nmodel A;\n\tvoid init(int n=make());\n\t\treturn",
            "model A;\n\tvoid init(int n, int other=n);\n\t\treturn",
            "model A;\n\tint n\n\tvoid init(int other=this.n);\n\t\treturn"]
        for source in cases:
            with self.subTest(source=source): self.compile_source(source+program("", "int main();", "\treturn 0"),error="エラー")

    def test_default_recipes_can_observe_generated_public_values(self):
        self.check_program(program("model Seed;", "\tint n", "\tvoid init();", "\t\tthis.n = 7", '\t\tprint("seed")',
            "model Value;", "\tint n = Seed().n + 1", "\tvoid init(int extra=Seed().n);", "\t\tthis.n += extra",
            "int main();", "\tValue v", "\tprint(v.n)"), "seed\nseed\n15\n")

    def test_default_recipe_cycles_are_rejected(self):
        for source in [program("model A;", "\tvoid init(A other={});", "\t\treturn"),
                program("model A;", "\tvoid init(B b={});", "\t\treturn", "model B;", "\tvoid init(A a={});", "\t\treturn")]:
            self.compile_source(source+program("int main();", "\treturn 0"),error="循環")
        self.compile_source(program("model A;", "\tB child", "model B;", "\tvoid init(A other={});", "\t\treturn",
            "int main();", "\treturn 0"), error="循環")

    def test_generation_in_assignment_arguments_returns_arrays_and_add(self):
        self.check_program(POINT + program("int sum(Point p) = p.x() + p.y()", "Point create(int n) = {x=n, y=n}",
            "int main();", "\tPoint p = {x=1}", "\tp = {x=2, y=3}", "\tprint(sum({x=4, y=5}))",
            "\tPoint a[] = [{x=6}]", "\ta.add({x=7})", "\ta.add()", "\tPoint r = create(8)",
            '\tprint("{p.x()} {p.y()} {a[0].x()} {a[1].x()} {a[2].x()} {r.y()}")'), "9\n2 3 6 7 0 8\n")

    def test_model_array_defaults_padding_empty_shapes_and_row_additions(self):
        self.check_program(program("model Data;", "\tint n", "\tvoid init(int n=7);", "\t\tthis.n = n", "int main();",
            "\tData a[2]", "\tData b[2] = [{n=3}]", "\tData grid[2][2] = [[{n=1}]]", "\tData empty[] = []",
            "\tData rows[][] = [[{n=4}]]", "\trows.add()", "\trows.add([{n=5}])",
            "\tprint(a)", "\tprint(b)", "\tprint(grid)", "\tprint(rows)"),
            "[Data{n=7}, Data{n=7}]\n[Data{n=3}, Data{n=7}]\n"
            "[[Data{n=1}, Data{n=7}], [Data{n=7}, Data{n=7}]]\n[[Data{n=4}], [], [Data{n=5}]]\n")

    def test_required_model_array_shapes_and_copies(self):
        self.check_program(REQUIRED + program("int main();", "\tRequired a[] = []", "\ta.add({n=1})",
            "\tRequired b[2] = [{n=2}, {n=3}]", "\tRequired empty[2][0]", "\tRequired copy[] = b",
            "\tcopy.add(b[0])", "\tprint(empty)", "\tprint(copy[2].get())"), "[[], []]\n2\n")
        for body in ["Required a[1]", "Required a[2] = [{n=1}]", "Required a[]\n\ta.add()",
                "Required source[] = [{n=1}]\n\tRequired a[2] = source", "Required rows[][1] = [[]]"]:
            with self.subTest(body=body): self.compile_source(REQUIRED+program("int main();", "\t"+body),error="必須引数")

    def test_required_child_fields_complete_before_success(self):
        self.check_program(REQUIRED + program("model Parent;", "\tprivate Required child", "\tvoid init(int n);",
            "\t\tthis.child = {n=n}", "\treadonly int get() = this.child.get()", "int main();",
            "\tParent p = {n=7}", "\tprint(p.get())"), "7\n")
        self.compile_source(REQUIRED+program("model Parent;", "\tRequired child", "int main();", "\treturn 0"),error="既定値")

    def test_uncompleted_fields_cannot_be_read_updated_or_aliased(self):
        prefix=REQUIRED+program("void take(ref readonly Required r);", "\tprint(r.get())")
        operations=["print(this.child)", "this.child.get()", "take(ref this.child)", "print(this)", "this.read()",
            "Parent copy = this", "ref Parent alias = this.addr", "this = {}"]
        for body in operations:
            with self.subTest(body=body):
                source=prefix+program("model Parent;", "\tprivate Required child", "\tvoid init();", "\t\t"+body,
                    "\t\tthis.child = {n=1}", "\treadonly int read() = this.child.get()", "int main();", "\treturn 0")
                self.compile_source(source,error="init" if "addr" in body or body.startswith("this =") else "未完了")

    def test_definite_completion_branches_false_paths_and_loops(self):
        prefix=REQUIRED+program("model Parent;", "\tRequired child")
        self.check_program(prefix+program("\tbool init(bool accept);", "\t\tif(not accept);", "\t\t\treturn false",
            "\t\tif(accept);", "\t\t\tthis.child = {n=1}", "\t\telse;", "\t\t\tthis.child = {n=2}", "\t\treturn true",
            "int main();", "\tParent p = {accept=true}", "\tprint(p.child.get())"), "1\n")
        for body in ["if(flag);\n\t\t\tthis.child = {n=1}",
                "for(0 <= i < 1, i++);\n\t\t\tthis.child = {n=1}"]:
            with self.subTest(body=body): self.compile_source(prefix+program("\tvoid init(bool flag);", "\t\t"+body,
                "int main();", "\treturn 0"),error="未完了")

    def test_pending_array_field_whole_assignment_and_reassignment(self):
        self.check_program(REQUIRED+program("model Parent;", "\tRequired items[2]", "\tvoid init();",
            "\t\tthis.items = [{n=1}, {n=2}]", "\t\tthis.items = [{n=3}]", "int main();",
            "\tParent p", "\tprint(p.items.length)", "\tprint(p.items[0].get())"), "1\n3\n")
        self.compile_source(REQUIRED+program("model Parent;", "\tRequired items[2]", "\tvoid init();",
            "\t\tthis.items[0] = {n=1}", "int main();", "\treturn 0"),error="未完了")

    def test_completed_fields_can_be_used_before_other_fields_are_completed(self):
        self.check_program(REQUIRED+program("model Parent;", "\tRequired first", "\tRequired second", "\tvoid init();",
            "\t\tthis.first = {n=4}", "\t\tprint(this.first.get())", "\t\tthis.second = this.first", "int main();",
            "\tParent p", "\tprint(p.second.get())"), "4\n4\n")

    def test_boolean_rejection_stops_at_creation_site(self):
        self.check_program(program("model Value;", "\tbool init(int n);", "\t\treturn n > 0", "int main();",
            "\tValue v = {n=0}", '\tprint("unreachable")'), "",status=70,error="Value の初期化が拒否")
        self.check_program(program("model Value;", "\tbool init();", "\t\treturn false", "int main();",
            "\tValue values[2]"), "",status=70,error="初期化が拒否")

    def test_nested_default_rejection_keeps_original_creation_location(self):
        source=program("model Reject;", "\tbool init();", "\t\tint scratch[2]", "\t\treturn false",
            "model Parent;", "\tReject child", "int main();", "\tParent p")
        result=self.check_program(source,"",status=70,error="Reject の初期化が拒否")
        self.assertIn(":8:2:",result.stderr.decode())

    def test_reassignment_constructs_before_replacing_the_old_value(self):
        self.check_program(program("model Value;", "\tint n", "\tvoid init(int n=1);", "\t\tthis.n = n",
            "int inspect(ref Value old);", "\tprint(old.n)", "\treturn 7", "int main();", "\tValue v",
            "\tv = {n=inspect(ref v)}", "\tprint(v.n)"), "1\n7\n")

    def test_const_constructor_types_are_rejected_recursively(self):
        prefix=POINT+program("model Outer;", "\tPoint child")
        for body in ["const Point p = {}", "const Point a[] = []", "const Outer o = {}"]:
            with self.subTest(body=body): self.compile_source(prefix+program("int main();", "\t"+body),error="const")
        self.check_program(POINT+program("int inspect(ref readonly Point p) = p.x()", "int main();", "\tPoint p = {x=7}",
            "\tprint(inspect(ref p))"), "7\n")

    def test_initializer_declarations_and_direct_calls_are_restricted(self):
        cases=["model A;\n\tint init() = 1", "model A;\n\tbool init()[];\n\t\treturn [true]", "model A;\n\tvoid init() = 1", "model A;\n\tprivate void init();\n\t\treturn",
            "model A;\n\treadonly void init();\n\t\treturn", "model A;\n\tvoid init(ref int n);\n\t\treturn",
            "model A;\n\tvoid init(file f);\n\t\treturn", "model A;\n\tint init", "model A;\n\tvoid init();\n\t\treturn\n\tvoid init();\n\t\treturn",
            "model A;\n\tbool init();\n\t\tint n", "int regular(int n=1) = n"]
        for source in cases:
            with self.subTest(source=source): self.compile_source(source+program("", "int main();", "\treturn 0"),error="エラー")
        for call in ["p.init()", "p.init(1)"]:
            self.compile_source(POINT+program("int main();", "\tPoint p", "\t"+call),error="生成専用")
        self.check_program(program("int init(int n) = n + 1", "int main();", "\tint init_value = init(2)", "\tprint(init_value)"), "3\n")

    def file_model(self, fields=(), body=()):
        return program("model Resource;", "\tfile handle", *fields, "\tbool init(str path);",
            '\t\tthis.handle = io.file.open(path, "w")', *body, "\t\treturn true", "\tvoid close();", "\t\tio.file.close(this.handle)")

    def test_constructor_file_success_transfers_close_responsibility(self):
        path=self.directory/"file.txt"; prefix=self.file_model()
        self.check_program(prefix+program("int main();", f'\tResource r = {{path="{path}"}}', "\tr.close()"), "")
        self.compile_source(prefix+program("int main();", f'\tResource r = {{path="{path}"}}'),error="close")
        self.compile_source(prefix+program("int main();", f'\tResource r = {{path="{path}"}}', "\tif(true);", "\t\treturn 0", "\tr.close()"),error="close")

    def test_false_and_indeterminate_returns_require_closing_constructor_resources(self):
        for body in [["\t\tif(path.length == 0);", "\t\t\treturn false"], ["\t\treturn path.length > 0"]]:
            self.compile_source(self.file_model(body=body)+program("int main();", "\treturn 0"),error="close")
        prefix=self.file_model(body=["\t\tif(path.length == 0);", "\t\t\tio.file.close(this.handle)", "\t\t\treturn false"])
        self.compile_source(prefix+program("int main();", "\treturn 0"))

    def test_file_aliases_and_distinct_handles_in_constructor_summaries(self):
        path=self.directory/"a.txt"
        prefix=program("model Resource;", "\tfile first", "\tfile second", "\tvoid init(str path);",
            '\t\tthis.first = io.file.open(path, "w")', "\t\tthis.second = this.first")
        self.check_program(prefix+program("int main();", f'\tResource r = {{path="{path}"}}', "\tio.file.close(r.first)"), "")
        prefix=prefix.replace("this.second = this.first", 'this.second = io.file.open(path, "w")')
        self.compile_source(prefix+program("int main();", f'\tResource r = {{path="{path}"}}', "\tio.file.close(r.first)"),error="close")
        self.check_program(prefix+program("int main();", f'\tResource r = {{path="{path}"}}', "\tio.file.close(r.first)", "\tio.file.close(r.second)"), "")

    def test_default_field_resource_is_closed_on_false_constructor_exit(self):
        path=self.directory/"default.txt"
        child=program("model Child;", "\tfile handle", f'\tvoid init(str path="{path}");', '\t\tthis.handle = io.file.open(path, "w")',
            "\tvoid close();", "\t\tio.file.close(this.handle)")
        bad=child+program("model Parent;", "\tChild child", "\tbool init();", "\t\treturn false", "int main();", "\treturn 0")
        self.compile_source(bad,error="close")
        good=bad.replace("\t\treturn false", "\t\tthis.child.close()\n\t\treturn false")
        self.compile_source(good)
        self.check_program(good.replace("\treturn 0", "\tParent p"), "",status=70,error="初期化が拒否")

    def test_resource_arrays_default_padding_addition_and_independent_identities(self):
        path=self.directory/"default.txt"
        prefix=program("model Resource;", "\tfile handle", f'\tvoid init(str path="{path}");', '\t\tthis.handle = io.file.open(path, "w")',
            "\tvoid close();", "\t\tio.file.close(this.handle)")
        self.check_program(prefix+program("int main();", "\tResource a[2]", "\ta[0].close()", "\ta[1].close()"), "")
        self.compile_source(prefix+program("int main();", "\tResource a[2]", "\ta[0].close()"),error="close")
        self.check_program(prefix+program("int main();", "\tResource a[2] = [{}]", "\ta[0].close()", "\ta[1].close()",
            "\tResource b[] = []", "\tb.add()", "\tb[0].close()"), "")

    def test_nested_resource_array_padding_tracks_every_fresh_handle(self):
        path=self.directory/"nested.txt"
        prefix=program("model Resource;", "\tprivate file handle", f'\tvoid init(str path="{path}");',
            '\t\tthis.handle = io.file.open(path, "w")', "\tvoid close();", "\t\tio.file.close(this.handle)")
        body=program("int main();", "\tResource a[2][2] = [[{}]]", "\ta[0][0].close()", "\ta[0][1].close()", "\ta[1][0].close()")
        self.compile_source(prefix+body,error="close")
        self.check_program(prefix+body+"\ta[1][1].close()\n", "")

    def test_private_file_argument_containers_are_rejected(self):
        self.compile_source(program("model Resource;", "\tprivate file handle", "model Consumer;", "\tvoid init(Resource given);",
            "\t\treturn", "int main();", "\treturn 0"),error="file")

    def test_imported_initializer_named_arguments_defaults_and_private_fields(self):
        (self.directory/"geometry.qy").write_text(POINT)
        self.check_program(program("import geometry", "using geometry as geo", "int main();", "\tgeo.Point p = {x=3, y=4}",
            "\tgeo.Point copy = p", '\tprint("{p.x()} {copy.y()}")'), "3 4\n")
        self.compile_source(program("import geometry", "int main();", "\tgeometry.Point p = {px=3}"),error="引数")

    def test_frequent_construction_gc_and_recursion(self):
        self.check_program(POINT+program("int recurse(int n);", "\tif(n == 0);", "\t\treturn 0", "\tPoint p = {x=n}",
            "\treturn p.x() + recurse(n - 1)", "int main();", "\tint total", "\tfor(0 <= i < 300, i++);",
            "\t\tPoint p = {x=i}", "\t\ttotal += p.x()", "\tprint(total)", "\tprint(recurse(5))"), "44850\n15\n")

    def test_constructor_body_can_recursively_construct_with_explicit_arguments(self):
        self.check_program(program("model Value;", "\tint n", "\tvoid init(int n=0);", "\t\tthis.n = n",
            "\t\tif(n > 0);", "\t\t\tValue child = {n=n - 1}", "\t\t\tthis.n += child.n",
            "int main();", "\tValue v = {n=4}", "\tprint(v.n)"), "10\n")

    def test_initializer_array_parameters_preserve_rank_capacity_and_snapshot_semantics(self):
        prefix=program("model Data;", "\tint sum", "\tvoid init(int values[2][2]=[[1]]);",
            "\t\tthis.sum = values[0][0]", "\t\tvalues[0][0] = 9")
        self.check_program(prefix+program("int main();", "\tint source[][] = [[3, 4]]", "\tData d = {values=source}",
            "\tData e", '\tprint("{d.sum} {e.sum} {source[0][0]}")'), "3 1 3\n")
        self.compile_source(prefix+program("int main();", "\tData d = {values=[[1,2,3]]}"),error="上限")
        self.compile_source(prefix+program("int main();", "\tData d = {values=[1]}"),error="変換")

    def test_windows_and_assembly_generation(self):
        path=self.directory/"init.qy"; path.write_text((ROOT/"examples/models/initialization.qy").read_text())
        binary=path.with_suffix(".exe"); assembly=path.with_suffix(".s")
        subprocess.run([str(self.compiler),str(path),"--target","windows","-o",str(binary)],check=True,capture_output=True,timeout=10)
        self.assertEqual(binary.read_bytes()[:2],b"MZ")
        subprocess.run([str(self.compiler),str(path),"-S","-o",str(assembly)],check=True,capture_output=True,timeout=10)
        subprocess.run(["gcc","-x","assembler","-c",str(assembly),"-o",str(path.with_suffix(".o"))],check=True,capture_output=True,timeout=10)


if __name__ == "__main__":
    run_suite()
