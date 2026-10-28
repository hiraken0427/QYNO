"""Borrowed interface contracts, concrete specialization and resource safety."""
import subprocess
from support import QynoTestCase, ROOT, program, run_suite

CONTRACT=program('interface Value;', '\treadonly int get();')
MODEL=program('model Counter implements Value;', '\tprivate int n', '\tvoid init(int n=1);', '\t\tthis.n = n',
    '\treadonly int get() = this.n', '\tvoid increase();', '\t\tthis.n++')
GET=program('int get(ref readonly Value value) = value.get()')

class InterfaceTests(QynoTestCase):
    def test_complete_example(self):
        self.check_program((ROOT/'examples/models/interfaces.qy').read_text(),'12.566371\n12.0\n')

    def test_multiple_models_and_forwarding(self):
        self.check_program(CONTRACT+MODEL+program('model Other implements Value;', '\treadonly int get() = 8',
            'void show(ref readonly Value v);', '\tprint(get(ref v))')+GET+program('int main();',
            '\tCounter a = {n=3}', '\tOther b', '\tshow(ref a)', '\tshow(ref b)', '\tshow(ref a)'), '3\n8\n3\n')

    def test_mutation_and_no_reinitialization(self):
        self.check_program(program('interface Increment;', '\tvoid increase();')+CONTRACT+
            MODEL.replace('implements Value','implements Value, Increment').replace('this.n = n','this.n = n\n\t\tprint("init")')+
            program('void bump(ref Increment v);', '\tv.increase()', 'int main();', '\tCounter c', '\tbump(ref c)', '\tprint(c.get())'), 'init\n2\n')

    def test_multiple_interface_slots_and_ordered_specialization_key(self):
        self.check_program(CONTRACT+MODEL+program('model Other implements Value;', '\treadonly int get() = 8',
            'int pair(ref readonly Value a, ref readonly Value b) = a.get() * 10 + b.get()', 'int main();',
            '\tCounter c = {n=3}', '\tOther o', '\tprint(pair(ref c, ref o))', '\tprint(pair(ref o, ref c))'), '38\n83\n')

    def test_recursive_and_mutually_recursive_specializations(self):
        self.check_program(CONTRACT+MODEL+program('int first(ref readonly Value v, int n);', '\tif(n == 0);', '\t\treturn v.get()',
            '\treturn second(ref v, n - 1)', 'int second(ref readonly Value v, int n) = first(ref v, n)',
            'int main();', '\tCounter c = {n=7}', '\tprint(first(ref c, 12))'), '7\n')

    def test_model_method_with_interface_arguments(self):
        self.check_program(CONTRACT+MODEL+program('model Printer;', '\treadonly int show(ref readonly Value v) = get(ref v)')+GET+
            program('int main();','\tCounter c','\tPrinter p','\tprint(p.show(ref c))'), '1\n')

    def test_private_access_retains_lexical_owner(self):
        self.check_program(CONTRACT+MODEL+program('model Helper;', '\tprivate int bias=5',
            '\tint sum(ref readonly Value v) = this.bias + v.get()', 'int main();', '\tHelper h', '\tCounter c', '\tprint(h.sum(ref c))'), '6\n')

    def test_const_aggregate_can_be_readonly_interface_argument(self):
        self.check_program(CONTRACT+program('model Fixed implements Value;', '\tint n', '\treadonly int get() = this.n',
            'const Fixed F = {n=9}')+GET+program('int main();', '\tprint(get(ref F))'), '9\n')

    def test_nested_and_indexed_places_evaluate_once(self):
        self.check_program(CONTRACT+MODEL+GET+program('model Box;', '\tCounter child', 'int index();', '\tprint("index")', '\treturn 0',
            'int main();', '\tCounter values[]=[{n=7}]', '\tBox b', '\tprint(get(ref values[index()]))', '\tprint(get(ref b.child))'), 'index\n7\n1\n')

    def test_same_ref_aliases_see_mutation(self):
        self.check_program(program('interface Increment;', '\tvoid increase();')+CONTRACT+MODEL.replace('implements Value','implements Value, Increment')+
            program('int update(ref readonly Value view, ref Increment writer);','\twriter.increase()', '\treturn view.get()',
            'int main();','\tCounter c','\tprint(update(ref c, ref c))'), '2\n')

    def test_readonly_receiver_cannot_use_mutating_contract(self):
        self.compile_source(program('interface Increment;', '\tvoid increase();', 'void bad(ref readonly Increment v);', '\tv.increase()', 'int main();','\treturn 0'),error='readonly')

    def test_readonly_implementation_does_not_strengthen_contract(self):
        prefix=program('interface Value;', '\tint get();')+MODEL
        self.compile_source(prefix+GET+program('int main();','\treturn 0'),error='readonly')
        self.check_program(prefix+program('int get(ref Value v) = v.get()', 'int main();','\tCounter c','\tprint(get(ref c))'), '1\n')

    def test_readonly_forwarding_rules(self):
        self.check_program(CONTRACT+MODEL+GET+program('int forward(ref Value v) = get(ref v)', 'int main();','\tCounter c','\tprint(forward(ref c))'), '1\n')
        self.compile_source(CONTRACT+program('void mutable(ref Value v);','\tprint(v.get())','void bad(ref readonly Value v);',
            '\tmutable(ref v)', 'int main();','\treturn 0'),error='readonly')

    def test_different_contract_forwarding_rejected_even_with_same_implementation(self):
        self.compile_source(CONTRACT+program('interface Other;', '\treadonly int get();', 'int other(ref readonly Other v) = v.get()',
            'int bad(ref readonly Value v) = other(ref v)', 'int main();','\treturn 0'),error='interface')

    def test_implements_is_required_not_structural(self):
        self.compile_source(CONTRACT+MODEL.replace(' implements Value','')+GET+program('int main();','\tCounter c','\tprint(get(ref c))'),error='implements')

    def test_bad_implementations_rejected_even_unused(self):
        for actual in ['private readonly int get() = 1', 'int get() = 1', 'readonly double get() = 1.0',
                       'readonly int get(int n) = n', 'int other = 1']:
            with self.subTest(actual=actual): self.compile_source(CONTRACT+program('model Bad implements Value;','\t'+actual,'int main();','\treturn 0'),error='interface')

    def test_multiple_contracts_shared_method_and_conflicts(self):
        self.check_program(CONTRACT+program('interface Other;', '\tint get();')+MODEL.replace('implements Value','implements Value, Other')+
            GET+program('int main();','\tCounter c','\tprint(get(ref c))'), '1\n')
        self.compile_source(CONTRACT+program('interface Other;', '\treadonly double get();')+
            MODEL.replace('implements Value','implements Value, Other')+program('int main();','\treturn 0'),error='一致')

    def test_argument_modes_and_dimension_contracts(self):
        prefix=program('interface Transform;', '\tint apply(ref readonly int source[1+1], val int delta);')
        good=program('model Op implements Transform;', '\tint apply(ref readonly int items[2], int amount) = items[0] + amount',
            'int calculate(ref Transform op, ref readonly int a[2]) = op.apply(ref a, 5)', 'int main();', '\tOp op',
            '\tint a[2]=[3,4]', '\tprint(calculate(ref op, ref a))')
        self.check_program(prefix+good,'8\n')
        for signature in ['ref int items[2], int amount','ref readonly int items[], int amount','ref readonly int items[2], ref int amount']:
            self.compile_source(prefix+good.replace('ref readonly int items[2], int amount',signature),error='一致')

    def test_array_returns_concrete_model_returns_and_value_arguments(self):
        self.check_program(program('model Data;','\tint n', 'interface Factory;', '\treadonly Data make(Data input_data);',
            '\treadonly int values()[];', 'model F implements Factory;', '\treadonly Data make(Data given) = given',
            '\treadonly int values()[] = [1,2]', 'int read(ref readonly Factory f);','\tData d = f.make({n=7})',
            '\tint a[] = f.values()', '\treturn d.n + a[1]', 'int main();','\tF f', '\tprint(read(ref f))'), '9\n')

    def test_invalid_declaration_positions(self):
        bodies=['Value value', 'ref Value value', 'Value values[]=[]', 'const Value value={}',
            'model Holder;\n\tValue field', 'Value make() = {}', 'void bad(Value v);\n\treturn',
            'void bad(ref Value values[]);\n\treturn', 'model Holder;\n\tvoid init(ref Value v);\n\t\treturn']
        for body in bodies:
            source=CONTRACT+(program('int main();','\t'+body) if body.startswith(('Value value','ref Value','Value values','const')) else body+'\nint main();\n\treturn 0\n')
            with self.subTest(body=body): self.compile_source(source,error='エラー')
        self.compile_source(CONTRACT+program('int main();','\tValue()'),error='interface')

    def test_whole_value_operations_rejected_even_unused(self):
        for body in ['print(v)','print(v == v)','print(v.addr)','print(v.value)','print(v.length)',
            'v = v', 'input(v)', 'get(ref v.value)', 'int n = v.get()\n\tprint(v)']:
            with self.subTest(body=body): self.compile_source(CONTRACT+GET+program('void bad(ref Value v);','\t'+body,'int main();','\treturn 0'),error='interface')

    def test_noncontract_members_rejected_even_in_dead_branches(self):
        for body in ['print(v.n)','v.increase()', 'if(false);\n\t\tv.increase()']:
            self.compile_source(CONTRACT+MODEL+program('void bad(ref Value v);','\t'+body,'int main();','\treturn 0'),error='interface')

    def test_signature_only_syntax_and_reserved_keywords(self):
        cases=['interface Empty;\n\t// no methods', 'interface Bad;\n\tprivate int get();',
            'interface Bad;\n\tint get() = 1', 'interface Bad;\n\tint get();\n\t\treturn 1',
            'interface Bad;\n\tint field', 'interface Bad;\n\tvoid init();',
            'interface Bad;\n\tint get(int n=1);', 'interface Bad;\n\tint get();\n\tint get();',
            'interface Bad;\n\tint get(int this);', 'interface Bad;\n\tint get(ref Bad other);',
            'model Bad implements int;\n\tint n', 'int interface() = 1', 'int implements() = 1']
        for source in cases:
            with self.subTest(source=source): self.compile_source(source+'\nint main();\n\treturn 0\n',error='エラー')

    def test_name_collisions_and_duplicate_implements(self):
        for body in ['interface Value;\n\tint other();','model Value;\n\tint n','int Value() = 1','const int Value=1',
            'model Bad implements Value, Value;\n\treadonly int get() = 1']:
            self.compile_source(CONTRACT+body+'\nint main();\n\treturn 0\n',error='重複')

    def test_ref_marker_and_places_required(self):
        for call in ['get(c)','get(ref Counter())','get(ref 3)']:
            self.compile_source(CONTRACT+MODEL+GET+program('int main();','\tCounter c','\t'+call),error='エラー')

    def resource_prefix(self,body='io.file.close(this.handle)'):
        return program('interface Closable;', '\tvoid close();','model Reader implements Closable;', '\tprivate file handle',
            '\tvoid init(str path);','\t\tthis.handle = io.file.open(path, "w")','\tvoid close();','\t\t'+body,
            'void finish(ref Closable r);','\tr.close()')

    def test_file_closing_through_interface(self):
        source=self.resource_prefix()+program('int main();',f'\tReader r = {{path="{self.directory}/a.txt"}}','\tfinish(ref r)')
        self.check_program(source,'')
        self.compile_source(source.replace('\tfinish(ref r)','\treturn 0'),error='close')

    def test_named_close_is_not_assumed_to_release_resource(self):
        self.compile_source(self.resource_prefix('return')+program('int main();',f'\tReader r = {{path="{self.directory}/a.txt"}}','\tfinish(ref r)'),error='close')
        self.compile_source(self.resource_prefix('if(this.handle == this.handle);\n\t\t\tio.file.close(this.handle)')+
            program('int main();',f'\tReader r = {{path="{self.directory}/a.txt"}}','\tfinish(ref r)'),error='close')

    def test_file_summaries_are_distinct_per_concrete_model(self):
        good=self.resource_prefix()
        bad=program('model Leaker implements Closable;','\tfile handle','\tvoid close();','\t\treturn')
        self.compile_source(good+bad+program('int main();',f'\tReader r = {{path="{self.directory}/a.txt"}}','\tfinish(ref r)',
            f'\tLeaker bad = {{handle=io.file.open("{self.directory}/b.txt", "w")}}','\tfinish(ref bad)'),error='close')

    def test_file_return_and_explicit_argument_responsibility(self):
        self.check_program(program('interface Files;','\tfile open(str path);','\tvoid close(file handle);',
            'model FilesImpl implements Files;','\tfile open(str path) = io.file.open(path, "w")',
            '\tvoid close(file handle);','\t\tio.file.close(handle)', 'void use(ref Files files, str path);',
            '\tfile handle = files.open(path)', '\tfiles.close(handle)', 'int main();','\tFilesImpl f',f'\tuse(ref f, "{self.directory}/a.txt")'), '')

    def test_pending_this_cannot_be_passed(self):
        self.compile_source(program('interface Value;','\treadonly int get();','void inspect(ref readonly Value v);','\tprint(v.get())',
            'model Required;','\tvoid init(int n);','\t\treturn', 'model Parent implements Value;','\tRequired child',
            '\tvoid init();','\t\tinspect(ref this)','\t\tthis.child={n=1}','\treadonly int get() = 1','int main();','\treturn 0'),error='未完了')

    def test_import_alias_selected_and_whole_using(self):
        (self.directory/'contracts.qy').write_text(CONTRACT)
        (self.directory/'counter.qy').write_text('import contracts\nusing contracts.Value\n'+MODEL)
        for using,typ in [('using contracts as api','api.Value'),('using contracts.Value','Value'),('using contracts','Value')]:
            self.check_program(program('import contracts','import counter',using,f'int fetch(ref readonly {typ} v) = v.get()',
                'int main();','\tcounter.Counter c={n=6}','\tprint(fetch(ref c))'),'6\n')

    def test_nominal_interfaces_from_different_modules(self):
        (self.directory/'a.qy').write_text(CONTRACT+MODEL)
        (self.directory/'b.qy').write_text(CONTRACT)
        self.compile_source(program('import a','import b','int get(ref readonly b.Value v) = v.get()',
            'int main();','\ta.Counter c','\tprint(get(ref c))'),error='implements')

    def test_template_locals_arrays_strings_input_and_const_sharing(self):
        self.check_program(CONTRACT+MODEL+program('int compute(ref readonly Value v, int count);', '\tconst int N=2','\tint a[2]=[v.get(),N]',
            '\tstr s="abc"','\tint result','\tfor(0 <= i < count, i++);','\t\tresult += a[i % 2]', '\treturn result + int(s.length)',
            'int main();','\tCounter a={n=3}','\tCounter b={n=5}','\tprint(compute(ref a,4))','\tprint(compute(ref b,4))'),'13\n17\n')

    def test_frequent_gc_and_borrow_prevents_array_resize(self):
        self.check_program(CONTRACT+MODEL+GET+program('int main();','\tCounter values[2]=[{n=3},{n=4}]','\tint total',
            '\tfor(0 <= i < 500, i++);','\t\ttotal += get(ref values[i % 2])','\tprint(total)'),'1750\n')
        self.check_program(CONTRACT+MODEL+program('void update(ref Value v, ref Counter values[]);','\tvalues.add()',
            'int main();','\tCounter a[]=[{}]','\tupdate(ref a[0], ref a)'),'',status=70,error='参照')

    def test_4096_specialization_limit_and_reuse(self):
        models=''.join(program(f'model M{i} implements Value;',f'\treadonly int get() = {i}') for i in range(65))
        source=CONTRACT+models+program('int pair(ref readonly Value a, ref readonly Value b) = a.get() + b.get()',
            'int main();',*[f'\tM{i} v{i}' for i in range(65)])
        calls=[f'\tpair(ref v{i}, ref v{j})' for i in range(65) for j in range(65)]
        # Repeating the same combination must not consume more entries.
        self.compile_source(source+program(*([calls[0]]*100),*calls[:4096]))
        self.compile_source(source+program(*calls[:4097]),error='4096')

    def test_unused_ordinary_function_still_resolves_its_template_calls(self):
        self.check_program(CONTRACT+MODEL+GET+program('void unused();','\tCounter c','\tprint(get(ref c))',
            'int main();','\tprint("main")'),'main\n')

    def test_contract_can_declare_main_as_an_instance_method(self):
        self.check_program(program('interface Runnable;','\tint main();','model App implements Runnable;',
            '\tint main() = 7','int run(ref Runnable app) = app.main()', 'int main();','\tApp app','\tprint(run(ref app))'),'7\n')

    def test_concrete_types_in_different_modules_do_not_share_specializations(self):
        (self.directory/'contracts.qy').write_text(CONTRACT+GET)
        for name,value in [('a',3),('b',8)]:
            (self.directory/f'{name}.qy').write_text('import contracts\nusing contracts.Value\n'+
                program('model Same implements Value;',f'\treadonly int get() = {value}'))
        self.check_program(program('import contracts','import a','import b','int main();','\ta.Same first',
            '\tb.Same second','\tprint(contracts.get(ref first))','\tprint(contracts.get(ref second))'),'3\n8\n')

    def test_hidden_input_locals_and_shared_comparison_nodes_in_template(self):
        self.check_program(CONTRACT+MODEL+program('int read(ref readonly Value v);','\tint n',
            '\tinput(n)','\tif(0 <= n <= v.get());','\t\treturn n','\treturn -1',
            'int main();','\tCounter c={n=7}','\tprint(read(ref c))','\tprint(read(ref c))'),'5\n-1\n',stdin='5\n8\n')

    def test_windows_and_assembly(self):
        path=self.directory/'example.qy';path.write_text((ROOT/'examples/models/interfaces.qy').read_text())
        exe=path.with_suffix('.exe');asm=path.with_suffix('.s')
        subprocess.run([str(self.compiler),str(path),'--target','windows','-o',str(exe)],check=True,capture_output=True,timeout=10)
        self.assertEqual(exe.read_bytes()[:2],b'MZ')
        subprocess.run([str(self.compiler),str(path),'-S','-o',str(asm)],check=True,capture_output=True,timeout=10)
        subprocess.run(['gcc','-x','assembler','-c',str(asm),'-o',str(path.with_suffix('.o'))],check=True,capture_output=True,timeout=10)

if __name__=='__main__': run_suite()
