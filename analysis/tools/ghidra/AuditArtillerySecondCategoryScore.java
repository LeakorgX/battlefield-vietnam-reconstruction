// Read-only structural audit for the second-pass category scorer interval.
import ghidra.app.script.GhidraScript;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;

public class AuditArtillerySecondCategoryScore extends GhidraScript {
    public void run() throws Exception {
        String hash=currentProgram.getExecutableSHA256();
        boolean client=hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if(!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d"))
            throw new Exception("Unsupported binary hash");
        long start=client?0x9a0dba:0x74b59a, end=client?0x9a0f5e:0x74b73e;
        long factor=client?0x9a0cf0:0x74b4d0, owner=client?0x99f2a0:0x749a80;
        byte[] guard=java.util.HexFormat.of().parseHex("8b8c24d80000008b51088b44243c");
        var function=getFunctionContaining(toAddr(start));
        if(function==null || function.getEntryPoint().getOffset()!=owner) throw new Exception("Unexpected owner");
        for(int i=0;i<guard.length;i++) if(getByte(toAddr(start+i))!=guard[i]) throw new Exception("Guard mismatch");
        long cursor=start; int instructions=0, external=0, overwritten=0, exits=0;
        while(cursor<end) {
            var instruction=getInstructionAt(toAddr(cursor));
            if(instruction==null || !function.getBody().contains(instruction.getMaxAddress())) throw new Exception("Invalid instruction");
            if(instruction.getFlowType().isJump()) for(var flow:instruction.getFlows()) {
                long destination=flow.getOffset();
                if(destination<start || destination>=end) {
                    if(destination!=factor && destination!=end) throw new Exception("Unexpected exit: "+flow);
                    ++exits;
                }
            }
            for(int i=0;i<instruction.getLength();i++) for(var reference:getReferencesTo(toAddr(cursor+i))) {
                long from=reference.getFromAddress().getOffset();
                if(cursor+i>start && cursor+i<start+guard.length) ++overwritten;
                if(cursor+i>start && (from<start || from>=end)) ++external;
            }
            cursor+=instruction.getLength(); ++instructions;
        }
        if(cursor!=end || external!=0 || overwritten!=0 || exits!=4) throw new Exception("Unsafe scorer boundary");
        String output="address\tsymbol\tend_exclusive\towner\tbytes\tinstructions\tpatch_bytes\tpatch_hex\texternal_interior_references\toverwritten_interior_references\texternal_exits\tstatus\toriginal_sha256\n"+
            String.format("%08x\tbfv_artillery_second_category_score_bridge\t%08x\t%08x\t%d\t%d\t%d\t%s\t%d\t%d\t%d\teligible\t%s\n",start,end,owner,end-start,instructions,guard.length,"8b8c24d80000008b51088b44243c",external,overwritten,exits,hash);
        Files.writeString(Paths.get(getScriptArgs()[0]),output,StandardCharsets.UTF_8);
        println("ARTILLERY_SECOND_CATEGORY_SCORE_AUDIT eligible=true instructions="+instructions+" exits="+exits);
    }
}
