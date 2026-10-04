// In-memory structural audit only; invoke with -readOnly.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.FlowOverride;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class AuditArtilleryQuery extends GhidraScript {
    public void run() throws Exception {
        String hash=currentProgram.getExecutableSHA256();
        boolean client=hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if(!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d"))
            throw new Exception("Unsupported hash");
        long[] entries=client?new long[]{0x92db30,0x970440,0x975ab0,0x490610,0x99fe3b}:
            new long[]{0x6eb4b0,0x727d50,0x72d870,0x6ff4f0,0x74a61b};
        String[] names={"bfv_component_event2_word","bfv_component_query_data","bfv_query_vector_append",
            "bfv_query_vector_destroy","bfv_artillery_query_gate_bridge"};
        String[] prefixes={"8b41048b4820","8b098b01ff5034","568b710485f6","568bf18b4604",
            client?"d944241cd80d2863bf00":"d944241cd80d28478700"};
        int[] sizes={23,38,77,42,502},cleanup={0,0,4,0,0};
        long owner=client?0x99f2a0:0x749a80,reject=client?0x9a0476:0x74ac56;
        // The raw-free routine is incorrectly marked no-return in this project.
        // Repair only the in-memory listing/body; check the missing ADD ESP,4.
        long destructor=entries[3],free=client?0x403680:0x403df0;
        var release=getFunctionAt(toAddr(free));
        var call=getInstructionAt(toAddr(destructor+11));
        if(release==null || call==null || !call.getMnemonicString().equals("CALL") ||
           call.getFlows().length!=1 || call.getFlows()[0].getOffset()!=free)
            throw new Exception("Unexpected destructor release call");
        release.setNoReturn(false);call.setFlowOverride(FlowOverride.NONE);
        var gap=toAddr(destructor+16);
        if((getByte(gap)&255)!=0x83 || (getByte(gap.add(1))&255)!=0xc4 || (getByte(gap.add(2))&255)!=4)
            throw new Exception("Unexpected stack adjustment");
        if(getInstructionAt(gap)==null && !disassemble(gap)) throw new Exception("Cannot decode stack adjustment");
        getFunctionAt(toAddr(destructor)).setBody(new AddressSet(toAddr(destructor),toAddr(destructor+41)));
        StringBuilder out=new StringBuilder("address\tsymbol\tend_exclusive\towner\tbytes\tinstructions\tpatch_bytes\tpatch_hex\tentry_references\texternal_interior_references\toverwritten_interior_references\tret_cleanup\trepaired_body_bytes\tindirect_tail_exits\tstatus\toriginal_sha256\n");
        for(int i=0;i<entries.length;i++) {
            long start=entries[i],end=start+sizes[i];boolean stage=i==4;
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
            long cursor=start;boolean aligned=false;int count=0,external=0,overwritten=0,returns=0,tail=0,exits=0;
            while(cursor<end) {
                monitor.checkCancelled();var inst=getInstructionAt(toAddr(cursor));
                if(inst==null || !function.getBody().contains(inst.getMaxAddress())) throw new Exception("Missing instruction");
                if(inst.getFlowType().isJump()) {
                    if(inst.getFlows().length==0) {
                        if(i!=1 || !inst.getFlowType().isComputed() || !inst.getMnemonicString().equals("JMP"))
                            throw new Exception("Unexpected indirect exit");
                        ++tail;
                    }
                    for(var dest:inst.getFlows()) {
                        long address=dest.getOffset();
                        if(address<start || address>=end) {
                            if(!stage || (address!=end && address!=reject)) throw new Exception("Unexpected outgoing branch: "+dest);
                            ++exits;
                        }
                    }
                }
                if(inst.getMnemonicString().equals("RET")) {
                    if(stage || (cleanup[i]==0 ? inst.getNumOperands()!=0 :
                        inst.getNumOperands()!=1 || inst.getScalar(0).getUnsignedValue()!=cleanup[i]))
                        throw new Exception("Unexpected return cleanup");
                    ++returns;
                }
                for(int n=0;n<inst.getLength();n++) for(var ref:getReferencesTo(toAddr(cursor+n))) {
                    long address=cursor+n,from=ref.getFromAddress().getOffset();
                    if(address>start && address<start+patch) ++overwritten;
                    if(address>start && (from<start || from>=end)) ++external;
                }
                cursor+=inst.getLength();++count;if(cursor==start+patch) aligned=true;
            }
            if(cursor!=end || !aligned || external!=0 || overwritten!=0 ||
               (!stage && returns!=(i==2?2:1)) || tail!=(i==1?1:0))
                throw new Exception("Unsafe boundary/reference: "+entry);
            if(stage) {
                var last=getInstructionBefore(toAddr(end));
                if(exits!=5 || last==null || !toAddr(end).equals(last.getFallThrough()) || getInstructionAt(toAddr(end))==null)
                    throw new Exception("Unexpected query continuation");
            }
            out.append(String.format("%08x\t%s\t%08x\t%08x\t%d\t%d\t%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\teligible\t%s\n",
                start,names[i],end,stage?owner:start,sizes[i],count,patch,prefixes[i],getReferencesTo(entry).length,
                external,overwritten,cleanup[i],i==3?3:0,tail,hash));
        }
        Files.writeString(Paths.get(getScriptArgs()[0]),out,StandardCharsets.UTF_8);
        println("ARTILLERY_QUERY_AUDIT eligible=5 sha256="+hash);
    }
}
