// Read-only structural and static-reference audit for complete MOV EAX,[ECX+disp32] / RET functions.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class AuditWordGetters extends GhidraScript {
    public void run() throws Exception {
        String hash = currentProgram.getExecutableSHA256();
        if (!hash.equals("79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5") &&
            !hash.equals("86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d"))
            throw new Exception("Unsupported binary hash");
        String[] args = getScriptArgs();
        StringBuilder output = new StringBuilder("address\tfield_offset\tstack_cleanup\tindexed_bytes\tstatus\treason\tentry_references\tinterior_references\n");
        int accepted = 0;
        for (String line : Files.readAllLines(Paths.get(args[0]), StandardCharsets.UTF_8)) {
            monitor.checkCancelled();
            if (line.startsWith("address\t") || line.isBlank()) continue;
            String[] columns = line.split("\t");
            long numeric = Long.parseUnsignedLong(columns[0], 16);
            var entry = toAddr(numeric);
            int size = Integer.parseInt(columns[3]);
            int cleanup = Integer.parseInt(columns[2]);
            Function f = getFunctionAt(entry);
            String reason = "";
            if ((numeric & 15) != 0) reason = "unaligned_entry";
            else if (cleanup % 4 != 0 || cleanup > 64) reason = "unsupported_cleanup";
            else if (f == null || f.getBody().getNumAddresses() != size ||
                     !f.getBody().getMinAddress().equals(entry) ||
                     !f.getBody().getMaxAddress().equals(entry.add(size - 1))) reason = "noncontiguous_body";
            Instruction first = getInstructionAt(entry);
            Instruction last = getInstructionAt(entry.add(6));
            byte[] bytes = new byte[size]; currentProgram.getMemory().getBytes(entry, bytes);
            long value = 0;
            for (int n = 0; n < 4; ++n) value |= (long)(bytes[n + 2] & 255) << (8 * n);
            if (reason.isEmpty() && (first == null || first.getLength() != 6 ||
                (bytes[0] & 255) != 0x8b || (bytes[1] & 255) != 0x81 || value != Integer.parseInt(columns[1]) ||
                last == null || !last.getMnemonicString().equals("RET") || last.getLength() != size - 6))
                reason = "instruction_mismatch";
            int incoming = 0, interior = 0;
            var references = currentProgram.getReferenceManager().getReferencesTo(entry);
            while (references.hasNext()) { references.next(); ++incoming; }
            for (int n = 1; n < 6; ++n) {
                var refs = currentProgram.getReferenceManager().getReferencesTo(entry.add(n));
                while (refs.hasNext()) { refs.next(); ++interior; }
            }
            if (reason.isEmpty() && interior != 0) reason = "interior_reference";
            boolean eligible = reason.isEmpty(); if (eligible) ++accepted;
            output.append(line).append("\t").append(eligible ? "eligible" : "rejected")
                .append("\t").append(eligible ? "verified_structure" : reason)
                .append("\t").append(incoming).append("\t").append(interior).append("\n");
        }
        Files.writeString(Paths.get(args[1]), output, StandardCharsets.UTF_8);
        println("WORD_GETTER_AUDIT eligible=" + accepted + " sha256=" + hash);
    }
}
