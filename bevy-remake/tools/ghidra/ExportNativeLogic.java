// Export native AI/gameplay functions and their direct call relationships.
// Output is a local reverse-engineering reference, not recompilable engine source.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
import java.io.*;
import java.util.*;

public class ExportNativeLogic extends GhidraScript {
    @Override public void run() throws Exception {
        String[] args = getScriptArgs();
        Path output = Paths.get(args[0]);
        Files.createDirectories(output);
        List<String> seeds = Files.readAllLines(Paths.get(args[1]), StandardCharsets.UTF_8);
        LinkedHashMap<Function, String> selected = new LinkedHashMap<>();
        try (PrintWriter index = new PrintWriter(Files.newBufferedWriter(output.resolve("all-functions.tsv"), StandardCharsets.UTF_8))) {
            index.println("address\tname\tbytes\tcalling_convention");
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                index.println(f.getEntryPoint()+"\t"+f.getName(true)+"\t"+f.getBody().getNumAddresses()+"\t"+f.getCallingConventionName());
            }
        }
        int missing = 0;
        try (PrintWriter manifest = new PrintWriter(Files.newBufferedWriter(output.resolve("seed-resolution.tsv"), StandardCharsets.UTF_8))) {
            manifest.println("requested_address\tcategory\tlabel\tfunction_entry\tstatus");
            for (String line : seeds) {
                if (line.isBlank()) continue;
                String[] fields = line.split("\t");
                var address = toAddr(Long.parseUnsignedLong(fields[0], 16));
                Function f = getFunctionAt(address);
                if (f == null) {
                    // Force only RTTI-referenced entries, avoiding a sweep of data/jump tables.
                    disassemble(address);
                    f = createFunction(address, fields[2]);
                }
                if (f == null) {
                    missing++; manifest.println(fields[0]+"\t"+fields[1]+"\t"+fields[2]+"\t\tunresolved"); continue;
                }
                if (f.getName().startsWith("FUN_")) {
                    try { f.setName(fields[2], SourceType.USER_DEFINED); } catch (Exception e) { }
                }
                selected.putIfAbsent(f, fields[1]);
                manifest.println(fields[0]+"\t"+fields[1]+"\t"+fields[2]+"\t"+f.getEntryPoint()+"\tresolved");
            }
        }
        // Include one level of direct callees, preserving the recovered function graph.
        LinkedHashMap<Function, String> expanded = new LinkedHashMap<>(selected);
        for (Function f : selected.keySet()) {
            for (Function callee : f.getCalledFunctions(monitor)) {
                if (!callee.isExternal() && !callee.isThunk()) expanded.putIfAbsent(callee, "direct-callee");
            }
        }
        if (args.length > 2 && args[2].equals("all")) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                if (!f.isExternal() && !f.isThunk()) expanded.putIfAbsent(f, "native");
            }
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.toggleCCode(true);
        decompiler.toggleSyntaxTree(true);
        decompiler.openProgram(currentProgram);
        int success = 0, failed = 0;
        try (PrintWriter manifest = new PrintWriter(Files.newBufferedWriter(output.resolve("decompilation.tsv"), StandardCharsets.UTF_8));
             PrintWriter graph = new PrintWriter(Files.newBufferedWriter(output.resolve("callgraph.tsv"), StandardCharsets.UTF_8))) {
            manifest.println("address\tcategory\tname\tstatus\terror");
            graph.println("caller\tcallee\tcallee_name");
            for (var entry : expanded.entrySet()) {
                monitor.checkCancelled();
                Function f = entry.getKey();
                for (Function callee : f.getCalledFunctions(monitor)) graph.println(f.getEntryPoint()+"\t"+callee.getEntryPoint()+"\t"+callee.getName(true));
                Path folder = output.resolve(entry.getValue());
                Files.createDirectories(folder);
                String filename = f.getEntryPoint()+"_"+f.getName().replaceAll("[^A-Za-z0-9_]", "_");
                Path source = folder.resolve(filename+".c");
                boolean cached = Files.exists(source);
                DecompileResults result = cached ? null : decompiler.decompileFunction(f, 20, monitor);
                boolean ok = cached || result != null && result.decompileCompleted() && result.getDecompiledFunction() != null;
                if (ok && !cached) {
                    String header = "/* Local binary analysis: " + currentProgram.getExecutableSHA256() + "\nEntry: " + f.getEntryPoint()+"\nClass/slot labels are evidence-based leads, not restored original method names. */\n\n";
                    Files.writeString(source, header+result.getDecompiledFunction().getC(), StandardCharsets.UTF_8);
                }
                if (ok) success++; else failed++;
                String error = ok ? "" : result == null ? "null result" : result.getErrorMessage().replace('\n',' ').replace('\t',' ');
                manifest.println(f.getEntryPoint()+"\t"+entry.getValue()+"\t"+f.getName(true)+"\t"+(ok ? "ok" : "failed")+"\t"+error);
                manifest.flush(); graph.flush();
                if ((success+failed)%50==0) println("NATIVE_LOGIC_PROGRESS completed="+(success+failed)+" total="+expanded.size()+" success="+success+" failed="+failed);
            }
        } finally { decompiler.dispose(); }
        Files.writeString(output.resolve("export-summary.txt"), "selected="+selected.size()+"\nexpanded="+expanded.size()+"\nsuccess="+success+"\nfailed="+failed+"\nunresolved_seeds="+missing+"\n", StandardCharsets.UTF_8);
        println("NATIVE_LOGIC_COMPLETE success="+success+" failed="+failed+" unresolved="+missing);
    }
}
