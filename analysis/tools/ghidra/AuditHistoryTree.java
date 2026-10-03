// Read-only function-boundary and incoming-reference audit for history-tree detours.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class AuditHistoryTree extends GhidraScript {
    public void run() throws Exception {
        String hash = currentProgram.getExecutableSHA256();
        boolean client = hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5");
        if (!client && !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d"))
            throw new Exception("Unsupported binary hash");
        long[] entries = client ? new long[]{0x99ef80,0x99ed10,0x68ee10,0x996f10,0x66b6c0,0x517c10}
                                : new long[]{0x749760,0x7494f0,0x73f240,0x5c7460,0x55c080,0x7686e0};
        String[] symbols = {"bfv_history_insert","bfv_history_insert_node","bfv_tree_previous",
                            "bfv_tree_rotate_left","bfv_tree_rotate_right","bfv_tree_construct"};
        String[] prefixes = {"51558b6c2410","83ec445657","8b018a5015","8b5424048b4208","8b5424048b02","8b5424088bc1"};
        int[] sizes = {185,428,93,85,89,51};
        StringBuilder out = new StringBuilder("address\tsymbol\tindexed_bytes\tpatch_bytes\tentry_references\tinterior_references\tstatus\toriginal_sha256\n");
        for (int i=0; i<entries.length; ++i) {
            monitor.checkCancelled();
            var entry = toAddr(entries[i]);
            Function f = getFunctionAt(entry);
            if (f == null || f.getBody().getNumAddresses()!=sizes[i] ||
                !f.getBody().getMinAddress().equals(entry) ||
                !f.getBody().getMaxAddress().equals(entry.add(sizes[i]-1)))
                throw new Exception("Unexpected function body: " + entry);
            int patch = prefixes[i].length()/2;
            StringBuilder actual = new StringBuilder();
            for (int n=0; n<patch; ++n) actual.append(String.format("%02x",getByte(entry.add(n)) & 255));
            if (!actual.toString().equals(prefixes[i])) throw new Exception("Prefix mismatch: " + entry);
            int consumed=0;
            while (consumed<patch) {
                Instruction inst=getInstructionAt(entry.add(consumed));
                if (inst==null) throw new Exception("Missing instruction: " + entry);
                consumed+=inst.getLength();
            }
            if (consumed!=patch) throw new Exception("Partial instruction: " + entry);
            int incoming=0, interior=0;
            var refs=getReferencesTo(entry); incoming=refs.length;
            for(int n=1; n<patch; ++n) interior+=getReferencesTo(entry.add(n)).length;
            if(interior!=0) throw new Exception("Interior reference: " + entry);
            out.append(String.format("%08x\t%s\t%d\t%d\t%d\t%d\teligible\t%s\n",
                entries[i],symbols[i],sizes[i],patch,incoming,interior,hash));
        }
        Files.writeString(Paths.get(getScriptArgs()[0]),out,StandardCharsets.UTF_8);
        println("HISTORY_TREE_AUDIT eligible=" + entries.length + " sha256=" + hash);
    }
}
