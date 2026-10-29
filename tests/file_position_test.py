"""Logical positions, bounded binary seeks and metadata on Linux and Windows."""
from pathlib import Path
import os, subprocess, shutil, unittest
from support import program, run_suite
from time_support import PlatformCase, WINDOWS

class PositionTests(PlatformCase):
    def file(self,name='input',data=bytes(range(256))*20):
        path=self.directory/name;path.write_bytes(data);return str(path)

    def read(self,*body,mode='rb',data=bytes(range(256))*20):
        path=self.file(data=data)
        return self.main('file f',f'try(f=io.file.open("{path}","{mode}"));',*('\t'+line for line in body),
            'finally;','\tio.file.close(f)')

    def test_size_and_tell_do_not_consume_buffer(self):
        self.check_program(self.read('io.print(io.file.size(f))','io.print(io.file.read_bytes(f,1))',
            'io.print(io.file.tell(f))','io.print(io.file.size(f))','io.print(io.file.read_bytes(f,2))',
            'io.print(io.file.tell(f))'),'5120\n[0]\n1\n5120\n[1, 2]\n3\n')

    def test_current_uses_logical_not_os_position(self):
        self.check_program(self.read('io.file.read_bytes(f,1)','io.print(io.file.seek(f,1,io.file.SeekOrigin.current))',
            'io.print(io.file.read_bytes(f,1))'),'2\n[2]\n')

    def test_seek_begin_and_end(self):
        self.check_program(self.read('io.print(io.file.seek(f,10))','io.print(io.file.read_bytes(f,2))',
            'io.print(io.file.seek(f,-2,io.file.SeekOrigin.end))','io.print(io.file.read_bytes(f,3))',
            'io.print(io.file.tell(f))'),'10\n[10, 11]\n5118\n[254, 255]\n5120\n')

    def test_buffer_boundary_rewind_and_alias(self):
        self.check_program(self.read('file alias=f','io.file.read_bytes(f,4095)',
            'io.print(io.file.tell(alias))','io.file.seek(alias,-1,io.file.SeekOrigin.current)',
            'io.print(io.file.read_bytes(f,4))','io.file.seek(f,0)','io.print(io.file.read_bytes(alias,1))'),
            '4095\n[254, 255, 0, 1]\n[0]\n')

    def test_empty_and_eof_positions(self):
        self.check_program(self.read('io.print(io.file.size(f))','io.print(io.file.tell(f))',
            'io.print(io.file.seek(f,0,io.file.SeekOrigin.end))','io.print(io.file.read_bytes(f,1))',data=b''),'0\n0\n0\n[]\n')

    def test_utf8_readline_counts_consumed_crlf_bytes(self):
        self.check_program(self.read('str line','io.file.readline(f,ref line)','io.print(line)',
            'io.print(io.file.tell(f))','io.file.readline(f,ref line)','io.print(io.file.tell(f))',
            'io.file.readline(f,ref line)','io.print(line)','io.print(io.file.tell(f))',mode='r',data='あ\r\nB\rC'.encode()),
            'あ\n5\n7\nC\n8\n')

    def test_readonly_metadata(self):
        prefix=program('int_64 inspect(ref readonly file source)=io.file.tell(source)+io.file.size(source)')
        self.check_program(prefix+self.read('io.print(inspect(ref f))'),'5120\n')
        self.compile_source(program('void wrong(ref readonly file f);','\tio.file.seek(f,0)','int main();','\treturn 0'),error='readonly')

    def test_file_bits_and_bytes_remain_different(self):
        self.check_program(self.read('unsigned int_8 data[]=io.file.read_bytes(f,4)',
            'io.print("{lib.size(data)} {io.file.size(f)}")'),'32 5120\n')
        self.compile_source(self.main('file f','io.print(lib.size(f))'),error='lib.size')

    def test_text_seek_rejected(self):
        self.check_program(self.read('io.file.seek(f,0)',mode='r'),'',status=70,error='バイナリ')

    def test_negative_constant_begin_and_invalid_types(self):
        for call,error in [('io.file.seek(f,-1)','非負'),('io.file.seek(f,1.5)','int_64'),
                ('io.file.seek(f,true)','int_64'),('io.file.size()','引数'),('io.file.tell(1)','file')]:
            self.compile_source(self.main('file f',call),error=error)

    def test_dynamic_invalid_offsets_are_not_catchable(self):
        for offset in ['-1','5121','9223372036854775807']:
            self.check_program(self.read(f'int_64 offset={offset}','io.file.seek(f,offset)'),'',status=70,error='seek')
        self.check_program(self.read('io.file.seek(f,1)','int_64 n=9223372036854775807',
            'io.file.seek(f,n,io.file.SeekOrigin.current)'),'',status=70,error='seek')

    def test_wb_rewrites_already_written_header(self):
        path=self.directory/'out'
        self.check_program(self.main('file f',f'try(f=io.file.open("{path}","wb"));',
            '\tio.file.write_bytes(f,[1,2,3,4])','\tio.file.seek(f,0)','\tio.file.write_bytes(f,[9,8])',
            '\tio.print("{io.file.size(f)} {io.file.tell(f)}")','finally;','\tio.file.close(f)'),'4 2\n')
        self.assertEqual(path.read_bytes(),bytes([9,8,3,4]))

    def test_append_initial_position_and_seek_does_not_redirect_write(self):
        path=self.file(data=b'abc')
        self.check_program(self.main('file f',f'try(f=io.file.open("{path}","ab"));',
            '\tio.print(io.file.tell(f))','\tio.file.seek(f,0)','\tio.file.write_bytes(f,[100])',
            '\tio.print("{io.file.tell(f)} {io.file.size(f)}")','finally;','\tio.file.close(f)'),'3\n4 4\n')
        self.assertEqual(Path(path).read_bytes(),b'abcd')

    def test_tell_is_not_size_after_other_handle_appends(self):
        path=self.file(data=b'abc')
        self.check_program(self.main('file a','file b',f'try(a=io.file.open("{path}","ab"));',
            f'\ttry(b=io.file.open("{path}","ab"));','\t\tio.file.write_bytes(b,[100])',
            '\tfinally;','\t\tio.file.close(b)','\tio.print("{io.file.tell(a)} {io.file.size(a)}")',
            'finally;','\tio.file.close(a)'),'3 4\n')

    def test_modified_at_uses_unix_seconds_and_does_not_consume(self):
        path=self.file();os.utime(path,ns=(1793233800125000000,1793233800125000000))
        self.check_program(self.main('file f',f'try(f=io.file.open("{path}","rb"));','\tio.file.read_bytes(f,1)',
            '\tquadruple stamp=io.file.modified_at(f)','\tio.print(time.isoformat(stamp))',
            '\tio.print(io.file.tell(f))','\tio.print(io.file.read_bytes(f,1))','finally;','\tio.file.close(f)'),
            '2026-10-29T00:30:00.125000000Z\n1\n[1]\n')

    def test_seek_clears_stale_read_ahead(self):
        path=self.file(data=b'abc')
        self.check_program(self.main('file f','file writer',f'try(f=io.file.open("{path}","rb"));',
            '\tio.file.read_bytes(f,1)',f'\ttry(writer=io.file.open("{path}","wb"));',
            '\t\tio.file.write_bytes(writer,[120,121,122])','\tfinally;','\t\tio.file.close(writer)',
            '\tio.file.seek(f,0,io.file.SeekOrigin.current)','\tio.print(io.file.read_bytes(f,1))','finally;',
            '\tio.file.close(f)'),'[121]\n')

    def test_using_and_enum_expected_type(self):
        prefix=program('using io.file as fs','using io.file.SeekOrigin')
        path=self.file(data=b'abc')
        self.check_program(prefix+self.main('file f',f'try(f=fs.open("{path}","rb"));',
            '\tSeekOrigin origin=end','\tio.print(fs.seek(f,-1,origin))','finally;','\tfs.close(f)'),'2\n')

    def test_closed_and_unopened_metadata_is_misuse(self):
        for function in ['size','tell','modified_at']:
            self.check_program(self.main('file f',f'io.file.{function}(f)'),'',status=70,error='未オープン')

    @unittest.skipIf(WINDOWS,'Linux syscall injection')
    def test_failed_seek_keeps_buffer_and_position(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        source=self.read('io.file.read_bytes(f,1)','try(io.file.seek(f,2)) as e;',
            '\tio.print("wrong")','case(io.ErrorCode.io_error);','\tio.print(e.operation)',
            'io.print(io.file.tell(f))','io.print(io.file.read_bytes(f,1))')
        binary=self.compile_source(source)
        # tell used to compute seek(current) is absent for begin. First raw seek is the move.
        r=subprocess.run(['strace','-qq','-o',str(self.directory/'trace'),'--inject=lseek:error=EIO:when=1',str(binary)],capture_output=True,timeout=5)
        self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,b'seek\n1\n[1]\n')

    def test_large_sparse_file_uses_int64_without_truncation(self):
        path=self.directory/'large'
        with path.open('wb') as f:f.truncate(4294967303)
        self.check_program(self.main('file f',f'try(f=io.file.open("{path}","rb"));',
            '\tio.print(io.file.size(f))','\tio.print(io.file.seek(f,-1,io.file.SeekOrigin.end))',
            '\tio.print(io.file.read_bytes(f,1))','finally;','\tio.file.close(f)'),
            '4294967303\n4294967302\n[0]\n')

    def test_mtime_before_unix_epoch_preserves_fraction(self):
        path=self.file(data=b'')
        os.utime(path,ns=(-125000000,-125000000))
        self.check_program(self.main('file f',f'try(f=io.file.open("{path}","rb"));',
            '\tio.print(time.isoformat(io.file.modified_at(f)))','finally;','\tio.file.close(f)'),
            '1969-12-31T23:59:59.875000000Z\n')

    @unittest.skipIf(WINDOWS,'Linux syscall injection')
    def test_seek_interrupted_syscall_is_retried(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        binary=self.compile_source(self.read('io.print(io.file.seek(f,2))','io.print(io.file.read_bytes(f,1))'))
        r=subprocess.run(['strace','-qq','-o',str(self.directory/'trace'),'--inject=lseek:error=EINTR:when=1',str(binary)],capture_output=True,timeout=5)
        self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,b'2\n[2]\n')

    def test_metadata_failure_has_source_context_and_new_range_code(self):
        if WINDOWS:
            from time_support import inject_windows_api_results
            for os_code,code in [(1,'unsupported_operation'),(50,'unsupported_operation'),
                                 (120,'unsupported_operation'),(534,'out_of_range'),(29,'io_error')]:
                source=self.read('io.file.read_bytes(f,1)','try(io.file.seek(f,2)) as e;',
                    '\tio.print("wrong")',f'case(io.ErrorCode.{code});',
                    '\tio.print("{e.operation} {e.os_code}")','io.print(io.file.read_bytes(f,1))')
                binary=self.compile_source(source)
                inject_windows_api_results(binary,{'SetFilePointerEx':0,'GetLastError':os_code})
                result=subprocess.run([os.environ['QYNO_FILE_WINDOWS_RUNNER'],str(binary)],capture_output=True,timeout=30)
                self.assertEqual(result.returncode,0,result.stderr)
                self.assertEqual(result.stdout,f'seek {os_code}\n[1]\n'.encode())
            return
        if not shutil.which('strace'):self.skipTest('strace not installed')
        for errno,code in [('EIO','io_error'),('EOVERFLOW','out_of_range')]:
            source=self.read('io.file.read_bytes(f,1)','try(io.file.size(f)) as e;',
                '\tio.print("wrong")',f'case(io.ErrorCode.{code});','\tio.print(e.operation)',
                'io.print(io.file.tell(f))','io.print(io.file.read_bytes(f,1))')
            binary=self.compile_source(source)
            r=subprocess.run(['strace','-qq','-o',str(self.directory/'trace'),f'--inject=fstat:error={errno}:when=2',str(binary)],capture_output=True,timeout=5)
            self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,b'size\n1\n[1]\n')


if __name__=='__main__':run_suite()
