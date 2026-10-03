// Measure database coverage without treating executable padding/data as missed functions.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
import java.io.PrintWriter;

public class AuditNativeCoverage extends GhidraScript {
    public void run() throws Exception {
        Path output = Paths.get(getScriptArgs()[0]);
        Files.createDirectories(output);
        AddressSet executable = new AddressSet();
        for (var block : currentProgram.getMemory().getBlocks()) {
            if (block.isExecute() && block.isInitialized()) executable.add(block.getStart(), block.getEnd());
        }
        AddressSet functions = new AddressSet();
        int count = 0, thunks = 0, external = 0;
        for (var f : currentProgram.getFunctionManager().getFunctions(true)) {
            count++;
            if (f.isExternal()) external++;
            if (f.isThunk()) thunks++;
            functions.add(f.getBody());
        }
        AddressSet instructions = new AddressSet();
        AddressSet paddingInstructions = new AddressSet();
        for (var instruction : currentProgram.getListing().getInstructions(executable, true)) {
            instructions.add(instruction.getMinAddress(), instruction.getMaxAddress());
            if (instruction.getMnemonicString().equals("NOP") || instruction.getMnemonicString().equals("INT3")) {
                paddingInstructions.add(instruction.getMinAddress(), instruction.getMaxAddress());
            }
        }
        var outsideFunctions = instructions.subtract(functions);
        var unclassified = executable.subtract(instructions);
        writeRanges(output.resolve("instructions-outside-functions.tsv"), outsideFunctions);
        writeRanges(output.resolve("nonpadding-instructions-outside-functions.tsv"), outsideFunctions.subtract(paddingInstructions));
        writeRanges(output.resolve("executable-without-instructions.tsv"), unclassified);
        String json = "{\n  \"binary_sha256\": \"" + currentProgram.getExecutableSHA256()
            + "\",\n  \"database_functions\": " + count + ",\n  \"thunks\": " + thunks
            + ",\n  \"external_functions\": " + external
            + ",\n  \"initialized_executable_bytes\": " + executable.getNumAddresses()
            + ",\n  \"instruction_bytes\": " + instructions.getNumAddresses()
            + ",\n  \"instruction_bytes_outside_functions\": " + outsideFunctions.getNumAddresses()
            + ",\n  \"nonpadding_instruction_bytes_outside_functions\": " + outsideFunctions.subtract(paddingInstructions).getNumAddresses()
            + ",\n  \"executable_bytes_without_instructions\": " + unclassified.getNumAddresses()
            + ",\n  \"scope\": \"Database coverage only. Unclassified executable bytes may include padding, tables or missed code; this is not source reconstruction coverage.\"\n}\n";
        Files.writeString(output.resolve("coverage.json"), json, StandardCharsets.UTF_8);
        println("COVERAGE " + json.replace('\n', ' '));
    }
    private void writeRanges(Path path, AddressSetView ranges) throws Exception {
        try (PrintWriter writer = new PrintWriter(Files.newBufferedWriter(path, StandardCharsets.UTF_8))) {
            writer.println("start\tend\tbytes");
            for (var range : ranges.getAddressRanges()) {
                monitor.checkCancelled();
                writer.println(range.getMinAddress() + "\t" + range.getMaxAddress() + "\t" + range.getLength());
            }
        }
    }
}
