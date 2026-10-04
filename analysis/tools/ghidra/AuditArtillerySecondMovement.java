// Read-only structural audit of the second-pass movement/flag/distance gate.
import ghidra.app.script.GhidraScript;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class AuditArtillerySecondMovement extends GhidraScript {
    public void run() throws Exception {
        String hash=currentProgram.getExecutableSHA256();
        boolean client=hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if (!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d"))
            throw new Exception("Unsupported binary hash");
        long start=client?0x9a08b8:0x74b098,end=client?0x9a0981:0x74b161;
        long score=client?0x9a0bc2:0x74b3a2,unit=client?0x9a0bba:0x74b39a;
        long owner=client?0x99f2a0:0x749a80;
        var function=getFunctionContaining(toAddr(start));
        if(function==null || function.getEntryPoint().getOffset()!=owner)
            throw new Exception("Unexpected owner");
        byte[] prefix={(byte)0x8b,0x7c,0x24,0x24,(byte)0x85,(byte)0xff};
        for(int n=0;n<prefix.length;n++)
            if(getByte(toAddr(start+n))!=prefix[n]) throw new Exception("Prefix mismatch");
        int instructions=0,external=0,overwritten=0;
        long cursor=start;boolean aligned=false;
        while(cursor<end) {
            monitor.checkCancelled();
            var instruction=getInstructionAt(toAddr(cursor));
            if(instruction==null || !function.getBody().contains(instruction.getMaxAddress()) ||
               instruction.getFlowType().isTerminal()) throw new Exception("Unexpected instruction");
            if(instruction.getFlowType().isJump()) for(var destination:instruction.getFlows()) {
                long address=destination.getOffset();
                if((address<start || address>=end) && address!=end && address!=score && address!=unit)
                    throw new Exception("Unexpected outgoing jump: "+destination);
            }
            for(int n=0;n<instruction.getLength();n++) for(var reference:getReferencesTo(toAddr(cursor+n))) {
                long address=cursor+n,from=reference.getFromAddress().getOffset();
                if(address>start && address<start+prefix.length) ++overwritten;
                if(address>start && (from<start || from>=end)) ++external;
            }
            cursor+=instruction.getLength();++instructions;
            if(cursor==start+prefix.length) aligned=true;
        }
        if(cursor!=end || !aligned || external!=0 || overwritten!=0)
            throw new Exception("Unsafe interval/references");
        for(long address:new long[]{end,score,unit})
            if(getInstructionAt(toAddr(address))==null || !function.getBody().contains(toAddr(address)))
                throw new Exception("Missing continuation");
        String output="address\tsymbol\tend_exclusive\towner\tbytes\tinstructions\tpatch_bytes\tpatch_hex\texternal_interior_references\toverwritten_interior_references\tstatus\toriginal_sha256\n"+
            String.format("%08x\tbfv_artillery_second_movement_gate_bridge\t%08x\t%08x\t%d\t%d\t6\t8b7c242485ff\t%d\t%d\teligible\t%s\n",
                start,end,owner,end-start,instructions,external,overwritten,hash);
        Files.writeString(Paths.get(getScriptArgs()[0]),output,StandardCharsets.UTF_8);
        println("ARTILLERY_SECOND_MOVEMENT_AUDIT eligible=true instructions="+instructions);
    }
}
