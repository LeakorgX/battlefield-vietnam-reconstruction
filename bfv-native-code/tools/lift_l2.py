"""L2 readable C: the verified L1 translation written as compact, readable C.

Every function in lifted/<target>/catalog.json is lifted again from the
original instructions (the same extent, stack gap and resume points as L1) and
written to readable/<target>/ in the same folders and files, with the L1 names
and notes as comments. The code differs from L1 only in spelling:

- Flag liveness over the function's control flow: a flag update that no later
  instruction reads is left out. Flags are dead after `ret` and at calls (the
  callee sets them) and live where the body leaves to native code, except
  where flag_abi() finds callers or callees that pass flags across call/ret.
- A compare or test followed by the branches, `set` or `cmov` that read it
  becomes one condition: `if ((int32_t)eax < 5) goto L_...;`. Only when every
  flag it sets dies there; otherwise the full L1 flag code stays.
- Addresses inline (`edx = M32(esp + 4);`), plain operators
  (`ecx += 0xc;`, `M32(esi + 8)++`), and PUSH/POP/CALL/RET/EXIT_TO macros
  (readable/runtime/l2.h). x87/SSE instructions stay native as FPU(...).

Verify with:
  uv run ... python tools/verify_lifted.py --target client --lifted-dir readable
"""
import argparse
import hashlib
import json
import re
import shutil
import struct
from pathlib import Path
import pefile
from capstone import Cs,CS_ARCH_X86,CS_MODE_32,CS_OP_REG,CS_OP_IMM,CS_OP_MEM
from capstone import x86_const as X
from build import PROJECT,GAME,TARGETS,code_sha256
import lift_c as L1
from structure_l2 import structure
import reg_abi as RA
from regvars import rename
import regvars
from symbols_l2 import Symbols,candidates
from slots_l2 import name_slots
from calls_l2 import fold
import fields_l2 as FL
POPS={}  # function address -> bytes its ret pops
ARGS_OF={}  # function address -> stack arguments its code reads (arg_N notes)
READS=None  # function address -> registers it reads (reg_abi), for typed calls also in fixed-register bodies
SYMS=None
STRUCTURED=True
from lift_c import Lifter,Reject,GPR,WIDTH_BITS,ALIAS,STRING_OPS,FLAG_SETTING_FP,SIMD_GROUPS,BRANCHES

CF,PF,AF,ZF,SF,OF=1,2,4,8,16,32;ALL=63
USES={'o':OF,'no':OF,'b':CF,'ae':CF,'e':ZF,'ne':ZF,'be':CF|ZF,'a':CF|ZF,'s':SF,'ns':SF,'p':PF,'np':PF,
      'l':SF|OF,'ge':SF|OF,'le':ZF|SF|OF,'g':ZF|SF|OF}
JCC=lambda m:m.startswith('j') and m not in ('jmp','jecxz')
import collections
FLAG_WHY=collections.Counter()
ENTRY=re.compile(r'^BFV_L1_ENTRY(?:_DEEP)?\((\w+),',re.M)
NOTE=re.compile(r'^    /\* ([0-9a-f]{8})  .*?(?:  ; (.*))? \*/$')

def cc(s):return ALIAS.get(s,s)

def num(v):return str(v) if 0<=v<10 else f'0x{v:x}'

def snum(v,W):
    v&=(1<<W)-1
    if v>=1<<(W-1):v-=1<<W
    if v==-(1<<31):return 'INT32_MIN'
    return num(v) if v>=0 else '-'+num(-v)

def is_simd(i):
    m=i.mnemonic;groups={i.group_name(g) for g in i.groups}
    vector=any(op.type==CS_OP_REG and i.reg_name(op.reg)[:2] in ('mm','xm') for op in i.operands)
    return bool(groups&SIMD_GROUPS) or vector or m[0]=='f' or m in ('wait','emms','femms')

def flag_info(i):
    """(uses, always-set, maybe-set) flag masks, as the L1 code for i computes them."""
    m=i.mnemonic;ops=i.operands
    if is_simd(i):return (0,ALL,ALL) if m in FLAG_SETTING_FP else (0,0,0)
    if i.prefix[0] in (0xf3,0xf2) and m.split()[0] in ('rep','repe','repne'):
        return (0,0,ALL) if m.split()[1][:4] in ('cmps','scas') else (0,0,0)
    if m in STRING_OPS:return (0,ALL,ALL) if m[:4] in ('cmps','scas') else (0,0,0)
    if m in ('add','sub','cmp','and','or','xor','test','neg'):return 0,ALL,ALL
    if m in ('adc','sbb'):return CF,ALL,ALL
    if m in ('inc','dec'):return 0,ALL&~CF,ALL&~CF
    if m in ('shl','sal','shr','sar','rol','ror'):
        d=CF|OF if m in ('rol','ror') else ALL
        if len(ops)<2:return 0,d,d
        if ops[1].type!=CS_OP_IMM:return 0,0,d
        return (0,d,d) if ops[1].imm&31 else (0,0,0)
    if m in ('shld','shrd'):
        return (0,ALL,ALL) if ops[2].type==CS_OP_IMM and ops[2].imm&31 else (0,0,ALL)
    if m in ('rcl','rcr'):return CF,CF|OF,CF|OF
    if m in ('mul','imul'):return 0,CF|OF,CF|OF
    if m in ('bt','bts','btr','btc','clc','stc'):return 0,CF,CF
    if m=='cmc':return CF,CF,CF
    if m=='sahf':return 0,ALL&~OF,ALL&~OF
    if m=='lahf':return ALL&~OF,0,0
    if m=='pushfd':return ALL,0,0
    if m=='popfd':return 0,ALL,ALL
    if m.startswith('set'):return USES[cc(m[3:])],0,0
    if m.startswith('cmov'):return USES[cc(m[4:])],0,0
    if m in ('loope','loopne'):return ZF,0,0
    if JCC(m) and m not in BRANCHES:return USES[cc(m[1:])],0,0
    return 0,0,0

def flag_abi(pe,md):
    """Whole-image flag ABI. MSVC's x87 math helpers hand results to each
    other in EFLAGS across call/ret (`call _fload; call acos`, where acos opens
    with `je`). Returns (takes, gives): functions that read flags on entry, and
    functions whose callers read the flags they return. Per-function
    verification can't see either, so the generator must."""
    text=next(x for x in pe.sections if x.Name.rstrip(b'\0')==b'.text')
    base=pe.OPTIONAL_HEADER.ImageBase+text.VirtualAddress
    code=text.get_data()[:text.Misc_VirtualSize];end=base+len(code);cache={}
    def at(a):
        if a not in cache:
            cache[a]=next(md.disasm(code[a-base:a-base+16],a,1),None) if base<=a<end else None
        return cache[a]
    calls=[];offset=0
    while offset<len(code):
        last=offset
        for i in md.disasm(code[offset:],base+offset):
            last=i.address-base+i.size;cache[i.address]=i
            if i.mnemonic=='call' and i.operands[0].type==CS_OP_IMM:calls.append(i)
        offset=last if last>offset else offset+1
    takes=set()
    def reads_first(a,limit=64):
        """Does code from a read a flag before setting it (calls to `takes` count)?"""
        seen=set();work=[(a,0)]
        while work:
            a,defined=work.pop()
            while len(seen)<limit and (a,defined) not in seen:
                seen.add((a,defined));i=at(a)
                if i is None:break
                m=i.mnemonic
                if m=='call':
                    o=i.operands[0]
                    if o.type==CS_OP_IMM and o.imm in takes and defined!=ALL:return True
                    break
                if m in ('ret','int3','hlt'):break
                u,must,_=flag_info(i)
                if u&~defined:return True
                defined|=must
                if defined==ALL:break
                o=i.operands
                if m=='jmp':
                    if o[0].type!=CS_OP_IMM:break
                    a=o[0].imm;continue
                if (JCC(m) or m in BRANCHES) and o and o[0].type==CS_OP_IMM:work.append((o[0].imm,defined))
                a=i.address+i.size
        return False
    targets={i.operands[0].imm for i in calls}
    while True:
        new={t for t in targets if t not in takes and reads_first(t)}
        if not new:break
        takes|=new
    gives={i.operands[0].imm for i in calls if reads_first(i.address+i.size)}
    return takes,gives

def liveness(insns,inside,strict=False,gives=False,takes=frozenset()):
    """live_in per instruction index (flag mask). strict: flags reach callees
    (a call into the middle of native code may read them). gives: callers read
    the flags at ret. takes: callees that read their incoming flags."""
    idx={i.address:n for n,i in enumerate(insns)};info=[flag_info(i) for i in insns]
    live=[0]*len(insns);inr=lambda t:t in inside
    def out(n):
        i=insns[n];m=i.mnemonic
        if m=='ret':return ALL if gives else 0
        if m=='call':
            o=i.operands[0]
            return ALL if strict or o.type==CS_OP_IMM and o.imm in takes else 0
        v=0
        for t in L1.successors(i,inr):v|=live[idx[t]] if t in inside else ALL
        o=i.operands
        if (m=='jmp' or JCC(m) or m in BRANCHES) and not (o and o[0].type==CS_OP_IMM and o[0].imm in inside):v|=ALL
        return v
    changed=True
    while changed:
        changed=False
        for n in range(len(insns)-1,-1,-1):
            u,must,_=info[n];v=u|(out(n)&~must)
            if v!=live[n]:live[n]=v;changed=True
    return live,[out(n) for n in range(len(insns))],info,idx

ARGN=re.compile(r'\barg_([0-9a-f]+)\b')
def signature(insns,notes):
    """One-line calling-convention summary, from the instructions only."""
    pops=next((i.operands[0].imm if i.operands else 0) for i in insns if i.mnemonic=='ret') if any(i.mnemonic=='ret' for i in insns) else None
    seen=set();this=fast=False
    for i in insns:  # registers read before written on the entry path
        try:r,w=i.regs_access()
        except Exception:break
        rn={i.reg_name(x) for x in r};wn={i.reg_name(x) for x in w}
        if 'ecx' in rn-seen and 'ecx' not in seen:this=True
        if 'edx' in rn-seen and 'edx' not in seen:fast=True
        seen|=wn
        if i.mnemonic in ('call','ret','jmp') or JCC(i.mnemonic) or i.mnemonic in BRANCHES:break
    used=[int(m,16) for n in notes.values() for m in ARGN.findall(n)]
    if pops:conv,args='fastcall' if fast and this else 'thiscall' if this else 'stdcall',pops//4
    elif pops==0:conv,args='thiscall' if this else 'cdecl',(max(used)//4+1 if used else 0)
    else:conv,args='no return',(max(used)//4+1 if used else 0)
    regs=', '.join(x for x,y in (('ecx',this),('edx',fast)) if y)
    return f'/* {conv}: {args} stack argument{"s" if args!=1 else ""}'+(f', reads {regs}' if regs else '')+(f', pops {pops}' if pops else '')+' */'

class L2(Lifter):
    hoist={}

    # ---- operands (also used by the inherited L1 fallback code)
    def ea(self,i,op):
        m=op.mem
        if m.segment not in (0,X.X86_REG_FS):raise Reject('segment')
        parts=[]
        if m.base:parts.append(self.reg32(i.reg_name(m.base)))
        if m.index:parts.append(f'{self.reg32(i.reg_name(m.index))} * {m.scale}' if m.scale>1 else self.reg32(i.reg_name(m.index)))
        d=m.disp
        if not parts:return f'0x{d&0xffffffff:x}'
        s=' + '.join(parts)
        if d>0:s+=f' + {num(d)}'
        elif d<0:s+=f' - {num(-d)}'
        return s

    def imm(self,op,W):return num(op.imm&((1<<W)-1))

    def read(self,i,op,ea=None):
        if op.type==CS_OP_IMM:return num(op.imm&((1<<(8*op.size))-1))
        return super().read(i,op,ea)

    def is_mem(self,op):return op.type==CS_OP_MEM

    # ---- conditions from a fused producer
    def condition(self,p,c,suffix,hoisted):
        """C condition for consumer suffix after producer p, or None."""
        m=p.mnemonic;ops=list(p.operands);W=WIDTH_BITS.get(ops[0].size,32);s=f'int{W}_t'
        rd=lambda n:hoisted.get(n) or self.read(p,ops[n])
        if m=='cmp':
            A=rd(0);B=rd(1)
            if A==B and 'M' not in A:  # cmp r, r
                return '1' if suffix in ('e','ae','be','ge','le','ns') else '0' if suffix in ('ne','b','a','l','g','s') else None
            if suffix in ('b','ae') and B=='0':return None  # always false / true: keep the flag code
            if suffix in ('e','ne','b','ae','be','a'):
                return f'{A} {dict(e="==",ne="!=",b="<",ae=">=",be="<=",a=">")[suffix]} {B}'
            if suffix in ('l','ge','le','g'):
                Bs=snum(ops[1].imm,W) if ops[1].type==CS_OP_IMM else f'({s}){B}'
                return f'({s}){A} {dict(l="<",ge=">=",le="<=",g=">")[suffix]} {Bs}'
            if suffix in ('s','ns'):return f'({s})({A} - {B}) {"<" if suffix=="s" else ">="} 0'
            return None
        if m=='test':
            same=ops[1].type==CS_OP_REG and ops[0].type==CS_OP_REG and ops[0].reg==ops[1].reg
            R=rd(0) if same else f'({rd(0)} & {rd(1)})'
        else:
            if ops[0].type!=CS_OP_REG:return None
            R=rd(0)
        logic=m in ('test','and','or','xor')
        table={'e':f'{R} == 0','ne':f'{R} != 0','s':f'({s}){R} < 0','ns':f'({s}){R} >= 0'}
        if logic:table.update(l=table['s'],ge=table['ns'],le=f'({s}){R} <= 0',g=f'({s}){R} > 0',be=table['e'],a=table['ne'])
        return table.get(suffix)

    # ---- plain spelling of an instruction whose flags are not needed
    def plain(self,i,flags_needed):
        m=i.mnemonic;ops=list(i.operands)
        if is_simd(i) or m.split()[0] in ('rep','repe','repne') or i.prefix[0]!=0 or m in STRING_OPS:return None
        W=WIDTH_BITS.get(ops[0].size,32) if ops else 32
        rd=lambda n:self.read(i,ops[n]);wr=lambda n,v:self.write(i,ops[n],v)
        fs=any(op.type==CS_OP_MEM and op.mem.segment for op in ops)
        if m=='mov':return [wr(0,rd(1))]
        if m=='lea':return [wr(0,self.ea(i,ops[1]))]
        if m=='movzx':return [wr(0,rd(1))]
        if m=='movsx':return [wr(0,f'(int{WIDTH_BITS[ops[1].size]}_t){rd(1)}')]
        if m=='push':
            if ops[0].type==CS_OP_IMM:return [f'PUSH(0x{ops[0].imm&0xffffffff:x});']
            if ops[0].size!=4:raise Reject('push16')
            return [f'PUSH({rd(0)});']
        if m=='pop' and ops[0].type==CS_OP_REG and ops[0].size==4:return [wr(0,'POP()')]
        if m=='leave':return ['esp = ebp;','ebp = POP();']
        if m=='ret':return [f'RET({ops[0].imm if ops else 0});']
        if m=='call':
            ret=i.address+i.size;k=len(self.resumes)+1;stub=f'bfv_{self.target}_l1r_{self.start:08x}_{k}'
            self.resumes.append((k,stub,ret));self.call_targets[k]=ops[0].imm if ops[0].type==CS_OP_IMM else None
            dest=f'0x{ops[0].imm:x}' if ops[0].type==CS_OP_IMM else rd(0)
            return [f'CALL({k}, {dest});',*([] if ret in self.inside else [f'EXIT_TO(0x{ret:x});'])]
        if m=='jmp':
            if ops[0].type!=CS_OP_IMM:return [f'EXIT_TO({rd(0)});']
            if ops[0].imm not in self.inside:return [f'EXIT_TO(0x{ops[0].imm:x});']
            return [f'goto L_{ops[0].imm:08x};']
        if JCC(m) and m not in BRANCHES:return [self.branch(i,L1.cond(m[1:]))]
        if flags_needed or fs:return None
        if m in ('add','sub','and','or','xor'):
            op={'add':'+','sub':'-','and':'&','or':'|','xor':'^'}[m]
            if m in ('xor','sub') and ops[0].type==CS_OP_REG and ops[1].type==CS_OP_REG and ops[0].reg==ops[1].reg:
                return [wr(0,'0')]
            if ops[0].type==CS_OP_MEM or GPR[i.reg_name(ops[0].reg)][1]==32:
                return [f'{rd(0)} {op}= {rd(1)};']
            return [wr(0,f'{rd(0)} {op} {rd(1)}')]
        if m in ('cmp','test'):return [f'(void){self.read(i,op)};' for op in ops if op.type==CS_OP_MEM]
        if m in ('inc','dec','neg','not'):
            sym={'inc':'+','dec':'-'}.get(m)
            if ops[0].type==CS_OP_MEM:
                return [f'{rd(0)} {sym}= 1;'] if sym else [wr(0,f'{"-" if m=="neg" else "~"}{rd(0)}')]
            if GPR[i.reg_name(ops[0].reg)][1]==32:
                return [f'{rd(0)}{sym*2};'] if sym else [wr(0,f'{"-" if m=="neg" else "~"}{rd(0)}')]
            return [wr(0,f'{rd(0)} {sym} 1' if sym else f'{"-" if m=="neg" else "~"}{rd(0)}')]
        if m in ('shl','sal','shr','sar') and W==32 and len(ops)==2 and ops[1].type==CS_OP_IMM and ops[1].imm&31:
            k=ops[1].imm&31
            if m=='sar':return [wr(0,f'(uint32_t)((int32_t){rd(0)} >> {k})')]
            return [f'{rd(0)} {"<<" if m in ("shl","sal") else ">>"}= {k};']
        if m=='imul' and len(ops)>=2 and W==32:
            if len(ops)==3:return [wr(0,f'{rd(1)} * {self.imm(ops[2],32)}')]
            return [f'{rd(0)} *= {rd(1)};']
        return None

    def branch(self,i,expr):
        t=i.operands[0].imm
        return f'if ({expr}) goto L_{t:08x};' if t in self.inside else f'if ({expr}) EXIT_TO(0x{t:x});'

    def consumer(self,i,expr):
        m=i.mnemonic;ops=list(i.operands)
        if JCC(m):return [self.branch(i,expr)]
        if m.startswith('set'):return [self.write(i,ops[0],expr)]
        src=self.read(i,ops[1])  # cmov: the source is read either way
        if ops[1].type==CS_OP_MEM:return [f't = {src};',f'if ({expr}) {self.write(i,ops[0],"t")}']
        return [f'if ({expr}) {self.write(i,ops[0],src)}']

    def fallback(self,i):
        lines=Lifter.lift(self,i,set())
        return [tidy(l) for l in fpu(lines)]

def fpu(lines):
    """Shorten the asm statements of x87/SSE instructions."""
    out=[];n=0
    plain=re.compile(r'^__asm__ volatile\("\.intel_syntax noprefix\\n\\t(.*?)\\n\\t\.att_syntax prefix"\);$')
    mem=re.compile(r'^__asm__ volatile\("\.intel_syntax noprefix\\n\\t(.*?)\\n\\t\.att_syntax prefix" :  : "r"\(ea\) : "memory", "cc"\);$')
    while n<len(lines):
        l=lines[n]
        if l.startswith('ea = ') and n+1<len(lines) and mem.match(lines[n+1]):
            out.append(f'FPU_MEM("{mem.match(lines[n+1]).group(1)}", {l[5:-1]});');n+=2;continue
        p=plain.match(l)
        if p:out.append(f'FPU("{p.group(1)}");')
        else:out.append(re.sub(r'"\.intel_syntax noprefix\\n\\t(.*?)\\n\\t\.att_syntax prefix',r'INTEL("\1") "',l).replace(' ""',''))
        n+=1
    return out

def tidy(l):return l

def lift_l2(lifter,target,address,insns,gap,restarts,notes,strict=False,gives=False,takes=frozenset(),flat=False,regs=None):
    inside={i.address for i in insns}
    lifter.target,lifter.start,lifter.inside,lifter.resumes,lifter.hoist,lifter.call_targets=target,address,inside,[],{},{}
    labels={o.imm for i in insns if i.mnemonic.startswith('j') or i.mnemonic in BRANCHES for o in i.operands
            if o.type==CS_OP_IMM and o.imm in inside}
    gaps=set()
    for n,i in enumerate(insns):
        ft=i.address+i.size;nxt=insns[n+1].address if n+1<len(insns) else None
        if ft in inside and ft!=nxt and ft in L1.successors(i,lambda t:t in inside):gaps.add(i.address)
        if i.mnemonic=='call' and ft in inside and ft!=nxt:gaps.add(i.address)
    labels|={i.address+i.size for i in insns if i.address in gaps}
    live,live_out,info,idx=liveness(insns,inside,strict,gives,takes)
    live_at=lambda t:live[idx[t]] if t in inside else ALL
    fused={}  # consumer index -> condition
    producers=set();n=0;stats=dict(fused=0,flag_code=0)
    PROD=('cmp','test','and','or','xor','add','sub','inc','dec','neg')
    def writes(k):
        i=insns[k]
        try:_,w=i.regs_access()
        except Exception:return set(GPR)
        return {GPR[i.reg_name(r)][0] for r in w if i.reg_name(r) in GPR}
    def neutral(k):
        """Neither touches the flags nor leaves or re-enters the body."""
        i=insns[k];m=i.mnemonic
        if m in ('call','ret','jmp') or JCC(m) or m in BRANCHES or i.address in restarts or i.address in gaps:return False
        return not L1.falls_out(i,inside) and not any(info[k])
    for n,p in enumerate(insns):
        if p.mnemonic not in PROD or p.address in gaps or p.prefix[0]!=0 or p.address in restarts:continue
        if any(op.type==CS_OP_MEM and op.mem.segment for op in p.operands):continue
        if p.mnemonic not in ('cmp','test') and p.operands[0].type!=CS_OP_REG:continue
        defs=info[n][2];chain=[];mid=[];k=n+1
        while k<len(insns) and insns[k].address==insns[k-1].address+insns[k-1].size and insns[k].address not in labels:
            c=insns[k];m=c.mnemonic
            if JCC(m) and m not in BRANCHES:suffix=cc(m[1:])
            elif (m.startswith('set') or m.startswith('cmov')) and c.prefix[0]==0:suffix=cc(m[3:] if m.startswith('set') else m[4:])
            elif neutral(k):mid.append(k);k+=1;continue
            else:break
            if info[k][0]&~info[n][1]:break
            chain.append((k,suffix,list(mid)))
            if not JCC(m) or c.address in gaps:break
            k+=1
        ops=list(p.operands)
        same=len(ops)==2 and ops[0].type==ops[1].type==CS_OP_REG and ops[0].reg==ops[1].reg
        # longest prefix of the chain whose conditions exist and after which p's flags are dead
        while chain:
            between=chain[-1][2];clobbered=set().union(*[writes(x) for x in between])
            last=chain[-1][0];hoisted={}
            for x,op in enumerate(ops if p.mnemonic in ('cmp','test') else ops[:1]):
                if x==1 and same:continue
                if op.type==CS_OP_MEM and (between or len(chain)>1 or insns[last].mnemonic.startswith('cmov')) or op.type==CS_OP_REG and GPR[p.reg_name(op.reg)][0] in clobbered:
                    hoisted[x]='lhs' if x==0 else 'rhs'
            if same and 0 in hoisted:hoisted[1]=hoisted[0]
            ok=True;conds=[]
            for k,suffix,_ in chain:
                e=lifter.condition(p,insns[k],suffix,hoisted)
                c=insns[k]
                if e is None or JCC(c.mnemonic) and live_at(c.operands[0].imm)&defs:ok=False;break
                conds.append(e)
            c=insns[last]
            after=live_at(c.address+c.size) if JCC(c.mnemonic) else live_out[last]
            if ok and not after&defs:
                producers.add(n);fused.update({k:e for (k,_,_),e in zip(chain,conds)})
                lifter.hoist={**lifter.hoist,n:hoisted}
                break
            chain.pop()
    chunks=[];post=[]
    for n,i in enumerate(insns):
        need=live_out[n]&info[n][2]
        if n in fused:lines=lifter.consumer(i,fused[n])
        else:
            lines=None
            if n in producers:
                ops=list(i.operands);h=lifter.hoist[n]
                if i.mnemonic in ('cmp','test'):lines=[f'{h[x]} = {lifter.read(i,ops[x])};' for x in sorted(h)]
                else:lines=(lifter.plain(i,False) or lifter.fallback(i))+[f'{h[x]} = {lifter.read(i,ops[x])};' for x in sorted(h)]
                stats['fused']+=1
            else:
                lines=lifter.plain(i,need)
                if lines is None:
                    lines=lifter.fallback(i)
                    if need:stats['flag_code']+=1;FLAG_WHY[i.mnemonic]+=1
        p=[]
        if i.address in restarts:
            k=len(lifter.resumes)+1;name=f'bfv_{target}_l1r_{address:08x}_{k}'
            lifter.resumes.append((k,name,i.address+i.size))
            p+=[f'    EXIT_TO((uint32_t)(uintptr_t){name});',f'R_{k}:']
        if i.address in gaps:p.append(f'    goto L_{i.address+i.size:08x};')
        elif L1.falls_out(i,inside):p.append(f'    EXIT_TO(0x{i.address+i.size:x});')
        chunks.append(lines);post.append(p)
    variables=[];aliases=[];abi=None;maps=None
    by_index={n:notes[i.address] for n,i in enumerate(insns) if i.address in notes}
    if regs is not None:
        inr=lambda t:t in inside
        succ=[[idx[t] for t in L1.successors(i,inr) if t in inside] for i in insns]
        reads,rgives,igives,stored,edges,kept=regs
        def call_abi(i):
            o=i.operands[0]
            if o.type==CS_OP_IMM:m=reads.get(o.imm,RA.REGS)|rgives.get(o.imm,0)&~RA.EAX
            else:m=RA.INDIRECT_IN|igives
            return (m|RA.SAVED)&RA.REGS
        def call_keeps(i):
            o=i.operands[0]
            return RA.REGS if o.type==CS_OP_IMM and o.imm in kept else 0
        ret_set=RA.EAX|RA.SAVED
        for t in inside:
            ret_set|=rgives.get(t,0)
            if t in stored:ret_set|=igives
            if any(x not in inside for x in edges.get(t,())):ret_set|=RA.ECX|RA.EDX
        chunks,variables,calls,aliases,maps=rename(insns,chunks,idx[address],succ,call_abi,ret_set,
            [bool(post[n]) and (insns[n].address in restarts or L1.falls_out(insns[n],inside)) for n in range(len(insns))],
            [insns[n].address in restarts for n in range(len(insns))],
            by_index,call_keeps)
        if variables:abi=dict(calls={f'{a:08x}':m for a,m in sorted(calls.items())},ret=ret_set)
    # stack slots: exact where no path from a call (or restart) leads
    inr=lambda t:t in inside;late=set();work=[i.address+i.size for i in insns if i.mnemonic=='call' or i.address in restarts]
    while work:
        t=work.pop()
        if t in late or t not in inside:continue
        late.add(t);work+=list(L1.successors(insns[idx[t]],inr))
    chunks,slots,uses_frame,rest=name_slots(insns,chunks,by_index,maps,{n for n,i in enumerate(insns) if i.address not in late})
    # struct fields: M32(this + 0x14) /* field readBitPos */ -> M32(this + readBitPos)
    fields={};taken={*variables,*(x for x,_ in aliases),*(x for x,_ in slots)}
    for n,i in enumerate(insns):
        m=re.fullmatch(r'field ([A-Za-z_]\w*(?:\.[A-Za-z_]\w*)?)(?: (\(.*\)))?',(rest[n] if n in rest else notes.get(i.address)) or '')
        mem=[o.mem for o in i.operands if o.type==CS_OP_MEM]
        if not m or not chunks[n] or len(mem)!=1 or mem[0].disp<0:continue
        x,off=m[1].replace('.','_'),mem[0].disp  # Class.member -> Class_member
        if x in regvars.RESERVED or x in regvars.AVOID or x in taken or fields.get(x,off)!=off or x.startswith(('L_','R_','bfv_')):continue
        lit=rf'M(8|16|32)\((\w+){rf" \+ {off:#x}" if off else ""}\)'
        text='\n'.join(chunks[n])
        if len({b for _,b in re.findall(lit,text)})!=1:continue
        fields[x]=off;chunks[n]=re.sub(lit,lambda h:f'M{h[1]}({h[2]} + {x})',text).split('\n')
        rest[n]=m[2] and m[2][1:-1]
    body=[]
    for n,i in enumerate(insns):
        if i.address in labels:body.append(f'L_{i.address:08x}:')
        lines=chunks[n]
        note=rest[n] if n in rest else notes.get(i.address)
        if note and lines:
            first,named=SYMS.apply(i,lines[0],note) if SYMS else (lines[0],False)
            lines=[first if named else f'{first}  /* {note} */',*lines[1:]]
        body+=['    '+l for l in lines]
        body+=post[n]
    # calls with their arguments, register saves
    defines={**dict(aliases),**dict(slots),**{x:f'{off:#x}' for x,off in fields.items()}}
    dropped=set()
    body,cs=fold(body,lifter.call_targets,POPS,defines,variables,ARGS_OF,inputs=READS,dropped=dropped)
    aliases=[(x,r) for x,r in aliases if x not in dropped]
    if STRUCTURED and not flat:body=structure(body)
    stub=f'bfv_{target}_l1_{address:08x}';fn=f'bfv_{target}_l1_body_{address:08x}'
    # a call's return stub returns into the body; a restart point re-enters it
    calls={k for k in lifter.call_targets};restart=[(k,name) for k,name,_ in lifter.resumes if k not in calls]
    if gap:
        head=[f'BFV_L1_ENTRY_DEEP({stub}, {fn}, {gap:#x});']
        head+=[f'BFV_L1_RESUME_DEEP({name}, {fn}, {k}, {gap:#x});' for k,name in restart]
    else:
        head=[f'BFV_L1_ENTRY({stub}, {fn});']
        head+=[f'BFV_L1_RESUME({name}, {fn}, {k});' for k,name in restart]
    head+=[f'BFV_L2_RET({name}, {gap or 0x400:#x});' for k,name,_ in lifter.resumes if k in calls]
    head=[*head,signature(insns,notes),f'void {fn}(struct bfv_cpu *c)','{',f'#define L2_FN {address:08x}','    L2_BEGIN;']
    for k in range(0,len(variables),12):head.append('    uint32_t '+', '.join(variables[k:k+12])+';')
    # values the outside sees (callers, callees) stay in their register
    head+=[f'#define {x} {r}' for x,r in aliases]
    # stack slots (arg0 is the first argument, local_4 the word below the return address)
    if uses_frame:head.append('    uint32_t frame = esp;  /* where the body starts: the return address */')
    head+=[f'#define {x} {d}' for x,d in slots]
    head+=[f'#define {x} {off:#x}  /* field offset */' for x,off in fields.items()]
    if restart:head.append('    switch (c->eip) { '+' '.join(f'case {k}: goto R_{k};' for k,_ in restart)+' }')
    tail=['    L2_END;','#undef L2_FN',*(f'#undef {x}' for x,_ in aliases),*(f'#undef {x}' for x,_ in slots),*(f'#undef {x}' for x in fields),'}','']
    resumes={name:f'{ret:08x}' for _,name,ret in lifter.resumes}
    stats['variables']=len(variables);stats['aliases']=len(aliases);stats['slots']=len(slots);stats['fields']=len(fields)
    stats.update(cs)
    return stub,'\n'.join(head+body+tail),resumes,stats,abi

def l1_functions(folder):
    """[(relative file, [(symbol, header comment, {address: note})])] in L1 order."""
    files=[]
    for path in sorted(folder.rglob('*.c')):
        src=path.read_text(encoding='utf-8');marks=list(ENTRY.finditer(src));funcs=[]
        for m,nxt in zip(marks,marks[1:]+[None]):
            prev=src.rfind('\n}\n',0,m.start());start=prev+3 if prev>=0 else src.find('\n',src.find('#include'))+1
            header=src[start:m.start()].strip()
            chunk=src[m.start():nxt.start() if nxt else len(src)]
            notes={}
            for line in chunk.splitlines():
                x=NOTE.match(line)
                if x and x.group(2):notes[int(x.group(1),16)]=x.group(2)
            funcs.append((m.group(1),header,notes))
        files.append((path.relative_to(folder),funcs))
    return files

FILE_HEAD='''/* L2 readable C: the verified L1 translation of the original game code in
 * compact form (see readable/runtime/l2.h), generated by tools/lift_l2.py.
 * Values only the function sees are variables; values a caller or callee sees
 * are named `#define`s of their register (`this` is ecx, `result` eax).
 * M8/M16/M32(address) read and write game memory, CALL(k, target) calls a
 * function. Address names are in runtime/symbols_{target}.h, notes in comments
 * come from tools/sort_lifted.py. */
#include "l2.h"
#include "symbols_{target}.h"
#include "layouts_{target}.h"
#define L2_PREFIX bfv_{target}_l1r_
'''
# names the runtime headers define (not usable as struct members)
RUNTIME_MACROS={m for f in ('l1.h','l2.h') for m in re.findall(r'^#\s*define\s+(\w+)',(PROJECT/'readable/runtime'/f).read_text(),re.M)}|{'L2_PREFIX','L2_FN'}

def main():
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both')
    p.add_argument('--only',nargs='*',help='addresses (pilot): write only these functions')
    p.add_argument('--out',type=Path,default=PROJECT/'readable')
    p.add_argument('--strict',type=Path,default=PROJECT/'readable/strict-flags.json',
                   help='{target: [addresses]} lifted with flags live at calls')
    p.add_argument('--fallback',type=Path,default=PROJECT/'readable/l1-fallback.json',
                   help='{target: [addresses]} kept as their verbatim L1 text')
    p.add_argument('--flat-list',type=Path,default=PROJECT/'readable/flat-functions.json',
                   help='{target: [addresses]} kept as goto-only bodies')
    p.add_argument('--flat',action='store_true',help='keep goto-only bodies (no if/while structuring)')
    p.add_argument('--no-vars',action='store_true',help='keep register names (no register variables)')
    p.add_argument('--fixed-registers',type=Path,default=PROJECT/'readable/fixed-registers.json',
                   help='{target: [addresses]} kept with register names (no register variables)')
    p.add_argument('--force',action='store_true',help='overwrite readable functions that were edited (mods)')
    args=p.parse_args()
    # each target's output directory is rebuilt from scratch, so a pilot run
    # with --only must not write into the full tree
    if args.only and args.out.resolve()==(PROJECT/'readable').resolve():
        raise SystemExit('--only rebuilds the output with just those functions: pass --out <scratch dir>')
    global STRUCTURED;STRUCTURED=not args.flat
    for target in ['client','server'] if args.target=='both' else [args.target]:
        out=args.out/target
        if (out/'catalog.json').exists() and not args.force:  # never overwrite mods silently (as lift_c.py)
            from build import edited as is_edited
            from sort_lifted import split_texts
            old_catalog=json.loads((out/'catalog.json').read_text());texts=split_texts(out)
            changed=[e['address'] for e in old_catalog['entries'] if is_edited(e,texts.get(e['symbol'],''))]
            if changed:raise SystemExit(f'{target}: {len(changed)} readable functions were edited ({", ".join(changed[:5])}); '
                'save them, then rerun with --force to overwrite')
    md=Cs(CS_ARCH_X86,CS_MODE_32);md.detail=True;lifter=L2(md)
    rt=args.out/'runtime';rt.mkdir(parents=True,exist_ok=True)
    # the readable runtime (shadow-stack calls) lives in readable/runtime
    if rt.resolve()!=(PROJECT/'readable/runtime').resolve():
        for f in ('l1.h','l1_runtime.c','l2.h'):shutil.copyfile(PROJECT/'readable/runtime'/f,rt/f)
    for target in ['client','server'] if args.target=='both' else [args.target]:
        spec=TARGETS[target];image=(args.game_dir/spec['file']).read_bytes()
        assert hashlib.sha256(image).hexdigest()==spec['sha']
        pe=pefile.PE(data=image,fast_load=True)
        catalog=json.loads((PROJECT/'lifted'/target/'catalog.json').read_text())
        by_symbol={e['symbol']:e for e in catalog['entries']}
        limits=dict(L1.candidates(target,pe,md,set()))
        takes,gives=flag_abi(pe,md);print(f'{target}: flag ABI: {len(takes)} take flags, {len(gives)} give flags',flush=True)
        regs=None
        if not args.no_vars:
            img=RA.Image(pe,md);stored=set()
            for x in pe.sections:
                d=x.get_data();stored|={v for (v,) in struct.iter_unpack('<I',d[:len(d)&~3]) if img.base<=v<img.end}
            reads,rgives,igives,_,edges=RA.reg_abi(pe,md,())
            kept={t for t in reads if RA.transparent(img,t)}
            regs=(reads,rgives,igives,stored,edges,kept)
            print(f'{target}: register ABI: {len(reads)} callees, {len(rgives)} give ecx/edx, indirect calls give {RA.names(igives)}, '
                  f'{len(kept)} write no register ({", ".join(f"{t:08x}" for t in sorted(kept))})',flush=True)
        global READS;READS=regs[0] if regs else None
        only={int(a,16) for a in args.only} if args.only else None
        strict={int(a,16) for a in json.loads(args.strict.read_text()).get(target,[])} if args.strict.exists() else set()
        flat={int(a,16) for a in json.loads(args.flat_list.read_text()).get(target,[])} if args.flat_list.exists() else set()
        fallback={int(a,16) for a in json.loads(args.fallback.read_text()).get(target,[])} if args.fallback.exists() else set()
        fixed={int(a,16) for a in json.loads(args.fixed_registers.read_text()).get(target,[])} if args.fixed_registers.exists() else set()
        from sort_lifted import split_texts
        l1_texts=split_texts(PROJECT/'lifted'/target) if fallback else {}
        out=args.out/target
        if out.exists():shutil.rmtree(out)
        l1=l1_functions(PROJECT/'lifted'/target)
        POPS.clear()
        for e in catalog['entries']:
            a=int(e['address'],16);insns,reason=L1.extent(pe,md,a,limits[a])
            r=next((i for i in insns if i.mnemonic=='ret'),None) if reason is None else None
            if r is not None:POPS[a]=r.operands[0].imm if r.operands else 0
        print(f'{target}: pop counts for {len(POPS)} functions',flush=True)
        ARGS_OF.clear()
        for _,funcs in l1:
            for symbol,_,notes in funcs:
                e=by_symbol.get(symbol)
                if e is None:continue
                used=[int(m,16) for x in notes.values() for m in ARGN.findall(x)]
                ARGS_OF[int(e['address'],16)]=max(used)//4+1 if used else 0
        regvars.AVOID.clear();regvars.AVOID.update(candidates({n for _,funcs in l1 for _,_,notes in funcs for n in notes.values()}))
        global SYMS;opt=pe.OPTIONAL_HEADER;SYMS=Symbols(opt.ImageBase,opt.ImageBase+opt.SizeOfImage)
        entries=[];totals=dict(fused=0,flag_code=0,functions=0,variables=0,aliases=0,slots=0,fields=0,with_variables=0,calls=0,cdecl=0,saves=0,restores=0,typed=0,this_fields=0)
        written=[]
        for rel,funcs in l1:
            texts=[]
            for symbol,header,notes in funcs:
                e=by_symbol.get(symbol)
                if e is None:continue
                a=int(e['address'],16)
                if only is not None and a not in only:continue
                if a in fallback:
                    text=l1_texts[symbol];texts.append([header,text,None]);totals['functions']+=1
                    entries.append({**e,'tier':'L1','c_sha256':hashlib.sha256(text.encode()).hexdigest(),'code_sha256':code_sha256(text)})
                    continue
                insns,reason=L1.extent(pe,md,a,limits[a])
                assert reason is None and L1.ranges(insns)==e['ranges'],(target,e['address'],reason)
                restarts=set();d=L1.stack_depth(insns,{i.address for i in insns},restarts)
                gap=(d+0x80+0xff)&~0xff if d>L1.STACK_LIMIT else None
                stub,text,resumes,stats,abi=lift_l2(lifter,target,a,insns,gap,restarts,notes,a in strict,a in gives,takes,a in flat,None if a in fixed else regs)
                assert stub==symbol and resumes==e['resumes'],(target,e['address'])
                for k in ('fused','flag_code','variables','aliases','slots','fields','calls','cdecl','saves','restores','typed'):totals[k]+=stats.get(k,0)
                totals['with_variables']+=bool(stats['variables'])
                totals['functions']+=1
                entry={**e,**({'abi':abi} if abi else {})}
                texts.append([header,text,entry]);entries.append(entry)
            if texts:written.append((rel,texts))
        # this->field: class layouts from all methods, then the texts and their hashes
        methods=[(h,t) for _,texts in written for h,t,x in texts if x is not None]
        reserved=set(SYMS.by_name)|set(regvars.RESERVED)|RUNTIME_MACROS
        layout,struct_name=FL.layouts(methods,reserved);full=FL.full_names(methods)
        (rt/f'layouts_{target}.h').write_text(FL.header(target,layout,struct_name),encoding='utf-8',newline='\n')
        for rel,texts in written:
            for item in texts:
                header,text,entry=item
                if entry is None:continue
                new=FL.rewrite(header,text,layout,struct_name,full);totals['this_fields']+=new!=text
                item[1]=new;entry.update(c_sha256=hashlib.sha256(new.encode()).hexdigest(),code_sha256=code_sha256(new))
            path=out/rel;path.parent.mkdir(parents=True,exist_ok=True)
            path.write_text(FILE_HEAD.replace('{target}',target)+'\n\n'+'\n\n'.join(f'{h}\n{t}' for h,t,_ in texts),encoding='utf-8',newline='\n')
        print(f'{target}: {len(layout)} class layouts, {sum(map(len,layout.values()))} members',flush=True)
        (rt/f'symbols_{target}.h').write_text(SYMS.header(target),encoding='utf-8',newline='\n')
        print(f'{target}: {len(SYMS.by_name)} address names',flush=True)
        (out/'catalog.json').write_text(json.dumps({**catalog,'level':'L2','entries':entries},indent=1)+'\n')
        print(f'{target}: {totals}');print('  flag code kept for',FLAG_WHY.most_common(20));FLAG_WHY.clear()
        import comment_l2;comment_l2.comment_target(target,out)  # comments only: code_sha256 is unchanged

if __name__=='__main__':main()
