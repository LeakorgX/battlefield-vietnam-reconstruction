// Read-only control-flow inventory for the second-pass scorer after the
// re-enterable category-factor bridge. It deliberately makes no eligibility
// claim: the result is evidence for choosing the next replacement boundary.
import ghidra.app.script.GhidraScript;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Collections;

public class InspectArtillerySecondCategoryScore extends GhidraScript {
    public void run() throws Exception {
        String hash=currentProgram.getExecutableSHA256();
        boolean client=hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if(!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d"))
            throw new Exception("Unsupported binary hash");
        long start=client?0x9a0dba:0x74b59a;
        long end=client?0x9a0f5e:0x74b73e;
        long owner=client?0x99f2a0:0x749a80;
        var function=getFunctionContaining(toAddr(start));
        if(function==null || function.getEntryPoint().getOffset()!=owner)
            throw new Exception("Unexpected owner function");
        long cursor=start; int instructions=0;
        ArrayList<String> incoming=new ArrayList<>(), exits=new ArrayList<>();
        while(cursor<end) {
            var instruction=getInstructionAt(toAddr(cursor));
            if(instruction==null || !function.getBody().contains(instruction.getMaxAddress()))
                throw new Exception("Invalid instruction boundary at "+Long.toHexString(cursor));
            for(int byteIndex=0;byteIndex<instruction.getLength();byteIndex++)
                for(var reference:getReferencesTo(toAddr(cursor+byteIndex))) {
                    long from=reference.getFromAddress().getOffset();
                    if(from<start || from>=end)
                        incoming.add(String.format("%08x\t%08x\t%s",from,cursor+byteIndex,reference.getReferenceType()));
                }
            if(instruction.getFlowType().isJump()) for(var destination:instruction.getFlows()) {
                long target=destination.getOffset();
                if(target<start || target>=end)
                    exits.add(String.format("%08x\t%08x\t%s",cursor,target,instruction.getFlowType()));
            }
            cursor+=instruction.getLength(); ++instructions;
        }
        if(cursor!=end) throw new Exception("End is not instruction-aligned");
        Collections.sort(incoming); Collections.sort(exits);
        StringBuilder output=new StringBuilder("key\tvalue\n");
        output.append(String.format("start\t%08x\nend_exclusive\t%08x\nowner\t%08x\nbytes\t%d\ninstructions\t%d\noriginal_sha256\t%s\n",start,end,owner,end-start,instructions,hash));
        output.append("incoming_from\tincoming_to\ttype\n");
        for(String row:incoming) output.append(row).append('\n');
        output.append("exit_from\texit_to\ttype\n");
        for(String row:exits) output.append(row).append('\n');
        Files.writeString(Paths.get(getScriptArgs()[0]),output,StandardCharsets.UTF_8);
        println("SECOND_CATEGORY_SCORE_INSPECTION instructions="+instructions+" incoming="+incoming.size()+" exits="+exits.size());
    }
}
