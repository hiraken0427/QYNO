"""Recoverable native I/O, lexical handlers, explicit cleanup and error effects."""
from pathlib import Path
import subprocess
import os
import shutil
import unittest
from support import QynoTestCase, program, run_suite


class RecoverableIoTests(QynoTestCase):
    def compile_source(self,source,**kwargs):
        if os.environ.get('QYNO_FILE_TARGET')!='windows' or kwargs.get('error') is not None:
            return super().compile_source(source,**kwargs)
        self.sequence+=1
        path=self.directory/f'case{self.sequence}.qy'
        source=source.replace('"'+str(self.directory),'"Z:'+str(self.directory))
        path.write_text(source)
        binary=path.with_suffix('.exe')
        result=subprocess.run([str(self.compiler),str(path),'--target','windows','-o',str(binary)],capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(result.stderr,'')
        return binary

    def check_program(self,source,expected,**kwargs):
        if os.environ.get('QYNO_FILE_TARGET')!='windows':return super().check_program(source,expected,**kwargs)
        binary=self.compile_source(source)
        result=subprocess.run([os.environ['QYNO_FILE_WINDOWS_RUNNER'],str(binary)],input=kwargs.get('stdin','').encode(),capture_output=True,timeout=30)
        self.assertEqual(result.returncode,kwargs.get('status',0),result.stderr.decode(errors='replace'))
        self.assertEqual(result.stdout,expected.encode())
        if kwargs.get('error') is not None:self.assertIn(kwargs['error'],result.stderr.decode())
        else:self.assertEqual(result.stderr,b'')
        return result

    def main(self, *lines, prefix='using io\n'):
        return prefix+program('int main();', *('\t'+line for line in lines), '\treturn 0')

    def missing(self):
        return str(self.directory/'missing.txt')

    def test_header_error_snapshot(self):
        self.check_program(self.main('file f',f'try(f=file.open("{self.missing()}")) as error;',
            '\tprint("wrong")','case(ErrorCode.not_found);',
            '\tprint("{error.code} {error.operation} {error.os_code > 0}")','finally;','\tfile.close(f)'),
            'not_found open true\n')

    def test_try_open_false_and_error(self):
        self.check_program(self.main('file f','Error error',f'bool ok=file.try_open("{self.missing()}",ref f,ref error)',
            'if(ok);','\tfile.close(f)','else;','\tprint("{error.code} {file.is_open(f)}")'),'not_found false\n')

    def test_try_open_success_resets_all_fields(self):
        path=self.directory/'file.txt';path.write_text('hello')
        self.check_program(self.main('file f','Error error',f'file.try_open("{self.missing()}",ref f,ref error)',
            'file.close(f)',f'if(file.try_open("{path}",ref f,ref error));','\tprint(file.read(f))','\tfile.close(f)',
            'print("{error.code} {error.message.length} {error.operation.length} {error.path.length} {error.os_code} {error.source.length} {error.line} {error.column}")'),
            'hello\nno_error 0 0 0 0 0 0 0\n')

    def test_try_open_false_does_not_enter_case(self):
        self.check_program(self.main('file f','Error error',f'try(file.try_open("{self.missing()}",ref f,ref error));',
            '\tprint("body")','case();','\tprint("wrong")','finally;','\tfile.close(f)'),'body\n')

    def test_no_header_and_multi_code(self):
        self.check_program(self.main('file f','try;',f'\tf=file.open("{self.missing()}")',
            'case(ErrorCode.permission_denied,ErrorCode.not_found);','\tprint("caught")',
            'finally;','\tfile.close(f)'),'caught\n')

    def test_false_header_is_not_a_condition(self):
        self.check_program(self.main('try(false);','\tprint("body")','case();','\tprint("wrong")'),'body\n')

    def test_success_skips_handlers(self):
        self.check_program(self.main('int n=0','try(n=3);','\tprint(n)','case();','\tprint("wrong")',
            'finally;','\tprint("finally")'),'3\nfinally\n')

    def test_nested_propagation_and_finally_order(self):
        self.check_program(self.main('file f','try;',f'\ttry(f=file.open("{self.missing()}"));',
            '\t\tprint("wrong")','\tcase(ErrorCode.permission_denied);','\t\tprint("wrong")',
            '\tfinally;','\t\tprint("inner finally")', '\t\tfile.close(f)',
            'case(ErrorCode.not_found);','\tprint("outer case")','finally;','\tprint("outer finally")'),
            'inner finally\nouter case\nouter finally\n')

    def test_same_try_does_not_recapture_handler_errors(self):
        self.check_program(self.main('file f','try;','\ttry;',f'\t\tf=file.open("{self.missing()}")',
            '\tcase(ErrorCode.not_found);',f'\t\tf=file.open("{self.missing()}")',
            '\tcase();','\t\tprint("wrong")','\tfinally;','\t\tfile.close(f)',
            'case();','\tprint("outer")'),'outer\n')

    def test_throw_preserves_origin(self):
        self.check_program(self.main('file f','try;',f'\ttry(f=file.open("{self.missing()}")) as first;',
            '\t\tprint("wrong")','\tcase();','\t\tprint(first.operation)','\t\tthrow;',
            '\tfinally;','\t\tfile.close(f)','case() ;','\tprint("outer")'),'open\nouter\n')

    def test_return_executes_nested_finally(self):
        prefix=program('using io','int answer();','\ttry;','\t\ttry;', '\t\t\treturn 42',
            '\t\tfinally;','\t\t\tprint("inner")','\tfinally;','\t\tprint("outer")')
        self.check_program(self.main('print(answer())',prefix=prefix),'inner\nouter\n42\n')

    def test_break_continue_finally(self):
        self.check_program(self.main('for(0 <= i < 3,i++);','\ttry;','\t\tif(i==0);','\t\t\tcontinue',
            '\t\tbreak','\tfinally;','\t\tprint(i)'),'0\n1\n')

    def test_local_loop_jump_does_not_leave_try(self):
        self.check_program(self.main('try;','\twhile(true);','\t\tbreak','\tprint("after")',
            'finally;','\tprint("finally")'),'after\nfinally\n')

    def test_cross_function_propagation(self):
        prefix=program('using io','void fail();','\tfile f',f'\ttry(f=file.open("{self.missing()}"));',
            '\t\tprint("wrong")','\tfinally;','\t\tfile.close(f)')
        self.check_program(self.main('try(fail()) as error;','\tprint("wrong")','case();',
            '\tprint("{error.code} {error.operation}")',prefix=prefix),'not_found open\n')

    def test_uncaught_error_and_finally(self):
        self.check_program(self.main('file f',f'try(f=file.open("{self.missing()}"));','\tprint("wrong")',
            'finally;','\tfile.close(f)','\tprint("cleanup")'),'cleanup\n',status=74,error='open')

    def test_new_finally_error_supersedes_pending(self):
        self.check_program(self.main('file f','try;',f'\ttry(f=file.open("{self.missing()}"));',
            '\t\tprint("wrong")','\tfinally;','\t\tfile.close(f)',f'\t\tf=file.open("{self.missing()}-again")',
            'case();','\tprint("outer")','finally;','\tfile.close(f)'),'outer\n')

    def test_unhandled_diagnostic_history(self):
        # Failure-only paths do not transfer handles; both opens fail in this test.
        self.check_program(self.main('file f',f'try(f=file.open("{self.missing()}"));','\tprint("wrong")',
            'finally;','\tfile.close(f)',f'\tf=file.open("{self.missing()}-again")','\tfile.close(f)'),
            '',status=74,error='again')

    def test_eof_is_normal(self):
        path=self.directory/'empty';path.write_bytes(b'')
        self.check_program(self.main('file f','str line="keep"',f'try(f=file.open("{path}"));',
            '\tprint(file.readline(f,ref line))','\tprint(line)','case();','\tprint("wrong")',
            'finally;','\tfile.close(f)'),'false\nkeep\n')

    def test_api_misuse_not_caught(self):
        self.check_program(self.main('file f','try;','\tfile.read(f)','case();','\tprint("wrong")'),
            '',status=70,error='未オープン')

    def test_not_regular_file(self):
        self.check_program(self.main('file f',f'try(f=file.open("{self.directory}"));','\tprint("wrong")',
            'case(ErrorCode.not_regular_file);','\tprint("directory")','finally;','\tfile.close(f)'),'directory\n')

    def test_qualified_and_using_types(self):
        self.check_program(self.main('io.Error e','io.ErrorCode c=io.ErrorCode.not_found','io.print("{e.code} {c}")',prefix=''),
            'no_error not_found\n')
        self.check_program(self.main('Error e','io.print(e.code)',prefix='using io.Error\n'),'no_error\n')
        self.check_program(self.main('Error e','io.print(e.code)',prefix='using io as stream\nusing stream.Error\n'),'no_error\n')

    def test_error_arrays_and_copies(self):
        self.check_program(self.main('Error errors[2]','Error e=errors[0]','e.code=ErrorCode.not_found',
            'print("{errors[0].code} {e.code} {lib.size(e.code)}")'),'no_error not_found 32\n')

    def test_case_validation(self):
        for cases,message in [(['case(ErrorCode.no_error);','\tprint(0)'],'no_error'),
                (['case(ErrorCode.not_found);','\tprint(0)','case(ErrorCode.not_found);','\tprint(1)'],'重複'),
                (['case();','\tprint(0)','case(ErrorCode.not_found);','\tprint(1)'],'最後'),
                (['case();','\tprint(0)','case();','\tprint(1)'],'最後'),
                (['case(74);','\tprint(0)'],'宣言されていない')]:
            with self.subTest(cases=cases):self.compile_source(self.main('try;','\tprint(0)',*cases),error=message)

    def test_throw_and_finally_jump_validation(self):
        for lines,message in [(['throw;'],'case'),(['try;','\tprint(0)','finally;','\treturn 0'],'finally'),
                (['while(true);','\ttry;','\t\tprint(0)','\tfinally;','\t\tbreak'],'finally')]:
            with self.subTest(lines=lines):self.compile_source(self.main(*lines),error=message)

    def test_binding_readonly_and_scope(self):
        self.compile_source(self.main('try;','\tprint(0)','case();','\terror.code=ErrorCode.not_found'),error='宣言')
        self.compile_source(self.main('try as e;','\tprint(0)','case();','\te.code=ErrorCode.not_found'),error='readonly')
        self.compile_source(self.main('try as e;','\tprint(e)','case();','\tprint(0)'),error='宣言')
        self.compile_source(self.main('try as e;','\tprint(0)','case();','\tprint(0)','finally;','\tprint(e)'),error='宣言')

    def test_try_open_requires_close_on_success(self):
        self.compile_source(self.main('file f','Error e',f'file.try_open("{self.missing()}",ref f,ref e)'),error='close')

    def test_try_open_signature_validation(self):
        for expression,message in [('file.try_open("x")','引数'),('file.try_open("x",f,ref e)','ref file'),
                ('file.try_open("x",ref f,e)','ref io.Error'),('file.try_open("x",ref f,ref e,"invalid")','モード')]:
            self.compile_source(self.main('file f','Error e',expression,'file.close(f)'),error=message)

    def test_recovered_callee_cannot_lose_local_file(self):
        path=self.directory/'f';path.write_text('text')
        prefix=program('using io','void unsafe();',f'\tfile f=file.open("{path}")','\tfile.read(f)','\tfile.close(f)')
        self.compile_source(self.main('try(unsafe());','\tprint(0)','case();','\tprint(1)',prefix=prefix),error='エラー経路')

    def test_old_unhandled_function_remains_valid(self):
        path=self.directory/'f';path.write_text('text')
        prefix=program('using io','void old();',f'\tfile f=file.open("{path}")','\tfile.read(f)','\tfile.close(f)')
        self.check_program(self.main('old()',prefix=prefix),'')

    def test_normal_exit_without_close_rejected(self):
        self.compile_source(self.main('file f',f'try(f=file.open("{self.missing()}"));','\tprint(0)',
            'case();','\tprint(1)'),error='close')

    def test_argument_error_releases_element_ref_pins(self):
        prefix=program('using io','void consume(ref int value,str text);','\tvalue++',
            'str fail();','\tfile f',f'\ttry(f=file.open("{self.missing()}"));','\t\treturn file.read(f)',
            '\tfinally;','\t\tfile.close(f)')
        self.check_program(self.main('int values[]=[1]','try(consume(ref values[0],fail()));',
            '\tprint("wrong")','case();','\tvalues.add(2)','print(values)',prefix=prefix),'[1, 2]\n')

    def test_helper_returned_try_open_result_tracks_ref_ownership(self):
        path=self.directory/'input';path.write_text('text')
        prefix=program('using io','bool acquire(str filename,ref file out);','\tError error',
            '\treturn file.try_open(filename,ref out,ref error)')
        self.check_program(self.main('file f',f'if(acquire("{path}",ref f));','\tprint(file.read(f))',
            '\tfile.close(f)',prefix=prefix),'text\n')
        self.check_program(self.main('file f',f'if(acquire("{self.missing()}",ref f));','\tfile.close(f)',
            'else;','\tprint("missing")',prefix=prefix),'missing\n')

    def test_inverted_success_and_bool_copy(self):
        path=self.directory/'input';path.write_text('text')
        self.check_program(self.main('file f','Error e',f'bool first=file.try_open("{path}",ref f,ref e)',
            'bool second=first','if(not second);','\tprint("wrong")','else;','\tfile.close(f)'),'')

    def test_error_does_not_commit_assignment_or_evaluate_later_operands(self):
        prefix=program('using io','int fail(ref int calls);','\tcalls++','\tfile f',
            f'\ttry(f=file.open("{self.missing()}"));','\t\treturn 1','\tfinally;','\t\tfile.close(f)',
            'int later(ref int calls);','\tcalls+=10','\treturn 1')
        self.check_program(self.main('int calls=0','int n=42','try(n=fail(ref calls)+later(ref calls));',
            '\tprint("wrong")','case();','\tprint("{n} {calls}")',prefix=prefix),'42 1\n')

    def test_generic_try_and_inferred_returns(self):
        prefix=program('using io','choose(x);','\ttry;','\t\treturn x','\tcase();','\t\treturn x',
            '\tfinally;','\t\tprint("done")')
        self.check_program(self.main('print(choose(42))','print(choose("text"))',prefix=prefix),'done\n42\ndone\ntext\n')

    def test_returned_file_transfers_after_finally(self):
        path=self.directory/'input';path.write_text('text')
        prefix=program('using io','file acquire();','\tfile f',f'\ttry(f=file.open("{path}"));',
            '\t\treturn f','\tcase();','\t\tfile.close(f)','\t\tthrow;','\tfinally;','\t\tprint("finish")')
        self.check_program(self.main('file f','try(f=acquire());','\tprint(file.read(f))','case();',
            '\tprint("wrong")','finally;','\tfile.close(f)',prefix=prefix),'finish\ntext\n')

    def test_live_local_file_cannot_escape_body_scope_into_case(self):
        self.compile_source(self.main('try;','\tfile local=file.open("input")','\tfile.read(local)',
            '\tfile.close(local)','case();','\tprint("caught")'),error='close')

    def test_ref_output_cannot_transfer_ownership_on_error(self):
        prefix=program('using io','void acquire(ref file out);','\tout=file.open("input")',
            '\tfile.read(out)','\tfile.close(out)')
        self.compile_source(self.main('file f','try(acquire(ref f));','\tprint(0)','case();','\tprint(1)',
            'finally;','\tfile.close(f)',prefix=prefix),error='エラー経路')

    def test_constructor_completion_cannot_be_hidden_in_try(self):
        prefix=program('model Required;','\tint number','\tvoid init(int n);','\t\tthis.number=n',
            'model Item;','\tRequired child','\tvoid init();','\t\ttry;',
            '\t\t\tthis.child={n=1}','\t\tcase();','\t\t\tio.print("caught")')
        self.compile_source(self.main('Item value',prefix=prefix),error='未完了')

    @unittest.skipIf(os.environ.get('QYNO_FILE_TARGET')=='windows','Linux syscall injection')
    def test_read_failures_leave_output_unchanged_and_require_close(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        path=self.directory/'input';path.write_text('text\n')
        for op,body in [('read','str value=file.read(f)'),('readline','file.readline(f,ref line)'),
                ('read_bytes','unsigned int_8 data[]=file.read_bytes(f,10)')]:
            mode='rb' if op=='read_bytes' else 'r'
            source=self.main('file f','str line="keep"',f'try(f=file.open("{path}","{mode}")) as error;',
                '\t'+body,'case(ErrorCode.io_error);','\tprint("{error.operation} {line} {file.is_open(f)}")',
                'finally;','\tfile.close(f)')
            binary=self.compile_source(source)
            result=subprocess.run(['strace','-qq','-o',str(self.directory/'trace'),'--inject=read:error=EIO:when=1',str(binary)],capture_output=True,timeout=5)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual(result.stdout,f'{op} keep true\n'.encode())

    @unittest.skipIf(os.environ.get('QYNO_FILE_TARGET')=='windows','Linux syscall injection')
    def test_close_failure_is_caught_logically_closed_and_not_retried(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        path=self.directory/'input';path.write_text('text')
        binary=self.compile_source(self.main('file f',f'try(f=file.open("{path}"));','\tfile.close(f)',
            'case(ErrorCode.io_error);','\tprint(file.is_open(f))','finally;','\tfile.close(f)'))
        trace=self.directory/'trace'
        result=subprocess.run(['strace','-qq','-o',str(trace),'--inject=close:error=EIO:when=1',str(binary)],capture_output=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(result.stdout,b'false\n')
        self.assertEqual(sum(line.startswith('close(') for line in trace.read_text().splitlines()),1)

    @unittest.skipIf(os.environ.get('QYNO_FILE_TARGET')=='windows','Linux syscall injection')
    def test_error_classification_and_retry(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        for errno,code in [('EACCES','permission_denied'),('EMFILE','too_many_open_files'),('ENOSPC','no_space'),
                ('EROFS','read_only'),('ENAMETOOLONG','invalid_path'),('EBUSY','resource_busy'),('EIO','io_error')]:
            binary=self.compile_source(self.main('file f',f'try(f=file.open("{self.missing()}")) as e;',
                '\tprint("wrong")','case();','\tprint(e.code)','finally;','\tfile.close(f)'))
            result=subprocess.run(['strace','-qq','-o',str(self.directory/'trace'),f'--inject=openat:error={errno}:when=1',str(binary)],capture_output=True,timeout=5)
            self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(result.stdout,(code+'\n').encode())


    def test_match_inside_try_and_try_inside_match(self):
        prefix=program('using io','enum State;','\tzero','\tone')
        self.check_program(self.main('State state=one','try;','\tmatch(state);','\t\tcase zero;',
            '\t\t\tprint("wrong")','\t\tcase one;','\t\t\tprint("matched")','finally;',
            '\tprint("finally")',prefix=prefix),'matched\nfinally\n')

    def test_models_and_array_defaults_propagate_errors(self):
        prefix=program('using io','model Item;','\tint number','\tvoid init();','\t\tfile f',
            f'\t\ttry(f=file.open("{self.missing()}"));','\t\t\tthis.number=1','\t\tfinally;','\t\t\tfile.close(f)')
        for declaration in ['Item item','Item items[2]']:
            self.check_program(self.main('try;','\t'+declaration,'\tprint("wrong")',
                'case();','\tprint("caught")',prefix=prefix),'caught\n')

    def test_finally_must_close_later_files_even_if_first_close_fails(self):
        prefix=program('using io','void close_both(file a,file b);','\ttry;','\t\tfile.close(a)',
            '\tfinally;','\t\tfile.close(b)')
        path=self.directory/'input';path.write_text('text')
        self.check_program(self.main('file a','file b','try;',f'\ta=file.open("{path}")',
            f'\tb=file.open("{path}")','\tprint("open")','finally;','\tclose_both(a,b)',prefix=prefix),'open\n')

    def test_repeated_errors_keep_snapshots_and_gc_roots(self):
        self.check_program(self.main('Error saved','for(0 <= i < 200,i++);','\tfile f',
            f'\ttry(f=file.open("{self.missing()}")) as error;','\t\tprint("wrong")','\tcase();',
            '\t\tsaved=error','\tfinally;','\t\tfile.close(f)',
            'print("{saved.code} {saved.operation} {saved.message.length > 0}")'),'not_found open true\n')

    def test_nested_finally_allows_local_loop_but_rejects_return_from_nested_try(self):
        self.check_program(self.main('try;','\tprint("body")','finally;','\twhile(true);',
            '\t\tbreak','\tprint("done")'),'body\ndone\n')
        self.compile_source(self.main('try;','\tprint(0)','finally;','\ttry;','\t\treturn 1',
            '\tcase();','\t\tprint(0)'),error='finally')

    def test_try_open_handles_modes_array_ref_outputs_and_aliases(self):
        path=self.directory/'out'
        self.check_program(self.main('file handles[1]','Error errors[1]',
            f'if(file.try_open("{path}",ref handles[0],ref errors[0],"wb"));',
            '\tfile.write_bytes(handles[0],[0,255,128])','\tfile.close(handles[0])',
            'print(errors[0].code)'),'no_error\n')
        self.assertEqual(path.read_bytes(),bytes([0,255,128]))

    @unittest.skipIf(os.environ.get('QYNO_FILE_TARGET')=='windows','Linux syscall injection')
    def test_write_failure_is_caught_and_preserves_partial_file(self):
        import resource,signal
        path=self.directory/'out'
        binary=self.compile_source(self.main('file f',f'try(f=file.open("{path}","w")) as error;',
            '\tfile.write(f,"abcdefghij")','case();','\tprint(error.operation)','finally;','\tfile.close(f)'))
        def limit_output():
            signal.signal(signal.SIGXFSZ,signal.SIG_IGN)
            resource.setrlimit(resource.RLIMIT_FSIZE,(5,5))
        result=subprocess.run([str(binary)],capture_output=True,preexec_fn=limit_output,timeout=5)
        self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(result.stdout,b'write\n')
        self.assertEqual(path.read_bytes(),b'abcde')


    def test_same_error_types_cross_module_boundaries(self):
        (self.directory/'helper.qy').write_text(program('io.Error empty();','\tio.Error result','\treturn result',
            'void fail();','\tfile f','\ttry(f=io.file.open("qyno-missing-cross-module"));','\t\treturn',
            '\tfinally;','\t\tio.file.close(f)'))
        source=self.main('Error e=helper.empty()','print(e.code)','try(helper.fail()) as error;',
            '\tprint("wrong")','case();','\tprint(error.source.length > 0)',prefix='import helper\nusing io\n')
        self.check_program(source,'no_error\ntrue\n')

    def test_assembly_output_executes_same_handlers(self):
        source=self.main('file f',f'try(f=file.open("{self.missing()}")) as e;',
            '\tprint("wrong")','case();','\tprint(e.code)','finally;','\tfile.close(f)')
        path=self.directory/'assembly.qy';path.write_text(source);asm=path.with_suffix('.s');binary=path.with_suffix('')
        result=subprocess.run([str(self.compiler),str(path),'-S','--target','linux','-o',str(asm)],capture_output=True,timeout=10)
        self.assertEqual(result.returncode,0,result.stderr)
        result=subprocess.run(['gcc','-nostdlib','-no-pie',str(asm),'-o',str(binary)],capture_output=True,timeout=10)
        self.assertEqual(result.returncode,0,result.stderr)
        result=subprocess.run([str(binary)],capture_output=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(result.stdout,b'not_found\n')

    def test_open_destination_is_api_misuse_even_with_handler(self):
        path=self.directory/'input';path.write_text('text')
        self.check_program(self.main(f'file f=file.open("{path}")','file alias=f','Error e',
            'try;','\tfile.try_open("unused",ref f,ref e)','case();','\tprint("wrong")',
            'finally;','\tfile.close(f)','\tfile.close(alias)'),'',status=70,error='未オープン')

    @unittest.skipIf(os.environ.get('QYNO_FILE_TARGET')=='windows','Linux syscall injection')
    def test_partial_readline_does_not_commit_output(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        path=self.directory/'input';path.write_text('a'*5000+'\n')
        binary=self.compile_source(self.main('file f','str line="keep"',f'try(f=file.open("{path}"));',
            '\tfile.readline(f,ref line)','case();','\tprint(line)','\tfile.readline(f,ref line)',
            '\tprint(line.length)','finally;','\tfile.close(f)'))
        result=subprocess.run(['strace','-qq','-o',str(self.directory/'trace'),'--inject=read:error=EIO:when=2',str(binary)],capture_output=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(result.stdout,b'keep\n904\n')


if __name__=='__main__':run_suite()
