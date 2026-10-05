// Read-only audit of the prefix and re-enterable category-factor boundaries.
import ghidra.app.script.GhidraScript;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;

public class AuditArtillerySecondCategoryFactor extends GhidraScript {
    public void run() throws Exception {
        String hash=currentProgram.getExecutableSHA256();
        boolean client=hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if(!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d")) throw new Exception("Unsupported hash");
        long owner=client?0x99f2a0:0x749a80;
        long[] starts=client?new long[]{0x9a0ca6,0x9a0cf0}:new long[]{0x74b486,0x74b4d0};
        long[] ends=client?new long[]{0x9a0cf0,0x9a0dba}:new long[]{0x74b4d0,0x74b59a};
        String[] names={"bfv_artillery_second_category_prefix_bridge","bfv_artillery_second_category_factor_bridge"};
        String[] prefixes={"8b0f894c24248b4b20","8d432485c074058b4330eb02"};
        StringBuilder out=new StringBuilder("address\tsymbol\tend_exclusive\towner\tbytes\tinstructions\tpatch_bytes\tpatch_hex\texternal_interior_references\toverwritten_interior_references\texternal_exits\tstatus\toriginal_sha256\n");
        for(int part=0;part<2;part++) {
            long start=starts[part],end=ends[part];var function=getFunctionContaining(toAddr(start));
            if(function==null || function.getEntryPoint().getOffset()!=owner) throw new Exception("Unexpected owner");
            byte[] prefix=java.util.HexFormat.of().parseHex(prefixes[part]);
            for(int i=0;i<prefix.length;i++) if(getByte(toAddr(start+i))!=prefix[i]) throw new Exception("Prefix mismatch");
            long cursor=start;int count=0,external=0,overwritten=0,exits=0;
            while(cursor<end) {
                var instruction=getInstructionAt(toAddr(cursor));
                if(instruction==null || !function.getBody().contains(instruction.getMaxAddress())) throw new Exception("Invalid instruction");
                if(instruction.getFlowType().isJump()) for(var flow:instruction.getFlows()) {
                    long destination=flow.getOffset();
                    if(destination<start || destination>=end) {
                        if(part==1 && destination!=end) throw new Exception("Unexpected exit: "+flow);
                        if(part==0) throw new Exception("Unexpected prefix exit: "+flow);
                        ++exits;
                    }
                }
                for(int i=0;i<instruction.getLength();i++) for(var reference:getReferencesTo(toAddr(cursor+i))) {
                    long from=reference.getFromAddress().getOffset();
                    if(cursor+i>start && cursor+i<start+prefix.length) ++overwritten;
                    if(cursor+i>start && (from<start || from>=end)) ++external;
                }
                cursor+=instruction.getLength();++count;
            }
            if(cursor!=end || external!=0 || overwritten!=0 || exits!=(part==1?1:0)) throw new Exception("Unsafe boundary");
            out.append(String.format("%08x\t%s\t%08x\t%08x\t%d\t%d\t%d\t%s\t%d\t%d\t%d\teligible\t%s\n",start,names[part],end,owner,end-start,count,prefix.length,prefixes[part],external,overwritten,exits,hash));
        }
        Files.writeString(Paths.get(getScriptArgs()[0]),out,StandardCharsets.UTF_8);
        println("ARTILLERY_SECOND_CATEGORY_FACTOR_AUDIT eligible=2");
    }
}
