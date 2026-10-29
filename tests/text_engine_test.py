"""Independent byte/boundary oracles for the shared const/native text engine."""
import ctypes, os, random, re, struct, subprocess, tempfile, unittest
from pathlib import Path
from support import ROOT, run_suite
from arguments_test import Request

class TextRequest(ctypes.Structure):
    _fields_=[('text',ctypes.c_void_p*3),('length',ctypes.c_uint64*3)]+[(n,ctypes.c_int64) for n in ['op','phase','status','result']]+[(n,ctypes.c_uint64) for n in ['scratch_size','capacity','used','count']]+[('scratch',ctypes.c_void_p),('buffer',ctypes.c_void_p)]

class TextEngineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory(prefix='qyno-text-engine-');cls.addClassCleanup(cls.temp.cleanup)
        so=Path(cls.temp.name)/'runtime.so'
        subprocess.run(['gcc','-std=c99','-O1','-shared','-fPIC',*(['-fsanitize=undefined','-fno-sanitize-recover=all'] if os.environ.get('QYNO_TEXT_CHECKED') else []),str(ROOT/'wide_runtime.c'),str(ROOT/'grapheme_runtime.c'),'-lm','-o',str(so)],check=True,capture_output=True)
        cls.library=ctypes.CDLL(str(so));cls.library.wide_execute.argtypes=[ctypes.POINTER(Request)]
        names=['W_CONVERT']+re.findall(r'W_[A-Z_]+',(ROOT/'wide_runtime.h').read_text().split('enum { W_CONVERT,',1)[1].split('};',1)[0]);cls.op=names.index('W_TEXT_METHOD')

    def call(self,t):
        wide=Request();wide.op=self.op;wide.input=ctypes.addressof(t);self.library.wide_execute(ctypes.byref(wide));self.assertEqual(wide.status,t.status)

    def setup(self,op,*values):
        buffers=[ctypes.create_string_buffer(v.encode() if isinstance(v,str) else v) for v in values]
        t=TextRequest();t.op=op
        for i,b in enumerate(buffers):t.text[i]=ctypes.addressof(b);t.length[i]=len(b.raw)-1
        return t,buffers

    def execute(self,op,*values,status=0):
        t,buffers=self.setup(op,*values);self.call(t);self.assertEqual(t.status,status)
        if status:return
        self.assertEqual(t.scratch_size,0 if op==4 else len(buffers[1].raw[:-1])*8)
        scratch=ctypes.create_string_buffer(t.scratch_size+16);t.scratch=ctypes.addressof(scratch);t.phase=1;self.call(t);self.assertEqual(t.status,0)
        used,count,result=t.used,t.count,t.result
        output=ctypes.create_string_buffer(used+16);t.buffer=ctypes.addressof(output);t.capacity=used;t.phase=2;self.call(t);self.assertEqual(t.status,0)
        self.assertEqual((t.used,t.count,t.result),(used,count,result));self.assertEqual(output.raw[used:],b'\0'*16);self.assertEqual(scratch.raw[t.scratch_size:],b'\0'*16)
        if op<=3:return t.result
        if op==5:
            source=buffers[0].raw[:-1]
            return [source[start:start+n] for start,n in struct.iter_unpack('<QQ',output.raw[:used])]
        return output.raw[:used]

    def oracle(self,clusters,pattern):
        source=''.join(clusters).encode();pat=pattern.encode();boundaries=[0]
        for c in clusters:boundaries.append(boundaries[-1]+len(c.encode()))
        points=set(boundaries)
        return source,pat,boundaries,[(p,p+len(pat)) for p in boundaries if source[p:p+len(pat)]==pat and p+len(pat) in points]

    def verify(self,clusters,pattern):
        source,pat,bounds,matches=self.oracle(clusters,pattern)
        for op,expected in [(0,bool(matches)),(1,any(a==0 for a,b in matches)),(2,any(b==len(source) for a,b in matches)),(3,bounds.index(matches[0][0]) if matches else -1)]:self.assertEqual(self.execute(op,source,pat),expected,(clusters,pattern,op))
        if not pat:return
        parts=[];out=[];last=0
        for a,b in matches:
            if a<last:continue
            parts.append(source[last:a]);out += [source[last:a],b'X'];last=b
        parts.append(source[last:]);out.append(source[last:])
        self.assertEqual(self.execute(5,source,pat),parts)
        self.assertEqual(self.execute(6,source,pat,b'X'),b''.join(out))
        self.assertEqual(self.execute(6,source,pat,pat),source)
        self.assertEqual(pat.join(self.execute(5,source,pat)),source)

    def test_official_unicode_boundaries_match_oracle(self):
        count=0
        for line in (ROOT/'unicode/17.0.0/GraphemeBreakTest.txt').read_text().splitlines():
            fields=line.split('#')[0].split()
            if not fields:continue
            clusters=[];current=''
            for v in fields:
                if v=='÷':
                    if current:clusters.append(current);current=''
                elif v!='×':current+=chr(int(v,16))
            text=''.join(clusters)
            for pattern in {text,text[:1],text[-1:],*clusters}:self.verify(clusters,pattern)
            count+=1
        self.assertEqual(count,766)

    def test_deterministic_ascii_properties_and_overlap(self):
        rng=random.Random(277)
        for _ in range(140):
            source=''.join(rng.choice('aab\0,') for _ in range(rng.randrange(45)))
            pattern=''.join(rng.choice('ab\0,') for _ in range(rng.randrange(6)))
            self.verify(list(source),pattern)

    def test_all_white_space_and_exclusions(self):
        white=[*range(9,14),32,133,160,5760,*range(8192,8203),8232,8233,8239,8287,12288]
        for c in white:self.assertEqual(self.execute(4,chr(c)+'x'+chr(c)),b'x')
        for c in [0,0x200b,0xfeff,*range(0x1c,0x20)]:self.assertEqual(self.execute(4,chr(c)),chr(c).encode())
        for s in [' \u0301x ','\r\nx\r\n','\u3000','a b','']:
            trimmed=self.execute(4,s);self.assertEqual(self.execute(4,trimmed),trimmed)
        self.assertEqual(self.execute(4,' \u0301x '),' \u0301x'.encode())

    def test_invalid_utf8_and_contract_order(self):
        malformed=[b'\xff',b'\xc0\x80',b'\xed\xa0\x80',b'\xf4\x90\x80\x80',b'\xf0\x9f',b'\x80']
        for value in malformed:
            for op in range(7):
                args=[value] if op==4 else [value,b'x'] if op!=6 else [value,b'',b'x']
                self.execute(op,*args,status=65)
            self.execute(0,b'',value,status=65);self.execute(6,b'a',b'',value,status=65)
        self.execute(5,b'a',b'',status=70);self.execute(6,b'a',b'',b'x',status=70)

    def test_size_and_capacity_checks_with_canaries(self):
        limit=(1<<63)-1-64
        t,buffers=self.setup(0,b'',b'a');t.length[1]=limit//8+1
        # Length overflow is checked before dereferencing an impossible input.
        t.length[0]=limit+1;self.call(t);self.assertEqual(t.status,70)
        t,buffers=self.setup(6,b'aa',b'a',b'x');self.call(t);scratch=ctypes.create_string_buffer(t.scratch_size);t.scratch=ctypes.addressof(scratch)
        t.phase=1;t.length[2]=limit;self.call(t);self.assertEqual(t.status,70)
        for op,values in [(5,[b'a,b',b',']),(6,[b'ab',b'a',b'XYZ']),(4,[b' a '])]:
            t,buffers=self.setup(op,*values);self.call(t);scratch=ctypes.create_string_buffer(t.scratch_size+1);t.scratch=ctypes.addressof(scratch);t.phase=1;self.call(t)
            output=ctypes.create_string_buffer(t.used+16);t.buffer=ctypes.addressof(output);t.capacity=t.used-1;t.phase=2;self.call(t);self.assertEqual(t.status,70);self.assertEqual(output.raw[t.capacity:],b'\0'*(len(output)-t.capacity))
        t,buffers=self.setup(0,b'abc',b'a');self.call(t);t.phase=1;self.call(t);self.assertEqual(t.status,70)

    def test_empty_text_may_have_null_storage(self):
        for op in range(5):
            t=TextRequest();t.op=op;self.call(t);self.assertEqual(t.status,0)
            t.phase=1;self.call(t);self.assertEqual(t.status,0)
            self.assertEqual(t.result,1 if op<3 else 0)
            self.assertEqual(t.used,0)

    def test_long_repeated_input_uses_pattern_sized_scratch(self):
        source=b'a'*250000+b'b';pattern=b'a'*4096+b'b'
        self.assertEqual(self.execute(3,source,pattern),250000-4096)
        self.assertEqual(self.execute(6,b'a,'*15000,b',',b'X'),b'aX'*15000)
        self.assertEqual(len(self.execute(5,b'a,'*15000,b',')),15001)

if __name__=='__main__':run_suite()
