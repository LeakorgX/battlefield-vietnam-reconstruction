// Candidate entry points from aligned pointers in non-executable initialized blocks.
// Outputs are research candidates; pointer values alone do not prove function identity.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
import java.io.PrintWriter;
import java.util.TreeMap;

public class DiscoverNativePointerTargets extends GhidraScript {
    public void run() throws Exception {
        Path output = Paths.get(getScriptArgs()[0]);
        Files.createDirectories(output);
        TreeMap<Address, Address> candidates = new TreeMap<>();
        for (var block : currentProgram.getMemory().getBlocks()) {
            if (block.isExecute() || !block.isInitialized() || block.getSize() > Integer.MAX_VALUE) continue;
            byte[] bytes = new byte[(int)block.getSize()];
            currentProgram.getMemory().getBytes(block.getStart(), bytes);
            for (int offset = 0; offset + 4 <= bytes.length; offset += 4) {
                if ((offset & 0xffff) == 0) monitor.checkCancelled();
                long pointer = (bytes[offset] & 255L) | ((bytes[offset + 1] & 255L) << 8)
                    | ((bytes[offset + 2] & 255L) << 16) | ((bytes[offset + 3] & 255L) << 24);
                var target = toAddr(pointer);
                var targetBlock = currentProgram.getMemory().getBlock(target);
                if (targetBlock == null || !targetBlock.isExecute() || !targetBlock.isInitialized()) continue;
                if (getInstructionAt(target) == null || getFunctionContaining(target) != null) continue;
                candidates.putIfAbsent(target, block.getStart().add(offset));
            }
        }
        int created = 0, unresolved = 0, absorbed = 0;
        try (PrintWriter writer = new PrintWriter(Files.newBufferedWriter(output.resolve("pointer-target-discovery.tsv"), StandardCharsets.UTF_8))) {
            writer.println("target\tpointer_location\tstatus\tbody_bytes");
            for (var entry : candidates.entrySet()) {
                monitor.checkCancelled();
                if (getFunctionContaining(entry.getKey()) != null) {
                    absorbed++;
                    writer.println(entry.getKey() + "\t" + entry.getValue() + "\tabsorbed\t0");
                    continue;
                }
                var function = createFunction(entry.getKey(), "PTR_CANDIDATE_" + entry.getKey());
                if (function == null) unresolved++; else created++;
                writer.println(entry.getKey() + "\t" + entry.getValue() + "\t"
                    + (function == null ? "unresolved" : "candidate_created") + "\t"
                    + (function == null ? 0 : function.getBody().getNumAddresses()));
            }
        }
        Files.writeString(output.resolve("pointer-discovery-summary.json"),
            "{\n  \"binary_sha256\": \"" + currentProgram.getExecutableSHA256()
            + "\",\n  \"pointer_candidates\": " + candidates.size() + ",\n  \"created_candidates\": " + created
            + ",\n  \"unresolved\": " + unresolved + ",\n  \"absorbed\": " + absorbed
            + ",\n  \"scope\": \"Pointer-supported candidates in decoded code, not validated original functions.\"\n}\n",
            StandardCharsets.UTF_8);
        println("POINTER_DISCOVERY candidates=" + candidates.size() + " created=" + created + " unresolved=" + unresolved);
    }
}
