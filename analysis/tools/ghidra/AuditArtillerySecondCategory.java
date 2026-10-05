// Read-only structural audit for the second-pass category eligibility gate.
import ghidra.app.script.GhidraScript;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;

public class AuditArtillerySecondCategory extends GhidraScript {
    public void run() throws Exception {
        String hash = currentProgram.getExecutableSHA256();
        boolean client = hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if (!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d"))
            throw new Exception("Unsupported binary hash");
        long start = client ? 0x9a0c7e : 0x74b45e;
        long accept = client ? 0x9a0ca6 : 0x74b486;
        long reject = client ? 0x9a0f5e : 0x74b73e;
        long owner = client ? 0x99f2a0 : 0x749a80;
        byte[] prefix = java.util.HexFormat.of().parseHex("8b4310c1e803a801");
        var function = getFunctionContaining(toAddr(start));
        if (function == null || function.getEntryPoint().getOffset() != owner)
            throw new Exception("Unexpected owner");
        for (int i = 0; i < prefix.length; ++i)
            if (getByte(toAddr(start + i)) != prefix[i]) throw new Exception("Prefix mismatch");
        long cursor = start;
        int instructions = 0, external = 0, overwritten = 0;
        while (cursor < accept) {
            var instruction = getInstructionAt(toAddr(cursor));
            if (instruction == null || !function.getBody().contains(instruction.getMaxAddress()))
                throw new Exception("Invalid instruction boundary");
            if (instruction.getFlowType().isJump()) for (var flow : instruction.getFlows()) {
                long destination = flow.getOffset();
                if (destination != reject) throw new Exception("Unexpected jump: " + flow);
            }
            for (int i = 0; i < instruction.getLength(); ++i) for (var reference : getReferencesTo(toAddr(cursor + i))) {
                long from = reference.getFromAddress().getOffset();
                if (cursor + i > start && cursor + i < start + prefix.length) ++overwritten;
                if (cursor + i > start && (from < start || from >= accept)) ++external;
            }
            cursor += instruction.getLength();
            ++instructions;
        }
        if (cursor != accept) throw new Exception("Continuation is not instruction-aligned");
        String output = "address\tsymbol\tend_exclusive\towner\tbytes\tinstructions\tpatch_bytes\tpatch_hex\texternal_interior_references\toverwritten_interior_references\tstatus\toriginal_sha256\n" +
            String.format("%08x\tbfv_artillery_second_category_gate_bridge\t%08x\t%08x\t%d\t%d\t8\t8b4310c1e803a801\t%d\t%d\teligible\t%s\n",
                start, accept, owner, accept - start, instructions, external, overwritten, hash);
        Files.writeString(Paths.get(getScriptArgs()[0]), output, StandardCharsets.UTF_8);
        println("ARTILLERY_SECOND_CATEGORY_AUDIT eligible=true instructions=" + instructions);
    }
}
