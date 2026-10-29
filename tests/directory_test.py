"""Working-directory state, path strings and recoverable OS failures."""
import os, shutil, subprocess, unittest
from pathlib import Path
from support import program, run_suite
from time_support import PlatformCase, WINDOWS, inject_windows_api_results

class DirectoryTests(PlatformCase):
    def display(self,path):
        return ('Z:' if WINDOWS else '')+str(path)

    def run_program(self,source,expected,*,cwd=None,status=0,error=None,inject=None):
        binary=self.compile_source(source)
        if inject:inject_windows_api_results(binary,inject)
        command=[os.environ['QYNO_FILE_WINDOWS_RUNNER'],str(binary)] if WINDOWS else [str(binary)]
        before=os.getcwd()
        r=subprocess.run(command,cwd=cwd or self.directory,capture_output=True,timeout=30 if WINDOWS else 5)
        self.assertEqual(os.getcwd(),before)
        self.assertEqual(r.returncode,status,r.stderr.decode(errors='replace'))
        self.assertEqual(r.stdout,expected.encode())
        if error:self.assertIn(error,r.stderr.decode(errors='replace'))
        else:self.assertEqual(r.stderr,b'')
        return r

    def test_current_is_absolute_snapshot(self):
        (self.directory/'child').mkdir()
        self.run_program(self.main('str before=io.dir.current()','io.print(io.path.is_absolute(before))',
            'io.dir.change("child")','io.print(before)','io.print(io.dir.current())'),
            f'true\n{self.display(self.directory)}\n{self.display(self.directory/"child")}\n')

    def test_absolute_relative_parent_and_dot_moves(self):
        (self.directory/'child').mkdir()
        self.run_program(self.main('io.dir.change("./child")','io.dir.change("..")',
            'io.print(io.dir.current())',f'io.dir.change("{self.directory}/child")',
            'io.dir.change(".")','io.print(io.dir.current())'),
            self.display(self.directory)+'\n'+self.display(self.directory/'child')+'\n')

    def test_unicode_directory_round_trip(self):
        child=self.directory/'日本語_😺_é';child.mkdir()
        self.run_program(self.main('io.dir.change("日本語_😺_é")','io.print(io.dir.current())'),self.display(child)+'\n')

    def test_long_current_directory_grows_buffer(self):
        child=self.directory
        for i in range(2 if WINDOWS else 9):child=child/(str(i)+'x'*80);child.mkdir()
        self.run_program(self.main('io.print(io.dir.current())'),self.display(child)+'\n',cwd=child)

    def test_namespaces_alias_selective_and_import(self):
        for prefix,call in [('','io.dir.current'),('using io\n','dir.current'),
                ('using io.dir\n','current'),('using io.dir.current\n','current'),
                ('using io.dir as directory\n','directory.current')]:
            self.run_program(self.main(f'io.print({call}())',prefix=prefix),self.display(self.directory)+'\n')
        self.compile_source(self.main('current()',prefix='import io.dir\n'),error='using io.dir')
        self.compile_source(self.main('change(".")',prefix='using io\n'),error='using io.dir')

    def test_absolute_nonexistent_and_empty_paths(self):
        self.run_program(self.main('io.print(io.path.absolute("new/output.bin"))',
            'io.print(io.path.absolute(""))','io.print(io.path.absolute("./"))'),
            f'{self.display(self.directory)}/new/output.bin\n{self.display(self.directory)}\n{self.display(self.directory)}\n')

    def test_absolute_normalizes_but_preserves_parent_segments(self):
        self.run_program(self.main('io.print(io.path.absolute("./missing//../out/./"))'),
            self.display(self.directory)+'/missing/../out\n')

    def test_absolute_uses_cwd_at_call_time(self):
        (self.directory/'child').mkdir()
        self.run_program(self.main('str before=io.path.absolute("input")','io.dir.change("child")',
            'io.print(before)','io.print(io.path.absolute("input"))'),
            self.display(self.directory)+'/input\n'+self.display(self.directory)+'/child/input\n')

    def test_failed_change_retains_cwd_and_reports_missing_path(self):
        self.run_program(self.main('try(io.dir.change("missing")) as e;','\tio.print("wrong")',
            'case(io.ErrorCode.not_found);','\tio.print("{e.operation} {e.path} {e.os_code > 0} {e.line > 0}")',
            'io.print(io.dir.current())'),'change missing true true\n'+self.display(self.directory)+'\n')

    def test_not_directory_error_is_distinct_and_stable(self):
        (self.directory/'plain').write_text('value')
        self.run_program(self.main('io.print(int(io.ErrorCode.not_directory))',
            'try(io.dir.change("plain")) as e;','\tio.print("wrong")','case(io.ErrorCode.not_directory);',
            '\tio.print("{e.code} {e.path}")','io.print(io.dir.current())'),
            '15\nnot_directory plain\n'+self.display(self.directory)+'\n')

    def test_finally_restores_original_directory(self):
        (self.directory/'child').mkdir()
        self.run_program(self.main('str before=io.dir.current()','try(io.dir.change("child"));',
            '\tio.dir.change("missing")','case(io.ErrorCode.not_found);','\tio.print("caught")',
            'finally;','\tio.dir.change(before)','io.print(io.dir.current())'),
            'caught\n'+self.display(self.directory)+'\n')

    def test_preopened_file_stays_bound_and_new_relative_open_uses_new_cwd(self):
        (self.directory/'input').write_text('old');(self.directory/'child').mkdir()
        (self.directory/'child'/'input').write_text('new')
        self.run_program(self.main('file old','file new','try(old=io.file.open("input"));',
            '\tio.dir.change("child")','\tnew=io.file.open("input")','\tio.print(io.file.read(old))',
            '\tio.print(io.file.read(new))','finally;','\tio.file.close(old)','\tio.file.close(new)'), 'old\nnew\n')

    def test_char_path_and_void_result(self):
        (self.directory/'a').mkdir()
        self.run_program(self.main('char name="a"','void result=io.dir.change(name)',
            'io.print(result)','io.print(io.path.basename(io.dir.current()))'),'()\na\n')

    def test_dynamic_invalid_paths_fail_before_changing_state(self):
        for path in ['','bad\\0path']:
            self.run_program(self.main(f'str target="{path}"','io.dir.change(target)'),'',status=70,error='パス')
        self.run_program(self.main('str target="bad\\0path"','io.print(io.path.absolute(target))'),'',status=70,error='パス')

    def test_argument_types_counts_and_ref_are_checked(self):
        for expr,needle in [('io.dir.current(1)','引数'),('io.dir.change()','引数'),
                ('io.dir.change(1)','str'),('io.path.absolute()','引数'),('io.path.absolute(true)','str'),
                ('io.dir.change("")','パス'),('io.path.absolute("a\\0b")','パス')]:
            self.compile_source(self.main(expr),error=needle)
        self.compile_source(self.main('str path="."','io.dir.change(ref path)'),error='ref')

    def test_effectful_paths_are_not_const_expressions(self):
        for expr in ['io.dir.current()','io.path.absolute("/already/absolute")']:
            self.compile_source(program('const str PATH='+expr,'int main();','\treturn 0'),error='const')

    def test_old_lexical_path_functions_remain_const(self):
        self.run_program(program('const str PATH=io.path.join("a", "b")','int main();','\tio.print(PATH)','\treturn 0'),'a/b\n')

    def test_exception_effects_require_callee_file_cleanup(self):
        source=program('void unsafe();','\tfile f=io.file.open("input")','\tio.dir.change("missing")',
            '\tio.file.close(f)','int main();','\ttry(unsafe());','\t\tio.print(0)','\tcase(io.ErrorCode.not_directory);','\t\tio.print("caught")','\treturn 0')
        self.compile_source(source,error='close')

    def test_module_call_propagates_and_closes_on_error(self):
        (self.directory/'helper.qy').write_text(program('public void run();',
            '\tfile source','\ttry(source=io.file.open("input"));','\t\tio.dir.change("plain")',
            '\tfinally;','\t\tio.file.close(source)'))
        (self.directory/'input').write_text('data');(self.directory/'plain').write_text('file')
        self.run_program(self.main('try(helper.run()) as e;','\tio.print("wrong")',
            'case(io.ErrorCode.not_directory);','\tio.print("{e.code} {e.operation}")',prefix='import helper\n'),
            'not_directory change\n')

    def test_windows_rejects_ambiguous_and_device_paths_at_compile_time(self):
        for operation in ['io.dir.change','io.path.absolute']:
            for value in ['C:relative','/relative','\\\\relative','//?/C:/work','//./C:/work']:
                path=self.directory/'invalid.qy';path.write_text(self.main(f'{operation}("{value}")'))
                result=subprocess.run([str(self.compiler),str(path),'--target','windows','-o',str(self.directory/'invalid.exe')],capture_output=True)
                self.assertEqual(result.returncode,1,result.stderr);self.assertIn('パス'.encode(),result.stderr)
                if WINDOWS:
                    self.run_program(self.main(f'str path="{value}"',f'{operation}(path)'),'',status=70,error='パス')

    def test_windows_absolute_and_unc_do_not_query_cwd(self):
        if not WINDOWS:self.skipTest('Windows API injection')
        self.run_program(self.main('io.print(io.path.absolute("C:/missing//./out"))',
            'io.print(io.path.absolute("//server/share/missing"))'),'C:/missing/out\n//server/share/missing\n',
            inject={'GetCurrentDirectoryW':0,'GetLastError':3})

    def test_windows_errors_and_relative_absolute_context(self):
        if not WINDOWS:self.skipTest('Windows API injection')
        for expression,operation,path in [('io.dir.current()','current',''),('io.path.absolute("relative")','absolute','relative')]:
            self.run_program(self.main('try('+expression+') as e;','\tio.print("wrong")','case(io.ErrorCode.not_found);',
                '\tio.print("{e.operation}|{e.path}|{e.os_code}")'),f'{operation}|{path}|3\n',
                inject={'GetCurrentDirectoryW':0,'GetLastError':3})
        for code,name in [(267,'not_directory'),(5,'permission_denied'),(50,'unsupported_operation')]:
            self.run_program(self.main('try(io.dir.change(".")) as e;','\tio.print("wrong")',
                f'case(io.ErrorCode.{name});','\tio.print("{e.code} {e.operation}")'),f'{name} change\n',
                inject={'SetCurrentDirectoryW':0,'GetLastError':code})

    def test_windows_invalid_utf16_is_not_silently_replaced(self):
        if not WINDOWS:self.skipTest('Windows API injection')
        self.run_program(self.main('io.dir.current()'),'',status=65,error='Unicode',
            inject={'WideCharToMultiByte':0,'GetLastError':1113})

    @unittest.skipIf(WINDOWS,'Linux syscall injection')
    def test_getcwd_and_chdir_retry_eintr_and_keep_call_context(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        for syscall,body,expected in [('getcwd','io.print(io.dir.current())',str(self.directory)+'\n'),
                ('chdir','io.dir.change(".")\nio.print(io.dir.current())',str(self.directory)+'\n')]:
            binary=self.compile_source(self.main(*body.splitlines()))
            r=subprocess.run(['strace','-qq','-o',str(self.directory/'trace'),f'--inject={syscall}:error=EINTR:when=1',str(binary)],cwd=self.directory,capture_output=True,timeout=5)
            self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,expected.encode())
        binary=self.compile_source(self.main('try(io.path.absolute("relative")) as e;','\tio.print("wrong")',
            'case(io.ErrorCode.not_found);','\tio.print("{e.operation} {e.path}")'))
        r=subprocess.run(['strace','-qq','-o',str(self.directory/'trace'),'--inject=getcwd:error=ENOENT:when=1',str(binary)],capture_output=True,timeout=5)
        self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,b'absolute relative\n')
        binary=self.compile_source(self.main('try(io.dir.current()) as e;','\tio.print("wrong")',
            'case(io.ErrorCode.system_error);','\tio.print(e.operation)'))
        r=subprocess.run(['strace','-qq','-o',str(self.directory/'trace'),'--inject=getcwd:poke_exit=@arg1=2e:when=1',str(binary)],capture_output=True,timeout=5)
        self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,b'current\n')

    @unittest.skipIf(WINDOWS,'Linux symlink cwd semantics')
    def test_symlink_parent_is_not_lexically_collapsed(self):
        (self.directory/'actual').mkdir();(self.directory/'actual'/'child').mkdir()
        (self.directory/'link').symlink_to(self.directory/'actual'/'child',target_is_directory=True)
        self.run_program(self.main('str path=io.path.absolute("link/..")','io.print(path)',
            'io.dir.change(path)','io.print(io.dir.current())'),str(self.directory)+'/link/..\n'+str(self.directory/'actual')+'\n')

    @unittest.skipIf(WINDOWS,'Linux invalid filename encoding')
    def test_invalid_utf8_cwd_fails_with_data_error(self):
        child=os.fsencode(self.directory)+b'/invalid-\xff';os.mkdir(child)
        self.run_program(self.main('io.dir.current()'),'',cwd=child,status=65,error='UTF-8')

    @unittest.skipIf(WINDOWS,'Linux permissions')
    def test_permission_denied_preserves_directory(self):
        if os.geteuid()==0:self.skipTest('root bypasses directory permissions')
        child=self.directory/'private';child.mkdir();child.chmod(0)
        self.addCleanup(child.chmod,0o700)
        self.run_program(self.main('try(io.dir.change("private"));','\tio.print("wrong")',
            'case(io.ErrorCode.permission_denied);','\tio.print(io.dir.current())'),str(self.directory)+'\n')

    def test_assembly_matches_direct_executable(self):
        path=self.directory/'source.qy';path.write_text(self.main('io.dir.change(".")','io.print(io.path.absolute("output"))'))
        asm=self.directory/'source.s';binary=self.directory/'assembled'
        r=subprocess.run([str(self.compiler),str(path),'-S','--target','linux','-o',str(asm)],capture_output=True)
        self.assertEqual(r.returncode,0,r.stderr)
        r=subprocess.run(['gcc','-nostdlib','-no-pie',str(asm),'-o',str(binary)],capture_output=True)
        self.assertEqual(r.returncode,0,r.stderr)
        r=subprocess.run([str(binary)],cwd=self.directory,capture_output=True)
        self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,(str(self.directory)+'/output\n').encode())

if __name__=='__main__':run_suite()
