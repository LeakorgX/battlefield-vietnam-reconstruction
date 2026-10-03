// Run with -readOnly. Applies instruction-verified interfaces in memory only.
// Generated native bodies are local research output, not reconstructed source.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.data.*;
import ghidra.program.model.symbol.SourceType;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class InspectArtilleryEvaluator extends GhidraScript {
    private Function at(long address) throws Exception {
        Function f = getFunctionAt(toAddr(address));
        if (f == null) throw new Exception("Missing function: " + Long.toHexString(address));
        return f;
    }
    private Parameter arg(String name, DataType type) throws Exception {
        return new ParameterImpl(name, type, currentProgram);
    }
    public void run() throws Exception {
        String hash = currentProgram.getExecutableSHA256();
        boolean client = hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        boolean server = hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d");
        if (!client && !server) throw new Exception("Unsupported binary hash");
        Path out = Paths.get(getScriptArgs()[0]); Files.createDirectories(out);
        Function evaluator = at(client ? 0x99f2a0 : 0x749a80);
        Function driver = at(client ? 0x9a13a0 : 0x74bb80);
        DataType pointer = new PointerDataType(VoidDataType.dataType);
        DataType word = UnsignedIntegerDataType.dataType;
        // The MS compiler model treats float10 as a hidden aggregate return.
        // The inspected engine returns ST0 directly, so use explicit storage.
        evaluator.updateFunction("__thiscall",
            new ReturnParameterImpl(Float10DataType.dataType, currentProgram.getRegister("ST0"), currentProgram),
            Function.FunctionUpdateType.CUSTOM_STORAGE, true, SourceType.USER_DEFINED,
            new ParameterImpl("behavior", pointer, currentProgram.getRegister("ECX"), currentProgram),
            new ParameterImpl("bot", pointer, 4, currentProgram),
            new ParameterImpl("recompute", word, 8, currentProgram),
            new ParameterImpl("scale", FloatDataType.dataType, 12, currentProgram),
            new ParameterImpl("gun", pointer, 16, currentProgram),
            new ParameterImpl("driver_component", pointer, 20, currentProgram));
        Parameter[] verified = evaluator.getParameters();
        if (verified.length != 6 || !evaluator.getReturn().getVariableStorage().equals(
            new VariableStorage(currentProgram, currentProgram.getRegister("ST0"))))
            throw new Exception("Unexpected evaluator return/parameter storage");
        for (int i=1; i<verified.length; i++)
            if (!verified[i].isStackVariable() || verified[i].getStackOffset()!=i*4)
                throw new Exception("Unexpected evaluator stack argument " + i);
        driver.updateFunction("__thiscall", new ReturnParameterImpl(FloatDataType.dataType, currentProgram),
            Function.FunctionUpdateType.DYNAMIC_STORAGE_ALL_PARAMS, true, SourceType.USER_DEFINED,
            arg("bot", pointer), arg("recompute", word), arg("scale", FloatDataType.dataType));
        Function allocate = at(client ? 0x403610 : 0x403d80);
        Function release = at(client ? 0x403680 : 0x403df0);
        allocate.setNoReturn(false); release.setNoReturn(false);
        allocate.updateFunction("__cdecl", new ReturnParameterImpl(pointer, currentProgram),
            Function.FunctionUpdateType.DYNAMIC_STORAGE_ALL_PARAMS, true, SourceType.USER_DEFINED,
            arg("bytes", word));
        release.updateFunction("__cdecl", new ReturnParameterImpl(VoidDataType.dataType, currentProgram),
            Function.FunctionUpdateType.DYNAMIC_STORAGE_ALL_PARAMS, true, SourceType.USER_DEFINED,
            arg("allocation", pointer));
        StringBuilder abi = new StringBuilder("entry\tparameter\tstorage\ttype\n");
        for (Function f : new Function[]{driver, evaluator, allocate, release}) {
            for (Parameter p : f.getParameters())
                abi.append(f.getEntryPoint()).append('\t').append(p.getName()).append('\t')
                    .append(p.getVariableStorage()).append('\t').append(p.getDataType()).append('\n');
            abi.append(f.getEntryPoint()).append("\tRETURN\t").append(f.getReturn().getVariableStorage())
                .append('\t').append(f.getReturnType()).append('\n');
        }
        Files.writeString(out.resolve("artillery-abi.tsv"), abi, StandardCharsets.UTF_8);
        DecompInterface decompiler = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.setRespectReadOnly(false); options.setInferConstantPointers(false);
        decompiler.setOptions(options); decompiler.openProgram(currentProgram);
        try {
            var result = decompiler.decompileFunction(evaluator, 180, monitor);
            boolean ok = result.decompileCompleted() && result.getDecompiledFunction() != null;
            Files.writeString(out.resolve("artillery-status.txt"), "success=" + ok + "\n" + result.getErrorMessage(), StandardCharsets.UTF_8);
            if (ok) Files.writeString(out.resolve(evaluator.getEntryPoint()+".c"),
                "/* Local native decompilation with verified argument positions.\n"
                + "Not reconstructed or compilable source; unresolved object/callee types remain. */\n"
                + result.getDecompiledFunction().getC(), StandardCharsets.UTF_8);
            println("ARTILLERY_EVALUATOR_RETRY success=" + ok + " entry=" + evaluator.getEntryPoint());
        } finally { decompiler.dispose(); }
    }
}
