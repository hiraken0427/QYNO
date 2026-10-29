"""Exact arithmetic oracles for reproducible typed random values."""
import ctypes, os, re, struct, subprocess, tempfile, unittest
from fractions import Fraction
from pathlib import Path
from support import ROOT, run_suite
from arguments_test import Bits,Request
MASK=(1<<64)-1
FORMATS={1016:(11,5,15),1032:(24,8,127),1064:(53,11,1023),1128:(113,15,16383)}

def seeded(seed):
    result=[]
    for _ in range(4):
        seed=(seed+0x9e3779b97f4a7c15)&MASK;z=seed
        z=((z^(z>>30))*0xbf58476d1ce4e5b9)&MASK;z=((z^(z>>27))*0x94d049bb133111eb)&MASK
        result.append(z^(z>>31))
    return result

def word(state):
    rotate=lambda x,n:((x<<n)|(x>>(64-n)))&MASK
    result=rotate((state[1]*5)&MASK,7)*9&MASK;t=state[1]<<17&MASK
    state[2]^=state[0];state[3]^=state[1];state[1]^=state[2];state[0]^=state[3];state[2]^=t;state[3]=rotate(state[3],45)
    return result

def draw(state,n):
    a=word(state)
    return a>>(64-n) if n<=64 else (a<<(n-64))|(word(state)>>(128-n))

def uniform(state,count):
    if count==1:return 0
    while True:
        value=draw(state,(count-1).bit_length())
        if value<count:return value

def fraction(raw,ty):
    p,e,bias=FORMATS[ty];sign=raw>>(p+e-1);exp=(raw>>(p-1))&((1<<e)-1);mant=raw&((1<<(p-1))-1)
    if exp:mant|=1<<(p-1)
    exponent=(exp or 1)-bias-p+1
    return (-1 if sign else 1)*Fraction(mant)*(Fraction(2)**exponent)

def rounded(value,ty):
    if not value:return 0
    p,e,bias=FORMATS[ty];sign=value<0;value=abs(value)
    exponent=value.numerator.bit_length()-value.denominator.bit_length()
    if value<Fraction(2)**exponent:exponent-=1
    exponent=max(exponent,1-bias);scaled=value/(Fraction(2)**(exponent-p+1));q,r=divmod(scaled.numerator,scaled.denominator)
    if r*2>scaled.denominator or (r*2==scaled.denominator and q%2):q+=1
    if q==1<<p:q>>=1;exponent+=1
    if q>=1<<(p-1):q=(q&((1<<(p-1))-1))|((exponent+bias)<<(p-1))
    return q|((1<<(p+e-1)) if q and sign else 0)

class RandomRequest(ctypes.Structure):
    _fields_=[(n,ctypes.c_int64) for n in ['op','ty','status','phase','flags','first_tag','second_tag']]+[(n,Bits) for n in ['first','second','out']]+[('state',ctypes.POINTER(ctypes.c_uint64)),('text',ctypes.c_void_p),('length',ctypes.c_int64),('count',ctypes.c_int64),('buffer',ctypes.c_void_p)]+[(n,ctypes.c_int64) for n in ['capacity','used','need_random','seed_auto']]+[('seed_value',ctypes.c_uint64)]

def bits(value):return Bits(value& MASK,(value>>64)&MASK)
def number(value):return value.lo|(value.hi<<64)

class RandomEngineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory(prefix='qyno-random-engine-');cls.addClassCleanup(cls.temp.cleanup)
        so=Path(cls.temp.name)/'runtime.so'
        subprocess.run(['gcc','-std=c99','-O1','-shared','-fPIC',*(['-fsanitize=undefined','-fno-sanitize-recover=all'] if os.environ.get('QYNO_RANDOM_CHECKED') else []),str(ROOT/'wide_runtime.c'),str(ROOT/'grapheme_runtime.c'),'-lm','-o',str(so)],check=True,capture_output=True)
        cls.library=ctypes.CDLL(str(so));cls.library.wide_execute.argtypes=[ctypes.POINTER(Request)]
        names=['W_CONVERT']+re.findall(r'W_[A-Z_]+',(ROOT/'wide_runtime.h').read_text().split('enum { W_CONVERT,',1)[1].split('};',1)[0])
        cls.op=names.index('W_RANDOM')

    def execute(self,r,state,*,status=0):
        c_state=(ctypes.c_uint64*4)(*state);r.state=c_state;wide=Request();wide.op=self.op;wide.input=ctypes.addressof(r)
        self.library.wide_execute(ctypes.byref(wide));self.assertEqual(r.status,status)
        if status:return list(c_state)
        capacity=r.capacity;output=ctypes.create_string_buffer(capacity+16)
        r.buffer=ctypes.addressof(output);r.phase=1
        self.library.wide_execute(ctypes.byref(wide));self.assertEqual(r.status,0)
        self.assertEqual(output.raw[capacity:],b'\0'*16)
        return list(c_state)

    def test_splitmix_and_xoshiro_full_width_vectors(self):
        for seed in [0,1,42,MASK]:
            reference=seeded(seed);r=RandomRequest(op=0,first=bits(seed));state=self.execute(r,[0]*4)
            self.assertEqual(state,reference)
            for _ in range(8):
                r=RandomRequest(op=1,ty=64);state=self.execute(r,state)
                self.assertEqual(number(r.out),word(reference));self.assertEqual(state,reference)
        self.assertEqual(word(seeded(42)),0x15780b2e0c2ec716)

    def test_signed_unsigned_all_widths_and_full_128_bit_values(self):
        for ty in [-8,-16,-32,-64,-128,8,16,32,64,128]:
            expected=seeded(42);r=RandomRequest(op=1,ty=ty);state=self.execute(r,expected)
            value=draw(expected,abs(ty))
            if ty<0 and value>>(abs(ty)-1):value-=1<<abs(ty)
            self.assertEqual(number(r.out),value% (1<<128));self.assertEqual(state,expected)

    def test_integer_rejection_and_clipping_match_exact_oracle(self):
        for ty,lo,hi in [(-8,-200,200),(-32,0,2**31-1),(32,0,2**31),(-128,-2**127,2**127-2),(128,0,2**128-2)]:
            expected=seeded(42);r=RandomRequest(op=1,ty=ty,flags=7,first_tag=-128 if lo<2**127 else 128,second_tag=-128 if hi<2**127 else 128,first=bits(lo),second=bits(hi))
            state=self.execute(r,expected);n=abs(ty);low=max(lo,0 if ty>0 else -(1<<(n-1)));high=min(hi,(1<<n)-1 if ty>0 else (1<<(n-1))-1)
            if high-low+1==1<<n:
                value=draw(expected,n)
                if ty<0 and value>>(n-1):value-=1<<n
            else:
                span=high-low+1;threshold=(1<<n)%span
                while True:
                    value=draw(expected,n)
                    if value>=threshold:break
                value=low+value%span
            self.assertEqual(number(r.out),value%(1<<128));self.assertEqual(state,expected)

    def test_invalid_and_singleton_ranges_do_not_advance_state(self):
        for low,high,flags in [(0,1,1),(5,4,7),(2**128-1,2**128-1,7)]:
            r=RandomRequest(op=1,phase=1,ty=-32,flags=flags,first_tag=128,second_tag=128,first=bits(low),second=bits(high))
            self.assertEqual(self.execute(r,seeded(42),status=70),seeded(42))
        r=RandomRequest(op=1,ty=-32,flags=7,first_tag=-128,second_tag=-128,first=bits(5),second=bits(5))
        self.assertEqual(self.execute(r,seeded(42)),seeded(42));self.assertEqual(number(r.out),5)

    def test_real_interpolation_rounds_exact_rational_once_at_every_precision(self):
        for ty,(p,e,bias) in FORMATS.items():
            one=bias<<(p-1);maxfinite=(((1<<e)-2)<<(p-1))|((1<<(p-1))-1);sign=1<<(p+e-1)
            cases=[(0,one),(sign|maxfinite,maxfinite),(1,9),(one,one+2),(0,maxfinite),(sign|one,one)]
            for a,b in cases:
                for closed in [0,2,4,6]:
                    lo=fraction(a,ty);hi=fraction(b,ty)
                    # Interior counts are checked independently by adjacent IEEE values.
                    low_raw=a if closed&2 else (1 if lo==0 else a-1 if lo<0 else a+1)
                    high_raw=b if closed&4 else ((sign|1) if hi==0 else b+1 if hi<0 else b-1)
                    if fraction(low_raw,ty)>fraction(high_raw,ty):continue
                    reference=seeded(42);r=RandomRequest(op=2,ty=ty,flags=1|closed,first_tag=ty,second_tag=ty,first=bits(a),second=bits(b))
                    state=self.execute(r,reference)
                    if fraction(low_raw,ty)==fraction(high_raw,ty):expected=0 if fraction(low_raw,ty)==0 else low_raw
                    else:
                        first=0 if closed&2 else 1;last=(1<<p)-(0 if closed&4 else 1)
                        while True:
                            k=first+uniform(reference,last-first+1)
                            expected=rounded(((1<<p)-k)*lo/(1<<p)+k*hi/(1<<p),ty)
                            value=fraction(expected,ty)
                            if (value>=lo if closed&2 else value>lo) and (value<=hi if closed&4 else value<hi):break
                    self.assertEqual(number(r.out),expected,(ty,a,b,closed));self.assertEqual(state,reference)

    def test_nonfinite_float_bounds_and_invalid_text_leave_state_unchanged(self):
        for ty,(p,e,bias) in FORMATS.items():
            for value in [((1<<e)-1)<<(p-1),(((1<<e)-1)<<(p-1))|1]:
                r=RandomRequest(op=2,ty=ty,flags=7,first_tag=ty,second_tag=ty,first=bits(0),second=bits(value))
                self.assertEqual(self.execute(r,seeded(42),status=70),seeded(42))
        text=ctypes.create_string_buffer(b'\xff');r=RandomRequest(op=4,text=ctypes.addressof(text),length=1)
        self.assertEqual(self.execute(r,seeded(42),status=65),seeded(42))

    def test_empty_float_intervals_and_singletons(self):
        for ty,(p,e,bias) in FORMATS.items():
            one=bias<<(p-1)
            r=RandomRequest(op=2,ty=ty,flags=1,first_tag=ty,second_tag=ty,first=bits(one),second=bits(one+1))
            self.assertEqual(self.execute(r,seeded(42),status=70),seeded(42))
            r=RandomRequest(op=2,ty=ty,flags=3,first_tag=ty,second_tag=ty,first=bits(0),second=bits(1))
            self.assertEqual(self.execute(r,seeded(42)),seeded(42));self.assertEqual(number(r.out),0)

if __name__=='__main__':run_suite()
