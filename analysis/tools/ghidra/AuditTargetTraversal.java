// Complete 41-byte traversal wrapper with RET 8; run read-only.
import ghidra.app.script.GhidraScript;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
public class AuditTargetTraversal extends GhidraScript {
    public void run() throws Exception {
        String hash=currentProgram.getExecutableSHA256();
        boolean client=hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if(!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d")) throw new Exception("Unsupported hash");
        long start=client?0x963a20:0x71b110,end=start+41;String prefix="8b49208b542404";
        var entry=toAddr(start);var function=getFunctionAt(entry);
        if(function==null || function.getBody().getMinAddress().getOffset()!=start || function.getBody().getMaxAddress().getOffset()!=end-1) throw new Exception("Unexpected body");
        int patch=prefix.length()/2;StringBuilder actual=new StringBuilder();
        for(int n=0;n<patch;n++) actual.append(String.format("%02x",getByte(entry.add(n))&255));
        if(!actual.toString().equals(prefix)) throw new Exception("Unexpected prefix");
        long cursor=start;int count=0,external=0,overwritten=0,returns=0;boolean aligned=false;
        while(cursor<end) {
            monitor.checkCancelled();var inst=getInstructionAt(toAddr(cursor));
            if(inst==null || !function.getBody().contains(inst.getMaxAddress())) throw new Exception("Missing instruction");
            if(inst.getMnemonicString().equals("RET")) {
                if(inst.getNumOperands()!=1 || inst.getScalar(0)==null || inst.getScalar(0).getUnsignedValue()!=8) throw new Exception("Unexpected cleanup");
                ++returns;
            }
            for(int n=0;n<inst.getLength();n++) for(var ref:getReferencesTo(toAddr(cursor+n))) {
                long address=cursor+n,from=ref.getFromAddress().getOffset();
                if(address>start && address<start+patch) ++overwritten;
                if(address>start && (from<start || from>=end)) ++external;
            }
            cursor+=inst.getLength();++count;if(cursor==start+patch) aligned=true;
        }
        if(cursor!=end || !aligned || external!=0 || overwritten!=0 || returns!=2) throw new Exception("Unsafe boundary/reference");
        String header="address\tsymbol\tend_exclusive\tbytes\tinstructions\tpatch_bytes\tpatch_hex\texternal_interior_references\toverwritten_interior_references\tret_cleanup\tstatus\toriginal_sha256\n";
        Files.writeString(Paths.get(getScriptArgs()[0]),header+String.format("%08x\tbfv_next_target_handle\t%08x\t41\t%d\t7\t%s\t%d\t%d\t8\teligible\t%s\n",start,end,count,prefix,external,overwritten,hash),StandardCharsets.UTF_8);
        println("TARGET_TRAVERSAL_AUDIT eligible=1 sha256="+hash);
    }
}
