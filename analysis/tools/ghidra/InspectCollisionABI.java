// Local diagnostic experiment. Run with -readOnly: inferred prototypes are never saved.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.data.*;
import ghidra.program.model.symbol.SourceType;
import ghidra.app.decompiler.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;

public class InspectCollisionABI extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        Path output = Paths.get(args[0]);
        Files.createDirectories(output);
        var function = getFunctionAt(toAddr(Long.parseUnsignedLong(args[1], 16)));
        ArrayList<Function> targets = new ArrayList<>();
        targets.add(function);
        targets.addAll(function.getCalledFunctions(monitor));
        StringBuilder evidence = new StringBuilder("entry\tret_purge_bytes\tinferred_convention\n");
        for (var target : targets) {
            int purge = -1;
            boolean inconsistent = false;
            for (var instruction : currentProgram.getListing().getInstructions(target.getBody(), true)) {
                if (!instruction.getMnemonicString().equals("RET")) continue;
                int value = instruction.getNumOperands() == 0 ? 0 : (int)instruction.getScalar(0).getUnsignedValue();
                if (purge >= 0 && purge != value) inconsistent = true;
                purge = value;
            }
            if (purge < 0 || inconsistent || purge % 4 != 0) continue;
            // ECX is supplied before these direct calls in the inspected collision handler.
            Parameter[] params = new Parameter[purge / 4];
            for (int i = 0; i < params.length; i++) {
                params[i] = new ParameterImpl("arg" + i, Undefined4DataType.dataType, currentProgram);
            }
            target.updateFunction("__thiscall", new ReturnParameterImpl(Undefined4DataType.dataType, currentProgram),
                Function.FunctionUpdateType.DYNAMIC_STORAGE_ALL_PARAMS, true, SourceType.USER_DEFINED, params);
            evidence.append(target.getEntryPoint()).append('\t').append(purge).append("\t__thiscall_experiment\n");
        }
        Files.writeString(output.resolve("abi-experiment.tsv"), evidence, StandardCharsets.UTF_8);
        DecompInterface decompiler = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.setRespectReadOnly(false);
        options.setInferConstantPointers(false);
        decompiler.setOptions(options);
        decompiler.openProgram(currentProgram);
        decompiler.enableDebug(output.resolve("abi-experiment.debug.xml").toFile());
        try {
            var result = decompiler.decompileFunction(function, 180, monitor);
            boolean ok = result.decompileCompleted() && result.getDecompiledFunction() != null;
            Files.writeString(output.resolve("abi-experiment.status.txt"), "success=" + ok + "\n" + result.getErrorMessage(), StandardCharsets.UTF_8);
            if (ok) Files.writeString(output.resolve(function.getEntryPoint() + ".c"),
                "/* Diagnostic decompilation with inferred thiscall prototypes. Not validated or buildable source. */\n"
                + result.getDecompiledFunction().getC(), StandardCharsets.UTF_8);
            println("COLLISION_ABI_EXPERIMENT success=" + ok + " " + result.getErrorMessage());
        } finally { decompiler.dispose(); }
    }
}
