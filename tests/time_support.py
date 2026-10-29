"""Run the same new standard-library cases as native ELF or Windows/Wine."""
import os
from pathlib import Path
import subprocess
from support import QynoTestCase, program

WINDOWS=os.environ.get('QYNO_FILE_TARGET')=='windows'
class PlatformCase(QynoTestCase):
    def main(self,*lines,prefix=''):
        return prefix+program('int main();',*('\t'+line for line in lines),'\treturn 0')

    def compile_source(self,source,**kwargs):
        if not WINDOWS or kwargs.get('error') is not None:return super().compile_source(source,**kwargs)
        self.sequence+=1;path=self.directory/f'case{self.sequence}.qy'
        path.write_text(source.replace('"'+str(self.directory),'"Z:'+str(self.directory)))
        binary=path.with_suffix('.exe')
        result=subprocess.run([str(self.compiler),str(path),'--target','windows','-o',str(binary)],capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(result.stderr,'');return binary

    def check_program(self,source,expected,**kwargs):
        if not WINDOWS:return super().check_program(source,expected,**kwargs)
        binary=self.compile_source(source)
        result=subprocess.run([os.environ['QYNO_FILE_WINDOWS_RUNNER'],str(binary)],input=kwargs.get('stdin','').encode(),capture_output=True,timeout=30)
        self.assertEqual(result.returncode,kwargs.get('status',0),result.stderr.decode(errors='replace'))
        self.assertEqual(result.stdout,expected.encode())
        if kwargs.get('error') is not None:self.assertIn(kwargs['error'],result.stderr.decode())
        else:self.assertEqual(result.stderr,b'')
        return result


def inject_windows_api_results(binary, results):
    """Replace selected PE import calls with ABI-compatible immediate results."""
    import struct
    data=bytearray(binary.read_bytes())
    u16=lambda offset:struct.unpack_from('<H',data,offset)[0]
    u32=lambda offset:struct.unpack_from('<I',data,offset)[0]
    pe=u32(0x3c);optional=pe+24;sections=[]
    for i in range(u16(pe+6)):
        start=optional+u16(pe+20)+40*i
        sections.append((u32(start+12),u32(start+20),u32(start+16)))
    def offset(rva):
        for base,raw,size in sections:
            if base<=rva<base+size:return raw+rva-base
        raise AssertionError('invalid PE RVA')
    descriptor=offset(u32(optional+120));slots={}
    while u32(descriptor):
        lookup=u32(descriptor);iat=u32(descriptor+16);index=0
        while True:
            hint=struct.unpack_from('<Q',data,offset(lookup+index*8))[0]
            if not hint:break
            start=offset(hint)+2;end=data.index(0,start)
            slots[data[start:end].decode()]=iat+index*8;index+=1
        descriptor+=20
    for name,result in results.items():
        count=0
        for base,raw,size in sections:
            for pos in range(raw,raw+size-6):
                if data[pos:pos+2]!=b'\xff\x15':continue
                target=base+pos-raw+6+struct.unpack_from('<i',data,pos+2)[0]
                if target==slots[name]:
                    data[pos:pos+6]=b'\xb8'+struct.pack('<I',result)+b'\x90';count+=1
        assert count, 'API import call not found: '+name
    binary.write_bytes(data)
