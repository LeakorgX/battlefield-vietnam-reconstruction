// Read-only control-flow audit; no native bodies are exported.
import ghidra.app.script.GhidraScript;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;

public class AuditArtilleryFinalWeights extends GhidraScript {
    public void run() throws Exception {
        String hash=currentProgram.getExecutableSHA256();
        boolean client=hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if(!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d"))
            throw new Exception("Unsupported binary hash");
        long owner=client?0x99f2a0:0x749a80;
        long[] starts=client?new long[]{0x9a0f5e,0x9a0f8c}:new long[]{0x74b73e,0x74b76c};
        long[] ends=client?new long[]{0x9a0f78,0x9a1001}:new long[]{0x74b758,0x74b7e1};
        int[] guards={11,16};
        String[] symbols={"bfv_artillery_second_iterator_bridge","bfv_artillery_final_weights_bridge"};
        StringBuilder output=new StringBuilder("address\tsymbol\tend_exclusive\towner\tbytes\tinstructions\tpatch_bytes\tpatch_hex\texternal_interior_references\toverwritten_interior_references\texternal_exits\tstatus\toriginal_sha256\n");
        for(int block=0;block<starts.length;block++) {
            long start=starts[block],end=ends[block],cursor=start;
            var function=getFunctionContaining(toAddr(start));
            if(function==null || function.getEntryPoint().getOffset()!=owner) throw new Exception("Unexpected owner");
            int count=0,external=0,overwritten=0,exits=0;
            while(cursor<end) {
                var instruction=getInstructionAt(toAddr(cursor));
                if(instruction==null || !function.getBody().contains(instruction.getMaxAddress())) throw new Exception("Invalid instruction");
                for(int i=0;i<instruction.getLength();i++) for(var reference:getReferencesTo(toAddr(cursor+i))) {
                    long from=reference.getFromAddress().getOffset();
                    if(cursor+i>start && cursor+i<start+guards[block]) ++overwritten;
                    if(cursor+i>start && (from<start || from>=end)) ++external;
                }
                if(instruction.getFlowType().isJump()) for(var flow:instruction.getFlows()) {
                    long destination=flow.getOffset();
                    if(destination<start || destination>=end) {
                        long loop=client?0x9a04f0:0x74acd0;
                        if(block!=0 || destination!=loop) throw new Exception("Unexpected exit");
                        ++exits;
                    }
                }
                cursor+=instruction.getLength();++count;
            }
            if(cursor!=end || external!=0 || overwritten!=0 || exits!=(block==0?1:0)) throw new Exception("Unsafe boundary");
            byte[] bytes=getBytes(toAddr(start),guards[block]);
            String guard=java.util.HexFormat.of().formatHex(bytes);
            output.append(String.format("%08x\t%s\t%08x\t%08x\t%d\t%d\t%d\t%s\t%d\t%d\t%d\teligible\t%s\n",start,symbols[block],end,owner,end-start,count,guards[block],guard,external,overwritten,exits,hash));
        }
        Files.writeString(Paths.get(getScriptArgs()[0]),output,StandardCharsets.UTF_8);
        println("ARTILLERY_FINAL_WEIGHTS_AUDIT eligible=true blocks=2");
    }
}
