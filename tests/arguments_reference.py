"""Build a test-only PE that asks Windows msvcrt for argv via __wgetmainargs.

The fixture uses the compiler's PE as a container and GNU as for a small ABI
adapter. It does not use QYNO's argument parser or its generated startup code.
"""
import struct, subprocess

def build_crt_reference(template,directory):
    data=bytearray(template.read_bytes())
    u16=lambda at:struct.unpack_from('<H',data,at)[0]
    u32=lambda at:struct.unpack_from('<I',data,at)[0]
    pe=u32(0x3c);optional=pe+24;base=struct.unpack_from('<Q',data,optional+24)[0]
    sections=[]
    for i in range(u16(pe+6)):
        at=optional+u16(pe+20)+40*i
        sections.append((bytes(data[at:at+8]).rstrip(b'\0'),u32(at+12),u32(at+20),u32(at+16)))
    def offset(rva):
        for _,address,raw,size in sections:
            if address<=rva<address+size:return raw+rva-address
        raise AssertionError('unknown RVA')
    descriptor=offset(u32(optional+120));lookup=u32(descriptor);iat=u32(descriptor+16);slots={};index=0
    while True:
        hint=struct.unpack_from('<Q',data,offset(lookup+index*8))[0]
        if not hint:break
        at=offset(hint)+2;end=data.index(0,at);name=data[at:end].decode();slots[name]=base+iat+index*8
        replacement={'GetCurrentDirectoryW':'LoadLibraryW','SetCurrentDirectoryW':'GetProcAddress'}.get(name)
        if replacement:
            assert len(replacement)<=end-at
            data[at:at+len(replacement)+1]=replacement.encode()+b'\0'
        index+=1
    _,rva,raw,size=next(s for s in sections if s[0]==b'.rdata')
    payload='msvcrt.dll\0'.encode('utf-16-le');data[raw:raw+len(payload)]=payload
    payload=b'__wgetmainargs\0';data[raw+64:raw+64+len(payload)]=payload
    def call(name):return f'movabs rax, {slots[name]}\ncall qword ptr [rax]'
    source=f'''.intel_syntax noprefix
.text
sub rsp, 520
movabs rcx, {base+rva}
{call('GetCurrentDirectoryW')}
test rax, rax
jz error
mov rcx, rax
movabs rdx, {base+rva+64}
{call('SetCurrentDirectoryW')}
test rax, rax
jz error
mov qword ptr [rsp+152], rax
mov qword ptr [rsp+104], 0
lea rcx, [rsp+80]
lea rdx, [rsp+88]
lea r8, [rsp+96]
xor r9d, r9d
lea rax, [rsp+104]
mov qword ptr [rsp+32], rax
call qword ptr [rsp+152]
test eax, eax
jnz error
sub dword ptr [rsp+80], 1
add qword ptr [rsp+88], 8
mov rcx, -11
{call('GetStdHandle')}
mov qword ptr [rsp+64], rax
{call('GetProcessHeap')}
mov rcx, rax
mov edx, 8
mov r8d, 131072
{call('HeapAlloc')}
test rax, rax
jz error
mov qword ptr [rsp+72], rax
lea rdx, [rsp+80]
mov r8d, 4
call write
mov qword ptr [rsp+112], 0
again:
mov rax, qword ptr [rsp+112]
cmp eax, dword ptr [rsp+80]
jae done
mov rdx, qword ptr [rsp+88]
mov rdx, qword ptr [rdx+rax*8]
mov qword ptr [rsp+120], rdx
xor eax, eax
length:
cmp word ptr [rdx+rax*2], 0
je convert
inc eax
jmp length
convert:
mov dword ptr [rsp+128], eax
test eax, eax
jz converted
mov ecx, 65001
mov edx, 128
mov r8, qword ptr [rsp+120]
mov r9d, eax
mov rax, qword ptr [rsp+72]
mov qword ptr [rsp+32], rax
mov qword ptr [rsp+40], 131072
mov qword ptr [rsp+48], 0
mov qword ptr [rsp+56], 0
{call('WideCharToMultiByte')}
test eax, eax
jz error
converted:
mov dword ptr [rsp+136], eax
lea rdx, [rsp+136]
mov r8d, 4
call write
mov rdx, qword ptr [rsp+72]
mov r8d, dword ptr [rsp+136]
call write
add qword ptr [rsp+112], 1
jmp again
write:
sub rsp, 56
mov rcx, qword ptr [rsp+128]
lea r9, [rsp+208]
mov qword ptr [rsp+32], 0
{call('WriteFile')}
add rsp, 56
ret
done:
xor ecx, ecx
jmp exit
error:
mov ecx, 90
exit:
{call('ExitProcess')}
'''
    asm=directory/'crt.s';obj=directory/'crt.o';raw_code=directory/'crt.bin';exe=directory/'crt.exe'
    asm.write_text(source)
    subprocess.run(['gcc','-c',str(asm),'-o',str(obj)],check=True,capture_output=True)
    subprocess.run(['objcopy','-O','binary','--only-section=.text',str(obj),str(raw_code)],check=True,capture_output=True)
    code=raw_code.read_bytes();entry=u32(optional+16);at=offset(entry)
    text=next(s for s in sections if s[0]==b'.text');assert at+len(code)<=text[2]+text[3]
    data[at:at+len(code)]=code;exe.write_bytes(data)
    return exe

def read_crt_arguments(output):
    count=struct.unpack_from('<I',output)[0];position=4;result=[]
    for _ in range(count):
        length=struct.unpack_from('<I',output,position)[0];position+=4
        result.append(output[position:position+length].decode('utf-8'));position+=length
    assert position==len(output)
    return result
