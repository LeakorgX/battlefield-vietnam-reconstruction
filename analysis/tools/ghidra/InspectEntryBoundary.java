// Read-only evidence for candidate entry points; no boundary is accepted automatically.
import ghidra.app.script.GhidraScript;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class InspectEntryBoundary extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        Path output = Paths.get(args[0]);
        Files.createDirectories(output);
        for (int n = 1; n < args.length; n++) {
            var address = toAddr(Long.parseUnsignedLong(args[n], 16));
            StringBuilder text = new StringBuilder();
            text.append("Binary: ").append(currentProgram.getExecutableSHA256()).append("\n");
            text.append("Candidate: ").append(address).append("\n");
            var function = getFunctionAt(address);
            text.append("Defined function: ").append(function == null ? "none" : function.getName()).append("\n");
            var refs = currentProgram.getReferenceManager().getReferencesTo(address);
            while (refs.hasNext()) {
                var ref = refs.next();
                text.append("Incoming: ").append(ref.getFromAddress()).append(" ").append(ref.getReferenceType()).append("\n");
            }
            var instructions = currentProgram.getListing().getInstructions(address.subtract(128), true);
            while (instructions.hasNext()) {
                var instruction = instructions.next();
                if (instruction.getAddress().compareTo(address.add(128)) > 0) break;
                monitor.checkCancelled();
                text.append(instruction.getAddress()).append(" ").append(instruction).append("\n");
            }
            Files.writeString(output.resolve(address + "-boundary.txt"), text, StandardCharsets.UTF_8);
            println("BOUNDARY_EVIDENCE " + address);
        }
    }
}
