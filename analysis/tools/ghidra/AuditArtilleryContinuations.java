// Guard and flow audit for the four remaining short evaluator entries.
import ghidra.app.script.GhidraScript;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import ghidra.program.model.listing.FlowOverride;
import ghidra.program.model.address.AddressSet;

public class AuditArtilleryContinuations extends GhidraScript {
    public void run() throws Exception {
        String hash=currentProgram.getExecutableSHA256();
        boolean client=hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if(!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d")) throw new Exception("Unsupported hash");
        long owner=client?0x99f2a0:0x749a80,release=client?0x403680:0x403df0,call=client?0x9a0f84:0x74b764;
        if((getByte(toAddr(call))&255)!=0xe8 || call+5+getInt(toAddr(call+1))!=release || (getByte(toAddr(release+0x59))&255)!=0xc3) throw new Exception("Release guards");
        var evaluator=getFunctionAt(toAddr(owner));getFunctionAt(toAddr(release)).setNoReturn(false);
        var releaseCall=getInstructionAt(toAddr(call));releaseCall.setFlowOverride(FlowOverride.NONE);releaseCall.clearFallThroughOverride();
        if(getInstructionAt(toAddr(call+5))==null && !disassemble(toAddr(call+5))) throw new Exception("Release continuation");
        long finalRet=client?0x9a12ea:0x74baca;
        if(!java.util.HexFormat.of().formatHex(getBytes(toAddr(finalRet),3)).equals("c21400")) throw new Exception("Return guard");
        evaluator.setBody(new AddressSet(toAddr(owner),toAddr(finalRet+2)));
        long[] starts=client?new long[]{0x9a0bba,0x9a0bc2,0x9a0f78,0x9a0f7f}:new long[]{0x74b39a,0x74b3a2,0x74b758,0x74b75f};
        long[] ends=client?new long[]{0x9a0bc2,0x9a0bc9,0x9a0f7f,0x9a0f8c}:new long[]{0x74b3a2,0x74b3a9,0x74b75f,0x74b76c};
        String[] symbols={"bfv_artillery_unit_score_bridge","bfv_artillery_driver_reload_bridge","bfv_artillery_allocation_reload_bridge","bfv_artillery_query_cleanup_bridge"};
        String[] guards={"c74424340000803f","8bbc24fc010000","8b842400010000","85c0740950"};
        StringBuilder output=new StringBuilder("address\tsymbol\tend_exclusive\towner\tbytes\tinstructions\tpatch_bytes\tpatch_hex\texternal_interior_references\toverwritten_interior_references\texternal_exits\tstatus\toriginal_sha256\n");
        for(int block=0;block<starts.length;block++) {
            long start=starts[block],end=ends[block],cursor=start;int count=0,external=0,overwritten=0,exits=0;
            byte[] guard=java.util.HexFormat.of().parseHex(guards[block]);
            if(!java.util.Arrays.equals(guard,getBytes(toAddr(start),guard.length))) throw new Exception("Entry guard");
            while(cursor<end) {
                var instruction=getInstructionAt(toAddr(cursor));
                if(instruction==null || !evaluator.getBody().contains(instruction.getMaxAddress())) throw new Exception("Invalid instruction");
                if(instruction.getFlowType().isJump()) for(var flow:instruction.getFlows()) {
                    long destination=flow.getOffset();if(destination<start || destination>=end) {
                        if(destination!=end) throw new Exception("Unexpected exit");++exits;
                    }
                }
                for(int i=0;i<instruction.getLength();i++) for(var ref:getReferencesTo(toAddr(cursor+i))) {
                    long from=ref.getFromAddress().getOffset();
                    if(cursor+i>start && (from<start || from>=end)) ++external;
                    if(cursor+i>start && cursor+i<start+guard.length) ++overwritten;
                }
                cursor+=instruction.getLength();++count;
            }
            if(cursor!=end || external!=0 || overwritten!=0 || exits!=(block==3?1:0)) throw new Exception("Unsafe boundary");
            output.append(String.format("%08x\t%s\t%08x\t%08x\t%d\t%d\t%d\t%s\t0\t0\t%d\teligible\t%s\n",start,symbols[block],end,owner,end-start,count,guard.length,guards[block],exits,hash));
        }
        Files.writeString(Paths.get(getScriptArgs()[0]),output,StandardCharsets.UTF_8);println("CONTINUATIONS_AUDIT eligible=true blocks=4");
    }
}
