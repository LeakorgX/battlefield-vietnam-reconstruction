// Read-only boundary inventory for the complete evaluator's final selection tail.
// Records references and returns, never native instruction bodies.
import ghidra.app.script.GhidraScript;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;

public class InspectArtilleryFinalSelection extends GhidraScript {
    public void run() throws Exception {
        String hash=currentProgram.getExecutableSHA256();
        boolean client=hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if(!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d"))
            throw new Exception("Unsupported binary hash");
        long start=client?0x9a1001:0x74b7e1,end=client?0x9a12ed:0x74bacd;
        long owner=client?0x99f2a0:0x749a80;
        var function=getFunctionContaining(toAddr(start));
        if(function==null || function.getEntryPoint().getOffset()!=owner) throw new Exception("Unexpected owner");
        int count=0,returns=0,interior=0,exits=0;
        StringBuilder references=new StringBuilder();
        long cursor=start;
        while(cursor<end) {
            var instruction=getInstructionAt(toAddr(cursor));
            if(instruction==null || !function.getBody().contains(instruction.getMaxAddress())) throw new Exception("Invalid boundary");
            if(instruction.getFlowType().isTerminal()) ++returns;
            if(instruction.getFlowType().isJump()) for(var flow:instruction.getFlows()) {
                long destination=flow.getOffset();
                if(destination<start || destination>=end) {
                    ++exits;references.append(String.format("exit\t%08x->%08x\n",cursor,destination));
                }
            }
            for(int i=0;i<instruction.getLength();i++) for(var reference:getReferencesTo(toAddr(cursor+i))) {
                long from=reference.getFromAddress().getOffset();
                if(from<start || from>=end) {
                    if(cursor+i>start) ++interior;
                    references.append(String.format("incoming\t%08x->%08x\n",from,cursor+i));
                }
            }
            cursor+=instruction.getLength();++count;
        }
        if(cursor!=end) throw new Exception("Unaligned end");
        String output="key\tvalue\n"+String.format("start\t%08x\nend_exclusive\t%08x\nowner\t%08x\nbytes\t%d\ninstructions\t%d\nreturns\t%d\nexternal_interior_references\t%d\nexternal_exits\t%d\noriginal_sha256\t%s\n",start,end,owner,end-start,count,returns,interior,exits,hash)+references;
        Files.writeString(Paths.get(getScriptArgs()[0]),output,StandardCharsets.UTF_8);
        println("FINAL_SELECTION_INSPECTION instructions="+count+" returns="+returns+" interior="+interior+" exits="+exits);
    }
}
