// Conservative candidates from aligned decoded prologues after terminal flow/padding.
// No undecoded bytes are swept; these remain inferred entries, not validated methods.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
import java.io.PrintWriter;
import java.util.TreeSet;

public class DiscoverOrphanPrologues extends GhidraScript {
    public void run() throws Exception {
        Path output = Paths.get(getScriptArgs()[0]);
        Files.createDirectories(output);
        AddressSet bodies = new AddressSet(), decoded = new AddressSet();
        for (var function : currentProgram.getFunctionManager().getFunctions(true)) bodies.add(function.getBody());
        for (var instruction : currentProgram.getListing().getInstructions(true)) {
            var block = currentProgram.getMemory().getBlock(instruction.getAddress());
            if (block != null && block.isExecute()) decoded.add(instruction.getMinAddress(), instruction.getMaxAddress());
        }
        TreeSet<Address> candidates = new TreeSet<>();
        for (var range : decoded.subtract(bodies).getAddressRanges()) {
            var address = range.getMinAddress();
            if ((address.getOffset() & 15) != 0 || range.getLength() < 5) continue;
            var instruction = getInstructionAt(address);
            if (instruction == null) continue;
            String text = instruction.toString();
            boolean prologue = text.startsWith("PUSH EBP") || text.startsWith("PUSH EBX")
                || text.startsWith("PUSH ESI") || text.startsWith("PUSH EDI") || text.startsWith("PUSH ECX")
                || text.startsWith("SUB ESP,");
            if (!prologue) continue;
            var previous = currentProgram.getListing().getInstructionBefore(address);
            if (previous == null || previous.getFallThrough() == null
                || previous.getMnemonicString().equals("NOP") || previous.getMnemonicString().equals("INT3")) {
                candidates.add(address);
            }
        }
        int created = 0, unresolved = 0, absorbed = 0;
        try (PrintWriter writer = new PrintWriter(Files.newBufferedWriter(output.resolve("orphan-prologues.tsv"), StandardCharsets.UTF_8))) {
            writer.println("entry\tstatus\tbody_bytes");
            for (var address : candidates) {
                monitor.checkCancelled();
                if (getFunctionContaining(address) != null) { absorbed++; continue; }
                var function = createFunction(address, "ORPHAN_CANDIDATE_" + address);
                if (function == null) unresolved++; else created++;
                writer.println(address + "\t" + (function == null ? "unresolved" : "candidate_created")
                    + "\t" + (function == null ? 0 : function.getBody().getNumAddresses()));
            }
        }
        Files.writeString(output.resolve("orphan-summary.json"), "{\n  \"candidates\": " + candidates.size()
            + ",\n  \"created_candidates\": " + created + ",\n  \"unresolved\": " + unresolved
            + ",\n  \"absorbed\": " + absorbed + ",\n  \"scope\": \"Aligned decoded prologue candidates following terminal flow or padding; boundaries remain inferred.\"\n}\n", StandardCharsets.UTF_8);
        println("ORPHAN_DISCOVERY candidates=" + candidates.size() + " created=" + created);
    }
}
