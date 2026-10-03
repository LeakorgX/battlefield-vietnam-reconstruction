// One targeted retry without treating static read-only memory as constant pointers.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class RetryNativeDecompile extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        Path output = Paths.get(args[0]);
        Files.createDirectories(output);
        DecompInterface decompiler = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.setRespectReadOnly(false);
        decompiler.setOptions(options);
        decompiler.openProgram(currentProgram);
        try {
            for (int i = 1; i < args.length; i++) {
                var address = toAddr(Long.parseUnsignedLong(args[i], 16));
                var f = getFunctionAt(address);
                if (f == null) continue;
                var result = decompiler.decompileFunction(f, 30, monitor);
                boolean ok = result.decompileCompleted() && result.getDecompiledFunction() != null;
                String header = "/* Binary SHA256: " + currentProgram.getExecutableSHA256()
                    + "\nEntry: " + address + "\nRespectReadOnly=false; inferred types require validation. */\n";
                if (ok) Files.writeString(output.resolve(address + ".c"), header + result.getDecompiledFunction().getC(), StandardCharsets.UTF_8);
                Files.writeString(output.resolve(address + ".status.txt"), "success=" + ok + "\n" + result.getErrorMessage(), StandardCharsets.UTF_8);
                println("TARGETED_RETRY " + address + " success=" + ok + " " + result.getErrorMessage());
            }
        } finally { decompiler.dispose(); }
    }
}
