// Preserve instruction evidence even where decompilation fails or guesses an ABI.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class InspectNativeFunctions extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        Path out = Paths.get(args[0]);
        Files.createDirectories(out);
        for (int n = 1; n < args.length; n++) {
            var address = toAddr(Long.parseUnsignedLong(args[n], 16));
            Function f = getFunctionAt(address);
            if (f == null) { println("No function at " + address); continue; }
            StringBuilder text = new StringBuilder();
            text.append("SHA256: ").append(currentProgram.getExecutableSHA256()).append("\n");
            text.append("Entry: ").append(address).append("\n");
            text.append("Inferred signature (requires validation): ").append(f.getSignature()).append("\n");
            text.append("Body: ").append(f.getBody()).append("\n");
            for (var called : f.getCalledFunctions(monitor)) {
                text.append("Direct callee: ").append(called.getEntryPoint()).append(" ")
                    .append(called.getName()).append(" noReturn=").append(called.hasNoReturn()).append("\n");
            }
            var instructions = currentProgram.getListing().getInstructions(f.getBody(), true);
            while (instructions.hasNext()) {
                monitor.checkCancelled();
                var instruction = instructions.next();
                text.append(instruction.getAddress()).append("  ").append(instruction).append("\n");
            }
            Files.writeString(out.resolve(address + ".asm"), text, StandardCharsets.UTF_8);
            println("INSTRUCTION_EVIDENCE " + address + " bytes=" + f.getBody().getNumAddresses());
        }
    }
}
