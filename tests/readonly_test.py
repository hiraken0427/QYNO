"""Read-only paths, constant model values and shared-file capability boundaries."""
import subprocess
from support import QynoTestCase, ROOT, program, run_suite


POINT = program("model Point;", "\tdouble x = 0.0", "\tdouble y = 0.0",
    "\tvoid move(double dx, double dy);", "\t\tthis.x += dx", "\t\tthis.y += dy",
    "\treadonly double distance() = math.sqrt(this.x^2 + this.y^2)")
DATA = program("model Data;", "\tint number", "\tint items[] = [1]", '\tstr text = "ab"',
    "\tPoint point", "\tPoint points[1]", "\tvoid change();", "\t\tthis.number++")


class ReadonlyTests(QynoTestCase):
    def test_complete_example(self):
        self.check_program((ROOT / "examples/models/readonly.qy").read_text(),
            "5.0\n5.0\nPoint{x=4.0, y=4.0}\nPoint{x=3.0, y=4.0}\n5.0\n")

    def test_global_forward_constants_fields_nested_arrays_and_limits(self):
        source = POINT + program("const double X = CONFIG.anchor.x", "const Point ORIGIN = {}",
            "const Point COPY = OFFSET", "const Point PAIR[2] = [ORIGIN, OFFSET]",
            "const Point OFFSET = {y=4.0, x=3.0}", "model Configuration;", "\tPoint anchor = {x=7.0}",
            "\tint size = 2", "\tint numbers[3] = [8]", "const Configuration CONFIG = {}",
            "int main();", '\tprint("{X} {COPY.distance()} {PAIR[1].x}")',
            "\tint numbers[CONFIG.size]", "\tprint(numbers)", "\tprint(CONFIG.numbers)")
        self.check_program(source, "7.0 5.0 3.0\n[0, 0]\n[8, 0, 0]\n")

    def test_defaults_model_arrays_and_multidimensional_padding(self):
        self.check_program(POINT + program("model Box;", "\tPoint points[2]", "\tint matrix[2][2]",
            "\tPoint point = {x=9.0}", "int main();", "\tconst Box box = {}", "\tprint(box)",
            "\tconst Point points[2][2] = [[{x=1.0}]]", "\tprint(points)"),
            "Box{points=[Point{x=0.0, y=0.0}, Point{x=0.0, y=0.0}], matrix=[[0, 0], [0, 0]], point=Point{x=9.0, y=0.0}}\n"
            "[[Point{x=1.0, y=0.0}, Point{x=0.0, y=0.0}], [Point{x=0.0, y=0.0}, Point{x=0.0, y=0.0}]]\n")

    def test_const_equality_private_char_bool_text_and_arrays(self):
        self.check_program(program("model Item;", "\tprivate int hidden = 7", '\tchar letter = "A"',
            "\tbool enabled = true", '\tstr text = "日本"', "\tint items[] = [1, 2]",
            "const Item A = {}", 'const Item B = {letter="B"}', "const Item C = {enabled=false}",
            "const bool SAME = A == Item()", "const bool DIFFERENT = A != B", "const Item ITEMS[] = [A, B]",
            "const bool ARRAY_SAME = ITEMS == [A, B]", "int main();",
            '\tprint("{SAME} {DIFFERENT} {A == C} {ARRAY_SAME}")', "\tprint(A)"),
            'true true false true\nItem{letter="A", enabled=true, text="日本", items=[1, 2]}\n')

    def test_constant_model_copy_value_argument_and_return_are_independent(self):
        self.check_program(POINT + program("Point change(Point p);", "\tp.move(10.0, 0.0)", "\treturn p",
            "int main();", "\tconst Point p = {x=3.0}", "\tPoint copy = p", "\tcopy.move(1.0, 0.0)",
            "\tPoint result = change(p)", '\tprint("{p.x} {copy.x} {result.x}")',
            "\tconst Point items[] = [p]", "\tPoint other[] = items", "\tother[0].move(5.0, 0.0)",
            '\tprint("{items[0].x} {other[0].x}")'), "3.0 4.0 13.0\n3.0 8.0\n")

    def test_const_initialization_rejects_runtime_and_readonly_calls(self):
        for declaration in ["const Point c = make()", "const double d = Point().distance()",
                "const double d = GLOBAL.distance()", "const Point c = variable"]:
            with self.subTest(declaration=declaration):
                self.compile_source(POINT + program("const Point GLOBAL = {}", "Point make() = {}",
                    "int main();", "\tPoint variable", "\t"+declaration), error="const")
        self.compile_source(POINT + program("const double D = GLOBAL.distance()", "const Point GLOBAL = {}",
            "int main();", "\treturn 0"), error="const")

    def test_const_cycles_limits_and_private_names_are_checked(self):
        self.compile_source(POINT + program("const Point A = B", "const Point B = A", "int main();", "\treturn 0"), error="循環")
        self.compile_source(POINT + program("int main();", "\tconst Point a[1] = [{}, {}]"), error="上限")
        prefix=program("model Secret;", "\tprivate int n = 7", "\treadonly int get() = this.n")
        self.check_program(prefix + program("const Secret S = {}", "int main();", "\tprint(S.get())", "\tprint(S)"), "7\nSecret{}\n")
        for code in ["const Secret S = {n=9}", "const Secret S = {}\nconst int N = S.n"]:
            self.compile_source(prefix + code + program("", "int main();", "\treturn 0"), error="private")

    def test_branching_defaults_have_bounded_constant_expansion(self):
        definitions = ["model Level0;\n\tint n = 7\n"]
        for depth in range(1, 21):
            definitions.append(f"model Level{depth};\n\tLevel{depth-1} left\n\tLevel{depth-1} right\n")
        prefix="".join(definitions)
        self.check_program(prefix + program("int main();", "\tconst Level4 value = {}", "\tprint(value.left.right.left.right.n)"), "7\n")
        self.compile_source(prefix + program("int main();", "\tconst Level20 value = {}"), error="100万")

    def test_all_readonly_mutations_and_permission_escalations_are_rejected(self):
        operations=["d = {}", "d.number = 1", "d.number++", "d.number--", "d.number += 2", "d.number -= 1",
            "d.items[0] = 1", "d.items[0]++", "d.items.add()", "d.items.remove_at(0)", "d.items.clear()",
            'd.text[0] = "z"', "d.point.x++", "d.points[0].x++", "d.change()", "d.point.move(1.0, 0.0)",
            "d.points[0].move(1.0, 0.0)", "d.value.number++", "io.input(d.number)",
            'io.input("value> {d.items[0]}")', "write(ref d.number)", "write_array(ref d.items)", "ref Data alias = d.addr"]
        helpers=program("void write(ref int n);", "\tn++", "void write_array(ref int a[]);", "\ta.add()")
        for operation in operations:
            for context in ["const", "parameter", "this"]:
                with self.subTest(operation=operation,context=context):
                    prefix=POINT+DATA+helpers
                    if context == "const": source=prefix+program("int main();", "\tconst Data d = {}", "\t"+operation)
                    elif context == "parameter": source=prefix+program("void inspect(ref readonly Data d);", "\t"+operation,
                        "int main();", "\treturn 0")
                    else:
                        changed=DATA+program("\treadonly void inspect();", "\t\t"+operation.replace("d.", "this.").replace("d =", "this ="))
                        source=POINT+changed+helpers+program("int main();", "\treturn 0")
                    message = "定数" if context == "const" and (operation == "d = {}" or operation.endswith("d.addr")) else "readonly"
                    self.compile_source(source, error=message)

    def test_scalar_readonly_parameters_and_address_rebinding_are_rejected(self):
        for body in ["n++", "n = 2", "n += 1", "io.input(n)", 'io.input("{n}")',
                "ref int alias = n.addr", "n.addr = other.addr"]:
            with self.subTest(body=body):
                self.compile_source(program("void inspect(ref readonly int n, ref int other);", "\t"+body,
                    "int main();", "\treturn 0"), error="readonly" if "addr =" not in body else "指す先")

    def test_readonly_calls_private_readonly_void_and_local_mutations(self):
        self.check_program(program("model Counter;", "\tprivate int n = 7", "\tprivate readonly int hidden() = this.n",
            "\tpublic readonly void show(ref int output);", "\t\tint local = this.hidden()", "\t\tlocal++",
            "\t\toutput = local", "\t\tprint(this)", "\tpublic readonly int get() = this.hidden()",
            "int main();", "\tconst Counter c = {}", "\tint n", "\tc.show(ref n)", "\tprint(n)", "\tprint(c.get())"),
            "Counter{}\n8\n7\n")

    def test_readonly_receiver_does_not_restrict_other_mutable_instance(self):
        self.check_program(program("model Counter;", "\tprivate int n = 7", "\treadonly void copy_into(ref Counter other);",
            "\t\tother.n = this.n", "\t\tother.n++", "\treadonly int get() = this.n", "int main();",
            "\tconst Counter c = {}", "\tCounter other", "\tc.copy_into(ref other)", '\tprint("{c.get()} {other.get()}")'), "7 8\n")

    def test_readonly_does_not_imply_purity_or_constant_evaluation(self):
        self.check_program(POINT + program("model Console;", "\treadonly void read(ref int out);", '\t\tio.print("input")',
            "\t\tio.input(out)", "int main();", "\tconst Console c = {}", "\tint n", "\tc.read(ref n)", "\tprint(n)"),
            "input\n8\n", stdin="8\n")

    def test_const_scalars_arrays_models_and_nested_places_can_be_readonly_refs(self):
        self.check_program(POINT + program("model Box;", "\tPoint point = {x=3.0, y=4.0}", "\tPoint points[1]",
            "double scalar(ref readonly double d) = d", "double forward(ref readonly Point p) = measure(ref p)",
            "double measure(ref readonly Point p) = p.distance()", "int total(ref readonly int items[]);",
            "\tint n", "\tfor(0 <= i < items.length, i++);", "\t\tn += items[i]", "\treturn n",
            "int main();", "\tconst double n = 7.0", "\tconst int items[] = [1, 2, 3]", "\tconst Box box = {}",
            '\tprint("{scalar(ref n)} {total(ref items)} {forward(ref box.point)} {measure(ref box.points[0])}")'),
            "7.0 6 5.0 0.0\n")

    def test_readonly_refs_cover_all_scalar_types_and_ranks(self):
        definitions=[]; calls=[]
        for ty, value in [("int","3"),("float","1.5"),("double","2.5"),("bool","true"),("str",'"日本"'),("char",'"界"')]:
            definitions += [f"void show_{ty}(ref readonly {ty} x);", "\tprint(x)"]
            calls += [f"\tconst {ty} {ty}_value = {value}", f"\tshow_{ty}(ref {ty}_value)"]
        self.check_program(program(*definitions,"void show_matrix(ref readonly int a[][]);", "\tprint(a[1])", "int main();", *calls,
            "\tconst int matrix[][] = [[1, 2], [3, 4]]", "\tshow_matrix(ref matrix)"), "3\n1.5\n2.5\ntrue\n日本\n界\n[3, 4]\n")

    def test_const_borrow_materializes_declared_array_capacity(self):
        self.check_program(program("model Box;", "\tint a[2] = [7]", "\treadonly int length() = size(ref this.a)",
            "int size(ref readonly int a[2]) = int(a.length)", "int matrix(ref readonly int a[2][2]) = int(a[1].length)",
            "int main();", "\tconst int a[2] = [1]", "\tconst int m[2][2] = []", "\tconst Box box = {}",
            '\tprint("{size(ref a)} {matrix(ref m)} {box.length()} {size(ref box.a)}")'), "2 2 2 2\n")

    def test_model_matrix_literals_and_row_additions_clone_child_arrays(self):
        self.check_program(POINT + program("int main();", "\tPoint rows[][] = [[{x=1.0}], []]",
            "\tPoint row[] = [{x=2.0}]", "\trows.add(row)", "\trow[0].x = 9.0", "\trows[0].add({x=3.0})",
            "\tprint(rows)", "\tPoint copy[][] = rows", "\tcopy[2][0].x = 8.0", "\tprint(rows[2][0].x)"),
            "[[Point{x=1.0, y=0.0}, Point{x=3.0, y=0.0}], [], [Point{x=2.0, y=0.0}]]\n2.0\n")

    def test_readonly_borrow_sees_writes_through_existing_mutable_alias(self):
        self.check_program(POINT + program("void change(ref readonly Point view, ref Point writer);", "\twriter.x++",
            "\tprint(view.x)", "int main();", "\tPoint p = {x=3.0}", "\tref Point alias = p.addr", "\tchange(ref alias, ref p)"), "4.0\n")

    def test_const_materialization_and_index_evaluation_are_once_and_ordered(self):
        self.check_program(POINT + program("int index(ref int n);", "\tn++", "\treturn 0", "int show(ref readonly int item, int later);",
            '\tprint("{item} {later}")', "\treturn item", "int main();", "\tconst int a[] = [7]", "\tint n",
            "\tshow(ref a[index(ref n)], n)", "\tprint(n)"), "7 1\n1\n")

    def test_const_materialization_can_be_repeated_in_loops_and_recursion(self):
        self.check_program(POINT + program("int count(ref readonly Point p, int n);", "\tif(n == 0);", "\t\treturn int(p.x)",
            "\treturn count(ref p, n - 1)", "int main();", "\tconst Point p = {x=3.0}", "\tint total",
            "\tfor(0 <= i < 500, i++);", "\t\ttotal += count(ref p, 2)", "\tprint(total)"), "1500\n")

    def test_explicit_ref_type_rank_capacity_loop_and_lifetime_rules(self):
        prefix=POINT+program("double read(ref readonly Point p) = p.x", "int sized(ref readonly int a[1]) = int(a.length)")
        for body, error in [("Point p\n\tread(p)","ref"), ("read(ref Point())","更新先"),
                ("int n\n\tread(ref n)","型"), ("int a[][] = [[1]]\n\tsized(ref a)","型"),
                ("int a[2]\n\tsized(ref a)","上限"), ("for(0 <= i < 2, i++);\n\t\tsized(ref i)","繰り返し")]:
            with self.subTest(body=body): self.compile_source(prefix+program("int main();", "\t"+body), error=error)
        self.compile_source(program("void read(ref readonly str c);", "\tprint(c)", "int main();", '\tstr text = "ab"',
            "\tread(ref text[0])"), error="文字列内")

    def test_readonly_field_borrow_retains_owner_and_array_relocation_guards(self):
        self.check_program(POINT+program("void change(ref readonly double x, ref Point owner);", "\towner = {}", "int main();",
            "\tPoint p = {x=3.0}", "\tchange(ref p.x, ref p)"), "", status=70, error="参照")
        self.check_program(program("void change(ref readonly int x, ref int a[]);", "\ta.add()", "int main();",
            "\tint a[] = [1]", "\tchange(ref a[0], ref a)"), "", status=70, error="参照")

    def test_const_call_rejects_unannotated_method_even_if_body_only_reads(self):
        self.compile_source(program("model A;", "\tint n = 7", "\tint get() = this.n", "int main();",
            "\tconst A a = {}", "\tprint(a.get())"), error="readonly ではありません")
        self.compile_source(program("model A;", "\tint n", "\tint get() = this.n", "\treadonly int read() = this.get()",
            "int main();", "\treturn 0"), error="readonly")

    def test_unreachable_and_unused_readonly_bodies_are_checked(self):
        self.compile_source(program("model A;", "\tint n", "\treadonly void unused();", "\t\tif(false);", "\t\t\tthis.n++",
            "int main();", "\treturn 0"), error="readonly")

    def test_readonly_file_observation_and_close_responsibility(self):
        path=self.directory/"input.txt"; path.write_text("line\n")
        prefix=program("model Reader;", "\tprivate file handle", "\tvoid open(str path);", "\t\tio.file.close(this.handle)",
            "\t\tthis.handle = io.file.open(path)", "\treadonly bool is_open() = observe(ref this.handle)",
            "\treadonly bool same() = this.handle == this.handle", "\tvoid close();", "\t\tio.file.close(this.handle)",
            "bool observe(ref readonly file f) = io.file.is_open(f)")
        body=program("int main();", "\tReader r", f'\tr.open("{path}")', '\tprint("{r.is_open()} {r.same()} {r}")')
        self.check_program(prefix+body+"\tr.close()\n", "true true Reader{}\n")
        self.compile_source(prefix+body, error="close")
        self.compile_source(prefix+body+"\tif(true);\n\t\treturn 0\n\tr.close()\n", error="close")

    def test_file_mutations_and_direct_or_aggregate_escapes_are_rejected(self):
        prefix=program("model Holder;", "\tfile handle", "\tfile handles[1]", "\tstr label", "model Outer;", "\tHolder child",
            "void consume(file f);", "\tio.file.close(f)", "void consume_holder(Holder h);", "\tio.file.close(h.handle)")
        operations=["io.file.read(h.handle)", "str line\n\tio.file.readline(h.handle, ref line)",
            'io.file.write(h.handle, "x")', "io.file.close(h.handle)", "file alias = h.handle", "file aliases[] = [h.handle]",
            "file alias\n\talias = h.handle", "file aliases[1]\n\taliases[0] = h.handle", "file aliases[]\n\taliases.add(h.handle)",
            "Holder alias = h", "Holder aliases[] = [h]", "consume(h.handle)", "consume_holder(h)",
            "Holder alias = {handle=h.handle}", "file aliases[] = h.handles", "io.file.close(h.handles[0])"]
        for body in operations:
            with self.subTest(body=body): self.compile_source(prefix+program("void inspect(ref readonly Holder h);", "\t"+body,
                "int main();", "\treturn 0"), error="readonly")
        for ret, expr in [("file", "h.handle"),("Holder","h"),("Outer","{child=h}")]:
            self.compile_source(prefix+program(f"{ret} escape(ref readonly Holder h);", "\treturn "+expr,
                "int main();", "\treturn 0"), error="readonly")
        self.compile_source(prefix+program("void inspect(ref readonly Outer outer);", "\tHolder h = outer.child",
            "int main();", "\treturn 0"), error="readonly")

    def test_readonly_file_param_and_independent_resource_and_owned_subcopies(self):
        path=self.directory/"independent.txt"
        self.check_program(program("model Reader;", "\tprivate file handle", "\tint n = 7",
            "\treadonly int get() = this.n", "\treadonly void independent(str path);", "\t\tint copy = this.n", "\t\tcopy++",
            '\t\tfile local = io.file.open(path, "w")', "\t\tio.file.write(local, copy)", "\t\tio.file.close(local)",
            "bool observe(ref readonly Reader reader) = reader.get() == 7", "int main();", "\tReader r",
            f'\tr.independent("{path}")', "\tprint(observe(ref r))"), "true\n")
        self.assertEqual(path.read_text(), "8")

    def test_readonly_file_containers_can_be_observed_and_readonly_forwarded(self):
        self.check_program(program("model Holder;", "\tfile handle", "\treadonly bool inspect() = io.file.is_open(this.handle)",
            "bool observe(ref readonly Holder h) = forward(ref h)", "bool forward(ref readonly Holder h) = h.inspect()",
            "bool array(ref readonly Holder h[]) = h[0] == h[0]", "int main();", "\tHolder h", "\tHolder list[1]",
            '\tprint("{observe(ref h)} {array(ref list)}")', "\tprint(h)"), "false true\nHolder{handle=<file>}\n")

    def test_const_models_containing_files_are_rejected_even_when_empty_or_private(self):
        for fields in [["private file handle"], ["file handles[]"], ["file handles[0]"]]:
            for declaration in ["const Resource r = {}", "const Resource r[] = []"]:
                with self.subTest(fields=fields, declaration=declaration):
                    self.compile_source(program("model Resource;", *("\t"+f for f in fields), "int main();", "\t"+declaration), error="file")
        self.compile_source(program("model Resource;", "\tfile handles[]", "model Box;", "\tResource children[]",
            "int main();", "\tconst Box box = {}"), error="file")
        self.check_program(program("const file handles[2] = []", "int main();", "\tio.file.close(handles[0])",
            "\tprint(io.file.is_open(handles[1]))"), "false\n")

    def test_invalid_modifiers_positions_and_reserved_names(self):
        sources=["readonly int top() = 1", "model A;\n\treadonly int n", "model A;\n\treadonly readonly int get() = 1",
            "model A;\n\treadonly public int get() = 1", "int main();\n\treadonly int n", "int main();\n\tref readonly int n",
            "int top(readonly int n) = n", "int top(val readonly int n) = n", "int top(ref readonly readonly int n) = n",
            "model A;\n\treadonly int get() = 1\n\tint get() = 2", "int main();\n\tint readonly",
            "int readonly() = 1", "model readonly;\n\tint n", "int top(ref readonly int readonly) = 1"]
        for source in sources:
            with self.subTest(source=source): self.compile_source(source+"\n", error="エラー")

    def test_modules_qualified_constants_fields_methods_aliases_and_ref_contracts(self):
        (self.directory/"geometry.qy").write_text(POINT+program("const Point ORIGIN = {x=3.0, y=4.0}",
            "double measure(ref readonly Point p) = p.distance()"))
        for using,root in [("", "geometry"),("using geometry as geo", "geo")]:
            with self.subTest(using=using):
                self.check_program(program("import geometry", using, f"const double X = {root}.ORIGIN.x", "int main();",
                    f'\tprint("{{X}} {{{root}.ORIGIN.distance()}} {{{root}.measure(ref {root}.ORIGIN)}}")'), "3.0 5.0 5.0\n")
                self.compile_source(program("import geometry", using, "int main();", f"\t{root}.ORIGIN.move(1.0, 0.0)"), error="readonly")
        self.check_program(program("import geometry", "using geometry.ORIGIN", "int main();", "\tprint(ORIGIN.distance())"), "5.0\n")

    def test_windows_and_assembly_generation(self):
        source=self.directory/"readonly.qy"; source.write_text((ROOT/"examples/models/readonly.qy").read_text())
        output=source.with_suffix(".exe")
        subprocess.run([str(self.compiler),str(source),"--target","windows","-o",str(output)],check=True,capture_output=True,timeout=10)
        self.assertEqual(output.read_bytes()[:2],b"MZ")
        assembly=source.with_suffix(".s")
        subprocess.run([str(self.compiler),str(source),"-S","-o",str(assembly)],check=True,capture_output=True,timeout=10)
        subprocess.run(["gcc","-x","assembler","-c",str(assembly),"-o",str(source.with_suffix(".o"))],check=True,capture_output=True,timeout=10)


if __name__ == "__main__":
    run_suite()
