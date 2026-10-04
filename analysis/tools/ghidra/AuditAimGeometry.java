// Read-only boundary/reference audit for position helpers and the inline aim gate.
import ghidra.app.script.GhidraScript;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class AuditAimGeometry extends GhidraScript {
    public void run() throws Exception {
        String hash=currentProgram.getExecutableSHA256();
        boolean client=hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if (!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d"))
            throw new Exception("Unsupported binary hash");
        long[] entries=client?new long[]{0x49b340,0x49b490,0x9be440,0x994a80,0x99fa61}
                             :new long[]{0x5b25f0,0x48d160,0x78df90,0x73f1e0,0x74a241};
        String[] symbols={"bfv_transform_point","bfv_vector_difference","bfv_aim_world_position",
                          "bfv_component_position","bfv_artillery_aim_gate_bridge"};
        String[] prefixes={"8b442404d94108","8b442408d900","568bf18b4e04","8b41048b542404","8b4424188a4814"};
        int[] sizes={90,37,113,31,208}, cleanup={8,8,4,4,0};
        long owner=client?0x99f2a0:0x749a80, reject=client?0x9a0476:0x74ac56;
        StringBuilder out=new StringBuilder("address\tsymbol\tend_exclusive\towner\tbytes\tinstructions\tpatch_bytes\tpatch_hex\tentry_references\texternal_interior_references\toverwritten_interior_references\tret_cleanup\toutgoing_jumps\tstatus\toriginal_sha256\n");
        for(int i=0;i<entries.length;i++) {
            long start=entries[i],end=start+sizes[i];
            boolean gate=i==4;
            var entry=toAddr(start);
            var function=getFunctionContaining(entry);
            long expectedOwner=gate?owner:start;
            if(function==null || function.getEntryPoint().getOffset()!=expectedOwner)
                throw new Exception("Unexpected containing function: "+entry);
            if(!gate && (function.getBody().getNumAddresses()!=sizes[i] ||
                function.getBody().getMinAddress().getOffset()!=start ||
                function.getBody().getMaxAddress().getOffset()!=end-1))
                throw new Exception("Unexpected function interval: "+entry);
            int patch=prefixes[i].length()/2,instructions=0,external=0,overwritten=0,returns=0;
            StringBuilder actual=new StringBuilder(),jumps=new StringBuilder();
            for(int n=0;n<patch;n++) actual.append(String.format("%02x",getByte(entry.add(n))&255));
            if(!actual.toString().equals(prefixes[i])) throw new Exception("Patch prefix mismatch: "+entry);
            long cursor=start;
            boolean patchAligned=false;
            while(cursor<end) {
                monitor.checkCancelled();
                var instruction=getInstructionAt(toAddr(cursor));
                if(instruction==null || !function.getBody().contains(instruction.getMaxAddress()))
                    throw new Exception("Missing or foreign instruction: "+toAddr(cursor));
                if(instruction.getFlowType().isJump()) {
                    for(var destination:instruction.getFlows()) {
                        long address=destination.getOffset();
                        if(address<start || address>=end) {
                            if(!gate || (address!=end && address!=reject))
                                throw new Exception("Unexpected outgoing jump: "+destination);
                            if(jumps.length()>0) jumps.append(',');
                            jumps.append(String.format("%08x",address));
                        }
                    }
                }
                if(instruction.getFlowType().isTerminal()) {
                    if(gate || !instruction.getMnemonicString().equals("RET") ||
                       instruction.getNumOperands()!=1 || instruction.getScalar(0).getUnsignedValue()!=cleanup[i])
                        throw new Exception("Unexpected terminal/cleanup: "+instruction);
                    ++returns;
                }
                for(int n=0;n<instruction.getLength();n++) {
                    long address=cursor+n;
                    for(var reference:getReferencesTo(toAddr(address))) {
                        long from=reference.getFromAddress().getOffset();
                        if(address>start && address<start+patch) ++overwritten;
                        if(address>start && (from<start || from>=end)) ++external;
                    }
                }
                cursor+=instruction.getLength(); ++instructions;
                if(cursor==start+patch) patchAligned=true;
            }
            if(cursor!=end || !patchAligned || external!=0 || overwritten!=0 || (!gate && returns!=1))
                throw new Exception("Unsafe boundary/reference/return: "+entry);
            if(gate) {
                if(getInstructionAt(toAddr(end))==null || getInstructionAt(toAddr(reject))==null ||
                   !function.getBody().contains(toAddr(end)) || !function.getBody().contains(toAddr(reject)))
                    throw new Exception("Missing gate continuations");
                var last=getInstructionBefore(toAddr(end));
                if(last==null || !last.getFallThrough().equals(toAddr(end)) || jumps.length()==0)
                    throw new Exception("Unexpected gate exit");
            }
            out.append(String.format("%08x\t%s\t%08x\t%08x\t%d\t%d\t%d\t%s\t%d\t%d\t%d\t%d\t%s\teligible\t%s\n",
                start,symbols[i],end,expectedOwner,sizes[i],instructions,patch,prefixes[i],
                getReferencesTo(entry).length,external,overwritten,cleanup[i],jumps.toString(),hash));
        }
        Files.writeString(Paths.get(getScriptArgs()[0]),out,StandardCharsets.UTF_8);
        println("AIM_GEOMETRY_AUDIT eligible=5 sha256="+hash);
    }
}
