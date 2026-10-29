"""Process arguments, the CRT grammar and explicit empty parameter lists."""
import ctypes, os, random, re, struct, subprocess, tempfile, unittest
from pathlib import Path
from support import ROOT, program, run_suite
from time_support import PlatformCase, WINDOWS, inject_windows_api_results

class Bits(ctypes.Structure):
    _fields_=[('lo',ctypes.c_uint64),('hi',ctypes.c_uint64)]
class Request(ctypes.Structure):
    _fields_=[('a',Bits),('b',Bits),('out',Bits)]+[(name,ctypes.c_int64) for name in ['op','ty','from_','aux','status','length','used','base']]+[('input',ctypes.c_void_p),('buffer',ctypes.c_void_p)]

class CommandLineGrammarTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary=tempfile.TemporaryDirectory(prefix='qyno-args-grammar-');cls.addClassCleanup(cls.temporary.cleanup)
        library=Path(cls.temporary.name)/'runtime.so'
        subprocess.run(['gcc','-std=c99','-O1','-shared','-fPIC',str(ROOT/'wide_runtime.c'),str(ROOT/'grapheme_runtime.c'),'-lm','-o',str(library)],check=True,capture_output=True)
        cls.library=ctypes.CDLL(str(library));cls.library.wide_execute.argtypes=[ctypes.POINTER(Request)]
        # The public internal enum is the single source of the request opcode.
        cls.op=re.findall(r'W_[A-Z_]+', (ROOT/'wide_runtime.h').read_text().split('enum { W_CONVERT',1)[1].split('};',1)[0]).index('W_ARGUMENTS')+1

    def decode(self,text,*,status=0):
        raw=text.encode('utf-16-le',errors='surrogatepass');source=ctypes.create_string_buffer(raw)
        request=Request();request.op=self.op;request.input=ctypes.addressof(source);request.length=len(raw)//2
        self.library.wide_execute(ctypes.byref(request));self.assertEqual(request.status,status)
        if status:return
        count,size=request.out.lo,request.out.hi
        output=ctypes.create_string_buffer(size+16);request.buffer=ctypes.addressof(output);request.base=size
        self.library.wide_execute(ctypes.byref(request));self.assertEqual(request.status,0)
        self.assertEqual((request.out.lo,request.out.hi),(count,size))
        result=[];position=0
        for _ in range(count):
            length=struct.unpack_from('<Q',output.raw,position)[0];position+=8
            result.append(output.raw[position:position+length].decode('utf-8'));position+=(length+7)&~7
        self.assertEqual(position,size);self.assertEqual(output.raw[size:],b'\0'*16)
        return result

    def test_documented_quotes_backslashes_and_empty_arguments(self):
        cases=[('prog',[]),('prog "" ""',['','']),('prog "a b c" d e',['a b c','d','e']),
            (r'prog "ab\"c" "\\" d',['ab"c','\\','d']),
            ('prog a"b c"d',['ab cd']),('prog "a""b"',['a"b']),('prog "unterminated tail',['unterminated tail']),
            ('prog "a"""',['a"']),('prog one\ttwo',['one','two']),(' prog',['prog']),('',[])]
        for text,expected in cases:
            with self.subTest(text=text):self.assertEqual(self.decode(text),expected)

    def test_executable_token_has_its_own_quote_rules(self):
        self.assertEqual(self.decode('"C:\\some path\\main.exe" a'),['a'])
        self.assertEqual(self.decode('"a b"suffix.exe a'),['a'])
        self.assertEqual(self.decode('"a\\" b'),['b'])

    def test_python_windows_quoting_round_trips_independent_vectors(self):
        rng=random.Random(275)
        alphabet=['a','b',' ','\t','"','\\','あ','😺','\n']
        for _ in range(120):
            args=[''.join(rng.choice(alphabet) for _ in range(rng.randrange(18))) for _ in range(rng.randrange(8))]
            self.assertEqual(self.decode(subprocess.list2cmdline(['C:\\path with space\\app.exe']+args)),args)

    def test_surrogates_are_validated_without_replacement(self):
        self.assertEqual(self.decode('prog "日本語😺é"'),['日本語😺é'])
        for value in ['\ud800','\udc00','\ud800x','\ud800\ud800']:
            self.decode('prog '+value,status=65)

    def test_output_capacity_is_checked_before_every_write(self):
        raw='prog "日本語"'.encode('utf-16-le');source=ctypes.create_string_buffer(raw)
        request=Request();request.op=self.op;request.input=ctypes.addressof(source);request.length=len(raw)//2
        self.library.wide_execute(ctypes.byref(request));size=request.out.hi
        for capacity in [0,1,7,8,size-1]:
            output=ctypes.create_string_buffer(b'X'*(size+16));request.status=0;request.buffer=ctypes.addressof(output);request.base=capacity
            self.library.wide_execute(ctypes.byref(request));self.assertEqual(request.status,70)
            self.assertEqual(output.raw[capacity:size+16],b'X'*(size+16-capacity))

class ArgumentTests(PlatformCase):
    source=program('int main(str args[]);','\tio.print(args.length)',
        '\tfor(0 <= i < args.length, i++);','\t\tio.print(args[i])','\treturn 0')
    def run_arguments(self,source,args=(),*,expected='',status=0,error=None):
        binary=self.compile_source(source)
        command=[os.environ['QYNO_FILE_WINDOWS_RUNNER'],str(binary)] if WINDOWS else [str(binary)]
        result=subprocess.run(command+list(args),capture_output=True,timeout=30 if WINDOWS else 5)
        self.assertEqual(result.returncode,status,result.stderr.decode(errors='replace'))
        self.assertEqual(result.stdout,expected.encode())
        if error:self.assertIn(error,result.stderr.decode(errors='replace'))
        else:self.assertEqual(result.stderr,b'')
        return result

    def test_empty_ordinary_and_unicode_arguments(self):
        for args in [[],['one','two'],['','two words','','日本語😺','é'],['--help','--','*.qy','$HOME','~']]:
            self.run_arguments(self.source,args,expected=str(len(args))+'\n'+''.join(a+'\n' for a in args))

    def test_executable_name_with_spaces_and_unicode_is_excluded(self):
        binary=self.compile_source(self.source)
        renamed=self.directory/('my application 日本語'+('.exe' if WINDOWS else ''))
        binary.rename(renamed)
        command=[os.environ['QYNO_FILE_WINDOWS_RUNNER'],str(renamed)] if WINDOWS else [str(renamed)]
        r=subprocess.run(command+['argument'],capture_output=True,timeout=30 if WINDOWS else 5)
        self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,b'1\nargument\n')

    def test_quotes_tabs_and_trailing_backslashes(self):
        args=['"quoted"','a\\"b','a b\\','\\\\','a\tb','a\nb','"','\\"','']
        self.run_arguments(self.source,args,expected=str(len(args))+'\n'+''.join(a+'\n' for a in args))

    def test_argument_array_is_mutable_and_value_copies_are_independent(self):
        source=program('void edit(ref str values[]);','\tvalues[0]="changed"','int main(str args[]);',
            '\tstr copy[]=args','\tedit(ref args)','\targs.add("extra")','\tio.print(copy)','\tio.print(args)',
            '\targs.clear()','\tio.print(args.length)','\treturn 0')
        self.run_arguments(source,['original'],expected='["original"]\n["changed", "extra"]\n0\n')

    def test_parameter_names_and_explicit_val_are_supported(self):
        for header in ['int main(str arguments[]);','int main(val str values[]);']:
            name='arguments' if 'arguments' in header else 'values'
            self.run_arguments(program(header,f'\tio.print({name}[0])','\treturn 0'),['42'],expected='42\n')

    def test_many_and_long_arguments(self):
        args=['x'*5000]+[str(i) for i in range(250)]
        self.run_arguments(program('int main(str args[]);','\tio.print(args.length)',
            '\tio.print(args[0].length)','\tio.print(args[250])','\treturn 0'),args,expected='251\n5000\n249\n')

    def test_arguments_survive_repeated_allocations_and_array_copies(self):
        source=program('int main(str args[]);','\tfor(0 <= i < 1500, i++);',
            '\t\tstr copy[]=args','\t\tcopy.add(str(i))','\tio.print(args[0])',
            '\tio.print(args[args.length-1])','\tio.print(args.length)','\treturn 0')
        self.run_arguments(source,['日本語']+[str(i) for i in range(20)],expected='日本語\n19\n21\n')

    def test_return_status_and_implicit_return(self):
        self.run_arguments(program('int main(str args[]);','\treturn int(args.length)'),['a','b','c'],status=3)
        self.run_arguments(program('int main(str args[]);','\tio.print(args.length)'),[],expected='0\n')

    def test_void_markers_and_existing_unit_value_parameter(self):
        source=program('void show(void);','\tio.print("empty")','void accept(void value);','\tio.print(value)',
            'int value(Void)=7','int main(void);','\tshow()','\taccept(())','\tio.print(value())','\treturn 0')
        self.run_arguments(source,['ignored'],expected='empty\n()\n7\n')

    def test_void_marker_keeps_this_and_interface_contract(self):
        source=program('interface Counter;','\tint get(void);','model Value implements Counter;','\tint count=9',
            '\tint get()=this.count','int inspect(ref Counter value)=value.get()',
            'int main(Void);','\tValue value','\tio.print(inspect(ref value))','\treturn 0')
        self.run_arguments(source,expected='9\n')

    def test_void_init_and_no_argument_generics(self):
        source=program('model Counter;','\tint count','\tvoid init(void);','\t\tthis.count=7',
            '\tint get(Void)=this.count','identity(x)=x','int main(str args[]);','\tCounter counter',
            '\tio.print(identity(counter.get()))','\treturn 0')
        self.run_arguments(source,expected='7\n')

    def test_dependency_main_remains_prohibited(self):
        (self.directory/'other.qy').write_text(program('int main(void)=99'))
        self.compile_source(program('import other','int main(str args[]);','\treturn 0'),
            error='依存モジュールには main',error_source='other.qy')

    def test_invalid_entry_signatures_are_rejected(self):
        for header in ['int main(int argc, str argv[]);','int main(str args[2]);','int main(str args[][]);',
                'int main(ref str args[]);','int main(ref readonly str args[]);','int main(auto args);',
                'auto main(str args[]);','int main(void value);','void main();']:
            with self.subTest(header=header):self.compile_source(program(header,'\treturn 0'),error='main 関数')

    def test_invalid_unnamed_parameters_and_calls_are_rejected(self):
        for header in ['void f(void, int value);','void f(ref void);','void f(val void);']:
            self.compile_source(program(header,'\treturn','int main();','\treturn 0'),error='引数')
        self.compile_source(program('void f(void);','\treturn','int main();','\tf(())','\treturn 0'),error='引数')
        self.compile_source(program('int main(str args[]);','\tmain(args)','\treturn 0'),error='main は呼び出せません')

    @unittest.skipIf(WINDOWS,'Linux argv encoding')
    def test_invalid_utf8_fails_before_main_but_no_args_form_ignores_it(self):
        result=self.run_arguments(self.source,[b'bad-\xff'],status=65,error='UTF-8')
        self.assertIn(b'runtime.arguments',result.stderr)
        for header in ['int main();','int main(void);']:
            self.run_arguments(program(header,'\tio.print("ready")','\treturn 0'),[b'\xff'],expected='ready\n')

    @unittest.skipUnless(WINDOWS,'Windows startup API injection')
    def test_no_argument_entry_never_reads_command_line(self):
        for header in ['int main();','int main(void);']:
            binary=self.compile_source(program(header,'\tio.print("ready")','\treturn 0'))
            inject_windows_api_results(binary,{'GetCommandLineW':0})
            r=subprocess.run([os.environ['QYNO_FILE_WINDOWS_RUNNER'],str(binary),'ignored'],capture_output=True,timeout=30)
            self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,b'ready\n')

    @unittest.skipUnless(WINDOWS,'Windows CRT reference')
    def test_arguments_match_actual_msvcrt_reference(self):
        from arguments_reference import build_crt_reference,read_crt_arguments
        binary=self.compile_source(self.source);reference=build_crt_reference(binary,self.directory)
        vectors=[[],['','a b','"','a\\"b','tail \\','日本語😺'],['one','two'],
                 ['a""b','""','\\\\','a\tb','a\nb']]
        for args in vectors:
            actual=subprocess.run([os.environ['QYNO_FILE_WINDOWS_RUNNER'],str(reference)]+args,capture_output=True,timeout=30)
            self.assertEqual(actual.returncode,0,actual.stderr)
            self.assertEqual(read_crt_arguments(actual.stdout),args)
            self.run_arguments(self.source,args,expected=str(len(args))+'\n'+''.join(a+'\n' for a in args))

    @unittest.skipUnless(WINDOWS,'Windows UTF-16 command line')
    def test_native_unpaired_surrogate_fails_before_main(self):
        marker='QYNO_COMMAND_LINE_BUFFER_'+'x'*256
        source=program('int main(str args[]);','\tio.print("entered main")',f'\tio.print("{marker}")','\treturn 0')
        for invalid in ['\ud800','\udc00']:
            binary=self.compile_source(source);data=bytearray(binary.read_bytes())
            pe=struct.unpack_from('<I',data,0x3c)[0];optional=pe+24
            struct.pack_into('<Q',data,optional+24,0x400000)
            location=data.index(marker.encode());address=None
            for i in range(struct.unpack_from('<H',data,pe+6)[0]):
                section=optional+struct.unpack_from('<H',data,pe+20)[0]+i*40
                rva,raw_size,raw=struct.unpack_from('<III',data,section+12)
                if raw<=location<raw+raw_size:address=0x400000+rva+location-raw
            self.assertIsNotNone(address)
            payload=('program '+invalid+'\0').encode('utf-16-le',errors='surrogatepass')
            data[location:location+len(payload)]=payload;binary.write_bytes(data)
            inject_windows_api_results(binary,{'GetCommandLineW':address})
            result=subprocess.run([os.environ['QYNO_FILE_WINDOWS_RUNNER'],str(binary)],capture_output=True,timeout=30)
            self.assertEqual(result.returncode,65,result.stderr);self.assertEqual(result.stdout,b'')
            self.assertIn(b'runtime.arguments',result.stderr)

    def test_assembly_receives_the_same_runtime_arguments(self):
        path=self.directory/'source.qy';path.write_text(self.source);asm=self.directory/'source.s';binary=self.directory/'assembled'
        subprocess.run([str(self.compiler),str(path),'-S','--target','linux','-o',str(asm)],check=True,capture_output=True)
        subprocess.run(['gcc','-nostdlib','-no-pie',str(asm),'-o',str(binary)],check=True,capture_output=True)
        r=subprocess.run([str(binary),'a b','','😺'],capture_output=True)
        self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,'3\na b\n\n😺\n'.encode())

if __name__=='__main__':run_suite()
