"""Exercise the generated Linux line editor on a real pseudo-terminal."""

import errno
import os
import select
import subprocess
import sys
import time
import unittest

if sys.platform == "linux":
    import pty
    import termios

from support import QynoTestCase, program, run_suite


@unittest.skipUnless(sys.platform == "linux", "Linux terminal runtime tests")
class LineEditTests(QynoTestCase):
    input_name = "input"
    output_name = "print"
    preamble = ""

    def exchange(self, source, *, steps, status=0):
        binary = self.compile_source(self.preamble + source)
        master, slave = pty.openpty()
        original = termios.tcgetattr(slave)
        child = None
        transcript = bytearray()
        deadline = time.monotonic() + 5

        def read_output():
            remaining = deadline - time.monotonic()
            self.assertGreater(remaining, 0, f"PTY timeout: {bytes(transcript)!r}")
            readable, _, _ = select.select([master], [], [], min(0.05, remaining))
            if readable:
                try:
                    transcript.extend(os.read(master, 65536))
                except OSError as error:
                    if error.errno != errno.EIO:
                        raise

        try:
            child = subprocess.Popen([str(binary)], stdin=slave, stdout=slave, stderr=slave)
            for prompt, keys in steps:
                while True:
                    raw = not (termios.tcgetattr(slave)[3] & (termios.ICANON | termios.ECHO | termios.ISIG))
                    if prompt in transcript and raw:
                        break
                    self.assertIsNone(child.poll(), f"Exited before input: {bytes(transcript)!r}")
                    read_output()
                # Wait for the actual prompt and raw mode, never a timing-based sleep.
                pending = memoryview(keys)
                while pending:
                    pending = pending[os.write(master, pending):]
            while child.poll() is None:
                read_output()
            while select.select([master], [], [], 0)[0]:
                read_output()
            self.assertEqual(child.returncode, status, bytes(transcript))
            self.assertEqual(termios.tcgetattr(slave), original, "Terminal settings were not restored")
            return transcript.decode("utf-8").replace("\r\n", "\n")
        finally:
            if child is not None:
                if child.poll() is None:
                    child.kill()
                child.wait(timeout=2)
            os.close(master)
            os.close(slave)

    def edit(self, keys, expected):
        source = program("int main();", f'\tstr s = {self.input_name}("prompt> ")',
                         f'\t{self.output_name}("RESULT=[{{s}}]")')
        transcript = self.exchange(source, steps=[(b"prompt> ", keys)])
        self.assertIn(f"RESULT=[{expected}]\n", transcript)

    def test_plain_enter(self):
        self.edit(b"hello\n", "hello")

    def test_empty_enter(self):
        self.edit(b"\n", "")

    def test_backspace(self):
        self.edit(b"abX\x7fc\n", "abc")

    def test_control_h_backspace(self):
        self.edit(b"abX\x08c\n", "abc")

    def test_left_and_right_insert(self):
        self.edit(b"ac\x1b[Db\x1b[Cd\n", "abcd")

    def test_delete(self):
        self.edit(b"abXc\x1b[D\x1b[D\x1b[3~\n", "abc")

    def test_home_end_sequences(self):
        for home, end in ((b"\x1b[H", b"\x1b[F"), (b"\x1bOH", b"\x1bOF"),
                          (b"\x1b[1~", b"\x1b[4~"), (b"\x1b[7~", b"\x1b[8~")):
            with self.subTest(home=home):
                self.edit(b"bc" + home + b"a" + end + b"d\n", "abcd")

    def test_control_cursor_keys(self):
        self.edit(b"bc\x01a\x05d\x02X\x06!\n", "abcXd!")

    def test_boundary_movements(self):
        self.edit(b"\x7f\x1b[D\x1b[3~a\x1b[C\x1b[3~\n", "a")

    def test_unicode_backspace(self):
        self.edit("日😀".encode() + b"\x7f" + "本\n".encode(), "日本")

    def test_grapheme_backspace_combining_sequence(self):
        self.edit('éX'.encode()+b'\x1b[D\x7f\n','X')

    def test_grapheme_delete_zwj_sequence(self):
        self.edit('👩🏽‍💻X'.encode()+b'\x01\x1b[3~\n','X')

    def test_grapheme_arrow_skips_regional_indicator_pair(self):
        self.edit('🇯🇵X'.encode()+b'\x01\x1b[CY\n','🇯🇵YX')

    def test_unicode_cursor_and_delete(self):
        self.edit("日😀a".encode() + b"\x1b[D\x1b[D\x1b[3~" + "本\n".encode(), "日本a")

    def test_buffer_growth(self):
        text = "日a😀" * 100
        self.edit(text.encode() + b"\n", text)

    def test_ignored_up_down_keys(self):
        self.edit(b"a\x1b[A\x1b[Bb\n", "ab")

    def test_control_d_deletes_on_nonempty_line(self):
        self.edit(b"abc\x1b[D\x1b[D\x04\n", "ac")

    def test_control_d_eof_restores_terminal(self):
        transcript = self.exchange(program(
            "int main();", f'\tstr s = {self.input_name}("prompt> ")', f'\t{self.output_name}("UNREACHABLE")',
        ), steps=[(b"prompt> ", b"\x04")], status=65)
        self.assertIn("入力が終わって", transcript)
        self.assertNotIn("UNREACHABLE\n", transcript)

    def test_control_c_restores_terminal(self):
        transcript = self.exchange(program(
            "int main();", f'\tstr s = {self.input_name}("prompt> ")', f'\t{self.output_name}("UNREACHABLE")',
        ), steps=[(b"prompt> ", b"abc\x03")], status=130)
        self.assertIn("^C", transcript)
        self.assertNotIn("UNREACHABLE\n", transcript)

    def test_multiple_input_calls(self):
        transcript = self.exchange(program(
            "int main();", f'\tstr a = {self.input_name}("first> ")', f'\tstr b = {self.input_name}("second> ")',
            f'\t{self.output_name}("RESULT=[{{a}}/{{b}}]")',
        ), steps=[(b"first> ", b"abc\n"), (b"second> ", "日本\n".encode())])
        self.assertIn("RESULT=[abc/日本]\n", transcript)

    def test_framed_input(self):
        transcript = self.exchange(program(
            "int main();", "\tint age", f'\t{self.input_name}("年齢> {{age}} 歳")',
            f'\t{self.output_name}("RESULT=[{{age}}]")',
        ), steps=[("年齢> ".encode(), b"2\x7f7\n")])
        self.assertIn(" 歳", transcript)
        self.assertIn("RESULT=[7]\n", transcript)


class NamespacedLineEditTests(LineEditTests):
    input_name = "io.input"
    output_name = "io.print"


class AliasedLineEditTests(LineEditTests):
    input_name = "terminal.input"
    output_name = "terminal.print"
    preamble = "using io as terminal\n"


if __name__ == "__main__":
    run_suite(report_success=True)
