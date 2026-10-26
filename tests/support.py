"""Shared black-box helpers; only Python's standard library is required."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


def program(*lines):
    return "\n".join(lines) + "\n"


class QynoTestCase(unittest.TestCase):
    compiler = ROOT / "qyc"

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qyno-test-")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.sequence = 0

    def compile_source(self, source, *, error=None):
        self.sequence += 1
        path = self.directory / f"case{self.sequence}.qy"
        path.write_bytes(source.encode("utf-8") if isinstance(source, str) else source)
        binary = path.with_suffix("")
        result = subprocess.run(
            [str(self.compiler), str(path), "-o", str(binary)],
            capture_output=True, timeout=10,
        )
        diagnostic = result.stderr.decode("utf-8", errors="replace")
        if error is not None:
            self.assertEqual(result.returncode, 1, diagnostic)
            self.assertIn("エラー", diagnostic)
            self.assertIn(error, diagnostic)
            self.assertIn(str(path), diagnostic)
            return result
        self.assertEqual(result.returncode, 0, diagnostic)
        self.assertEqual(result.stderr, b"")
        self.assertTrue(binary.is_file())
        return binary

    def check_program(self, source, expected, *, stdin="", status=0, error=None):
        binary = self.compile_source(source)
        result = subprocess.run(
            [str(binary)],
            input=stdin.encode("utf-8") if isinstance(stdin, str) else stdin,
            capture_output=True, timeout=5,
        )
        self.assertEqual(result.returncode, status, result.stderr.decode("utf-8", errors="replace"))
        self.assertEqual(result.stdout, expected.encode("utf-8"))
        if error is None:
            self.assertEqual(result.stderr, b"")
        else:
            diagnostic = result.stderr.decode("utf-8")
            self.assertIn("QYNO 実行時エラー", diagnostic)
            self.assertIn(error, diagnostic)
            self.assertIn(str(binary.with_suffix(".qy")), diagnostic)
        return result


def run_suite(*, report_success=False):
    if len(sys.argv) > 1 and not sys.argv[1].startswith("-"):
        QynoTestCase.compiler = Path(sys.argv.pop(1)).resolve()
    if report_success:
        class ReportingResult(unittest.TextTestResult):
            def addSuccess(self, test):
                super().addSuccess(test)
                print(f"OK  {test.id()}")

        runner = unittest.TextTestRunner(resultclass=ReportingResult, verbosity=0)
        unittest.main(testRunner=runner)
    else:
        unittest.main()
