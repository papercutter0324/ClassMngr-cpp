# W7.5.3 Windows F564/F598 capture evidence

- Task: W753-F564-F598-FINAL-CAPTURE.
- Source revision: fe25e2ff7a82b8da34d970a3591b64b3065aeb9e; the recorded preflight was clean.
- Capture run: 6d028cab-6954-43a2-be78-6d6c91005deb, process ID 11472, optimized Release WPR capture with dryRun=false.
- Route workflow: lifecycle-sub-prep with PDF page-render observer opt-in.
- ETL: capture-attempt-1/classmngr-pdf-heap-snapshots.etl, 56,623,104 bytes, SHA-256 9a6301e0a68ce3947362b4107fb62d9856ed7a35c4b7d39e10fe323635b3eca2.

## Recorded result and limits

The capture validation record reports validationPassed: true; it binds the ETL, run ID, PID and source head. The bundle includes the capture summary and validation, WPR command/controller records, workflow manifest and trace, workflow JSON, controller summary, ready/ack JSONs for all three ordered markers, token preflight/postflight records, and the generated large_startup.tps fixture snapshot. These are copied from the recorded run without edits.

The plan records xperf parser diagnostics for invalid events from unrelated PID 4248 and a total-data count of 406,489 versus processed-reference count 359,697. Those diagnostics limit trace-completeness claims; they do not negate the named ClassMngr symbol resolutions recorded in the plan.

## Omitted source files

Build trees, application and test binaries, and PDB/symbol-analysis outputs (including heap-data text and symcache) are excluded. The raw ETL, token preflight/postflight records, fixture snapshot and compact command/controller/workflow evidence are included.

SHA256SUMS.txt covers every included raw evidence file and this README using forward-slash relative paths; it does not hash itself.

