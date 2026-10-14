// Read-only audit of the fresh-search prefix, excluding the existing cache path.
import ghidra.app.script.GhidraScript;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import ghidra.program.model.listing.FlowOverride;
import ghidra.program.model.address.AddressSet;

public class AuditArtillerySearchPrefix extends GhidraScript {
    public void run() throws Exception {
        String hash=currentProgram.getExecutableSHA256();
        boolean client=hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if(!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d")) throw new Exception("Unsupported hash");
        long start=client?0x99f3cf:0x749baf,end=client?0x99f6c3:0x749ea3;
        long owner=client?0x99f2a0:0x749a80,empty=client?0x9a0493:0x74ac73;
        String guard=client?"8b0d50efe000":"8b0db014c300";
        if(!java.util.HexFormat.of().formatHex(getBytes(toAddr(start),6)).equals(guard)) throw new Exception("Guard mismatch");
        var function=getFunctionContaining(toAddr(start));
        if(function==null || function.getEntryPoint().getOffset()!=owner) throw new Exception("Unexpected owner");
        // Repair a known private-analysis no-return annotation, only after
        // verifying the actual CALL target and the allocator's normal RET.
        // Headless -readOnly discards this metadata repair after the audit.
        long call=client?0x99f45b:0x749c3b,release=client?0x403680:0x403df0;
        if((getByte(toAddr(call))&255)!=0xe8 || call+5+getInt(toAddr(call+1))!=release)
            throw new Exception("Unexpected release call encoding");
        if((getByte(toAddr(release+0x59))&255)!=0xc3) throw new Exception("Release return guard mismatch");
        var releaseFunction=getFunctionAt(toAddr(release));
        if(releaseFunction==null) throw new Exception("Missing release function");
        releaseFunction.setNoReturn(false);
        var releaseCall=getInstructionAt(toAddr(call));
        releaseCall.setFlowOverride(FlowOverride.NONE);releaseCall.clearFallThroughOverride();
        var continuation=toAddr(call+5);
        var conflicting=getFunctionContaining(continuation);
        if(conflicting!=null && !conflicting.equals(function)) throw new Exception("Conflicting continuation owner");
        if(getInstructionAt(continuation)==null && !disassemble(continuation)) throw new Exception("Cannot decode continuation");
        long finalRet=client?0x9a12ea:0x74baca;
        if(!java.util.HexFormat.of().formatHex(getBytes(toAddr(finalRet),3)).equals("c21400")) throw new Exception("Evaluator return guard mismatch");
        function.setBody(new AddressSet(toAddr(owner),toAddr(finalRet+2)));
        long cursor=start;int count=0,interior=0,overwritten=0,exits=0,saved_ebp=0;
        while(cursor<end) {
            var instruction=getInstructionAt(toAddr(cursor));
            if(instruction==null || !function.getBody().contains(instruction.getMaxAddress())) throw new Exception("Invalid instruction at "+Long.toHexString(cursor)+" missing="+(instruction==null));
            if(instruction.getFlowType().isTerminal()) throw new Exception("Unexpected terminal at "+instruction.getAddress()+": "+instruction+" flow="+instruction.getFlowType());
            if(instruction.getMnemonicString().equals("PUSH") && instruction.toString().equals("PUSH EBP")) ++saved_ebp;
            if(instruction.getFlowType().isJump()) for(var flow:instruction.getFlows()) {
                long destination=flow.getOffset();
                if(destination<start || destination>=end) {
                    if(destination!=empty) throw new Exception("Unexpected exit");++exits;
                }
            }
            for(int i=0;i<instruction.getLength();i++) for(var reference:getReferencesTo(toAddr(cursor+i))) {
                long from=reference.getFromAddress().getOffset();
                if(cursor+i>start && (from<start || from>=end)) ++interior;
                if(cursor+i>start && cursor+i<start+6) ++overwritten;
            }
            cursor+=instruction.getLength();++count;
        }
        if(cursor!=end || interior!=0 || overwritten!=0 || exits!=1 || saved_ebp!=1) throw new Exception("Unsafe prefix boundary");
        String output="address\tsymbol\tend_exclusive\towner\tbytes\tinstructions\tpatch_bytes\tpatch_hex\texternal_interior_references\toverwritten_interior_references\texternal_exits\tsaved_ebp\trepaired_release_call\tstatus\toriginal_sha256\n"+
            String.format("%08x\tbfv_artillery_search_prefix_bridge\t%08x\t%08x\t756\t%d\t6\t%s\t0\t0\t1\t1\t%08x\teligible\t%s\n",start,end,owner,count,guard,call,hash);
        Files.writeString(Paths.get(getScriptArgs()[0]),output,StandardCharsets.UTF_8);
        println("SEARCH_PREFIX_AUDIT eligible=true instructions="+count);
    }
}
