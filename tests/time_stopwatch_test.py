"""Deterministic pause/resume/lap semantics without scheduler-dependent timing."""
from pathlib import Path
import re, shlex, subprocess, tempfile
from support import QynoTestCase, ROOT, run_suite
from time_support import PlatformCase

class StopwatchTests(PlatformCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary=tempfile.TemporaryDirectory(prefix='qyno-stopwatch-compiler-')
        cls.original_compiler=cls.compiler
        cls.compiler=Path(cls.temporary.name)/'qyc'
        source_line=re.search(r'^SRCS = (.+)$',(ROOT/'Makefile').read_text(),re.M).group(1)
        result=subprocess.run(['gcc','-std=c99','-O1','-DQYNO_TEST_CLOCK_STEP_NS=1000000000',
            *[str(ROOT/p) for p in shlex.split(source_line)],'-lm','-o',str(cls.compiler)],capture_output=True,text=True,timeout=60)
        if result.returncode:raise AssertionError(result.stderr)

    @classmethod
    def tearDownClass(cls):
        cls.compiler=cls.original_compiler
        cls.temporary.cleanup()

    def test_elapsed_lap_pause_resume_reset_restart(self):
        self.check_program(self.main('time.Stopwatch watch','watch.start()','watch.start()',
            'io.print(watch.elapsed())','io.print(watch.lap())','io.print(watch.lap())',
            'io.print(watch.stop())','io.print(watch.elapsed())','io.print(watch.lap())','io.print(watch.lap())',
            'watch.start()','io.print(watch.stop())','watch.reset()','io.print(watch.elapsed())',
            'watch.restart()','io.print(watch.elapsed())','io.print(watch.stop())'),
            '1.0\n2.0\n1.0\n4.0\n4.0\n1.0\n0.0\n5.0\n0.0\n1.0\n2.0\n')

    def test_running_copy_retains_origin_but_owns_separate_state(self):
        self.check_program(self.main('time.Stopwatch a','a.start()','time.Stopwatch b=a',
            'io.print(a.stop())','io.print(b.elapsed())','b.reset()','io.print(a.elapsed())',
            'a.start()','io.print(a.stop())','io.print(b.elapsed())'),'1.0\n2.0\n1.0\n2.0\n0.0\n')

    def test_reset_and_stopped_operations_do_not_read_clock(self):
        self.check_program(self.main('time.Stopwatch watch','io.print(watch.stop())','io.print(watch.lap())',
            'watch.reset()','watch.start()','io.print(watch.stop())','io.print(watch.stop())',
            'watch.restart()','io.print(watch.stop())'),'0.0\n0.0\n1.0\n1.0\n1.0\n')

if __name__=='__main__':run_suite()
