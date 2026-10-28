"""io.file text semantics and explicit-close dataflow on Linux and Windows/Wine."""
import os
from pathlib import Path
import subprocess
import shutil
import unittest

from support import QynoTestCase, program, run_suite

PREFIX = 'using io.file as fs\n'


class FileTests(QynoTestCase):
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
        return binary

    def run_file(self, lines, expected='', *, files=None, stdin=b'', status=0, error=None, prefix=PREFIX,
                 preexec_fn=None, tracer=None):
        for name, data in (files or {}).items():
            path = self.directory / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data.encode() if isinstance(data, str) else data)
        binary = self.compile_source(prefix + program(*lines))
        runner = [os.environ['QYNO_FILE_WINE']] if os.environ.get('QYNO_FILE_TARGET') == 'windows' else []
        result = subprocess.run((tracer or []) + runner + [str(binary)], cwd=self.directory,
                                input=stdin.encode() if isinstance(stdin, str) else stdin,
                                capture_output=True, timeout=15, preexec_fn=preexec_fn)
        self.assertEqual(result.returncode, status, result.stderr.decode(errors='replace'))
        self.assertEqual(result.stdout, expected.encode())
        if error:
            self.assertIn(error, result.stderr.decode(errors='replace'))
            self.assertIn('.qy:', result.stderr.decode(errors='replace'))
        else:
            self.assertEqual(result.stderr, b'')
        return result

    def check_error(self, lines, message='close'):
        return self.compile_source(PREFIX + program(*lines), error=message)

    def test_read_preserves_bytes(self):
        self.run_file(['int main();', '\tfile f = fs.open("in")', '\tio.print(fs.read(f))', '\tfs.close(f)'],
                      '\ufeff日😀\0\r\n\n', files={'in': '\ufeff日😀\0\r\n'})

    def test_empty_and_repeated_eof(self):
        self.run_file(['int main();', '\tfile f = fs.open("in")', '\tstr line = "keep"',
                       '\tio.print(fs.read(f))', '\tio.print(fs.readline(f, ref line))', '\tio.print(line)',
                       '\tio.print(fs.read(f))', '\tfs.close(f)'], '\nfalse\nkeep\n\n', files={'in': ''})

    def test_line_endings_and_final_unterminated_line(self):
        for data, expected in [('a\nb\n', '[a]\n[b]\nkeep\n'), ('a\r\nb\rc', '[a]\n[b]\n[c]\nkeep\n'),
                               ('\n\r\n\r', '[]\n[]\n[]\nkeep\n'), ('abc', '[abc]\nkeep\n')]:
            with self.subTest(data=data):
                self.run_file(['int main();', '\tfile f = fs.open("in")', '\tstr line',
                               '\twhile(fs.readline(f, ref line));', '\t\tio.print("[{line}]")',
                               '\tline = "keep"', '\tfs.readline(f, ref line)', '\tio.print(line)', '\tfs.close(f)'],
                              expected, files={'in': data})

    def test_buffer_boundaries(self):
        for count in (4094, 4095, 4096, 8191):
            data = 'x' * count + '\r\n日本😀\rnext'
            with self.subTest(count=count):
                self.run_file(['int main();', '\tfile f = fs.open("in")', '\tstr line',
                               '\tfs.readline(f, ref line)', '\tio.print(line.length)',
                               '\tfs.readline(f, ref line)', '\tio.print(line)', '\tio.print(fs.read(f))', '\tfs.close(f)'],
                              f'{count}\n日本😀\nnext\n', files={'in': data})

    def test_unicode_separators_are_content(self):
        self.run_file(['int main();', '\tfile f = fs.open("in")', '\tstr line',
                       '\tfs.readline(f, ref line)', '\tio.print(line)', '\tfs.close(f)'],
                      'a\x85b\u2028c\u2029d\n', files={'in': 'a\x85b\u2028c\u2029d\n'})

    def test_read_and_readline_share_position(self):
        self.run_file(['int main();', '\tfile f = fs.open("in")', '\tstr line',
                       '\tfs.readline(f, ref line)', '\tio.print(line)', '\tio.print(fs.read(f))',
                       '\tio.print(fs.readline(f, ref line))', '\tfs.close(f)'],
                      'first\nrest\r\ntail\nfalse\n', files={'in': 'first\nrest\r\ntail'})

    def test_eof_can_see_later_append(self):
        self.run_file(['int main();', '\tfile r = fs.open("in")', '\tio.print(fs.read(r))',
                       '\tfile a = fs.open("in", "a")', '\tfs.write(a, "new")', '\tfs.close(a)',
                       '\tio.print(fs.read(r))', '\tfs.close(r)'], 'old\nnew\n', files={'in': 'old'})

    def test_write_and_append_exact_utf8(self):
        self.run_file(['int main();', '\tfile f = fs.open("out", "w")', '\tio.print(fs.write(f, "日本😀\\0\\r\\n"))',
                       '\tfs.close(f)', '\tf = fs.open("out", "a")', '\tfs.write(f, "tail")', '\tfs.close(f)'],
                      '13\n', files={'out': 'discarded'})
        self.assertEqual((self.directory / 'out').read_bytes(), '日本😀\0\r\ntail'.encode())

    def test_open_w_truncates_immediately(self):
        self.run_file(['int main();', '\tfile f = fs.open("out", "w")', '\tfs.close(f)'], files={'out': 'old'})
        self.assertEqual((self.directory / 'out').read_bytes(), b'')

    def test_append_creates_missing_and_each_write_uses_end(self):
        self.run_file(['int main();', '\tfile a = fs.open("out", "a")', '\tfile b = fs.open("out", "a")',
                       '\tfs.write(a, "a")', '\tfs.write(b, "b")', '\tfs.write(a, "c")',
                       '\tfs.close(a)', '\tfs.close(b)'])
        self.assertEqual((self.directory / 'out').read_bytes(), b'abc')

    def test_write_scalar_format_and_interpolation(self):
        self.run_file(['int main();', '\tfile f = fs.open("out", "w")', '\tint n = -42', '\tfloat x = 3.14',
                       '\tdouble z = 0.0', '\tchar c = "😀"',
                       '\tfs.write(f, "{n} {x} {1.0 / 3} {true} {c} {1.0 / z} {z / z}")',
                       '\tfs.write(f, false)', '\tfs.write(f, 12)', '\tfs.close(f)'])
        self.assertEqual((self.directory / 'out').read_text(), '-42 3.14 0.333333 true 😀 inf nanfalse12')

    def test_write_side_effects_are_not_redirected(self):
        self.run_file(['int next();', '\tio.print("side")', '\treturn 42', 'int main();',
                       '\tfile f = fs.open("out", "w")', '\tfs.write(f, "n={next()}")', '\tfs.close(f)'], 'side\n')
        self.assertEqual((self.directory / 'out').read_bytes(), b'n=42')

    def test_empty_write_and_byte_counts(self):
        self.run_file(['int main();', '\tfile f = fs.open("out", "w")', '\tio.print(fs.write(f, ""))',
                       '\tio.print(fs.write(f, "😀"))', '\tfs.close(f)'], '0\n4\n')

    def test_aliases_share_position_and_close(self):
        self.run_file(['void done(file f);', '\tfs.close(f)', 'int main();', '\tfile a = fs.open("in")',
                       '\tfile b = a', '\tstr line', '\tfs.readline(a, ref line)', '\tio.print(fs.read(b))',
                       '\tdone(b)', '\tio.print(fs.is_open(a))', '\tfs.close(a)'], 'second\nfalse\n', files={'in': 'first\nsecond'})

    def test_distinct_handles_have_distinct_positions(self):
        self.run_file(['int main();', '\tfile a = fs.open("in")', '\tfile b = fs.open("in")', '\tstr line',
                       '\tfs.readline(a, ref line)', '\tio.print(fs.read(b))', '\tio.print(fs.read(a))',
                       '\tio.print(a == b)', '\tfs.close(a)', '\tfs.close(b)'], 'first\nlast\nlast\nfalse\n', files={'in': 'first\nlast'})

    def test_default_closed_and_double_close(self):
        self.run_file(['int main();', '\tfile f', '\tfile g', '\tio.print(f == g)', '\tio.print(fs.is_open(f))',
                       '\tfs.close(f)', '\tfs.close(f)'], 'true\nfalse\n')

    def test_file_return_ref_rebind_and_local_alias(self):
        self.run_file(['file make();', '\treturn fs.open("in")',
                       'void reset(ref file f);', '\tfs.close(f)', '\tf = make()',
                       'int main();', '\tfile f = make()', '\treset(ref f)', '\tref file alias = f.addr',
                       '\tio.print(fs.read(f))', '\tfs.close(alias)'], 'data\n', files={'in': 'data'})

    def test_file_arrays_copy_containers_and_share_handles(self):
        self.run_file(['file pair()[];', '\treturn [fs.open("a"), fs.open("b")]', 'int main();',
                       '\tfile files[] = pair()', '\tfile copies[] = files', '\tio.print(fs.read(copies[0]))',
                       '\tfs.close(files[0])', '\tfs.close(files[1])', '\tio.print(fs.is_open(copies[0]))'],
                      'A\nfalse\n', files={'a': 'A', 'b': 'B'})

    def test_default_file_array_growth_and_ref_element(self):
        self.run_file(['void make(ref file f);', '\tf = fs.open("in")', 'int main();', '\tfile files[]',
                       '\tfiles.add()', '\tmake(ref files[0])', '\tio.print(fs.read(files[0]))', '\tfs.close(files[0])'],
                      'data\n', files={'in': 'data'})
        self.run_file(['const file handles[2] = []',
                       'const bool equal = handles[0] == handles[1]', 'int main();',
                       '\tio.print(fs.is_open(handles[0]))', '\tio.print(equal)',
                       '\tfs.close(handles[1])'], 'false\ntrue\n')

    def test_readline_into_array_element(self):
        self.run_file(['int main();', '\tfile f = fs.open("in")', '\tstr lines[][] = [["old"]]',
                       '\tfs.readline(f, ref lines[0][0])', '\tio.print(lines[0][0])', '\tfs.close(f)'],
                      '日本語\n', files={'in': '日本語\n'})

    def test_unicode_paths_and_exists(self):
        self.run_file(['int main();', '\tio.print(fs.exists("日本😀.txt"))', '\tfile f = fs.open("日本😀.txt")',
                       '\tio.print(fs.read(f))', '\tfs.close(f)', '\tio.print(fs.exists("missing"))',
                       '\tio.print(fs.exists("."))'], 'true\ntext\nfalse\nfalse\n', files={'日本😀.txt': 'text'})

    def test_using_forms(self):
        for prefix, name in [('using io\n', 'file'), ('using io.file\n', ''),
                             ('import io.file\n', 'io.file'), ('using io as console\n', 'console.file'),
                             ('using io.file as fs\nusing fs as disk\n', 'disk')]:
            with self.subTest(prefix=prefix):
                dot = name + '.' if name else ''
                self.run_file(['int main();', f'\tfile f = {dot}open("in")', f'\tio.print({dot}read(f))', f'\t{dot}close(f)'],
                              'data\n', files={'in': 'data'}, prefix=prefix)

    def test_relative_paths_use_process_cwd(self):
        source = self.directory / 'nested' / 'main.qy'
        source.parent.mkdir()
        source.write_text(PREFIX + program('int main();', '\tfile f = fs.open("in")', '\tio.print(fs.read(f))', '\tfs.close(f)'))
        (self.directory / 'in').write_text('cwd')
        (source.parent / 'in').write_text('entry')
        target = os.environ.get('QYNO_FILE_TARGET', 'linux')
        binary = source.with_suffix('.exe' if target == 'windows' else '')
        result = subprocess.run([str(self.compiler), str(source), '--target', target, '-o', str(binary)], capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        command = [os.environ['QYNO_FILE_WINE'], str(binary)] if target == 'windows' else [str(binary)]
        result = subprocess.run(command, cwd=self.directory, capture_output=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, b'cwd\n')

    def test_invalid_utf8_read(self):
        for data in (b'\xc0\x80', b'\xed\xa0\x80', b'\xf4\x90\x80\x80', b'\xe6\x97', b'\xff'):
            with self.subTest(data=data):
                self.run_file(['int main();', '\tfile f = fs.open("in")', '\tfs.read(f)', '\tfs.close(f)'],
                              files={'in': data}, status=65, error='UTF-8')

    def test_invalid_utf8_write_does_not_modify_file(self):
        self.run_file(['int main();', '\tstr text = io.input()', '\tfile f = fs.open("out", "a")',
                       '\tfs.write(f, text)', '\tfs.close(f)'], files={'out': 'keep'}, stdin=b'\xff\n', status=65, error='UTF-8')
        self.assertEqual((self.directory / 'out').read_bytes(), b'keep')

    def test_invalid_and_missing_paths(self):
        for path, code, diagnostic in [('missing', 74, 'OS error'), ('.', 74, 'ファイル'), ('bad\\0path', 70, 'NUL')]:
            with self.subTest(path=path):
                self.run_file(['int main();', f'\tfile f = fs.open("{path}")', '\tfs.close(f)'], status=code, error=diagnostic)

    def test_wrong_mode_closed_handle_and_dynamic_mode(self):
        for mode, operation in [('w', 'fs.read(f)'), ('a', 'fs.readline(f, ref line)'), ('r', 'fs.write(f, "")')]:
            with self.subTest(mode=mode):
                self.run_file(['int main();', '\tstr line', f'\tfile f = fs.open("in", "{mode}")',
                               '\t' + operation, '\tfs.close(f)'], files={'in': 'text'}, status=70, error='用ではありません')
        self.run_file(['int main();', '\tstr mode = io.input()', '\tfile f = fs.open("in", mode)', '\tfs.close(f)'],
                      stdin='r+\n', status=70, error='モード')
        self.run_file(['int main();', '\tfile f = fs.open("in")', '\tfile old = f', '\tfs.close(f)',
                       '\tf = fs.open("in")', '\tfs.read(old)', '\tfs.close(f)'], files={'in': 'x'}, status=70, error='閉じられています')

    def test_live_handle_survives_gc(self):
        self.run_file(['int main();', '\tfile f = fs.open("in")', '\tstr values[]', '\tfor(0 <= i < 500, i++);',
                       '\t\tvalues.add("日本語😀")', '\tio.print(fs.read(f))', '\tfs.close(f)'], 'alive\n', files={'in': 'alive'})

    def test_loop_opens_and_closes_each_iteration(self):
        self.run_file(['int main();', '\tfor(0 <= i < 100, i++);', '\t\tfile f = fs.open("in")',
                       '\t\tfs.close(f)'], files={'in': ''})

    def test_static_argument_and_output_errors(self):
        for statement, error in [('fs.open(1)', 'パス'), ('fs.open("in", "br")', 'モード'),
                                 ('fs.read(1)', 'file'), ('fs.readline(f, line)', 'ref str'),
                                 ('fs.readline(f, ref n)', '型'), ('fs.write(f, [1])', 'write'),
                                 ('fs.write(f, "{items}")', 'write'), ('io.print(f)', 'file'),
                                 ('io.print(files)', 'file'), ('fs.close()', '引数')]:
            with self.subTest(statement=statement):
                self.check_error(['int main();', '\tfile f', '\tfile files[]', '\tint n', '\tstr line', '\tint items[] = [1]', '\t' + statement], error)

    def test_unclosed_and_discarded_open_are_compile_errors(self):
        for body in [ ['\tfile f = fs.open("in")'], ['\tfs.open("in")'],
                      ['\tfile f = fs.open("in")', '\treturn 0', '\tfs.close(f)'],
                      ['\tfile f = fs.open("in")', '\tf = fs.open("in")', '\tfs.close(f)'] ]:
            with self.subTest(body=body): self.check_error(['int main();'] + body)

    def test_branch_and_early_return_require_close(self):
        self.check_error(['int main();', '\tfile f = fs.open("in")', '\tbool b = io.input()', '\tif(b);', '\t\tfs.close(f)'])
        self.check_error(['int main();', '\tfile f = fs.open("in")', '\tbool b = io.input()', '\tif(b);', '\t\treturn 0', '\tfs.close(f)'])
        self.run_file(['int main();', '\tbool b = io.input()', '\tfile f', '\tif(b);', '\t\tf = fs.open("in")',
                       '\telse;', '\t\tf = fs.open("in")', '\tfs.close(f)'], files={'in': ''}, stdin='true\n')

    def test_conditional_helper_cannot_hide_unclosed_handle(self):
        self.check_error(['void maybe(file f, bool b);', '\tif(b);', '\t\tfs.close(f)', 'int main();',
                          '\tfile f = fs.open("in")', '\tbool b = io.input()', '\tmaybe(f, b)'])

    def test_returned_handle_transfers_close_obligation(self):
        self.check_error(['file make();', '\treturn fs.open("in")', 'int main();', '\tfile f = make()'])
        self.check_error(['file make();', '\tfile lost = fs.open("in")', '\treturn fs.open("in")', 'int main();', '\tfile f = make()', '\tfs.close(f)'])

    def test_ref_output_transfers_close_obligation(self):
        self.check_error(['void make(ref file f);', '\tf = fs.open("in")', 'int main();', '\tfile f', '\tmake(ref f)'])
        self.check_error(['void ensure(ref file f);', '\tif(not fs.is_open(f));', '\t\tf = fs.open("in")',
                          'int main();', '\tfile f', '\tensure(ref f)'])

    def test_loop_break_and_continue_closure(self):
        self.check_error(['int main();', '\tfor(0 <= i < 2, i++);', '\t\tfile f = fs.open("in")', '\t\tcontinue', '\t\tfs.close(f)'])
        self.check_error(['int main();', '\tfor(0 <= i < 2, i++);', '\t\tfile f = fs.open("in")', '\t\tbreak'])
        self.check_error(['int main();', '\tfile f = fs.open("in")', '\twhile(true);', '\t\tio.print(1)'])

    def test_short_circuit_close_is_not_definite(self):
        self.check_error(['bool done(file f);', '\tfs.close(f)', '\treturn true', 'int main();',
                          '\tfile f = fs.open("in")', '\tbool b = io.input()', '\tbool result = b and done(f)'])
        self.check_error(['bool done(file f);', '\tfs.close(f)', '\treturn true', 'int main();',
                          '\tfile f = fs.open("in")', '\tint n = io.input()', '\tbool result = n < 0 < int(done(f))'], '数値')

    def test_dynamic_array_close_is_conservative(self):
        self.check_error(['int main();', '\tfile files[] = [fs.open("in")]', '\tint i = io.input()', '\tfs.close(files[i])'])

    def test_resource_calls_are_not_constant_expressions(self):
        self.check_error(['const bool present = fs.exists("in")', 'int main();', '\treturn 0'], 'const')

    def test_comparison_chain_cannot_hide_conditional_close(self):
        self.check_error(['bool done(file f);', '\tfs.close(f)', '\treturn true', 'int main();',
                          '\tfile f = fs.open("in")', '\tbool result = false == true == done(f)'])

    def test_file_effects_inside_input_indices_are_analyzed(self):
        self.check_error(['file make();', '\treturn fs.open("in")',
                          'int index(file f) = 0', 'int main();',
                          '\tint values[] = [1]', '\tio.input(values[index(make())])'])

    def test_factory_branches_transfer_ownership(self):
        self.run_file(['file make(bool b);', '\tif(b);', '\t\treturn fs.open("a")',
                       '\treturn fs.open("b")', 'int main();',
                       '\tfile f = make(true)', '\tio.print(fs.read(f))', '\tfs.close(f)'],
                      'A\n', files={'a': 'A', 'b': 'B'})

    def test_file_functions_inside_imports(self):
        (self.directory / 'disk.qy').write_text(PREFIX + program(
            'file make();', '\treturn fs.open("in")',
            'void done(file f);', '\tfs.close(f)',
        ))
        self.run_file(['int main();', '\tfile f = disk.make()',
                       '\tio.print(fs.read(f))', '\tdisk.done(f)'],
                      'data\n', files={'in': 'data'}, prefix=PREFIX + 'import disk\n')

    def test_missing_close_is_reported_inside_import(self):
        (self.directory / 'disk.qy').write_text(PREFIX + program(
            'void leak();', '\tfile f = fs.open("in")',
        ))
        source = self.directory / 'main.qy'
        source.write_text(program('import disk', 'int main();', '\tdisk.leak()'))
        result = subprocess.run([str(self.compiler), str(source)], capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 1)
        self.assertIn('close', result.stderr.decode())
        self.assertIn('disk.qy:3:', result.stderr.decode())

    def test_guarded_close_helper_is_definite(self):
        self.run_file(['void done(file f);', '\tif(fs.is_open(f));', '\t\tfs.close(f)',
                       'int main();', '\tfile f = fs.open("in")', '\tdone(f)'],
                      files={'in': ''})

    def test_growing_file_array_retains_close_obligation(self):
        self.run_file(['int main();', '\tfile handles[]',
                       '\thandles.add(fs.open("in"))', '\tfs.close(handles[0])'],
                      files={'in': ''})
        self.check_error(['int main();', '\tfile handles[]',
                          '\thandles.add(fs.open("in"))', '\thandles.clear()'])

    def test_loop_condition_cannot_overwrite_unclosed_handle(self):
        self.check_error(['bool next(ref file f);', '\tf = fs.open("in")', '\treturn true',
                          'int main();', '\tfile f', '\twhile(next(ref f));',
                          '\t\tio.print("body")', '\tfs.close(f)'])

    def test_ref_rebinding_preserves_captured_alias_address(self):
        self.check_error(['int main();', '\tfile f = fs.open("a")', '\tfile g = fs.open("b")',
                          '\tref file p = f.addr', '\tref file q = p.addr',
                          '\tp.addr = g.addr', '\tfs.close(q)', '\tfs.close(f)'])
        self.run_file(['int main();', '\tfile f = fs.open("a")', '\tfile g = fs.open("b")',
                       '\tref file p = f.addr', '\tref file q = p.addr',
                       '\tp.addr = g.addr', '\tfs.close(q)', '\tfs.close(p)',
                       '\tio.print(fs.is_open(f))', '\tio.print(fs.is_open(g))'],
                      'false\nfalse\n', files={'a': '', 'b': ''})

    @unittest.skipIf(os.environ.get('QYNO_FILE_TARGET') == 'windows', 'Linux mode bits and links')
    def test_permissions_and_symlink_targets(self):
        (self.directory / 'target').write_text('linked')
        (self.directory / 'link').symlink_to('target')
        (self.directory / 'broken').symlink_to('missing')
        self.run_file(['int main();', '\tio.print(fs.exists("link"))',
                       '\tio.print(fs.exists("broken"))', '\tfile f = fs.open("link")',
                       '\tio.print(fs.read(f))', '\tfs.close(f)'], 'true\nfalse\nlinked\n')
        (self.directory / 'target').chmod(0)
        self.addCleanup(lambda: (self.directory / 'target').chmod(0o600))
        self.run_file(['int main();', '\tfile f = fs.open("target")', '\tfs.close(f)'],
                      status=74, error='OS error')

    @unittest.skipIf(os.environ.get('QYNO_FILE_TARGET') == 'windows', 'Linux RLIMIT_FSIZE')
    def test_partial_write_then_failure_reports_error_and_keeps_prefix(self):
        import resource
        import signal
        def limit_output():
            signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
            resource.setrlimit(resource.RLIMIT_FSIZE, (5, 5))
        self.run_file(['int main();', '\tfile f = fs.open("out", "w")',
                       '\tfs.write(f, "abcdefghij")', '\tfs.close(f)'],
                      status=74, error='OS error', preexec_fn=limit_output)
        self.assertEqual((self.directory / 'out').read_bytes(), b'abcde')

    @unittest.skipIf(os.environ.get('QYNO_FILE_TARGET') == 'windows', 'Linux syscall injection')
    def test_interrupted_syscalls_are_retried(self):
        if not shutil.which('strace'):
            self.skipTest('strace not installed')
        for syscall in ('read', 'write', 'openat', 'fstat', 'newfstatat'):
            with self.subTest(syscall=syscall):
                tracer = ['strace', '-qq', '-o', str(self.directory / 'trace'),
                          '--inject=' + syscall + ':error=EINTR:when=1']
                self.run_file(['int main();', '\tfile f = fs.open("in")',
                               '\tstr text = fs.read(f)', '\tfs.close(f)',
                               '\tf = fs.open("out", "w")', '\tfs.write(f, text)',
                               '\tfs.close(f)', '\tbool exists = fs.exists("out")'],
                              files={'in': 'retry'}, tracer=tracer)
                self.assertEqual((self.directory / 'out').read_bytes(), b'retry')

    @unittest.skipIf(os.environ.get('QYNO_FILE_TARGET') == 'windows', 'Linux close fault injection')
    def test_close_failure_is_not_retried(self):
        if not shutil.which('strace'):
            self.skipTest('strace not installed')
        tracer = ['strace', '-qq', '-o', str(self.directory / 'trace'),
                  '--inject=close:error=EIO:when=1']
        self.run_file(['int main();', '\tfile f = fs.open("in")', '\tfs.close(f)'],
                      files={'in': ''}, status=74, error='close', tracer=tracer)
        trace = (self.directory / 'trace').read_text()
        self.assertEqual(sum(line.startswith('close(') for line in trace.splitlines()), 1)


if __name__ == '__main__':
    run_suite()
