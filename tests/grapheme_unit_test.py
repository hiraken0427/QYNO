"""Unicode 17 extended grapheme conformance and QYNO's unique void value."""
from support import QynoTestCase, ROOT, program, run_suite


def quote(text):
    replacements={'\\':'\\\\','"':'\\"','\n':'\\n','\r':'\\r','\t':'\\t','\0':'\\0','{':'\\{','}':'\\}'}
    return '"'+''.join(replacements.get(c,c) for c in text)+'"'


class GraphemeUnitTests(QynoTestCase):
    def test_unicode_17_official_grapheme_break_conformance(self):
        cases=[]
        for line in (ROOT/'unicode/17.0.0/GraphemeBreakTest.txt').read_text().splitlines():
            fields=line.split('#')[0].split()
            if not fields:continue
            clusters=[];current=''
            for value in fields:
                if value=='÷':
                    if current:clusters.append(current);current=''
                elif value!='×':current+=chr(int(value,16))
            cases.append(clusters)
        self.assertGreater(len(cases),700)
        for start in range(0,len(cases),80):
            constants=[];body=['int main();']
            for i,clusters in enumerate(cases[start:start+80]):
                text=''.join(clusters);constants += [f'const str S{i}={quote(text)}',f'const int_64 N{i}=S{i}.length']
                checks=[f's{i}.length=={len(clusters)}',f'N{i}=={len(clusters)}']
                for j,cluster in enumerate(clusters):
                    constants.append(f'const char C{i}_{j}=S{i}[{j}]')
                    checks += [f's{i}[{j}]=={quote(cluster)}',f'C{i}_{j}=={quote(cluster)}']
                body += [f'\tstr s{i}=S{i}','\tprint('+ ' and '.join(checks)+')']
            self.check_program(program(*constants,*body),'true\n'*len(cases[start:start+80]))

    def test_combining_emoji_indic_hangul_and_crlf(self):
        for cluster in ['e\u0301','👩🏽\u200d💻','🇯🇵','👨\u200d👩\u200d👧\u200d👦','한','क्\u200dष','\r\n']:
            self.check_program(program('int main();',f'\tchar c={quote(cluster)}',f'\tstr s="A"+{quote(cluster)}+"B"',
                '\tprint("{c.length} {s.length}")','\tprint(s[1]==c)'), '1 3\ntrue\n')

    def test_normalization_is_not_implicit(self):
        self.check_program(program('int main();','\tchar a="é"','\tchar b="é"','\tprint("{a.length} {b.length} {a==b}")'), '1 1 false\n')

    def test_replacement_and_copy_preserve_whole_clusters(self):
        self.check_program(program('int main();','\tstr a="Aé🇯🇵B"','\tstr b=a','\ta[1]="👩🏽‍💻"',
            '\ta[2]="👍🏽"','\tprint(a)','\tprint(b)','\tprint(a.length)'), 'A👩🏽‍💻👍🏽B\nAé🇯🇵B\n4\n')

    def test_char_rejects_empty_and_multiple_clusters(self):
        for text in ['', 'ab', '🇯🇵🇨🇦','éx']:
            self.compile_source(program('int main();',f'\tchar c={quote(text)}'),error='書記素')
        self.check_program(program('int main();','\tstr s=input()','\tchar c=s','\tprint(c)'), '',stdin='🇯🇵🇨🇦\n',status=70,error='書記素')

    def test_direct_pattern_and_value_character_input(self):
        self.check_program(program('int main();','\tchar a=input()','\tchar b','\tchar c','\tinput(b)',
            '\tbool ok=input("{c}")','\tprint("{a} {b} {c} {ok}")'), 'é 🇯🇵 👩🏽‍💻 true\n',stdin='é\n🇯🇵\n👩🏽‍💻\n')

    def test_invalid_character_input_preserves_all_targets(self):
        self.check_program(program('int main();','\tchar c="é"','\tint n=3','\tbool ok=input("{n}, {c}")',
            '\tprint("{ok} {n} {c}")'), 'false 3 é\n',stdin='5, 🇯🇵🇨🇦\n')

    def test_input_stages_aliasing_string_elements_before_commit(self):
        self.check_program(program('int main();','\tstr s="abc"','\tint n=1','\tbool ok=input("{n}, {s[1]}, {s[2]}")',
            '\tprint("{ok} {n} {s}")'), 'false 1 abc\n',stdin='2, ́, X\n')
        self.check_program(program('int main();','\tstr s="éab"','\tbool ok=input("{s[0]}, {s[2]}")',
            '\tprint("{ok} {s}")'), 'true 🇯🇵a👍🏽\n',stdin='🇯🇵, 👍🏽\n')

    def test_invalid_utf8_is_rejected_even_after_selected_cluster(self):
        self.check_program(program('int main();','\tstr s=input()','\tprint(s[0])'),'',stdin=b'a\xff\n',status=70,error='UTF-8')

    def test_unit_variables_alias_default_and_unique_equality(self):
        self.check_program(program('const Void EMPTY=()','int main();','\tVoid a','\tvoid b=EMPTY','\tauto c=()',
            '\tprint("{a} {b} {c} {a==b} {a!=c}")','\tprint(str(a))'), '() () () true false\n()\n')

    def test_unit_parameters_returns_calls_and_side_effect_order(self):
        self.check_program(program('void work(int n);','\tprint(n)','\treturn ()','void nothing()=()',
            'void forward(void x)=x','int main();','\tvoid a=forward(work(1))','\tprint(a)',
            '\tprint(work(2)==work(3))','\tprint(nothing())'), '1\n()\n2\n3\ntrue\n()\n')

    def test_unit_inference_with_bare_and_explicit_returns(self):
        self.check_program(program('choose(bool b);','\tif(b);','\t\treturn','\treturn ()','identity(x)=x',
            'int main();','\tu=choose(true)','\tprint(identity(u))','\tprint(identity([(),()]))'), '()\n[(), ()]\n')

    def test_unit_arrays_all_dimensions_and_independent_copy(self):
        self.check_program(program('const void C[2]=[()]','void create()[][] = [[()],[(),()]]','int main();',
            '\tvoid a[][]=[[(),()],[(),()]]','\tvoid b[][]=a','\ta[0].add()','\tprint(a)','\tprint(b)','\tprint(C)',
            '\tprint(create())','\tprint(a==b)'), '[[(), (), ()], [(), ()]]\n[[(), ()], [(), ()]]\n[(), ()]\n[[()], [(), ()]]\nfalse\n')

    def test_unit_element_ref_whole_ref_and_addr(self):
        self.check_program(program('void set(ref void x);','\tx=()','void append(ref Void xs[]);','\txs.add(())',
            'int main();','\tvoid v','\tref void p=v.addr','\tset(ref p)','\tvoid xs[]=[v]','\tset(ref xs[0])',
            '\tappend(ref xs)','\tprint(p.value)','\tprint(xs)'), '()\n[(), ()]\n')

    def test_unit_models_init_defaults_const_and_interface(self):
        self.check_program(program('interface Action;','\tvoid run();','model Marker implements Action;','\tVoid state',
            '\tvoid run();','\t\tthis.state=()','invoke(ref Action action)=action.run()',
            'const Marker C={}','int main();','\tMarker m=C','\tprint(m)','\tprint(invoke(ref m))','\tprint(C==m)'), 'Marker{state=()}\n()\ntrue\n')

    def test_unit_methods_and_close_return_unit_with_side_effects(self):
        file=str(self.directory/'unit.txt')
        self.check_program(program('int main();','\tint xs[]','\tvoid result=xs.add(3)','\tprint("{result} {xs}")',
            f'\tfile f=io.file.open("{file}","w")','\tprint(io.file.write(f,()))','\tvoid closed=io.file.close(f)','\tprint(closed)'), '() [3]\n2\n()\n')
        self.assertEqual((self.directory/'unit.txt').read_text(),'()')

    def test_unit_input_and_atomic_failure(self):
        self.check_program(program('int main();','\tvoid a=input()','\tvoid b','\tint n=3',
            '\tbool ok=input("{b}, {n}")','\tprint("{ok} {a} {b} {n}")'), 'true () () 5\n',stdin='()\n(), 5\n')
        self.check_program(program('int main();','\tvoid a','\tint n=3','\tbool ok=input("{n}, {a}")',
            '\tprint("{ok} {n} {a}")'), 'false 3 ()\n',stdin='5, invalid\n')

    def test_unit_type_safety_and_const_readonly(self):
        for expression in ['()+1','not ()','()<()','int(())','char(())']:
            self.compile_source(program('int main();',f'\tprint({expression})'),error='エラー')
        for declaration in ['void v=1','int n=()','void a[1]=[1]']:
            self.compile_source(program('int main();','\t'+declaration),error='エラー')
        self.compile_source(program('void f(ref readonly void v);','\tv=()','int main();','\tvoid v','\tf(ref v)'),error='readonly')
        self.compile_source(program('int main();','\tconst Void C=()','\tC=()'),error='定数')

    def test_module_exports_and_generic_grapheme_text(self):
        (self.directory/'text.qy').write_text('const char LETTER="é"\nvoid empty()=()\nfirst(xs)=xs[0]\n')
        self.check_program(program('import text','int main();','\tprint(text.LETTER.length)',
            '\tprint(text.first("👩🏽‍💻abc"))','\tprint(text.empty())'), '1\n👩🏽‍💻\n()\n')

    def test_gc_and_nested_arrays_preserve_graphemes_and_units(self):
        self.check_program(program('int main();','\tstr s="é🇯🇵"','\tvoid units[][]=[[()]]',
            '\tfor(0<=i<1000,i++);','\t\ts[0]="👍🏽"','\t\tunits[0].clear()','\t\tunits[0].add(())',
            '\tprint(s)','\tprint(units)'), '👍🏽🇯🇵\n[[()]]\n')


if __name__=='__main__':run_suite()
