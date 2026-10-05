// Closed three-return boundary audit for the complete final-selection tail.
import ghidra.app.script.GhidraScript;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;

public class AuditArtilleryFinalSelection extends GhidraScript {
    public void run() throws Exception {
        String hash=currentProgram.getExecutableSHA256();
        boolean client=hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if(!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d"))
            throw new Exception("Unsupported binary hash");
        long start=client?0x9a1001:0x74b7e1,end=client?0x9a12ed:0x74bacd;
        long owner=client?0x99f2a0:0x749a80;
        String guard="d9842480000000";
        if(!java.util.HexFormat.of().formatHex(getBytes(toAddr(start),7)).equals(guard)) throw new Exception("Guard mismatch");
        var function=getFunctionContaining(toAddr(start));
        if(function==null || function.getEntryPoint().getOffset()!=owner) throw new Exception("Unexpected owner");
        int count=0,returns=0,interior=0,overwritten=0,exits=0;
        long cursor=start;
        while(cursor<end) {
            var instruction=getInstructionAt(toAddr(cursor));
            if(instruction==null || !function.getBody().contains(instruction.getMaxAddress())) throw new Exception("Invalid boundary");
            if(instruction.getFlowType().isTerminal()) {
                if(!instruction.getMnemonicString().equals("RET")) throw new Exception("Unexpected terminal");
                ++returns;
            }
            if(instruction.getFlowType().isJump()) for(var flow:instruction.getFlows()) {
                long destination=flow.getOffset();if(destination<start || destination>=end) ++exits;
            }
            for(int i=0;i<instruction.getLength();i++) for(var reference:getReferencesTo(toAddr(cursor+i))) {
                long from=reference.getFromAddress().getOffset();
                if(cursor+i>start && (from<start || from>=end)) ++interior;
                if(cursor+i>start && cursor+i<start+7) ++overwritten;
            }
            cursor+=instruction.getLength();++count;
        }
        if(cursor!=end || count!=232 || returns!=3 || interior!=0 || overwritten!=0 || exits!=0)
            throw new Exception("Unsafe final-selection boundary");
        String output="address\tsymbol\tend_exclusive\towner\tbytes\tinstructions\tpatch_bytes\tpatch_hex\texternal_interior_references\toverwritten_interior_references\texternal_exits\treturns\tstatus\toriginal_sha256\n"+
            String.format("%08x\tbfv_artillery_final_selection_bridge\t%08x\t%08x\t748\t232\t7\t%s\t0\t0\t0\t3\teligible\t%s\n",start,end,owner,guard,hash);
        Files.writeString(Paths.get(getScriptArgs()[0]),output,StandardCharsets.UTF_8);
        println("FINAL_SELECTION_AUDIT eligible=true returns=3");
    }
}
