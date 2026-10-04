// Complete node-search helper and following inline score; use -readOnly.
import ghidra.app.script.GhidraScript;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
public class AuditCategoryScore extends GhidraScript {
    public void run() throws Exception {
        String hash=currentProgram.getExecutableSHA256();
        boolean client=hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if(!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d")) throw new Exception("Unsupported hash");
        long[] entries=client?new long[]{0x9bf7c0,0x9a007a}:new long[]{0x775bf0,0x74a85a};
        String[] names={"bfv_find_word_in_nodes","bfv_artillery_category_score_bridge"};
        String[] prefixes={"568b742408","f64507010f8475010000"};int[] sizes={40,383};
        long owner=client?0x99f2a0:0x749a80,next=client?0x9a0476:0x74ac56;
        StringBuilder out=new StringBuilder("address\tsymbol\tend_exclusive\towner\tbytes\tinstructions\tpatch_bytes\tpatch_hex\texternal_interior_references\toverwritten_interior_references\tret_cleanup\tstatus\toriginal_sha256\n");
        for(int i=0;i<entries.length;i++) {
            long start=entries[i],end=start+sizes[i];boolean stage=i==1;
            var entry=toAddr(start);var function=getFunctionContaining(entry);
            if(function==null || function.getEntryPoint().getOffset()!=(stage?owner:start)) throw new Exception("Unexpected owner");
            if(!stage && (function.getBody().getNumAddresses()!=sizes[i] || function.getBody().getMinAddress().getOffset()!=start || function.getBody().getMaxAddress().getOffset()!=end-1)) throw new Exception("Unexpected body");
            int patch=prefixes[i].length()/2;StringBuilder actual=new StringBuilder();
            for(int n=0;n<patch;n++) actual.append(String.format("%02x",getByte(entry.add(n))&255));
            if(!actual.toString().equals(prefixes[i])) throw new Exception("Prefix mismatch");
            long cursor=start;int count=0,external=0,overwritten=0,returns=0,exits=0;boolean aligned=false;
            while(cursor<end) {
                monitor.checkCancelled();var inst=getInstructionAt(toAddr(cursor));
                if(inst==null || !function.getBody().contains(inst.getMaxAddress())) throw new Exception("Missing instruction");
                if(inst.getFlowType().isJump()) for(var dest:inst.getFlows()) {
                    long address=dest.getOffset();
                    if(address<start || address>=end) {
                        if(!stage || (address!=end && address!=next)) throw new Exception("Unexpected exit");++exits;
                    }
                }
                if(inst.getMnemonicString().equals("RET")) {
                    if(stage || inst.getNumOperands()!=1 || inst.getScalar(0).getUnsignedValue()!=8) throw new Exception("Unexpected cleanup");++returns;
                }
                for(int n=0;n<inst.getLength();n++) for(var ref:getReferencesTo(toAddr(cursor+n))) {
                    long address=cursor+n,from=ref.getFromAddress().getOffset();
                    if(address>start && address<start+patch) ++overwritten;
                    if(address>start && (from<start || from>=end)) ++external;
                }
                cursor+=inst.getLength();++count;if(cursor==start+patch) aligned=true;
            }
            if(cursor!=end || !aligned || external!=0 || overwritten!=0 || returns!=(stage?0:2) || exits!=(stage?3:0)) throw new Exception("Unsafe boundary/reference");
            if(stage) {
                var last=getInstructionBefore(toAddr(end));
                if(last==null || !last.getFlowType().isUnConditional() || last.getFlows().length!=1 || last.getFlows()[0].getOffset()!=next || getInstructionAt(toAddr(end))==null) throw new Exception("Unexpected last exit");
            }
            out.append(String.format("%08x\t%s\t%08x\t%08x\t%d\t%d\t%d\t%s\t%d\t%d\t%d\teligible\t%s\n",start,names[i],end,stage?owner:start,sizes[i],count,patch,prefixes[i],external,overwritten,stage?0:8,hash));
        }
        Files.writeString(Paths.get(getScriptArgs()[0]),out,StandardCharsets.UTF_8);
        println("CATEGORY_SCORE_AUDIT eligible=2 sha256="+hash);
    }
}
