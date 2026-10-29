"""Public text methods: native values, constant folding, Unicode and effects."""
import os, subprocess, unittest
from support import ROOT, program, run_suite
from time_support import PlatformCase, WINDOWS
from grapheme_unit_test import quote

class TextMethodsTests(PlatformCase):
    def both(self,expression,expected,type_='auto'):
        self.check_program(self.main(f'{type_} result = {expression}','io.print(result)',prefix=f'const {type_} C = {expression}\n') .replace('\tio.print(result)','\tio.print(C)\n\tio.print(result)'),expected*2)

    def test_search_ascii_and_empty(self):
        for expression,result in [('"banana".find("ana")','1'),('"banana".find("NA")','-1'),('"banana".contains("ana")','true'),('"banana".starts_with("ana")','false'),('"banana".ends_with("ana")','true')]:
            with self.subTest(expression=expression):self.both(expression,result+'\n')
        for s in ['', 'abc']:
            for name,result in [('contains','true'),('starts_with','true'),('ends_with','true'),('find','0')]:self.both(f'{quote(s)}.{name}("")',result+'\n')
        self.both('"".find("x")','-1\n')

    def test_unicode_find_uses_clusters(self):
        self.both('"A日😀é".find("é")','3\n')
        self.both('"🇯🇵日".find("日")','1\n')

    def test_both_match_boundaries(self):
        for text,partial in [('é','e'),('é','́'),('👩🏽‍💻','👩'),('🇯🇵','🇯'),('한','ᄒ'),('क्‍ष','ष'),('\r\n','\n'),('\r\n','\r')]:
            for method,result in [('contains','false'),('starts_with','false'),('ends_with','false'),('find','-1')]:self.both(f'{quote(text)}.{method}({quote(partial)})',result+'\n')
            self.both(f'{quote(text)}.split({quote(partial)})==[{quote(text)}]','true\n')
            self.both(f'{quote(text)}.replace({quote(partial)},"x")=={quote(text)}','true\n')

    def test_no_normalization_or_case_folding(self):
        self.both('"é".contains("é")','false\n');self.both('"QYNO".contains("qy")','false\n')

    def test_nul_is_content(self):
        self.both('"a\\0b".find("\\0")','1\n')
        self.both('"a\\0b".replace("\\0","-")','a-b\n')
        self.both('"a\\0b".split("\\0")==["a","b"]','true\n')

    def test_unicode_white_space(self):
        spaces=''.join(chr(c) for c in [*range(9,14),32,133,160,5760,*range(8192,8203),8232,8233,8239,8287,12288])
        self.both(f'{quote(spaces+"Ken"+spaces)}.trim()','Ken\n')
        self.both(f'{quote(spaces)}.trim()==""','true\n');self.both('"".trim()==""','true\n')

    def test_trim_preserves_non_whitespace_and_combining_clusters(self):
        for text,expected in [(' \u0301x ',' \u0301x'),('a  b','a  b'),('\ufeff','\ufeff'),('\u200b','\u200b'),('\0','\0'),('\x1c\x1d\x1e\x1f','\x1c\x1d\x1e\x1f')]:
            self.both(f'{quote(text)}.trim()=={quote(expected)}','true\n')

    def test_split_empty_fields_multibyte_and_overlap(self):
        for text,sep,parts in [(',a,,b,',',',['','a','','b','']),('a::b',':',['a','','b']),('a::b','::',['a','b']),('abc',',',['abc']),('',',',['']),(',',',',['','']),('aaa','aa',['','a']),('aaaa','aa',['','','']),('日😀日','日',['','😀',''])]:
            self.both(f'{quote(text)}.split({quote(sep)})==['+','.join(map(quote,parts))+']','true\n')

    def test_replace_all_nonoverlapping_and_no_rescan(self):
        for text,old,new,expected in [('banana','na','NA','baNANA'),('aaa','aa','b','ba'),('a','a','aa','aa'),('abc','b','','ac'),('abc','x','y','abc'),('','x','y',''),('日😀日','日','界','界😀界')]:
            self.both(f'{quote(text)}.replace({quote(old)},{quote(new)})=={quote(expected)}','true\n')

    def test_replace_junctions_resegment(self):
        self.both('"🇯,🇵".replace(",", "").length','1\n')
        self.both('"e,x".replace(",x", "́").length','1\n')

    def test_char_receivers_and_fixed_result_types(self):
        self.check_program(self.main('char c=" "','str s=c.trim()','str t=c.replace(" ","ab")','str parts[]=c.split(" ")','io.print(s=="")','io.print(t)','io.print(parts.length)','io.print(c.contains(" "))'),'true\nab\n2\ntrue\n')
        self.compile_source(self.main('char c=" ".trim()'),error='書記素')

    def test_temporary_index_field_and_chaining(self):
        self.check_program(self.main('Box b={text="  a,b  "}','str xs[]=["c,d"]','io.print(b.text.trim().split(","))','io.print(xs[0].split(","))','io.print(make().replace("a","A").trim())',prefix='model Box;\n\tstr text\nstr make()=" a "\n'),'["a", "b"]\n["c", "d"]\nA\n')

    def test_readonly_and_generic_receivers(self):
        self.check_program(self.main('const str s=" a,b "','io.print(parse(s))','io.print(generic(" x "))',prefix='str parse(ref readonly str s)[]=s.trim().split(",")\nauto generic(x)=x.trim()\n').replace('parse(s)','parse(ref s)'),'["a", "b"]\nx\n')

    def test_split_value_copy_mutation_and_ref(self):
        self.check_program(self.main('str source="a,b"','str a[]=source.split(",")','str b[]=a','a[0]="X"','append(ref a)','a.remove_at(1)','io.print(a)','io.print(b)','io.print(source)','a.clear()','io.print(a)',prefix='void append(ref str a[]);\n\ta.add("z")\n'),'["X", "z"]\n["a", "b"]\na,b\n[]\n')

    def test_split_fixed_array_existing_padding_and_limits(self):
        self.check_program(self.main('str a[3]="a,b".split(",")','io.print(a)'),'["a", "b", ""]\n')
        self.compile_source(self.main('const str a[1]="a,b".split(",")'),error='配列')

    def test_receiver_and_arguments_once_left_to_right(self):
        prefix='str get(int n,str s);\n\tio.print(n)\n\treturn s\n'
        self.check_program(self.main('io.print(get(1,"a").replace(get(2,"a"),get(3,"b")))',prefix=prefix),'1\n2\n3\nb\n')

    def test_snapshots_precede_later_argument_mutations(self):
        self.check_program(self.main('str s="a"','str old="a"','io.print(s.replace(old,mutate(ref s,ref old)))','io.print(s)','io.print(old)',prefix='str mutate(ref str s,ref str old);\n\ts="changed"\n\told="changed"\n\treturn "b"\n'),'b\nchanged\nchanged\n')

    def test_user_model_same_names(self):
        self.check_program(self.main('Box b','io.print(b.trim())','io.print(b.split("x"))',prefix='model Box;\n\tint trim()=42\n\tint split(str s)=3\n'),'42\n3\n')

    def test_type_count_and_ref_rejections(self):
        for expression in ['"a".contains(1)','"a".split(true)','"a".replace("x",2)','"a".trim(1)','"a".find()','"a".replace("b")','"a".contains(ref s)']:
            self.compile_source(self.main('str s="a"','io.print('+expression+')'),error='引数' if 'ref ' not in expression else 'ref')
        self.compile_source(self.main('int n=2','n.trim()'),error='メソッド')

    def test_const_empty_pattern_rejected_even_with_dynamic_receiver(self):
        for expression in ['"a".split("")','"a".replace("","x")','s.split("")','s.replace("","x")']:
            self.compile_source(self.main('str s="a"','io.print('+expression+')'),error='空')

    def test_dynamic_empty_pattern_fails_70(self):
        for expression in ['s.split(empty)','s.replace(empty,"x")']:
            self.check_program(self.main('str s="a"','str empty=""',expression),'',status=70,error='文字列メソッド')

    def test_argument_io_failure_stops_later_arguments(self):
        missing=str(self.directory/'missing')
        prefix='str fail();\n\tfile f=io.file.open('+quote(missing)+')\n\tio.file.close(f)\n\treturn "x"\nstr later();\n\tio.print("wrong")\n\treturn "y"\n'
        self.check_program(self.main('try;','\tio.print("a".replace(fail(),later()))','case();','\tio.print("caught")',prefix=prefix),'caught\n')

    def test_receiver_io_failure_stops_all_arguments(self):
        missing=str(self.directory/'missing')
        prefix='str fail();\n\tfile f=io.file.open('+quote(missing)+')\n\tio.file.close(f)\n\treturn "x"\nstr later();\n\tio.print("wrong")\n\treturn "y"\n'
        self.check_program(self.main('try;','\tio.print(fail().replace(later(),later()))','case();','\tio.print("caught")',prefix=prefix),'caught\n')

    def test_module_boundary_and_exported_const_array(self):
        (self.directory/'words.qy').write_text(program('public const str PARTS[]="a,b".split(",")','public str parse(str text)[]=text.trim().split(",")'))
        self.check_program(self.main('io.print(words.PARTS)','io.print(words.parse(" x,y "))',prefix='import words\n'),'["a", "b"]\n["x", "y"]\n')

    def test_all_arguments_evaluated_before_utf8_validation(self):
        prefix='str old();\n\tio.print("old")\n\treturn "a"\nstr new();\n\tio.print("new")\n\treturn "b"\n'
        binary=self.compile_source(self.main('str s="TEXTBAD"','io.print(s.replace(old(),new()))',prefix=prefix))
        binary.write_bytes(binary.read_bytes().replace(b'TEXTBAD',b'TEXTBA\xff'))
        command=[os.environ['QYNO_FILE_WINDOWS_RUNNER'],str(binary)] if WINDOWS else [str(binary)]
        result=subprocess.run(command,capture_output=True,timeout=30)
        self.assertEqual(result.returncode,65,result.stderr);self.assertEqual(result.stdout,b'old\nnew\n')

    def test_close_analysis_visits_second_argument(self):
        path=quote(str(self.directory/'data'))
        prefix='str close_text(file f);\n\tio.file.close(f)\n\treturn "b"\n'
        self.check_program(self.main('file f=io.file.open('+path+',"w")','io.print("a".replace("a",close_text(f)))',prefix=prefix),'b\n')
        self.compile_source(self.main('file f=io.file.open('+path+',"w")','io.print("a".replace("a","b"))'),error='close')

    def test_many_parts_and_gc(self):
        self.check_program(self.main('str source="a,"','for(0 <= i < 12,i++);','\tsource=source+source','str parts[]=source.split(",")','io.print(parts.length)','str copy[]=parts','parts[0]="X"','io.print(copy[0])','io.print(source.replace("a,","b").length)'),'4097\na\n4096\n')

    def test_source_assembly_path(self):
        if WINDOWS:self.skipTest('GNU assembler runs on Linux')
        path=self.directory/'assembly.qy';path.write_text(self.main('io.print(" a,b ".trim().split(","))'))
        asm=path.with_suffix('.s');binary=path.with_suffix('')
        result=subprocess.run([str(self.compiler),'-S',str(path),'-o',str(asm)],capture_output=True)
        self.assertEqual(result.returncode,0,result.stderr)
        subprocess.run(['gcc','-nostdlib','-no-pie',str(asm),'-o',str(binary)],check=True,capture_output=True)
        self.assertEqual(subprocess.check_output([str(binary)]),b'["a", "b"]\n')

    def test_invalid_utf8_all_inputs_validated_before_search_and_contract(self):
        for expression,needle in [('s.contains("")','TEXTBAD'),('"".contains(s)','TEXTBAD'),('"a".replace("",s)','TEXTBAD')]:
            # Empty old must be dynamic so the native validation order is observed.
            if 'replace' in expression:expression='"a".replace(empty,s)'
            binary=self.compile_source(self.main('str s="TEXTBAD"','str empty=""','io.print('+expression+')'))
            data=binary.read_bytes();self.assertIn(needle.encode(),data)
            binary.write_bytes(data.replace(needle.encode(),b'TEXTBA\xff'))
            command=[os.environ['QYNO_FILE_WINDOWS_RUNNER'],str(binary)] if WINDOWS else [str(binary)]
            result=subprocess.run(command,capture_output=True,timeout=30)
            self.assertEqual(result.returncode,65,result.stderr);self.assertIn(b'UTF-8',result.stderr)

if __name__=='__main__':run_suite()
