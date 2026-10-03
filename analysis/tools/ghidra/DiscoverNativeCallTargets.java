// Recover function entries supported by direct calls, never by blindly sweeping bytes.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
import java.io.PrintWriter;
import java.util.TreeMap;

public class DiscoverNativeCallTargets extends GhidraScript {
    public void run() throws Exception {
        Path output = Paths.get(getScriptArgs()[0]);
        Files.createDirectories(output);
        TreeMap<Address, Address> candidates = new TreeMap<>();
        for (var instruction : currentProgram.getListing().getInstructions(true)) {
            monitor.checkCancelled();
            if (!instruction.getFlowType().isCall()) continue;
            for (var target : instruction.getFlows()) {
                var block = currentProgram.getMemory().getBlock(target);
                if (block == null || !block.isExecute() || !block.isInitialized()) continue;
                if (getInstructionAt(target) == null || getFunctionContaining(target) != null) continue;
                candidates.putIfAbsent(target, instruction.getAddress());
            }
        }
        int created = 0, failed = 0, absorbed = 0;
        try (PrintWriter writer = new PrintWriter(Files.newBufferedWriter(output.resolve("call-target-discovery.tsv"), StandardCharsets.UTF_8))) {
            writer.println("target\tcaller\tstatus\tbody_bytes");
            for (var entry : candidates.entrySet()) {
                monitor.checkCancelled();
                if (getFunctionContaining(entry.getKey()) != null) {
                    absorbed++;
                    writer.println(entry.getKey() + "\t" + entry.getValue() + "\tabsorbed_by_recovered_body\t0");
                    continue;
                }
                var function = createFunction(entry.getKey(), "FUN_" + entry.getKey());
                if (function == null) failed++; else created++;
                writer.println(entry.getKey() + "\t" + entry.getValue() + "\t"
                    + (function == null ? "unresolved" : "created") + "\t"
                    + (function == null ? 0 : function.getBody().getNumAddresses()));
                if ((created + failed + absorbed) % 100 == 0) {
                    writer.flush();
                    println("DISCOVERY_PROGRESS created=" + created + " unresolved=" + failed + " absorbed=" + absorbed);
                }
            }
        }
        Files.writeString(output.resolve("discovery-summary.json"),
            "{\n  \"binary_sha256\": \"" + currentProgram.getExecutableSHA256()
            + "\",\n  \"direct_call_candidates\": " + candidates.size()
            + ",\n  \"created_functions\": " + created + ",\n  \"unresolved\": " + failed
            + ",\n  \"absorbed_by_recovered_body\": " + absorbed
            + ",\n  \"scope\": \"Entries inferred from existing decoded direct calls. Function boundaries and behavior still require validation.\"\n}\n",
            StandardCharsets.UTF_8);
        println("DISCOVERY_COMPLETE candidates=" + candidates.size() + " created=" + created + " unresolved=" + failed);
    }
}
