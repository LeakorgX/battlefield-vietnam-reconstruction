// Read-only audit of movement scoring and its two complete helper detours.
import ghidra.app.script.GhidraScript;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class AuditMovementScore extends GhidraScript {
    public void run() throws Exception {
        String hash=currentProgram.getExecutableSHA256();
        boolean client=hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if(!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d"))
            throw new Exception("Unsupported hash");
        long[] entries=client?new long[]{0x6eb030,0x970550,0x99fc01}:new long[]{0x611430,0x72e7d0,0x74a3e1};
        String[] names={"bfv_vector_cross_assign","bfv_component_event2_scalar","bfv_artillery_movement_score_bridge"};
        String[] prefixes={"83ec0c8bc1","8b41048b4820","85ff0f842a020000"};
        int[] sizes={86,23,570},cleanup={4,0,0};
        long owner=client?0x99f2a0:0x749a80,unit=client?0x99fe33:0x74a613;
        long unitFrom=client?0x99fbfb:0x74a3db;
        StringBuilder out=new StringBuilder("address\tsymbol\tend_exclusive\towner\tbytes\tinstructions\tpatch_bytes\tpatch_hex\tentry_references\texternal_interior_references\toverwritten_interior_references\tret_cleanup\tretained_unit_entry\tretained_unit_references\tstatus\toriginal_sha256\n");
        for(int i=0;i<entries.length;i++) {
            long start=entries[i],end=start+sizes[i];boolean stage=i==2;
            var entry=toAddr(start);var function=getFunctionContaining(entry);
            if(function==null || function.getEntryPoint().getOffset()!=(stage?owner:start))
                throw new Exception("Unexpected owner: "+entry);
            if(!stage && (function.getBody().getNumAddresses()!=sizes[i] ||
               function.getBody().getMinAddress().getOffset()!=start || function.getBody().getMaxAddress().getOffset()!=end-1))
                throw new Exception("Unexpected body: "+entry);
            int patch=prefixes[i].length()/2;
            StringBuilder actual=new StringBuilder();
            for(int n=0;n<patch;n++) actual.append(String.format("%02x",getByte(entry.add(n))&255));
            if(!actual.toString().equals(prefixes[i])) throw new Exception("Prefix mismatch: "+entry);
            long cursor=start;boolean aligned=false;int count=0,external=0,overwritten=0,returns=0,unitRefs=0,exitJumps=0;
            while(cursor<end) {
                monitor.checkCancelled();var inst=getInstructionAt(toAddr(cursor));
                if(inst==null || !function.getBody().contains(inst.getMaxAddress())) throw new Exception("Missing instruction");
                if(inst.getFlowType().isJump()) for(var dest:inst.getFlows()) {
                    long address=dest.getOffset();
                    if(address<start || address>=end) {
                        if(!stage || address!=end) throw new Exception("Unexpected outgoing branch: "+dest);
                        ++exitJumps;
                    }
                }
                if(inst.getFlowType().isTerminal()) {
                    if(stage || !inst.getMnemonicString().equals("RET") ||
                       (cleanup[i]==0 ? inst.getNumOperands()!=0 :
                         inst.getNumOperands()!=1 || inst.getScalar(0).getUnsignedValue()!=cleanup[i]))
                        throw new Exception("Unexpected terminal/cleanup");
                    ++returns;
                }
                for(int n=0;n<inst.getLength();n++) for(var ref:getReferencesTo(toAddr(cursor+n))) {
                    long address=cursor+n,from=ref.getFromAddress().getOffset();
                    if(address>start && address<start+patch) ++overwritten;
                    if(address>start && (from<start || from>=end)) {
                        if(stage && address==unit && from==unitFrom) ++unitRefs;
                        else ++external;
                    }
                }
                cursor+=inst.getLength();++count;if(cursor==start+patch) aligned=true;
            }
            if(cursor!=end || !aligned || external!=0 || overwritten!=0 || (!stage && returns!=1))
                throw new Exception("Unsafe boundary/reference");
            if(stage) {
                var retained=getInstructionAt(toAddr(unit));var last=getInstructionBefore(toAddr(end));
                if(retained==null || retained.getLength()!=8 || !retained.getMnemonicString().equals("MOV") ||
                   retained.getScalar(1).getUnsignedValue()!=0x3f800000L || unitRefs!=1 || exitJumps!=3 ||
                   last==null || !toAddr(end).equals(last.getFallThrough()) || getInstructionAt(toAddr(end))==null)
                    throw new Exception("Unexpected retained alias/continuation");
            }
            out.append(String.format("%08x\t%s\t%08x\t%08x\t%d\t%d\t%d\t%s\t%d\t%d\t%d\t%d\t%s\t%d\teligible\t%s\n",
                start,names[i],end,stage?owner:start,sizes[i],count,patch,prefixes[i],getReferencesTo(entry).length,
                external,overwritten,cleanup[i],stage?String.format("%08x",unit):"",unitRefs,hash));
        }
        Files.writeString(Paths.get(getScriptArgs()[0]),out,StandardCharsets.UTF_8);
        println("MOVEMENT_SCORE_AUDIT eligible=3 sha256="+hash);
    }
}
