// Retry failures with explicit profiles; preserve every outcome without changing the database.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
import java.io.PrintWriter;

public class RecoverFailedFunctions extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        Path output = Paths.get(args[0]);
        Files.createDirectories(output);
        try (PrintWriter status = new PrintWriter(Files.newBufferedWriter(output.resolve("recovery.tsv"), StandardCharsets.UTF_8))) {
            status.println("address\tprofile\tstatus\terror");
            for (int i = 1; i < args.length; i++) {
                var address = toAddr(Long.parseUnsignedLong(args[i], 16));
                var function = getFunctionAt(address);
                if (function == null) {
                    status.println(address + "\tnone\tmissing\tNo function at requested entry");
                    continue;
                }
                for (String profile : new String[]{"decompile", "conservative", "normalize"}) {
                    monitor.checkCancelled();
                    DecompInterface decompiler = new DecompInterface();
                    DecompileOptions options = new DecompileOptions();
                    options.setRespectReadOnly(false);
                    options.setMaxPayloadMBytes(128);
                    if (!profile.equals("decompile")) {
                        options.setInferConstantPointers(false);
                        options.setEliminateUnreachable(false);
                        options.setSimplifyDoublePrecision(false);
                        options.setSplitStructures(false);
                        options.setSplitArrays(false);
                        options.setSplitPointers(false);
                    }
                    decompiler.setOptions(options);
                    decompiler.setSimplificationStyle(profile.equals("normalize") ? "normalize" : "decompile");
                    decompiler.openProgram(currentProgram);
                    decompiler.enableDebug(output.resolve(address + "_" + profile + ".debug.xml").toFile());
                    try {
                        var result = decompiler.decompileFunction(function, 180, monitor);
                        boolean ok = result.decompileCompleted() && result.getDecompiledFunction() != null;
                        String error = result.getErrorMessage().replace('\n', ' ').replace('\t', ' ');
                        status.println(address + "\t" + profile + "\t" + (ok ? "ok" : "failed") + "\t" + error);
                        status.flush();
                        if (ok) {
                            Files.writeString(output.resolve(address + "_" + profile + ".c"),
                                "/* Binary SHA256: " + currentProgram.getExecutableSHA256() + "\nEntry: " + address
                                + "\nProfile: " + profile + "; inferred C-like output, not verified reconstructed source. */\n"
                                + result.getDecompiledFunction().getC(), StandardCharsets.UTF_8);
                        }
                        println("RECOVERY " + address + " profile=" + profile + " success=" + ok + " " + error);
                        if (ok) break;
                    } finally { decompiler.dispose(); }
                }
            }
        }
    }
}
