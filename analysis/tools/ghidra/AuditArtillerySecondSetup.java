// Read-only second-pass routing and query-vector initialization.
import ghidra.app.script.GhidraScript;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
public class AuditArtillerySecondSetup extends GhidraScript {
    public void run() throws Exception {
        String hash=currentProgram.getExecutableSHA256();
        boolean client=hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if(!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d")) throw new Exception("Unsupported hash");
        long start=client?0x9a0493:0x74ac73,end=start+93,owner=client?0x99f2a0:0x749a80;
        long empty=client?0x9a0f7f:0x74b75f,skip=client?0x9a0f8c:0x74b76c;
        String prefix="8b4c24508b11";int patch=6;
        var function=getFunctionContaining(toAddr(start));
        if(function==null || function.getEntryPoint().getOffset()!=owner) throw new Exception("Unexpected owner");
        StringBuilder actual=new StringBuilder();
        for(int n=0;n<patch;n++) actual.append(String.format("%02x",getByte(toAddr(start+n))&255));
        if(!actual.toString().equals(prefix)) throw new Exception("Prefix mismatch");
        long cursor=start;int count=0,external=0,overwritten=0,empty_exits=0,skip_exits=0;boolean aligned=false;
        while(cursor<end) {
            monitor.checkCancelled();var inst=getInstructionAt(toAddr(cursor));
            if(inst==null || !function.getBody().contains(inst.getMaxAddress())) throw new Exception("Missing instruction");
            if(inst.getFlowType().isJump()) for(var dest:inst.getFlows()) {
                long address=dest.getOffset();
                if(address==empty) ++empty_exits;else if(address==skip) ++skip_exits;else throw new Exception("Unexpected branch");
            }
            if(inst.getFlowType().isTerminal()) throw new Exception("Unexpected terminal");
            for(int n=0;n<inst.getLength();n++) for(var ref:getReferencesTo(toAddr(cursor+n))) {
                long address=cursor+n,from=ref.getFromAddress().getOffset();
                if(address>start && address<start+patch) ++overwritten;
                if(address>start && (from<start || from>=end)) ++external;
            }
            cursor+=inst.getLength();++count;if(cursor==start+patch) aligned=true;
        }
        var last=getInstructionBefore(toAddr(end));
        if(cursor!=end || !aligned || external!=0 || overwritten!=0 || empty_exits!=1 || skip_exits!=1 || last==null || !toAddr(end).equals(last.getFallThrough()) || getInstructionAt(toAddr(end))==null) throw new Exception("Unsafe boundary/reference");
        String header="address\tsymbol\tend_exclusive\towner\tbytes\tinstructions\tpatch_bytes\tpatch_hex\texternal_interior_references\toverwritten_interior_references\tstatus\toriginal_sha256\n";
        Files.writeString(Paths.get(getScriptArgs()[0]),header+String.format("%08x\tbfv_artillery_second_setup_bridge\t%08x\t%08x\t93\t%d\t6\t%s\t%d\t%d\teligible\t%s\n",start,end,owner,count,prefix,external,overwritten,hash),StandardCharsets.UTF_8);
        println("ARTILLERY_SECOND_SETUP_AUDIT eligible=1 sha256="+hash);
    }
}
