# Original-code reference provenance

The `ios` fixture names identify the original ARM binary being executed, not the implementation
of every imported library service. In particular, `ship-ios.txt` and
`play-ios.txt` were generated on Linux with host glibc/libm math calls. They do
not record original Darwin-libm results.

Windows/UCRT comparisons are stored separately in
[../diagnostics/ucrt](../diagnostics/ucrt). Neither set
uses the C++ port to calculate expected state. Both retain the analysis
harness's intercepted services and initialization assumptions.

Bitwise comparison across math backends can report different last bits even
when the source operation boundaries agree. Use a matching backend to test the
translation, and keep cross-backend results as a distinct portability
diagnostic. Neither comparison establishes Darwin-libm identity or fidelity
outside the recorded cases. Do not overwrite a reference or relax assertions
to hide a discrepancy.

The `tools/minos/*_exec.py` names in capture headers identify unpublished
original-execution harnesses. They are provenance identifiers, not paths to
regeneration tools shipped in this source package. The committed records are
sufficient to run the checks; reproducing a capture also needs the original
binary, the matching imported-math backend and the capture harness.
