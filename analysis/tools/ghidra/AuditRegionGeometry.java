// Four complete, directly tested region/projection helpers; use -readOnly.
import ghidra.app.script.GhidraScript;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
public class AuditRegionGeometry extends GhidraScript {
    public void run() throws Exception {
        String hash=currentProgram.getExecutableSHA256();
        boolean client=hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if(!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d")) throw new Exception("Unsupported hash");
        long[] entries=client?new long[]{0x97f070,0x9d5cc0,0x964ac0,0x964b70}:new long[]{0x728f20,0x78cd10,0x71c280,0x71c330};
        String[] names={"bfv_point_xz","bfv_point_in_symmetric_bounds","bfv_region_contains_point2","bfv_region_contains_point3"};
        String[] prefixes={"8bc28b118910","d90183ec08","8a81c4010000","83ec08568bf1"};
        int[] sizes={13,89,144,34},cleanup={0,4,4,4},retCounts={1,2,3,1};
        StringBuilder out=new StringBuilder("address\tsymbol\tend_exclusive\tbytes\tinstructions\tpatch_bytes\tpatch_hex\tentry_references\texternal_interior_references\toverwritten_interior_references\tret_cleanup\tstatus\toriginal_sha256\n");
        for(int i=0;i<entries.length;i++) {
            long start=entries[i],end=start+sizes[i];var entry=toAddr(start);var function=getFunctionAt(entry);
            if(function==null || function.getBody().getNumAddresses()!=sizes[i] || function.getBody().getMinAddress().getOffset()!=start || function.getBody().getMaxAddress().getOffset()!=end-1) throw new Exception("Unexpected body");
            int patch=prefixes[i].length()/2;StringBuilder actual=new StringBuilder();
            for(int n=0;n<patch;n++) actual.append(String.format("%02x",getByte(entry.add(n))&255));
            if(!actual.toString().equals(prefixes[i])) throw new Exception("Prefix mismatch: "+entry);
            long cursor=start;int count=0,external=0,overwritten=0,returns=0;boolean aligned=false;
            while(cursor<end) {
                monitor.checkCancelled();var inst=getInstructionAt(toAddr(cursor));
                if(inst==null || !function.getBody().contains(inst.getMaxAddress())) throw new Exception("Missing instruction");
                if(inst.getFlowType().isJump()) for(var dest:inst.getFlows()) if(dest.getOffset()<start || dest.getOffset()>=end) throw new Exception("Unexpected branch exit");
                if(inst.getMnemonicString().equals("RET")) {
                    if(cleanup[i]==0 ? inst.getNumOperands()!=0 : inst.getNumOperands()!=1 || inst.getScalar(0).getUnsignedValue()!=cleanup[i]) throw new Exception("Unexpected cleanup");
                    ++returns;
                }
                for(int n=0;n<inst.getLength();n++) for(var ref:getReferencesTo(toAddr(cursor+n))) {
                    long address=cursor+n,from=ref.getFromAddress().getOffset();
                    if(address>start && address<start+patch) ++overwritten;
                    if(address>start && (from<start || from>=end)) ++external;
                }
                cursor+=inst.getLength();++count;if(cursor==start+patch) aligned=true;
            }
            if(cursor!=end || !aligned || returns!=retCounts[i] || external!=0 || overwritten!=0) throw new Exception("Unsafe boundary/reference");
            out.append(String.format("%08x\t%s\t%08x\t%d\t%d\t%d\t%s\t%d\t%d\t%d\t%d\teligible\t%s\n",start,names[i],end,sizes[i],count,patch,prefixes[i],getReferencesTo(entry).length,external,overwritten,cleanup[i],hash));
        }
        Files.writeString(Paths.get(getScriptArgs()[0]),out,StandardCharsets.UTF_8);
        println("REGION_GEOMETRY_AUDIT eligible=4 sha256="+hash);
    }
}
