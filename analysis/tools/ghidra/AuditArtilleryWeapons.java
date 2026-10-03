// Read-only audit of the first-pass inline weapon-selection block.
import ghidra.app.script.GhidraScript;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class AuditArtilleryWeapons extends GhidraScript {
    public void run() throws Exception {
        String hash=currentProgram.getExecutableSHA256();
        boolean client=hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if (!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d"))
            throw new Exception("Unsupported binary hash");
        long start=client?0x99f8d7:0x74a0b7, end=client?0x99fa61:0x74a241;
        long reject=client?0x9a0476:0x74ac56, owner=client?0x99f2a0:0x749a80;
        var function=getFunctionContaining(toAddr(start));
        if (function==null || function.getEntryPoint().getOffset()!=owner)
            throw new Exception("Unexpected containing function");
        byte[] prefix={(byte)0x8b,0x4c,0x24,0x50,(byte)0x8b,0x11};
        for(int n=0;n<prefix.length;n++)
            if(getByte(toAddr(start+n))!=prefix[n]) throw new Exception("Prefix mismatch");
        int instructions=0,externalInterior=0,overwrittenInterior=0;
        long cursor=start;
        while(cursor<end) {
            monitor.checkCancelled();
            var instruction=getInstructionAt(toAddr(cursor));
            if(instruction==null || !function.getBody().contains(instruction.getMaxAddress()))
                throw new Exception("Missing or foreign instruction");
            if(instruction.getFlowType().isJump()) {
                for(var destination:instruction.getFlows()) {
                    long address=destination.getOffset();
                    if((address<start || address>=end) && address!=end && address!=reject)
                        throw new Exception("Unexpected outgoing branch: "+destination);
                }
            }
            for(int n=0;n<instruction.getLength();n++) {
                long address=cursor+n;
                for(var reference:getReferencesTo(toAddr(address))) {
                    long from=reference.getFromAddress().getOffset();
                    if(address>start && address<start+6) ++overwrittenInterior;
                    if(address>start && (from<start || from>=end)) ++externalInterior;
                }
            }
            cursor+=instruction.getLength();++instructions;
            if(cursor>start && cursor<start+6 && cursor!=start+4)
                throw new Exception("Unexpected entry instructions");
        }
        if(cursor!=end || externalInterior!=0 || overwrittenInterior!=0)
            throw new Exception("Unsafe block boundary/reference");
        String output="start\tend_exclusive\treject\towner\tbytes\tinstructions\tpatch_bytes\texternal_interior_references\toverwritten_interior_references\toriginal_sha256\n"+
            String.format("%08x\t%08x\t%08x\t%08x\t%d\t%d\t6\t%d\t%d\t%s\n",
                start,end,reject,owner,end-start,instructions,externalInterior,overwrittenInterior,hash);
        Files.writeString(Paths.get(getScriptArgs()[0]),output,StandardCharsets.UTF_8);
        println("ARTILLERY_WEAPONS_AUDIT eligible=true instructions="+instructions);
    }
}
