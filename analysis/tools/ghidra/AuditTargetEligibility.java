// Complete alternate-path eligibility function; use -readOnly.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;

public class AuditTargetEligibility extends GhidraScript {
    public void run() throws Exception {
        String hash=currentProgram.getExecutableSHA256();
        boolean client=hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if(!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d"))
            throw new Exception("Unsupported hash");
        long[] starts=client?new long[]{0x97ede0}:new long[]{0x728c90};
        int[] sizes={399};
        String[] names={"bfv_target_handle_eligible"};
        String[] prefixes={"51568bc125ffff0000"};
        StringBuilder out=new StringBuilder("address\tsymbol\tend_exclusive\tbytes\tinstructions\tpatch_bytes\tpatch_hex\texternal_interior_references\toverwritten_interior_references\tret_cleanup\tindirect_tail_jump\tstatus\toriginal_sha256\n");
        for(int i=0;i<starts.length;i++) {
            long start=starts[i],end=start+sizes[i];
            var entry=toAddr(start); Function function=getFunctionAt(entry);
            if(function==null || function.getBody().getMinAddress().getOffset()!=start ||
                function.getBody().getMaxAddress().getOffset()!=end-1)
                throw new Exception("Unexpected function body at "+entry);
            int patch=prefixes[i].length()/2; StringBuilder actual=new StringBuilder();
            for(int n=0;n<patch;n++) actual.append(String.format("%02x",getByte(entry.add(n))&255));
            if(!actual.toString().equals(prefixes[i])) throw new Exception("Prefix mismatch at "+entry);
            long cursor=start; int instructions=0,external=0,overwritten=0,returns=0;
            boolean aligned=false;
            while(cursor<end) {
                var instruction=getInstructionAt(toAddr(cursor));
                if(instruction==null || !function.getBody().contains(instruction.getMaxAddress()))
                    throw new Exception("Missing instruction at "+toAddr(cursor));
                if(instruction.getMnemonicString().equals("RET")) {
                    if(instruction.getNumOperands()!=0) throw new Exception("Unexpected RET");
                    ++returns;
                }
                for(int n=0;n<instruction.getLength();n++) for(var ref:getReferencesTo(toAddr(cursor+n))) {
                    long address=cursor+n,from=ref.getFromAddress().getOffset();
                    if(address>start && address<start+patch) ++overwritten;
                    if(address>start && (from<start || from>=end)) ++external;
                }
                cursor+=instruction.getLength(); ++instructions;
                if(cursor==start+patch) aligned=true;
            }
            boolean tail=false;
            if(cursor!=end || !aligned || external!=0 || overwritten!=0 || returns!=2)
                throw new Exception("Unsafe boundary/reference at "+entry);
            out.append(String.format("%08x\t%s\t%08x\t%d\t%d\t%d\t%s\t%d\t%d\t%d\t%d\teligible\t%s\n",
                start,names[i],end,sizes[i],instructions,patch,prefixes[i],external,overwritten,0,tail?1:0,hash));
        }
        Files.writeString(Paths.get(getScriptArgs()[0]),out,StandardCharsets.UTF_8);
        println("TARGET_ELIGIBILITY_AUDIT eligible=1 sha256="+hash);
    }
}
