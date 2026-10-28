"""Binary file modes, exact octets, bounded reads and file safety rules."""
import os
from pathlib import Path
import shutil
import subprocess
import unittest
from support import QynoTestCase, program, run_suite
import file_test as file_support


class BinaryFileTests(QynoTestCase):
    run_file = file_support.FileTests.run_file
    check_error = file_support.FileTests.check_error

    def compile_source(self, source, *, error=None):
        if error is not None or os.environ.get('QYNO_FILE_TARGET') != 'windows':
            return super().compile_source(source, error=error)
        self.sequence += 1
        path = self.directory / f'case{self.sequence}.qy'
        path.write_text(source)
        binary = path.with_suffix('.exe')
        result = subprocess.run([str(self.compiler), str(path), '--target', 'windows', '-o', str(binary)],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, '')
        return binary

    def test_all_octets_round_trip_without_encoding_or_metadata(self):
        raw=bytes(range(256))+b'\x00\xff\xef\xbb\xbf\r\n\r\n\xc0\x80'
        self.run_file(['int main();','\tfile source=fs.open("in", "rb")',
            '\tunsigned int_8 data[]=fs.read_bytes(source, 65536)','\tfs.close(source)',
            '\tfile out=fs.open("out", "wb")','\tio.print(fs.write_bytes(out, data))','\tfs.close(out)',
            '\tio.print("{lib.length(data)} {lib.size(data)} {lib.size(data[255])}")'],
            f'{len(raw)}\n{len(raw)} {len(raw)*8} 8\n',files={'in':raw})
        self.assertEqual((self.directory/'out').read_bytes(),raw)

    def test_read_count_zero_does_not_consume_input(self):
        self.run_file(['int main();','\tfile f=fs.open("in", "rb")',
            '\tio.print(fs.read_bytes(f, 0))','\tio.print(fs.read_bytes(f, 1))',
            '\tio.print(fs.read_bytes(f, 0))','\tio.print(fs.read_bytes(f, 10))','\tfs.close(f)'],
            '[]\n[0]\n[]\n[128, 255]\n',files={'in':b'\x00\x80\xff'})

    def test_empty_repeated_eof_and_huge_count(self):
        for raw,expected in [(b'','[]\n[]\n'),(b'\xff','[255]\n[]\n')]:
            self.run_file(['int main();','\tfile f=fs.open("in", "rb")',
                '\tio.print(fs.read_bytes(f, 9223372036854775807))',
                '\tio.print(fs.read_bytes(f, 9223372036854775807))','\tfs.close(f)'],expected,files={'in':raw})

    def test_read_counts_preserve_buffered_position_and_aliases(self):
        self.run_file(['int main();','\tfile f=fs.open("in", "rb")','\tfile other=f',
            '\tio.print(fs.read_bytes(f, 1))','\tio.print(fs.read_bytes(other, 2))',
            '\tio.print(fs.read_bytes(f, 1))','\tio.print(fs.read_bytes(other, 100))',
            '\tfs.close(other)','\tio.print(fs.is_open(f))'],
            '[1]\n[2, 3]\n[4]\n[5]\nfalse\n',files={'in':bytes(range(1,6))})

    def test_buffer_boundaries_and_chunk_copy(self):
        for count in [1,4095,4096,4097,65536]:
            with self.subTest(count=count):
                raw=bytes(range(256))*35+b'\xff'
                self.run_file(['int main();','\tfile source=fs.open("in", "rb")','\tfile out=fs.open("out", "wb")',
                    '\tint_64 total','\twhile(true);',f'\t\tunsigned int_8 data[]=fs.read_bytes(source, {count})',
                    '\t\tif(data.length==0);','\t\t\tbreak','\t\ttotal+=fs.write_bytes(out, data)',
                    '\tfs.close(out)','\tfs.close(source)','\tio.print(total)'],f'{len(raw)}\n',files={'in':raw})
                self.assertEqual((self.directory/'out').read_bytes(),raw)

    def test_large_array_is_packed_in_finite_chunks(self):
        raw=bytes(range(256))*4096
        self.run_file(['int main();','\tfile source=fs.open("in", "rb")',
            '\tunsigned int_8 data[]=fs.read_bytes(source, 1048576)','\tfs.close(source)',
            '\tfile out=fs.open("out", "wb")','\tio.print(fs.write_bytes(out,data))','\tfs.close(out)'],
            '1048576\n',files={'in':raw})
        self.assertEqual((self.directory/'out').read_bytes(),raw)

    def test_returned_arrays_are_independent_and_survive_close(self):
        self.run_file(['int main();','\tfile f=fs.open("in", "rb")',
            '\tunsigned int_8 first[]=fs.read_bytes(f,2)','\tunsigned int_8 second[]=fs.read_bytes(f,2)',
            '\tfs.close(f)','\tfirst[0]=99','\tfirst.add(5)','\tio.print(first)','\tio.print(second)'],
            '[99, 2, 5]\n[3, 4]\n',files={'in':b'\x01\x02\x03\x04'})

    def test_fixed_array_initializer_still_pads(self):
        self.run_file(['int main();','\tfile f=fs.open("in", "rb")',
            '\tunsigned int_8 data[3]=fs.read_bytes(f,3)','\tfs.close(f)','\tio.print(data)'],
            '[255, 0, 0]\n',files={'in':b'\xff'})

    def test_eof_can_read_later_append(self):
        self.run_file(['int main();','\tfile f=fs.open("in", "rb")','\tio.print(fs.read_bytes(f,10))',
            '\tio.print(fs.read_bytes(f,10))','\tfile a=fs.open("in", "ab")',
            '\tfs.write_bytes(a,[255])','\tfs.close(a)','\tio.print(fs.read_bytes(f,10))','\tfs.close(f)'],
            '[1]\n[]\n[255]\n',files={'in':b'\x01'})

    def test_wb_truncates_at_open_and_creates_missing(self):
        self.run_file(['int main();','\tfile f=fs.open("out", "wb")','\tfs.close(f)'],files={'out':b'old'})
        self.assertEqual((self.directory/'out').read_bytes(),b'')
        self.run_file(['int main();','\tfile f=fs.open("new", "wb")','\tfs.close(f)'])
        self.assertEqual((self.directory/'new').read_bytes(),b'')

    def test_ab_uses_current_end_for_each_handle_and_creates(self):
        self.run_file(['int main();','\tfile a=fs.open("out", "ab")','\tfile b=fs.open("out", "ab")',
            '\tfs.write_bytes(a,[1])','\tfs.write_bytes(b,[2])','\tfs.write_bytes(a,[3])','\tfs.close(a)','\tfs.close(b)'])
        self.assertEqual((self.directory/'out').read_bytes(),b'\x01\x02\x03')

    def test_empty_literals_const_readonly_child_arrays_and_results(self):
        prefix=program('using io.file as fs','const unsigned int_8 HEADER[]=[0,255]',
            'unsigned int_8 make()[];','\treturn [128,129]',
            'void write(file f, ref readonly unsigned int_8 data[]);','\tfs.write_bytes(f,data)')
        self.run_file(['int main();','\tfile out=fs.open("out", "wb")',
            '\tio.print(fs.write_bytes(out,[]))','\tio.print(fs.write_bytes(out,HEADER))',
            '\tunsigned int_8 rows[][]=[[4,5],[6]]','\tfs.write_bytes(out,rows[0])',
            '\twrite(out,ref rows[1])','\tfs.write_bytes(out,make())','\tfs.close(out)','\tio.print(rows)'],
            '0\n2\n[[4, 5], [6]]\n',prefix=prefix)
        self.assertEqual((self.directory/'out').read_bytes(),bytes([0,255,4,5,6,128,129]))

    def test_qualified_selective_using_and_namespace_aliases(self):
        for prefix,open_,read,close in [('', 'io.file.open','io.file.read_bytes','io.file.close'),
                ('using io.file\n','open','read_bytes','close'),
                ('using io.file.open,io.file.read_bytes,io.file.close\n','open','read_bytes','close'),
                ('using io\n','file.open','file.read_bytes','file.close')]:
            self.run_file(['int main();',f'\tfile f={open_}("in","rb")',f'\tio.print({read}(f,2))',f'\t{close}(f)'],
                '[255]\n',prefix=prefix,files={'in':b'\xff'})
        self.check_error(['int main();','\tfile f','\tread_bytes(f,1)'],'using io.file.read_bytes')

    def test_argument_evaluation_order_and_single_evaluation(self):
        prefix=program('using io.file as fs','file first(file f,ref int n);','\tn=n*10+1','\treturn f',
            'int count(ref int n);','\tn=n*10+2','\treturn 1',
            'unsigned int_8 data(ref int n)[];','\tn=n*10+3','\treturn [255]')
        self.run_file(['int main();','\tint n','\tfile f=fs.open("in","rb")',
            '\tio.print(fs.read_bytes(first(f,ref n),count(ref n)))','\tfs.close(f)','\tio.print(n)',
            '\tn=0','\tfile out=fs.open("out","wb")','\tfs.write_bytes(first(out,ref n),data(ref n))',
            '\tfs.close(out)','\tio.print(n)'], '[7]\n12\n13\n',files={'in':b'\x07'},prefix=prefix)
        self.assertEqual((self.directory/'out').read_bytes(),b'\xff')

    def test_later_argument_can_close_shared_handle_before_validation(self):
        prefix=program('using io.file as fs','int close_count(file f);','\tfs.close(f)','\treturn 0',
            'unsigned int_8 close_data(file f)[];','\tfs.close(f)','\treturn []')
        self.run_file(['int main();','\tfile f=fs.open("in","rb")','\tfs.read_bytes(f,close_count(f))'],
            files={'in':b'\xff'},prefix=prefix,status=70,error='閉じられ')
        self.run_file(['int main();','\tfile f=fs.open("out","wb")','\tfs.write_bytes(f,close_data(f))'],
            prefix=prefix,status=70,error='閉じられ')
        self.assertEqual((self.directory/'out').read_bytes(),b'')

    def test_mode_matrix_including_empty_operations(self):
        for mode in ['r','w','a','rb','wb','ab']:
            operations=[('fs.read(f)',mode=='r'),('fs.readline(f,ref line)',mode=='r'),
                ('fs.write(f,"")',mode in ['w','a']),('fs.read_bytes(f,0)',mode=='rb'),
                ('fs.write_bytes(f,[])',mode in ['wb','ab'])]
            for operation,valid in operations:
                with self.subTest(mode=mode,operation=operation):
                    self.run_file(['int main();','\tstr line',f'\tfile f=fs.open("in","{mode}")',
                        '\t'+operation,'\tfs.close(f)'],files={'in':b''},status=0 if valid else 70,
                        error=None if valid else '用ではありません')

    def test_dynamic_modes_and_invalid_spellings(self):
        for mode in ['rb','wb','ab']:
            self.run_file(['int main();','\tstr mode=io.input()','\tfile f=fs.open("in",mode)','\tfs.close(f)'],
                stdin=mode+'\n',files={'in':b''})
        for mode in ['br','r+b','rb+','RB','rb ','','rbb']:
            self.check_error(['int main();',f'\tfile f=fs.open("in","{mode}")','\tfs.close(f)'],'モード')
            self.run_file(['int main();','\tstr mode=io.input()','\tfile f=fs.open("in",mode)','\tfs.close(f)'],
                stdin=mode+'\n',files={'in':b''},status=70,error='モード')

    def test_negative_count_constant_and_dynamic(self):
        self.check_error(['int main();','\tfile f','\tfs.read_bytes(f,-1)'],'非負')
        self.run_file(['int main();','\tfile f=fs.open("in","rb")','\tint count=-1',
            '\tfs.read_bytes(f,count)','\tfs.close(f)'],files={'in':b'\xff'},status=70,error='非負')

    def test_count_conversions_are_checked(self):
        for decl in ['int_8 n=1','unsigned int n=1','int_64 n=1','int_128 n=1','unsigned int_64 n=1']:
            count='int_64(n)' if '128' in decl or 'unsigned int_64' in decl else 'n'
            self.run_file(['int main();','\tfile f=fs.open("in","rb")','\t'+decl,
                f'\tio.print(fs.read_bytes(f,{count}))','\tfs.close(f)'],'[255]\n',files={'in':b'\xff'})
        for decl,count in [('double n=1.0','n'),('bool n=true','n'),('unsigned int_64 n=18446744073709551615','n'),('int_128 n=9223372036854775808','n')]:
            self.check_error(['int main();','\tfile f','\t'+decl,f'\tfs.read_bytes(f,{count})'],'count')

    def test_data_type_rank_and_literal_range_are_checked(self):
        for decl in ['int data[]=[1]','int_8 data[]=[1]','unsigned int_16 data[]=[1]',
                     'unsigned int_8 data[][]=[[1]]','str data="a"','char data[]=["a"]']:
            self.check_error(['int main();','\tfile f','\t'+decl,'\tfs.write_bytes(f,data)'],'unsigned int_8')
        for data in ['[-1]','[256]','[true]','["a"]','[[1]]']:
            self.check_error(['int main();','\tfile f',f'\tfs.write_bytes(f,{data})'],'エラー')

    def test_argument_counts_and_ref_misuse(self):
        for call in ['fs.read_bytes()','fs.read_bytes(f)','fs.read_bytes(f,1,2)',
                     'fs.write_bytes()','fs.write_bytes(f)','fs.write_bytes(f,[],[])']:
            self.check_error(['int main();','\tfile f','\t'+call],'引数')
        for call in ['fs.read_bytes(ref f,1)','fs.read_bytes(f,ref n)',
                     'fs.write_bytes(ref f,data)','fs.write_bytes(f,ref data)']:
            self.check_error(['int main();','\tfile f','\tint n','\tunsigned int_8 data[]','\t'+call],'ref')

    def test_unopened_closed_and_aliased_handles_even_for_empty_operations(self):
        for op in ['fs.read_bytes(f,0)','fs.write_bytes(f,[])']:
            self.run_file(['int main();','\tfile f','\t'+op],status=70,error='未オープン')
        for mode,op in [('rb','fs.read_bytes(alias,0)'),('wb','fs.write_bytes(alias,[])')]:
            self.run_file(['int main();',f'\tfile f=fs.open("in","{mode}")','\tfile alias=f','\tfs.close(f)','\t'+op],
                files={'in':b''},status=70,error='閉じられ')

    def test_explicit_close_is_required_on_all_paths(self):
        for lines in [['\tfs.read_bytes(f,1)'],['\tif(io.input(prompt="") == "yes");','\t\tfs.close(f)'],
                      ['\tunsigned int_8 data[]=fs.read_bytes(f,1)','\treturn 0']]:
            self.check_error(['int main();','\tfile f=fs.open("in","rb")',*lines],'close')
        prefix=program('using io.file as fs','unsigned int_8 consume(file f)[];',
            '\tunsigned int_8 data[]=fs.read_bytes(f,1)','\tfs.close(f)','\treturn data')
        self.run_file(['int main();','\tfile f=fs.open("in","rb")','\tio.print(consume(f))'],
            '[255]\n',files={'in':b'\xff'},prefix=prefix)

    def test_readonly_file_is_rejected(self):
        for call in ['fs.read_bytes(f,1)','fs.write_bytes(f,[])']:
            self.compile_source(program('using io.file as fs','void inspect(ref readonly file f);','\t'+call,
                'int main();','\treturn 0'),error='readonly')
        self.compile_source(program('model Reader;','\tprivate file handle',
            '\treadonly unsigned int_8 read()[] = io.file.read_bytes(this.handle,1)',
            'int main();','\treturn 0'),error='readonly')

    def test_handles_in_models_and_arrays_keep_shared_mode_and_position(self):
        prefix=program('using io.file as fs','model Holder;','\tfile handle')
        self.run_file(['int main();','\tHolder h','\th.handle=fs.open("in","rb")',
            '\tfile handles[]=[h.handle]','\tio.print(fs.read_bytes(handles[0],1))',
            '\tio.print(fs.read_bytes(h.handle,1))','\tfs.close(h.handle)'],
            '[1]\n[2]\n',prefix=prefix,files={'in':b'\x01\x02'})

    def test_unicode_paths_and_nul_path_validation(self):
        self.run_file(['int main();','\tfile f=fs.open("資料/日本😀.bin","rb")',
            '\tio.print(fs.read_bytes(f,1))','\tfs.close(f)'],'[255]\n',files={'資料/日本😀.bin':b'\xff'})
        self.run_file(['int main();','\tfile f=fs.open("bad\\0path","wb")','\tfs.close(f)'],status=70,error='NUL')

    def test_constant_io_is_rejected(self):
        self.compile_source(program('const unsigned int_8 DATA[]=io.file.read_bytes(io.file.open("in","rb"),1)',
            'int main();','\treturn 0'),error='const')

    @unittest.skipIf(os.environ.get('QYNO_FILE_TARGET') == 'windows', 'Linux syscall injection')
    def test_interrupted_read_and_write_retry(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        for syscall in ['read','write']:
            tracer=['strace','-qq','-o',str(self.directory/'trace'),'--inject='+syscall+':error=EINTR:when=1']
            self.run_file(['int main();','\tfile f=fs.open("in","rb")','\tunsigned int_8 data[]=fs.read_bytes(f,100)',
                '\tfs.close(f)','\tf=fs.open("out","wb")','\tfs.write_bytes(f,data)','\tfs.close(f)'],
                files={'in':b'\x00\xff\x80'},tracer=tracer)
            self.assertEqual((self.directory/'out').read_bytes(),b'\x00\xff\x80')

    @unittest.skipIf(os.environ.get('QYNO_FILE_TARGET') == 'windows', 'Linux syscall injection')
    def test_zero_write_progress_and_os_read_error(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        for syscall,injection,operation,mode,error in [('write','retval=0','fs.write_bytes(f,[1])','wb','進みません'),
                ('read','error=EIO','fs.read_bytes(f,1)','rb','OS error')]:
            tracer=['strace','-qq','-o',str(self.directory/'trace'),'--inject='+syscall+':'+injection+':when=1']
            self.run_file(['int main();',f'\tfile f=fs.open("in","{mode}")','\t'+operation,'\tfs.close(f)'],
                files={'in':b'\xff'},status=74,error=error,tracer=tracer)

    @unittest.skipIf(os.environ.get('QYNO_FILE_TARGET') == 'windows', 'Linux RLIMIT_FSIZE')
    def test_partial_write_then_failure_keeps_written_prefix(self):
        import resource,signal
        def limit():
            signal.signal(signal.SIGXFSZ,signal.SIG_IGN)
            resource.setrlimit(resource.RLIMIT_FSIZE,(3,3))
        self.run_file(['int main();','\tfile f=fs.open("out","wb")',
            '\tfs.write_bytes(f,[1,2,3,4,5])','\tfs.close(f)'],status=74,error='OS error',preexec_fn=limit)
        self.assertEqual((self.directory/'out').read_bytes(),b'\x01\x02\x03')

    def test_loop_close_analysis_keeps_exit_and_backedge_checks(self):
        for body in [
                ['\twhile(true);','\t\tif(false);','\t\t\tbreak'],
                ['\twhile(true);','\t\twhile(true);','\t\t\tbreak'],
                ['\twhile(true);','\t\tif(io.input(prompt="") == "yes");','\t\t\tbreak']]:
            self.check_error(['int main();','\tfile f=fs.open("in","rb")',*body],'close')
        self.check_error(['int main();','\twhile(true);','\t\tfile f=fs.open("in","rb")',
            '\t\tif(io.input(prompt="") == "yes");','\t\t\tbreak','\t\tio.print(1)'],'close')

    @unittest.skipIf(os.environ.get('QYNO_FILE_TARGET') == 'windows', 'Linux syscall injection')
    def test_short_read_continues_until_requested_count(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        tracer=['strace','-qq','-o',str(self.directory/'trace'),
                '--inject=read:retval=1:poke_exit=@arg2=61:when=1']
        self.run_file(['int main();','\tfile f=fs.open("in","rb")',
            '\tio.print(fs.read_bytes(f,3))','\tio.print(fs.read_bytes(f,1))','\tfs.close(f)'],
            '[97, 98, 99]\n[]\n',files={'in':b'bc'},tracer=tracer)

    @unittest.skipIf(os.environ.get('QYNO_FILE_TARGET') == 'windows', 'Linux syscall injection')
    def test_short_write_retries_remaining_elements(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        tracer=['strace','-qq','-o',str(self.directory/'trace'),'--inject=write:retval=2:when=1']
        self.run_file(['int main();','\tfile f=fs.open("out","wb")',
            '\tio.print(fs.write_bytes(f,[1,2,3,4,5,6]))','\tfs.close(f)'],'6\n',tracer=tracer)
        # The injected first success replaces the syscall; retry the remaining bytes.
        self.assertEqual((self.directory/'out').read_bytes(),bytes([3,4,5,6]))

    @unittest.skipIf(os.environ.get('QYNO_FILE_TARGET') == 'windows', 'Linux syscall injection')
    def test_empty_operations_do_not_call_read_or_write(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        tracer=['strace','-qq','-o',str(self.directory/'trace'),
                '--inject=read:error=EIO:when=1','--inject=write:error=EIO:when=1']
        self.run_file(['int main();','\tfile f=fs.open("out","wb")','\tfs.write_bytes(f,[])',
            '\tfs.close(f)','\tf=fs.open("out","rb")','\tfs.read_bytes(f,0)','\tfs.close(f)'],tracer=tracer)
        self.assertFalse(any(line.startswith(('read(', 'write(')) for line in (self.directory/'trace').read_text().splitlines()))

    @unittest.skipIf(os.environ.get('QYNO_FILE_TARGET') == 'windows', 'Linux syscall injection')
    def test_partial_read_followed_by_failure_is_not_success(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        tracer=['strace','-qq','-o',str(self.directory/'trace'),'--inject=read:error=EIO:when=2']
        self.run_file(['int main();','\tfile f=fs.open("in","rb")','\tfs.read_bytes(f,8192)','\tfs.close(f)'],
            files={'in':b'x'*8192},status=74,error='OS error',tracer=tracer)
        reads=[line for line in (self.directory/'trace').read_text().splitlines() if line.startswith('read(')]
        self.assertGreaterEqual(len(reads),2)
        self.assertRegex(reads[0],r'= [1-9][0-9]*$');self.assertIn('EIO',reads[1])

    @unittest.skipIf(os.environ.get('QYNO_FILE_TARGET') == 'windows', 'Linux allocation fault injection')
    def test_read_growth_allocation_failure_is_diagnosed(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        lines=['int main();','\tfile f=fs.open("in","rb")','\tfs.read_bytes(f,1)','\tfs.close(f)']
        trace=self.directory/'trace'
        self.run_file(lines,files={'in':b'\xff'},tracer=['strace','-qq','-o',str(trace)])
        allocations=0;after_read=False;failure_at=None
        for line in trace.read_text().splitlines():
            if line.startswith('read('):after_read=True
            if line.startswith('mmap('):
                allocations+=1
                if after_read:failure_at=allocations;break
        self.assertIsNotNone(failure_at)
        binary=self.compile_source('using io.file as fs\n'+program(*lines))
        result=subprocess.run(['strace','-qq','-o',str(trace),
            f'--inject=mmap:error=ENOMEM:when={failure_at}+',str(binary)],cwd=self.directory,
            capture_output=True,timeout=10)
        self.assertEqual(result.returncode,70,result.stderr.decode())
        self.assertEqual(result.stdout,b'')
        self.assertIn('メモリ',result.stderr.decode())

    def test_windows_and_assembly_outputs(self):
        path=self.directory/'native.qy'
        path.write_text(program('int main();','\tfile f=io.file.open("in","rb")',
            '\tunsigned int_8 data[]=io.file.read_bytes(f,1)','\tio.file.close(f)','\treturn 0'))
        for target in ['windows','linux']:
            output=self.directory/(target+'.s')
            r=subprocess.run([str(self.compiler),str(path),'--target',target,'-S','-o',str(output)],capture_output=True)
            self.assertEqual(r.returncode,0,r.stderr.decode())
            self.assertIn('file.bytes',output.read_text())


if __name__ == '__main__':
    run_suite()
