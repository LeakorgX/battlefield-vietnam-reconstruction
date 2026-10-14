// Read-only boundary audit for first-pass timestamp/position/distance setup.
import ghidra.app.script.GhidraScript;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;

public class AuditArtilleryCandidateSetup extends GhidraScript {
    public void run() throws Exception {
        String hash=currentProgram.getExecutableSHA256();
        boolean client=hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if(!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d")) throw new Exception("Unsupported hash");
        long start=client?0x99f7ab:0x749f8b,end=client?0x99f8d7:0x74a0b7,owner=client?0x99f2a0:0x749a80;
        String guard="8b7c24188b0f";
        if(!java.util.HexFormat.of().formatHex(getBytes(toAddr(start),6)).equals(guard)) throw new Exception("Guard mismatch");
        var function=getFunctionContaining(toAddr(start));
        if(function==null || function.getEntryPoint().getOffset()!=owner) throw new Exception("Unexpected owner");
        long cursor=start;int count=0,interior=0,overwritten=0,exits=0;
        while(cursor<end) {
            var instruction=getInstructionAt(toAddr(cursor));
            if(instruction==null || !function.getBody().contains(instruction.getMaxAddress())) throw new Exception("Invalid instruction");
            if(instruction.getFlowType().isTerminal()) throw new Exception("Unexpected terminal");
            if(instruction.getFlowType().isJump()) for(var flow:instruction.getFlows()) {
                long destination=flow.getOffset();if(destination<start || destination>=end) ++exits;
            }
            for(int i=0;i<instruction.getLength();i++) for(var reference:getReferencesTo(toAddr(cursor+i))) {
                long from=reference.getFromAddress().getOffset();
                if(cursor+i>start && (from<start || from>=end)) ++interior;
                if(cursor+i>start && cursor+i<start+6) ++overwritten;
            }
            cursor+=instruction.getLength();++count;
        }
        if(cursor!=end || count!=70 || interior!=0 || overwritten!=0 || exits!=0) throw new Exception("Unsafe boundary count="+count);
        String output="address\tsymbol\tend_exclusive\towner\tbytes\tinstructions\tpatch_bytes\tpatch_hex\texternal_interior_references\toverwritten_interior_references\texternal_exits\tstatus\toriginal_sha256\n"+
            String.format("%08x\tbfv_artillery_candidate_setup_bridge\t%08x\t%08x\t300\t%d\t6\t%s\t0\t0\t0\teligible\t%s\n",start,end,owner,count,guard,hash);
        Files.writeString(Paths.get(getScriptArgs()[0]),output,StandardCharsets.UTF_8);
        println("CANDIDATE_SETUP_AUDIT eligible=true instructions="+count);
    }
}
