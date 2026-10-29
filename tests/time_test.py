"""Binary128 time values, clocks, calendar and standard Stopwatch contracts."""
import datetime, os, random, shutil, subprocess, unittest
from pathlib import Path
from support import program, run_suite
from time_support import PlatformCase, WINDOWS

class TimeTests(PlatformCase):
    def test_namespaces_using_selective_alias_and_constants(self):
        for prefix,call in [('', 'time.seconds'),('using time\n','seconds'),('using time.seconds\n','seconds'),('using time as clock\n','clock.seconds')]:
            self.check_program(self.main(f'io.print({call}(2))',prefix=prefix),'2.0\n')
        self.compile_source(self.main('monotonic()'),error='using time')
        self.compile_source(self.main('monotonic()',prefix='import time\n'),error='using time')
        self.check_program(self.main('quadruple total=SECOND+MILLISECOND','io.print(|total-1.001|<1e-30)','Stopwatch watch',
            'io.print(watch.is_running())',prefix='using time\n'),'true\nfalse\n')

    def test_unit_conversions_and_inverse_are_quadruple(self):
        names=['nanoseconds','microseconds','milliseconds','seconds','minutes','hours','days','weeks']
        for name in names:
            self.check_program(self.main(f'quadruple duration=time.{name}(123.5)',
                f'io.print(|time.in_{name}(duration)-123.5| < 1e-28)','io.print(lib.size(duration))'),'true\n128\n')
        self.check_program(self.main('io.print(time.in_seconds(time.days(2)+time.hours(3)+time.minutes(4)+5))'),'183845.0\n')

    def test_unit_constants_const_and_context_precision(self):
        source=program('const quadruple X=time.nanoseconds(1)','const quadruple Y=time.minutes(2)',
            'const quadruple SUM=time.HOUR+time.MINUTE+time.SECOND','int main();',
            '\tio.print("{time.in_nanoseconds(X)>0.9999999999} {Y} {SUM}")','\tquadruple tiny=time.seconds(1.000000000000000000000000000001)',
            '\tio.print(tiny>1.0)')
        self.check_program(source,'true 120.0 3661.0\ntrue\n')

    def test_double_requires_explicit_conversion(self):
        self.compile_source(self.main('double n=time.monotonic()'),error='変換')
        self.check_program(self.main('double n=double(time.milliseconds(500))','io.print(n)'),'0.5\n')
        for expr in ['time.seconds(true)','time.seconds(i3)','time.sleep("1")']:
            self.compile_source(self.main(expr),error='数値')

    def test_clocks_are_runtime_values_and_monotonic(self):
        self.check_program(self.main('quadruple start=time.monotonic()','quadruple cpu=time.cpu()',
            'io.print(time.monotonic()>=start)','io.print(time.cpu()>=cpu)','io.print(time.now()>0.0)',
            'io.print(time.elapsed(start)>=0.0)','io.print(time.elapsed(start+time.days(365))<0.0)'),
            'true\ntrue\ntrue\ntrue\ntrue\n')
        for expr in ['time.now()','time.monotonic()','time.cpu()','time.elapsed(0.0)','time.sleep(0.0)']:
            self.compile_source(program('const auto X='+expr,'int main();','\treturn 0'),error='const')

    def test_sleep_zero_small_and_absolute_deadline(self):
        self.check_program(self.main('time.sleep(0)','quadruple started=time.monotonic()',
            'time.sleep(time.milliseconds(1))','io.print(time.elapsed(started)>=time.milliseconds(1))',
            'quadruple deadline=time.monotonic()+time.milliseconds(1)','time.sleep_until(deadline)',
            'io.print(time.monotonic()>=deadline)','time.sleep_until(time.monotonic()-1.0)'),
            'true\ntrue\n')

    def test_sleep_invalid_values(self):
        self.compile_source(self.main('time.sleep(-0.1)'),error='sleep')
        self.check_program(self.main('quadruple n=-0.1','time.sleep(n)'),'',status=70,error='time')
        self.check_program(self.main('quadruple n=1e4932','n=n*10','time.sleep_until(n)'),'',status=70,error='time')

    def test_datetime_epoch_defaults_and_fraction(self):
        self.check_program(self.main('time.DateTime date','io.print(time.to_timestamp(date))',
            'date=time.to_datetime(-0.125)','io.print("{date.year} {date.month} {date.day} {date.hour} {date.minute} {date.second}")',
            'io.print(time.to_timestamp(date))'),'0.0\n1969 12 31 23 59 59.875\n-0.125\n')

    def test_fixed_offset_and_weekday(self):
        self.check_program(self.main('time.DateTime d={year=2026,month=10,day=29,hour=9,minute=30,second=0.125,offset_minutes=540}',
            'quadruple stamp=time.to_timestamp(d)','io.print(time.isoformat(stamp,540))',
            'io.print(time.isoformat(stamp))','io.print(time.weekday(d))','io.print(time.day_of_year(d))'),
            '2026-10-29T09:30:00.125000000+09:00\n2026-10-29T00:30:00.125000000Z\nthursday\n302\n')

    def test_calendar_leap_century_rules(self):
        self.check_program(self.main('io.print(time.is_leap_year(1900))','io.print(time.is_leap_year(2000))',
            'io.print(time.is_leap_year(2100))','io.print(time.days_in_month(2000,2))','io.print(time.days_in_month(1900,2))'),
            'false\ntrue\nfalse\n29\n28\n')

    def test_calendar_boundaries_and_negative_timestamps(self):
        for text in ['0001-01-01T00:00:00Z','1900-02-28T23:59:59Z','1969-12-31T23:59:59.125Z','2000-02-29T12:00:00Z','9999-12-31T23:59:59Z']:
            expected=text.replace('Z','.000000000Z') if '.' not in text else text.replace('.125Z','.125000000Z')
            self.check_program(self.main(f'quadruple t=time.parse("{text}")','io.print(time.isoformat(t))'),expected+'\n')

    def test_calendar_matches_independent_python_dates(self):
        rng=random.Random(273);lines=[];expected=[]
        for i in range(25):
            year=rng.randint(2,9998);month=rng.randint(1,12);day=rng.randint(1,28)
            date=datetime.date(year,month,day)
            lines.extend([f'time.DateTime d{i}={{year={year},month={month},day={day}}}',
                f'io.print("{{int(time.weekday(d{i}))}} {{time.day_of_year(d{i})}}")'])
            expected.append(f'{date.weekday()} {(date-datetime.date(year,1,1)).days+1}')
        self.check_program(self.main(*lines),'\n'.join(expected)+'\n')

    def test_datetime_const_evaluation_and_copy(self):
        source=program('const time.DateTime DATE=time.to_datetime(0.125,540)',
            'const quadruple STAMP=time.to_timestamp(DATE)','const str TEXT=time.isoformat(STAMP)',
            'const time.Weekday DAY=time.weekday(DATE)','int main();','\tio.print(TEXT)',
            '\tio.print(DAY)','\ttime.DateTime copy=DATE','\tcopy.year=2000','\tio.print(DATE.year)')
        self.check_program(source,'1970-01-01T00:00:00.125000000Z\nthursday\n1970\n')

    def test_datetime_validation_and_timezone_bounds(self):
        for initializer in ['{year=0}','{year=10000}','{month=0}','{month=13}','{year=1900,month=2,day=29}',
                '{hour=24}','{minute=60}','{second=60}','{second=-0.1}','{offset_minutes=841}']:
            self.compile_source(self.main(f'const time.DateTime d={initializer}','io.print(time.to_timestamp(d))'),error='time')
            self.check_program(self.main(f'time.DateTime d={initializer}','io.print(time.to_timestamp(d))'),'',status=70,error='time')
        self.compile_source(self.main('io.print(time.to_datetime(-62135596800,-1))'),error='time')

    def test_iso_parse_requires_exact_format_and_zone(self):
        invalid=['','2026-10-29','2026-10-29T00:00:00','2026-10-29t00:00:00Z','2026-10-29T00:00:00z',
            '2026-02-29T00:00:00Z','2026-10-29T00:00:60Z','2026-10-29T00:00:00+14:01',
            '2026-10-29T00:00:00+00:60','2026-10-29T00:00:00.1234567890Z',' 2026-10-29T00:00:00Z',
            '2026-10-29T00:00:00Z ','2026-10-29T00:00:00.Z']
        lines=['quadruple destination=42']
        for text in invalid:lines.append(f'io.print(time.try_parse("{text}",ref destination))')
        lines.append('io.print(destination)')
        self.check_program(self.main(*lines),'false\n'*len(invalid)+'42.0\n')

    def test_runtime_parse_data_error_and_const_error(self):
        self.compile_source(self.main('io.print(time.parse("bad"))'),error='time')
        self.check_program(self.main('str text="bad"','io.print(time.parse(text))'),'',status=65,error='time')
        self.compile_source(self.main('double out','time.try_parse("bad",ref out)'),error='quadruple')

    def test_try_parse_updates_only_success_and_supports_array_element(self):
        self.check_program(self.main('quadruple stamps[]=[42.0]','io.print(time.try_parse("1970-01-01T00:00:00.125Z",ref stamps[0]))',
            'io.print(stamps[0])','io.print(time.try_parse("bad",ref stamps[0]))','io.print(stamps[0])'),'true\n0.125\nfalse\n0.125\n')

    def test_format_tokens_and_unicode(self):
        self.check_program(self.main('quadruple t=time.parse("2026-10-29T09:30:00.125+09:00")',
            'io.print(time.format(t,"%Y/%m/%d %H:%M:%S.%f %N %z %:z %j %u %% 日本語",540))'),
            '2026/10/29 09:30:00.125000 125000000 +0900 +09:00 302 4 % 日本語\n')
        for pattern in ['%','%Q','%:Y']:
            self.compile_source(self.main(f'io.print(time.format(0.0,"{pattern}"))'),error='time')

    def test_format_rounding_carries_into_next_day(self):
        self.check_program(self.main('quadruple t=time.parse("1999-12-31T23:59:59Z")',
            't+=0.9999999996','io.print(time.isoformat(t))'),'2000-01-01T00:00:00.000000000Z\n')

    def test_format_duration_sign_rounding_and_precision(self):
        values=[('0.125',3,'00:00:00.125'),('90061.5',3,'1d 01:01:01.500'),('-0.5',3,'-00:00:00.500'),
            ('2.5',0,'00:00:02'),('3.5',0,'00:00:04'),('-0.0001',3,'00:00:00.000')]
        for value,digits,text in values:
            self.check_program(self.main(f'io.print(time.format_duration({value},{digits}))'),text+'\n')
        self.compile_source(self.main('io.print(time.format_duration(0.0,10))'),error='time')

    def test_stopwatch_pause_lap_restart_and_reset(self):
        self.check_program(self.main('time.Stopwatch watch','io.print(watch.elapsed())','io.print(watch.stop())',
            'io.print(watch.lap())','watch.start()','watch.start()','time.sleep(time.milliseconds(1))',
            'quadruple lap=watch.lap()','quadruple stopped=watch.stop()','io.print(lap>0.0 and stopped>=lap)',
            'time.sleep(time.milliseconds(1))','io.print(watch.elapsed()==stopped)',
            'watch.start()','time.sleep(time.milliseconds(1))','io.print(watch.stop()>stopped)',
            'watch.reset()','io.print(watch.elapsed()==0.0 and not watch.is_running())',
            'watch.restart()','io.print(watch.is_running())','watch.stop()'),
            '0.0\n0.0\n0.0\ntrue\ntrue\ntrue\ntrue\ntrue\n')

    def test_stopwatch_copies_are_independent_and_readonly(self):
        prefix=program('quadruple observe(ref readonly time.Stopwatch watch)=watch.elapsed()')
        self.check_program(self.main('time.Stopwatch a','a.start()','time.Stopwatch b=a','a.reset()',
            'io.print(not a.is_running() and b.is_running())','io.print(observe(ref b)>=0.0)',
            'b.stop()','io.print(a.elapsed())',prefix=prefix),'true\ntrue\n0.0\n')
        self.compile_source(program('void mutate(ref readonly time.Stopwatch watch);','\twatch.start()',
            'int main();','\treturn 0'),error='readonly')
        self.compile_source(self.main('time.Stopwatch watch','io.print(watch.frequency)'),error='private')

    def test_internal_clock_helpers_are_not_public(self):
        for call in ['time._ticks()','time._frequency()']:
            self.compile_source(self.main('io.print('+call+')'),error='内部')

    @unittest.skipIf(WINDOWS,'Linux syscall injection')
    def test_clock_error_is_caught_with_source_and_empty_path(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        for errno,code in [('EIO','system_error'),('ENOSYS','unsupported_operation')]:
            binary=self.compile_source(self.main('try(time.now()) as e;','\tio.print("wrong")',
                f'case(io.ErrorCode.{code});','\tio.print("{e.code} {e.operation} {e.path.length} {e.os_code > 0}")'))
            r=subprocess.run(['strace','-qq','-o',str(self.directory/'trace'),f'--inject=clock_gettime:error={errno}:when=1',str(binary)],capture_output=True,timeout=5)
            self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,f'{code} now 0 true\n'.encode())

    def test_clock_effect_cannot_hide_callee_unclosed_file(self):
        prefix=program('void unsafe();','\tfile f=io.file.open("input")','\ttime.monotonic()','\tio.file.close(f)')
        self.compile_source(self.main('try(unsafe());','\tio.print(0)','case();','\tio.print(1)',prefix=prefix),error='エラー経路')

    @unittest.skipIf(WINDOWS,'Linux syscall injection')
    def test_clock_failure_executes_file_finally(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        path=self.directory/'input';path.write_bytes(b'')
        prefix=program('void read();','\tfile f',f'\ttry(f=io.file.open("{path}"));','\t\ttime.monotonic()',
            '\tfinally;','\t\tio.file.close(f)')
        binary=self.compile_source(self.main('try(read()) as e;','\tio.print("wrong")','case(io.ErrorCode.system_error);',
            '\tio.print(e.operation)',prefix=prefix))
        trace=self.directory/'trace'
        r=subprocess.run(['strace','-qq','-o',str(trace),'--inject=clock_gettime:error=EIO:when=1',str(binary)],capture_output=True,timeout=5)
        self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,b'monotonic\n')
        self.assertEqual(sum(line.startswith('close(') for line in trace.read_text().splitlines()),1)

    def test_zero_quadruple_inputs_and_array_dates(self):
        self.check_program(self.main('quadruple zero','time.sleep(zero)','time.sleep_until(zero)',
            'io.print(time.seconds(zero))','io.print(time.isoformat(zero))','time.DateTime dates[2]',
            'io.print(time.to_timestamp(dates[0]))','io.print(time.weekday(dates[1]))'),
            '0.0\n1970-01-01T00:00:00.000000000Z\n0.0\nthursday\n')

    def test_nine_digit_timestamps_survive_far_epoch(self):
        for text in ['9999-12-30T23:59:59.123456789Z','0001-01-02T00:00:00.123456789Z',
                '2026-10-29T09:30:00.123456789+14:00','1969-12-31T23:59:59.999999999Z']:
            self.check_program(self.main(f'str text="{text}"','quadruple stamp=time.parse(text)',
                'str normalized=time.isoformat(stamp)','io.print(time.isoformat(time.parse(normalized))==normalized)'),
                'true\n')

    def test_datetime_input_is_not_changed_and_subnanoseconds_are_preserved(self):
        self.check_program(self.main('time.DateTime date={second=0.000000000001}',
            'quadruple stamp=time.to_timestamp(date)','io.print(stamp>0.0 and stamp<time.nanoseconds(1))',
            'io.print(date.second==stamp)','io.print(time.to_datetime(stamp).second==stamp)'),
            'true\ntrue\ntrue\n')

    def test_pattern_expansion_and_embedded_nul(self):
        pattern='%N'*100
        self.check_program(self.main(f'io.print(time.format(0.125,"{pattern}"))'),'125000000'*100+'\n')
        self.check_program(self.main('str text="1970-01-01T00:00:00Z\\0"','quadruple stamp=42',
            'io.print(time.try_parse(text,ref stamp))','io.print(stamp)'),'false\n42.0\n')

    def test_weekday_enum_match_and_pure_functions_reject_wrong_types(self):
        self.check_program(self.main('time.DateTime date','time.Weekday day=time.weekday(date)',
            'match(day);','\tcase monday,tuesday,wednesday,friday,saturday,sunday;','\t\tio.print("wrong")',
            '\tcase thursday;','\t\tio.print(day)'),'thursday\n')
        for expression in ['time.days_in_month(2000.5,2)','time.to_timestamp(0.0)','time.try_parse("x",42)',
                'time.isoformat(0.0,1.5)','time.format(0.0,42)']:
            self.compile_source(self.main('io.print('+expression+')'),error='time' if 'try_parse' not in expression else 'ref')

    def test_assembly_executes_same_pure_and_clock_code(self):
        source=self.main('io.print(time.isoformat(time.parse("2026-10-29T00:30:00.125Z")))',
            'io.print(time.monotonic()>=0.0)')
        path=self.directory/'assembly.qy';path.write_text(source);asm=path.with_suffix('.s');exe=path.with_suffix('')
        r=subprocess.run([str(self.compiler),str(path),'-S','--target','linux','-o',str(asm)],capture_output=True,timeout=10)
        self.assertEqual(r.returncode,0,r.stderr)
        r=subprocess.run(['gcc','-nostdlib','-no-pie',str(asm),'-o',str(exe)],capture_output=True,timeout=10)
        self.assertEqual(r.returncode,0,r.stderr)
        r=subprocess.run([str(exe)],capture_output=True,timeout=5)
        self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,b'2026-10-29T00:30:00.125000000Z\ntrue\n')

    @unittest.skipIf(WINDOWS,'Linux syscall injection')
    def test_stopwatch_failure_keeps_state_and_reports_public_call_site(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        for declaration,receiver in [('time.Stopwatch watch','watch'),('time.Stopwatch watches[1]','watches[0]')]:
            binary=self.compile_source(self.main(declaration,f'{receiver}.start()',f'try({receiver}.stop()) as error;',
                '\tio.print("wrong")','case(io.ErrorCode.system_error);','\tio.print(error.operation)',
                f'\tio.print({receiver}.is_running())',f'{receiver}.stop()',f'io.print({receiver}.is_running())'))
            r=subprocess.run(['strace','-qq','-o',str(self.directory/'trace'),'--inject=clock_gettime:error=EIO:when=2',str(binary)],capture_output=True,timeout=5)
            self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,b'Stopwatch.stop\ntrue\nfalse\n')

    @unittest.skipIf(WINDOWS,'Linux syscall injection')
    def test_zero_sleep_performs_no_clock_syscall(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        binary=self.compile_source(self.main('time.sleep(0.0)','io.print("done")'))
        trace=self.directory/'trace'
        r=subprocess.run(['strace','-qq','-o',str(trace),'--inject=clock_gettime:error=EIO:when=1',str(binary)],capture_output=True,timeout=5)
        self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,b'done\n')
        self.assertNotIn('clock_gettime(',trace.read_text())


    @unittest.skipIf(WINDOWS,'Linux syscall injection')
    def test_sleep_os_failure_uses_existing_try(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        binary=self.compile_source(self.main('try(time.sleep(time.days(1))) as error;',
            '\tio.print("wrong")','case(io.ErrorCode.system_error);','\tio.print(error.operation)'))
        r=subprocess.run(['strace','-qq','-o',str(self.directory/'trace'),'--inject=nanosleep:error=EIO:when=1',str(binary)],capture_output=True,timeout=5)
        self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,b'sleep\n')

    @unittest.skipIf(WINDOWS,'Linux syscall injection')
    def test_interrupted_clock_read_retries(self):
        if not shutil.which('strace'):self.skipTest('strace not installed')
        binary=self.compile_source(self.main('io.print(time.now()>0.0)'))
        r=subprocess.run(['strace','-qq','-o',str(self.directory/'trace'),'--inject=clock_gettime:error=EINTR:when=1',str(binary)],capture_output=True,timeout=5)
        self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,b'true\n')


if __name__=='__main__':run_suite()
