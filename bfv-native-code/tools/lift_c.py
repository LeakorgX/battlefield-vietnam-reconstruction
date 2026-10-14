"""L1 lifter: translate native functions into mechanical, verifiable C.

Each instruction becomes C over integer-register and flag locals (see
lifted/runtime/l1.h). x87/MMX/3DNow!/SSE instructions are kept as single
one-instruction asm statements so floating-point results stay bit-exact; their
memory operands become a C-computed address and their general-register operands
become bound C locals. Guest memory goes through volatile accessors, so reads and
writes keep the native order, width and count.

Calls push a resume stub as the return address and leave through the shared
trampoline; the callee returns into the stub, which re-enters the C body after
the call. Jumps that leave the function (tail calls, indirect jumps) continue at
their native target. Anything not handled is reported with the reason. Output (generated, not hand-edited):
  lifted/<target>/lifted_NNN.c    C bodies plus native-ABI entry stubs (shards)
  lifted/<target>/catalog.json    entries, rejection counts, unhandled mnemonics
"""
import argparse
import collections
import csv
import hashlib
import json
import re
from pathlib import Path
import pefile
from capstone import Cs,CS_ARCH_X86,CS_MODE_32,CS_OP_REG,CS_OP_IMM,CS_OP_MEM
from capstone import x86_const as X
from build import PROJECT,GAME,TARGETS

REPORTS=PROJECT.parent/'reports'
GAP=0x400;STACK_LIMIT=GAP-0x80;DEEP_LIMIT=0xe00;SHARD=500  # deeper bodies use their own gap (BFV_L1_ENTRY_DEEP)
R32=['eax','ecx','edx','ebx','esp','ebp','esi','edi']
GPR={}  # capstone register name -> (local, kind) kind: 32, 16, 'l', 'h'
for r in R32:GPR[r]=(r,32)
for r in R32:GPR[r[1:]]=(r,16)
for r in 'acdb':GPR[r+'l']=('e'+r+'x','l');GPR[r+'h']=('e'+r+'x','h')
WIDTH_BITS={1:8,2:16,4:32}
MASK={8:'0xffu',16:'0xffffu',32:'0xffffffffu'}
COND={'o':'of','no':'!of','b':'cf','ae':'!cf','e':'zf','ne':'!zf','be':'(cf | zf)','a':'!(cf | zf)',
      's':'sf','ns':'!sf','p':'pf','np':'!pf','l':'(sf != of)','ge':'(sf == of)',
      'le':'(zf | (sf != of))','g':'!(zf | (sf != of))'}
ALIAS={'c':'b','nae':'b','nb':'ae','nc':'ae','z':'e','nz':'ne','na':'be','nbe':'a','pe':'p','po':'np',
       'nge':'l','nl':'ge','ng':'le','nle':'g'}
SIMD_GROUPS={'fpu','mmx','sse1','sse2','sse3','ssse3','sse41','sse42','3dnow'}
FLAG_SETTING_FP={'fcomi','fcomip','fucomi','fucomip','comiss','ucomiss','comisd','ucomisd'}
SIZELESS={'fnsave','fsave','frstor','fnstenv','fstenv','fldenv','fxsave','fxrstor'}
SIZE_WORD={'byte':'byte','word':'word','dword':'dword','qword':'qword','xmmword':'xmmword','xword':'tbyte',
           'tbyte':'tbyte'}

class Reject(Exception):pass

STRING_OPS={p+w for p in ('movs','stos','lods','cmps','scas') for w in 'bwd'}

def cond(suffix):
    suffix=ALIAS.get(suffix,suffix)
    if suffix not in COND:raise Reject(f'condition {suffix}')
    return COND[suffix]

class Lifter:
    def __init__(self,md):self.md=md

    # ---- operands
    def ea(self,i,op):
        m=op.mem
        if m.segment not in (0,X.X86_REG_FS):raise Reject('segment')  # fs: is handled by read()/write()
        parts=[]
        if m.base:parts.append(self.reg32(i.reg_name(m.base)))
        if m.index:parts.append(f'{self.reg32(i.reg_name(m.index))} * {m.scale}u' if m.scale>1 else self.reg32(i.reg_name(m.index)))
        if m.disp or not parts:parts.append(f'0x{m.disp&0xffffffff:08x}u')
        return '(uint32_t)(' + ' + '.join(parts) + ')'

    def reg32(self,name):
        if name not in GPR or GPR[name][1]!=32:raise Reject(f'address register {name}')
        return name

    def read(self,i,op,ea=None):
        if op.type==CS_OP_IMM:return f'0x{op.imm&((1<<(8*op.size))-1):x}u'
        if op.type==CS_OP_REG:
            name=i.reg_name(op.reg)
            if name not in GPR:raise Reject(f'register {name}')
            local,kind=GPR[name]
            return {32:local,16:f'LO16({local})','l':f'LO8({local})','h':f'HI8({local})'}[kind]
        a=ea or self.ea(i,op)
        if op.mem.segment==X.X86_REG_FS:
            if op.size!=4:raise Reject('fs width')
            return f'bfv_fs_read32({a})'
        return f'{ {1:"M8",2:"M16",4:"M32"}[op.size] }({a})'

    def write(self,i,op,value,ea=None):
        if op.type==CS_OP_REG:
            local,kind=GPR[i.reg_name(op.reg)]
            return {32:f'{local} = {value};',16:f'SET16({local}, {value});','l':f'SET8L({local}, {value});',
                    'h':f'SET8H({local}, {value});'}[kind]
        a=ea or self.ea(i,op)
        if op.mem.segment==X.X86_REG_FS:
            if op.size!=4:raise Reject('fs width')
            return f'bfv_fs_write32({a}, {value});'
        return f'{ {1:"M8",2:"M16",4:"M32"}[op.size] }({a}) = {value};'

    def mem_ea(self,i,ops,out):
        """Compute each memory operand's address once (read-modify-write order)."""
        eas={}
        for n,op in enumerate(ops):
            if op.type==CS_OP_MEM:out.append(f'ea = {self.ea(i,op)};');eas[n]='ea'
        return eas

    # ---- instructions
    def lift(self,i,labels):
        m=i.mnemonic;ops=list(i.operands);out=[]
        groups={i.group_name(g) for g in i.groups}
        vector=any(op.type==CS_OP_REG and i.reg_name(op.reg)[:2] in ('mm','xm') for op in ops)
        if groups&SIMD_GROUPS or vector or m[0]=='f' or m in ('wait','emms','femms'):return self.simd(i,m,ops)
        if i.prefix[0] in (0xf3,0xf2) and m.split()[0] in ('rep','repe','repne'):return self.string(i,m)
        if m in STRING_OPS and i.prefix[0]==0:return self.string(i,'once '+m)
        if i.prefix[0]!=0:raise Reject(f'prefix {m}')
        if m=='nop':return out
        if m=='lea':return [self.write(i,ops[0],self.ea(i,ops[1]))]
        W=WIDTH_BITS.get(ops[0].size,32) if ops else 32
        eas=self.mem_ea(i,ops,out)
        rd=lambda n:self.read(i,ops[n],eas.get(n))
        wr=lambda n,v:self.write(i,ops[n],v,eas.get(n))
        mask=MASK[W]
        if m=='mov':out.append(wr(0,rd(1)));return out
        if m in ('movzx','movsx'):
            src=rd(1);sw=WIDTH_BITS[ops[1].size]
            v=src if m=='movzx' else f'(uint32_t)(int32_t)(int{sw}_t){src}'
            out.append(wr(0,f'({v}) & {mask}' if m=='movsx' and W<32 else v));return out
        if m=='push':
            if ops[0].type==CS_OP_IMM:out.append(f't = 0x{ops[0].imm&0xffffffff:x}u;')
            elif ops[0].size==4:out.append(f't = {rd(0)};')
            else:raise Reject('push16')
            out+=['esp -= 4;','M32(esp) = t;'];return out
        if m=='pop':
            if ops[0].size!=4:raise Reject('pop16')
            out=['t = M32(esp);','esp += 4;']
            if ops[0].type==CS_OP_MEM:out.append(f'ea = {self.ea(i,ops[0])};');out.append(self.write(i,ops[0],'t','ea'))
            else:out.append(self.write(i,ops[0],'t'))
            return out
        if m in ('add','sub','cmp','adc','sbb'):
            out+=[f'a = {rd(0)};',f'b = {self.read(i,ops[1],eas.get(1)) if ops[1].type!=CS_OP_IMM else self.imm(ops[1],W)};']
            if m in ('add','adc'):
                out+=[f'r = (a + b{" + cf" if m=="adc" else ""}) & {mask};']
                out+=[f'F_ADD({W}, a, b, r);' if m=='add' else f'F_ADC({W}, a, b, c0, r);']
                if m=='adc':out.insert(-2,'c0 = cf;')
            else:
                out+=[f'r = (a - b{" - cf" if m=="sbb" else ""}) & {mask};']
                out+=[f'F_SUB({W}, a, b, r);' if m!='sbb' else f'F_SBB({W}, a, b, c0, r);']
                if m=='sbb':out.insert(-2,'c0 = cf;')
            if m!='cmp':out.append(wr(0,'r'))
            return out
        if m in ('and','or','xor','test'):
            op={'and':'&','test':'&','or':'|','xor':'^'}[m]
            b=self.read(i,ops[1],eas.get(1)) if ops[1].type!=CS_OP_IMM else self.imm(ops[1],W)
            if m=='xor' and ops[0].type==CS_OP_REG and ops[1].type==CS_OP_REG and ops[0].reg==ops[1].reg:
                out+=['r = 0;']
            else:out+=[f'r = ({rd(0)} {op} {b}) & {mask};']
            out.append(f'F_LOGIC({W}, r);')
            if m!='test':out.append(wr(0,'r'))
            return out
        if m in ('inc','dec','neg','not'):
            out.append(f'a = {rd(0)};')
            if m=='not':out.append(wr(0,f'~a & {mask}'));return out
            expr={'inc':'a + 1u','dec':'a - 1u','neg':'0u - a'}[m]
            out+=[f'r = ({expr}) & {mask};',f'F_{m.upper()}({W}, a, r);',wr(0,'r')];return out
        if m in ('shl','sal','shr','sar','rol','ror'):return out+self.shift(i,m,ops,W,eas)
        if m=='imul' and len(ops)>=2:
            a=rd(1) if len(ops)==3 else rd(0)
            b=self.imm(ops[2],W) if len(ops)==3 else rd(1)
            st=f'int{W}_t'
            out+=[f'p = (int64_t)({st}){a} * (int64_t)({st}){b};',f'r = (uint32_t)p & {mask};',
                  f'cf = of = p != (int64_t)({st})r;',self.write(i,ops[0],'r',eas.get(0))];return out
        if m in ('mul','imul'):
            src=rd(0)
            if W==32:
                if m=='mul':out.append(f'u = (uint64_t)eax * {src};')
                else:out.append(f'u = (uint64_t)((int64_t)(int32_t)eax * (int32_t){src});')
                out+=['eax = (uint32_t)u;','edx = (uint32_t)(u >> 32);',
                      'cf = of = ' + ('edx != 0;' if m=='mul' else '(int64_t)u != (int32_t)eax;')];return out
            if W==8:
                if m=='mul':out.append(f'r = (uint32_t)LO8(eax) * {src};');out+=['SET16(eax, r);','cf = of = (r >> 8) != 0;']
                else:out+=[f'r = (uint32_t)(int32_t)((int8_t)LO8(eax) * (int8_t){src}) & 0xffffu;','SET16(eax, r);',
                           'cf = of = (int16_t)r != (int8_t)r;']
                return out
            raise Reject('mul16')
        if m in ('div','idiv'):
            src=rd(0);s='b' if W==8 else 'w' if W==16 else 'l'
            if W==16:raise Reject('div16')
            out.append(f't = {src};')
            if W==32:out.append(f'__asm__ volatile("{m}l %2" : "+a"(eax), "+d"(edx) : "r"(t));')
            else:out.append(f'__asm__ volatile("{m}b %b1" : "+a"(eax) : "q"(t));')
            return out
        if m=='cdq':return ['edx = (uint32_t)((int32_t)eax >> 31);']
        if m=='cwde':return ['eax = (uint32_t)(int32_t)(int16_t)LO16(eax);']
        if m=='cbw':return ['SET16(eax, (uint16_t)(int16_t)(int8_t)LO8(eax));']
        if m=='cwd':return ['SET16(edx, ((int16_t)LO16(eax) < 0) ? 0xffffu : 0u);']
        if m=='xchg':
            out+=[f'a = {rd(0)};',f'b = {rd(1)};',wr(0,'b'),wr(1,'a')];return out
        if m.startswith('set'):out.append(wr(0,f'({cond(m[3:])}) ? 1u : 0u'));return out
        if m.startswith('cmov'):
            out+=[f't = {rd(1)};',f'if ({cond(m[4:])}) {wr(0,"t")}'];return out
        if m=='leave':return ['esp = ebp;','ebp = M32(esp);','esp += 4;']
        if m=='sahf':return ['SAHF_LOAD(HI8(eax));']
        if m=='lahf':return ['SET8H(eax, FLAGS_WORD());']
        if m in ('cld','std'):return [f'df = {int(m=="std")};']
        if m=='clc':return ['cf = 0;']
        if m=='stc':return ['cf = 1;']
        if m=='cmc':return ['cf ^= 1u;']
        if m=='pushfd':return ['esp -= 4;','M32(esp) = (eflags_rest & ~FLAG_MASK) | FLAGS_WORD();']
        if m=='popfd':return ['t = M32(esp);','esp += 4;','FLAGS_LOAD(t);','eflags_rest = t;']
        if m=='bswap':return [wr(0,f'__builtin_bswap32({rd(0)})')]
        if m=='bt' and ops[0].type==CS_OP_REG:
            b=self.imm(ops[1],8) if ops[1].type==CS_OP_IMM else rd(1)
            return [f'cf = ({rd(0)} >> ({b} & {W-1}u)) & 1u;']
        if m in ('bt','bts','btr','btc') and (ops[0].type==CS_OP_REG or ops[1].type==CS_OP_IMM):
            # a register bit offset into memory may address outside the operand: not handled
            b=self.imm(ops[1],8) if ops[1].type==CS_OP_IMM else rd(1)
            out+=[f'n = {b} & {W-1}u;',f'a = {rd(0)};','cf = (a >> n) & 1u;']
            if m!='bt':
                expr={'bts':'a | (1u << n)','btr':'a & ~(1u << n)','btc':'a ^ (1u << n)'}[m]
                out.append(wr(0,f'({expr}) & {mask}'))
            return out
        if m in ('bt','bts','btr','btc') and ops[0].type==CS_OP_MEM and ops[1].type==CS_OP_REG and W in (16,32):
            # a register bit offset is signed and selects the word around the operand
            acc={16:'M16',32:'M32'}[W];sh={16:4,32:5}[W];st={16:'int16_t',32:'int32_t'}[W]
            out+=[f'b = {rd(1)};',f'ea = (uint32_t)({self.ea(i,ops[0])}) + (uint32_t)(({st})b >> {sh}) * {W//8}u;',
                  f'n = b & {W-1}u;',f'a = {acc}(ea);','cf = (a >> n) & 1u;']
            if m!='bt':
                expr={'bts':'a | (1u << n)','btr':'a & ~(1u << n)','btc':'a ^ (1u << n)'}[m]
                out.append(f'{acc}(ea) = ({expr}) & {mask};')
            return out
        if m=='xlatb':return ['SET8L(eax, M8(ebx + LO8(eax)));']
        if m=='pushal':
            return ['t = esp;',*[f'esp -= 4; M32(esp) = {r};' for r in ('eax','ecx','edx','ebx','t','ebp','esi','edi')]]
        if m=='popal':
            return [*[f'{r} = M32(esp); esp += 4;' for r in ('edi','esi','ebp')],'esp += 4;',
                    *[f'{r} = M32(esp); esp += 4;' for r in ('ebx','edx','ecx','eax')]]
        if m=='cpuid':return ['__asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx));']
        if m=='rdtsc':return ['__asm__ volatile("rdtsc" : "=a"(eax), "=d"(edx));']
        if m=='ret':
            n=ops[0].imm if ops else 0
            return ['c->eip = M32(esp);',f'esp += {4+n};','goto out;']
        if m=='call':
            # Push a resume stub as the return address and leave through the
            # trampoline; the callee returns into the stub, which re-enters this
            # body at the label after the call. When the code after the call is
            # not part of this function (a call that does not return, as far as
            # the function extent shows), the resume point continues natively.
            ret=i.address+i.size;k=len(self.resumes)+1;stub=f'bfv_{self.target}_l1r_{self.start:08x}_{k}'
            self.resumes.append((k,stub,ret))
            dest=f'0x{ops[0].imm:08x}u' if ops[0].type==CS_OP_IMM else rd(0)
            tail=[] if ret in self.inside else [f'c->eip = 0x{ret:08x}u;','goto out;']
            return [*out,f't = {dest};','esp -= 4;',f'M32(esp) = (uint32_t)(uintptr_t){stub};','c->eip = t;',
                    'goto out;',f'R_{k}:',*tail]
        if m=='jmp':
            if ops[0].type!=CS_OP_IMM:return [*out,f'c->eip = {rd(0)};','goto out;']  # continues natively
            if ops[0].imm not in self.inside:return [f'c->eip = 0x{ops[0].imm:08x}u;','goto out;']
            return [f'goto L_{ops[0].imm:08x};']
        if m in ('jecxz','loop','loope','loopne'):
            go=f'goto L_{ops[0].imm:08x};' if ops[0].imm in self.inside else f'{{ c->eip = 0x{ops[0].imm:08x}u; goto out; }}'
            if m=='jecxz':return [f'if (!ecx) {go}']
            extra={'loop':'','loope':' && zf','loopne':' && !zf'}[m]
            return ['ecx--;',f'if (ecx{extra}) {go}']
        if m.startswith('j'):
            if ops[0].imm not in self.inside:
                return [f'if ({cond(m[1:])}) {{ c->eip = 0x{ops[0].imm:08x}u; goto out; }}']
            return [f'if ({cond(m[1:])}) goto L_{ops[0].imm:08x};']
        if m in ('shld','shrd') and W==32:
            cnt=self.imm(ops[2],8) if ops[2].type==CS_OP_IMM else 'LO8(ecx)'
            out+=[f'n = {cnt} & 31u;','if (n) {',f'    a = {rd(0)};',f'    b = {rd(1)};']
            if m=='shld':out+=['    r = (a << n) | (n == 0 ? 0u : b >> (32u - n));','    cf = (a >> (32u - n)) & 1u;']
            else:out+=['    r = (a >> n) | (n == 0 ? 0u : b << (32u - n));','    cf = (a >> (n - 1u)) & 1u;']
            out+=['    of = SIGN(32, r) ^ SIGN(32, a);','    F_SZP(32, r); af = 0;','    '+wr(0,'r'),'}']
            return out
        if m in ('rcl','rcr') and W==32 and (len(ops)==1 or ops[1].type==CS_OP_IMM and ops[1].imm==1):
            out.append(f'a = {rd(0)};')
            if m=='rcr':out+=['r = (a >> 1) | (cf << 31);','of = SIGN(32, a) ^ cf;','cf = a & 1u;']
            else:out+=['r = (a << 1) | cf;','cf = SIGN(32, a);','of = SIGN(32, r) ^ cf;']
            out.append(wr(0,'r'));return out
        raise Reject(f'mnemonic {m}')

    def imm(self,op,W):
        return f'0x{op.imm&((1<<W)-1):x}u'

    def shift(self,i,m,ops,W,eas):
        out=[]
        cnt=self.imm(ops[1],8) if len(ops)>1 and ops[1].type==CS_OP_IMM else ('LO8(ecx)' if len(ops)>1 else '1u')
        mem=ops[0].type==CS_OP_MEM  # a zero count still reads and writes memory back
        if mem:out+=[f'n = {cnt} & 31u;',f'a = {self.read(i,ops[0],eas.get(0))};','r = a;','if (n) {']
        else:out+=[f'n = {cnt} & 31u;','if (n) {',f'    a = {self.read(i,ops[0],eas.get(0))};']
        sw=f'int{W}_t'
        if m in ('shl','sal'):
            out+=[f'    r = (uint32_t)((uint64_t)a << n) & {MASK[W]};',f'    cf = n <= {W} ? (uint32_t)(((uint64_t)a >> ({W} - n)) & 1u) : 0u;',
                  f'    of = SIGN({W}, r) ^ cf;','    F_SZP(%d, r); af = 0;'%W]
        elif m=='shr':
            out+=[f'    r = n < 32 ? a >> n : 0u;',f'    cf = (uint32_t)(((uint64_t)a >> (n - 1)) & 1u);',
                  f'    of = SIGN({W}, a);','    F_SZP(%d, r); af = 0;'%W]
        elif m=='sar':
            out+=[f'    r = (uint32_t)((int64_t)({sw})a >> n) & {MASK[W]};',
                  f'    cf = (uint32_t)(((int64_t)({sw})a >> (n - 1)) & 1u);','    of = 0;','    F_SZP(%d, r); af = 0;'%W]
        else:
            k=f'(n % {W}u)' if W!=32 else 'n'
            if m=='rol':out+=[f'    r = {k} ? ((a << {k}) | (a >> ({W}u - {k}))) & {MASK[W]} : a;','    cf = r & 1u;',f'    of = SIGN({W}, r) ^ cf;']
            else:out+=[f'    r = {k} ? ((a >> {k}) | (a << ({W}u - {k}))) & {MASK[W]} : a;',f'    cf = SIGN({W}, r);',
                       f'    of = SIGN({W}, r) ^ ((r >> {W-2}) & 1u);']
        out+=['}',self.write(i,ops[0],'r',eas.get(0))] if mem else ['    '+self.write(i,ops[0],'r',eas.get(0)),'}']
        return out

    def string(self,i,m):
        """String instructions; `once` is the unprefixed single step."""
        rep,op=m.split()[:2]
        if op not in STRING_OPS:raise Reject(f'string {m}')
        size={'b':1,'w':2,'d':4}[op[-1]];acc={1:'M8',2:'M16',4:'M32'}[size];W=8*size
        step=f'(df ? -{size}u : {size}u)';reg={1:'LO8(eax)',2:'LO16(eax)',4:'eax'}[size]
        setreg={1:'SET8L(eax, {});',2:'SET16(eax, {});',4:'eax = {};'}[size]
        kind=op[:4]
        if kind=='movs':body=[f'{acc}(edi) = {acc}(esi);',f'esi += {step};',f'edi += {step};']
        elif kind=='stos':body=[f'{acc}(edi) = {reg};',f'edi += {step};']
        elif kind=='lods':body=[setreg.format(f'{acc}(esi)'),f'esi += {step};']
        elif kind=='cmps':body=[f'b = {acc}(edi);',f'a = {acc}(esi);',f'r = (a - b) & {MASK[W]};',f'F_SUB({W}, a, b, r);',
                                f'esi += {step};',f'edi += {step};']
        else:body=[f'a = {reg};',f'b = {acc}(edi);',f'r = (a - b) & {MASK[W]};',f'F_SUB({W}, a, b, r);',f'edi += {step};']
        if rep=='once':return body
        if kind in ('cmps','scas'):
            if rep=='rep':rep='repe'  # f3 on cmps/scas is repe
            stop={'repe':'!zf','repne':'zf'}[rep]
            return ['while (ecx) {',*['    '+b for b in body],'    ecx--;',f'    if ({stop}) break;','}']
        if rep!='rep':raise Reject(f'string {m}')
        return ['while (ecx) {',*['    '+b for b in body],'    ecx--;','}']

    def simd(self,i,m,ops):
        """One native instruction in an asm statement with bound operands.

        The instruction keeps its Intel spelling; a memory operand becomes
        `[reg]` holding the C-computed address and a general register becomes the
        bound C local (%k/%w/%b/%h select the width)."""
        if i.eflags and i.eflags&(X.X86_EFLAGS_TEST_CF|X.X86_EFLAGS_TEST_ZF|X.X86_EFLAGS_TEST_PF):
            raise Reject(f'flag-reading {m}')
        texts=[t.strip() for t in i.op_str.split(', ')] if i.op_str else []
        plain=all(op.type==CS_OP_REG and i.reg_name(op.reg) not in GPR for op in ops)
        if plain and m not in FLAG_SETTING_FP:  # x87/SIMD registers only: keep the native text
            return [f'__asm__ volatile(".intel_syntax noprefix\\n\\t{i.op_str and m+" "+i.op_str or m}\\n\\t.att_syntax prefix");']
        if len(texts)!=len(ops):raise Reject(f'operand split {m}')
        outs=[];ins=[];pre=[];post=[];memory=False;pieces=[]
        for op,text in zip(ops,texts):
            if op.type==CS_OP_MEM:
                if op.mem.segment:raise Reject(f'segment {m}')
                pre.append(f'ea = {self.ea(i,op)};')
                prefix=text.split('[')[0].strip()
                if m in SIZELESS:prefix=''  # state images: gas takes no size here
                if prefix:
                    w=prefix.split()[0]
                    if w not in SIZE_WORD:raise Reject(f'memory size {w}')
                    prefix=SIZE_WORD[w]+' ptr '
                ins.append(('r','ea'));pieces.append(('in',len(ins)-1,prefix));memory=True
            elif op.type==CS_OP_REG and i.reg_name(op.reg) in GPR:
                local,kind=GPR[i.reg_name(op.reg)]
                # bind the native register itself (fnstsw only accepts AX)
                pin={'eax':'a','ecx':'c','edx':'d','ebx':'b','esi':'S','edi':'D'}.get(local,'r')
                outs.append(('+'+pin,local))
                pieces.append(('out',len(outs)-1,{32:'k',16:'w','l':'b','h':'h'}[kind]))
            else:pieces.append(('text',text,None))
        flags=None
        if m in FLAG_SETTING_FP:
            outs.append(('=r','t'));flags=len(outs)-1
            post.append('FLAGS_LOAD((t & 0x8d5u) | (df << 10));')
        def operand(piece):
            kind,value,extra=piece
            if kind=='in':return f'{extra}[%k{len(outs)+value}]'
            if kind=='out':return f'%{extra}{value}'
            return value
        text=m+(' '+', '.join(operand(x) for x in pieces) if pieces else '')
        asm=f'.intel_syntax noprefix\\n\\t{text}\\n\\t.att_syntax prefix'
        if flags is not None:asm+=f'\\n\\tpushfl\\n\\tpopl %k{flags}'
        o=', '.join(f'"{c}"({v})' for c,v in outs);n_=', '.join(f'"{c}"({v})' for c,v in ins)
        clobbers='"memory", "cc"' if memory else '"cc"'
        return [*pre,f'__asm__ volatile("{asm}" : {o} : {n_} : {clobbers});',*post]

TRAPS=('int3','int','hlt','ud2','into','iretd')
BRANCHES=('jecxz','loop','loope','loopne')

def successors(i,inside_range):
    """Native successors of an instruction that stay in the function, and whether it
    can fall through. `inside_range(t)` says whether a direct target belongs to it."""
    m=i.mnemonic;ops=i.operands;ft=i.address+i.size
    if m=='ret' or m in TRAPS:return []
    if m=='jmp':return [ops[0].imm] if ops[0].type==CS_OP_IMM and inside_range(ops[0].imm) else []
    if m.startswith('j') or m in BRANCHES:
        return ([ops[0].imm] if inside_range(ops[0].imm) else [])+[ft]
    return [ft]

def extent(pe,md,a,limit):
    """Instructions reachable from entry `a` without leaving [a, limit) (the next
    indexed entry): internal branches are followed, other targets are exits. The
    code after a call is followed only when it lies in range and is not padding,
    so calls that never return end the path. Falling through into the next
    entry and reaching a trap instruction are exits too. Returns (sorted insns, reason)."""
    base=pe.OPTIONAL_HEADER.ImageBase;inside_range=lambda t:a<=t<limit
    found={};work=[a]
    while work:
        x=work.pop()
        if x in found:continue
        if len(found)>20000:return None,'too_large'
        code=pe.get_data(x-base,16);i=next(md.disasm(code,x),None)
        if i is None:return None,'undecoded'
        if i.mnemonic in TRAPS:
            if x==a:return None,'trap'
            continue  # an exit: the native trap runs as itself
        found[x]=i
        nxt=successors(i,inside_range)
        if i.mnemonic=='call':
            ft=i.address+i.size;peek=next(md.disasm(pe.get_data(ft-base,16),ft),None)
            if not inside_range(ft) or peek is None or peek.mnemonic in TRAPS:nxt=[]
        for t in nxt:
            if inside_range(t):work.append(t)  # else falls into the next entry: an exit
    insns=sorted(found.values(),key=lambda i:i.address)
    for p,q in zip(insns,insns[1:]):
        if p.address+p.size>q.address:return None,'overlapping'
    return insns,None

def stack_depth(insns,inside,restarts=None):
    """Deepest guest-stack use below the ESP at which the C body was (re)entered.

    The C frame is rebuilt BFV_L1_GAP bytes below the guest ESP on entry and on
    every resume after a call, so only pushes within one call-free segment
    matter. Offsets restart at 0 after a call (callee clean-up is not needed).
    Frame teardown (`leave`, `mov esp, ebp`) only raises ESP and also restarts
    the count. `lea esp, [ebp+d]` with an EBP the function did not derive from
    ESP (set by a prolog helper such as __SEH_prolog) restores the function's own
    frame: the body leaves through a resume stub right after it, which rebuilds
    the C frame below the new ESP, and the count restarts there too (`restarts`
    collects these instructions). Returns None if ESP is assigned in any other
    untracked way."""
    index={i.address:i for i in insns};start=insns[0].address
    state={start:(0,None)};work=[start];deepest=0;steps=0
    for i in insns:  # resume points after calls restart at 0
        ft=i.address+i.size
        if i.mnemonic=='call' and ft in inside and ft not in state:state[ft]=(0,None);work.append(ft)
    while work:
        steps+=1
        if steps>20*len(insns)+100:return None
        a=work.pop();sp,bp=state[a];i=index[a];m=i.mnemonic;ops=i.operands
        reg=lambda n:len(ops)>n and ops[n].type==CS_OP_REG and i.reg_name(ops[n].reg)
        if m in ('push','pushfd'):sp-=4
        elif m=='pushal':sp-=32
        elif m=='popal':sp+=32
        elif m in ('pop','popfd'):
            if reg(0)=='esp':return None
            sp+=4
            if reg(0)=='ebp':bp=None
        elif m in ('sub','add') and reg(0)=='esp':
            if ops[1].type!=CS_OP_IMM:return None
            sp+=-ops[1].imm if m=='sub' else ops[1].imm
        elif m=='mov' and reg(0)=='ebp' and reg(1)=='esp':bp=sp
        elif m=='leave' or (m=='mov' and reg(0)=='esp' and reg(1)=='ebp'):
            sp=(bp if bp is not None else 0)+(4 if m=='leave' else 0);bp=None
            if m=='mov':bp=sp
        elif m=='call':continue  # the resume point was seeded above
        elif m=='lea' and reg(0)=='esp' and ops[1].mem.index==0:
            base=ops[1].mem.base and i.reg_name(ops[1].mem.base)
            if base=='esp':sp+=ops[1].mem.disp  # `lea esp, [esp]` is alignment padding
            elif base=='ebp' and bp is not None:sp=bp+ops[1].mem.disp
            elif base=='ebp' and restarts is not None:restarts.add(i.address);sp=0
            else:return None
        elif m=='and' and reg(0)=='esp' and ops[1].type==CS_OP_IMM and (ops[1].imm&0xffffffff)>=0xfffff000:
            sp-=(~ops[1].imm)&0xfff  # stack alignment lowers ESP by at most this much
        elif reg(0) in ('esp','sp') and m not in ('cmp','test','push'):return None
        elif m=='lea' and reg(0)=='ebp' and ops[1].mem.index==0 and ops[1].mem.base and i.reg_name(ops[1].mem.base)=='esp':
            bp=sp+ops[1].mem.disp
        elif reg(0) in ('ebp','bp') and m not in ('cmp','test','push'):bp=None
        deepest=min(deepest,sp)
        for t in successors(i,lambda t:t in inside):
            if t not in inside:continue
            old=state.get(t)
            merged=(sp,bp) if old is None else (min(old[0],sp),old[1] if old[1]==bp else None)
            if merged!=old:state[t]=merged;work.append(t)
    return -deepest

def lift_function(lifter,target,address,insns,gap=None,restarts=()):
    inside={i.address for i in insns}
    lifter.target,lifter.start,lifter.inside,lifter.resumes=target,address,inside,[]
    labels={o.imm for i in insns if i.mnemonic.startswith('j') or i.mnemonic in BRANCHES for o in i.operands
            if o.type==CS_OP_IMM and o.imm in inside}
    gaps=set()  # fall-through to an instruction that is not emitted next
    for n,i in enumerate(insns):
        ft=i.address+i.size;nxt=insns[n+1].address if n+1<len(insns) else None
        if ft in inside and ft!=nxt and ft in successors(i,lambda t:t in inside):gaps.add(i.address)
        if i.mnemonic=='call' and ft in inside and ft!=nxt:gaps.add(i.address)
    labels|={i.address+i.size for i in insns if i.address in gaps}
    body=[]
    for i in insns:
        if i.address in labels:body.append(f'L_{i.address:08x}:')
        text=f'{i.mnemonic} {i.op_str}'.strip()
        lines=lifter.lift(i,labels)
        body.append(f'    /* {i.address:08x}  {text} */')
        body+=[l if l.startswith('R_') else '    '+l for l in lines]
        if i.address in restarts:  # ESP restored from EBP: rebuild the C frame below it
            k=len(lifter.resumes)+1;name=f'bfv_{target}_l1r_{address:08x}_{k}'
            lifter.resumes.append((k,name,i.address+i.size))
            body+=[f'    c->eip = (uint32_t)(uintptr_t){name};','    goto out;',f'R_{k}:']
        if i.address in gaps:body.append(f'    goto L_{i.address+i.size:08x};')
        elif falls_out(i,inside):body.append(f'    c->eip = 0x{i.address+i.size:08x}u; goto out;')
    stub=f'bfv_{target}_l1_{address:08x}';fn=f'bfv_{target}_l1_body_{address:08x}'
    src='\n'.join(l for l in body if not l.lstrip().startswith('/*'))
    src=re.sub(r'"(?:[^"\\]|\\.)*"','""',src)  # asm templates are not C code
    used=lambda name:re.search(r'\b'+name+r'\b',src) is not None
    temps=[v for v in ('t','a','b','r','n','c0','ea') if used(v)]
    if gap:
        head=[f'BFV_L1_ENTRY_DEEP({stub}, {fn}, {gap:#x});']
        head+=[f'BFV_L1_RESUME_DEEP({name}, {fn}, {k}, {gap:#x});' for k,name,_ in lifter.resumes]
    else:
        head=[f'BFV_L1_ENTRY({stub}, {fn});']
        head+=[f'BFV_L1_RESUME({name}, {fn}, {k});' for k,name,_ in lifter.resumes]
    head+=[f'void {fn}(struct bfv_cpu *c)','{',
          '    uint32_t eax = c->eax, ecx = c->ecx, edx = c->edx, ebx = c->ebx;',
          '    uint32_t esp = c->esp, ebp = c->ebp, esi = c->esi, edi = c->edi;',
          '    uint32_t eflags_rest = c->eflags, cf, pf, af, zf, sf, df, of;',
          '    FLAGS_LOAD(eflags_rest);']
    if temps:head.append('    uint32_t '+', '.join(temps)+';')
    if used('p'):head.append('    int64_t p;')
    if used('u'):head.append('    uint64_t u;')
    if lifter.resumes:
        head.append('    switch (c->eip) { /* 0: entry; k: return from the k-th call */')
        head+=[f'    case {k}: goto R_{k};' for k,_,_ in lifter.resumes]
        head+=['    default: break;','    }']
    if insns[0].address!=address:raise Reject('entry')
    tail=['out: __attribute__((unused));','    c->eax = eax; c->ecx = ecx; c->edx = edx; c->ebx = ebx;',
          '    c->esp = esp; c->ebp = ebp; c->esi = esi; c->edi = edi;',
          '    c->eflags = (eflags_rest & ~FLAG_MASK) | FLAGS_WORD();','}','']
    resumes={name:f'{ret:08x}' for _,name,ret in lifter.resumes}
    return stub,'\n'.join(head+body+tail),resumes

def falls_out(i,inside):
    """Whether i falls through past the function's own code: into the next indexed
    entry (shared tail code) or onto a trap. The body continues natively there."""
    ft=i.address+i.size
    return i.mnemonic!='call' and ft not in inside and ft in successors(i,lambda t:t in inside)

def ranges(insns):
    """Contiguous [start, end) address runs covered by the instructions."""
    out=[]
    for i in insns:
        if out and out[-1][1]==i.address:out[-1][1]=i.address+i.size
        else:out.append([i.address,i.address+i.size])
    return [f'{x:08x}-{y:08x}' for x,y in out]

def exits(insns):
    """Direct call and jump targets outside the function and fall-through exits
    (verifier stubs them)."""
    inside={i.address for i in insns}
    return sorted({o.imm for i in insns if i.mnemonic=='call' or i.mnemonic.startswith('j') or i.mnemonic in BRANCHES
                   for o in i.operands if o.type==CS_OP_IMM and o.imm not in inside}|
                  {i.address+i.size for i in insns if falls_out(i,inside)})

def candidates(target,pe,md,taken):
    """(entry, next indexed entry) for every indexed function not recovered yet."""
    with (REPORTS/target/'all-functions.tsv').open(encoding='utf-8') as f:
        starts=sorted({int(row['address'],16) for row in csv.DictReader(f,delimiter='\t')})
    text=next(s for s in pe.sections if s.Name.rstrip(b'\0')==b'.text')
    text_end=pe.OPTIONAL_HEADER.ImageBase+text.VirtualAddress+text.Misc_VirtualSize
    for a,limit in zip(starts,starts[1:]+[text_end]):
        if a not in taken:yield a,limit

def eligibility(insns,leaf_only):
    for i in insns:
        m=i.mnemonic
        if leaf_only and m=='call':return 'call'
        if leaf_only and (m.startswith('j') or m in BRANCHES):
            inside={j.address for j in insns}
            if i.operands[0].type!=CS_OP_IMM:return 'indirect_jump'
            if i.operands[0].imm not in inside:return 'external_jump'
    first=insns[0].address;run=first  # the detour jmp overwrites 5 contiguous bytes
    for i in insns:
        if i.address==run:run+=i.size
    if run-first<5:return 'too_short'
    return None

def padded(pe,insns,limit):
    """A function shorter than the 5-byte detour still takes one when the bytes
    after its first contiguous run, up to entry+5, are int3/nop alignment padding
    before the next indexed entry. build.py rejects the detour if anything
    branches or points into those bytes."""
    first=insns[0].address;run=first
    for i in insns:
        if i.address==run:run+=i.size
    if first+5>limit:return False
    pad=pe.get_data(run-pe.OPTIONAL_HEADER.ImageBase,first+5-run)
    return all(b in (0xcc,0x90) for b in pad)

def main():
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both')
    p.add_argument('--limit',type=int,default=0,help='lift at most this many functions (pilot batches)')
    p.add_argument('--leaf-only',action='store_true',help='only functions without calls or exits')
    p.add_argument('--out',type=Path,default=PROJECT/'lifted')
    args=p.parse_args()
    md=Cs(CS_ARCH_X86,CS_MODE_32);md.detail=True;lifter=Lifter(md)
    registry=json.loads((REPORTS/'recovery-registry.json').read_text(encoding='utf-8'))
    for target in ['client','server'] if args.target=='both' else [args.target]:
        spec=TARGETS[target];image=(args.game_dir/spec['file']).read_bytes()
        assert hashlib.sha256(image).hexdigest()==spec['sha']
        pe=pefile.PE(data=image,fast_load=True)
        taken={int(v,16) for e in registry for t,v in e['addresses'].items() if t==target}
        rejected=collections.Counter();unhandled=collections.Counter();entries=[];sources=[]
        base=pe.OPTIONAL_HEADER.ImageBase
        for a,limit in candidates(target,pe,md,taken):
            insns,reason=extent(pe,md,a,limit)
            if reason is None:reason=eligibility(insns,args.leaf_only)
            if reason=='too_short' and padded(pe,insns,limit):reason=None
            if reason is None:
                restarts=set();d=stack_depth(insns,{i.address for i in insns},restarts)
                if d is None:reason='stack_untracked'
                elif d>DEEP_LIMIT:reason='stack_too_deep'
            if reason is None:
                gap=(d+0x80+0xff)&~0xff if d>STACK_LIMIT else None  # 0x80 spare like the default gap; at most 0xf00
                try:stub,text,resumes=lift_function(lifter,target,a,insns,gap,restarts)
                except Reject as error:
                    reason='unhandled';unhandled[str(error)]+=1
            if reason:rejected[reason]+=1;continue
            span=insns[-1].address+insns[-1].size-a;code=pe.get_data(a-base,span)
            entries.append(dict(address=f'{a:08x}',symbol=stub,c_sha256=hashlib.sha256(text.encode()).hexdigest(),bytes=span,instructions=len(insns),stack_depth=d,
                resumes=resumes,exits=[f'{t:08x}' for t in exits(insns)],
                ranges=ranges(insns),guarded_prefix=pe.get_data(a-base,5).hex(),native_body=code.hex()));sources.append(text)
            if args.limit and len(entries)>=args.limit:break
        out=args.out/target;out.mkdir(parents=True,exist_ok=True)
        for old in out.rglob('*.c'):old.unlink()
        # Shards keep each translation unit within 32-bit cc1's memory.
        for n in range(0,len(sources),SHARD):
            (out/f'lifted_{n//SHARD:03d}.c').write_text('/* Generated by tools/lift_c.py (L1 lifted C). */\n'
                '#include "l1.h"\n\n'+'\n'.join(sources[n:n+SHARD]),encoding='utf-8',newline='\n')
        (out/'catalog.json').write_text(json.dumps(dict(original_sha256=spec['sha'],level='L1',
            rejected=dict(rejected.most_common()),unhandled=dict(unhandled.most_common()),entries=entries),indent=1)+'\n')
        if not args.limit:
            import sort_lifted  # the game's own folder and file layout
            sort_lifted.layout(target,image,out)
        print(f'{target}: lifted {len(entries)} ({sum(e["bytes"] for e in entries)} bytes); rejected {dict(rejected.most_common())}')
        print('  unhandled',unhandled.most_common(25))

if __name__=='__main__':main()
