# Current Slice Batches

Phase 3 is In Progress on the mandatory Heavy route. Its exit gate remains
open while the accepted §3.3 structured-error queue is incomplete; the
completion audit and accepted contract are in the [Phase 3 progress
log](04-Phase-3-Progress-Log.md).

## Batch 22 — Active

F1444 implementation and independent focused verification are complete; its
commit `fef61c849b4f845ba89a008dcee697bc380895ce` is local and not pushed.
F1445 implementation and focused verification are complete; commit
`0f9b8bf6f9165d51f53e3f46af616368a46bdba0` is local and not pushed. F1446
is committed locally as `fe3edd1e4ff890eea1f1b57c9dc3b5afcd2cfae6` and not
pushed; focused verification passed. F1447 implementation and independent
focused verification are complete; commit
`ba685fbbc69d6b8564d2896671cd82ee3a0810ed` is local and not pushed. F1448
ClassRepository implementation and independent focused verification are
complete; commit `28cd7f39def7cc701e02cd8b488a7f21505fc745` is local and not
pushed. F1449 TestingClassRepository implementation and independent
verification are complete; commit
`71424f7334399ec3735c23172e4627b3cefb0641` is local and not pushed. F1450
ClassInfoRepository implementation and focused verification are complete;
it is committed locally as `bd03110d7de6a034db9f631099164d9562c2435b` and not
pushed. Evidence and limits are in the [progress
log](04-Phase-3-Progress-Log.md). F1451 TeacherRepository is independently
verified; it is committed locally as
`f8261f96cb57f2c7baf41941be2636f1014fd9a4` and is not pushed. F1452
GsTeamRepository is committed locally as
`4ccb5c8ceda848e4f09afcd41a958437939aca14` and is not pushed. F1453
NativeEnglishTeacherRepository implementation and independent verification
are complete; the slice is accepted. F1453 is committed locally as
`78189be6fe288a1ac6e8fef8760c151edf687f20` and is not pushed. F1454
IntensiveSlotStateRepository implementation and independent verification are
complete; the slice is accepted. F1454 is committed locally as
`b362325f8a3d2f162a14e30c59ee171a52024e77` and is not pushed. F1455
RosterRepository is independently verified and accepted; it is committed
locally as `5cf27a6a7589eb19c0d859684b5e44f53b197c4e` and is not pushed.
F1456 CalendarEventRepository is independently verified and accepted; it is
committed locally as `86667de56547273c8897198470e9c271a583035c` and is not
pushed. F1457 SpeakingEvalRepository is independently verified and accepted;
it is committed locally as `2ccba709122ae44a246302bdf8dab8d6a179e3cb` and is
not pushed. F1458 ScheduleImportRepository is independently verified and
accepted; it is committed locally as
`b968a9d2f93016ac03c1df9af8704ed5854ef2c2` and is not pushed. F1459
ClassTransferRepository is committed locally as
`12feaf380f2bc5368f3a709821a73a81b34d50fd` and is not pushed. F1460
SettingsRepository is independently verified and accepted; it is committed
locally as `46ffe1a8711ee5ec8cb997b5921868aab553ebce` and is not pushed.
F1461 PersonalDetailsRepository is independently verified and accepted; it is
committed locally as `630260d30ec2c4df935c6dffd758d2cba2c7f071` and is not
pushed. F1462 CampusRecordRepository is committed locally as
`9228864cbb40393bd84515ab06d02ca8c2d758ac` and is not pushed. F1463
CampusJsonRepository is independently verified and accepted; its local commit
is pending. Batch 22 is ready for closeout after the F1463 commit. Batch 23
discovery is complete with no new candidate queue accepted pending the
post-F1463 exit audit.
Evidence and limits are in the [progress log](04-Phase-3-Progress-Log.md).
Batch 22 remains open until closeout; push only after F1463
and Phase 3 closeout. The accepted queue is:

1. **F1444** — shared `RepositoryError`/`RepositoryResult`/status contract and SQL error mapping.
2. **F1445** — Workspace/database transaction, schema, lifecycle, and file-operation errors.
3. **F1446** — Teacher Import repository/apply-port error preservation.
4. **F1447** — TestingBlock/assignment structured errors, including occupied-slot classification from `Technical` to `Conflict`.
5. **F1448** — ClassRepository.
6. **F1449** — TestingClassRepository.
7. **F1450** — ClassInfoRepository.
8. **F1451** — TeacherRepository.
9. **F1452** — GsTeamRepository.
10. **F1453** — NativeEnglishTeacherRepository.
11. **F1454** — IntensiveSlotStateRepository.
12. **F1455** — RosterRepository.
13. **F1456** — CalendarEventRepository.
14. **F1457** — SpeakingEvalRepository.
15. **F1458** — ScheduleImportRepository.
16. **F1459** — ClassTransferRepository.
17. **F1460** — SettingsRepository.
18. **F1461** — PersonalDetailsRepository.
19. **F1462** — CampusRecordRepository.
20. **F1463** — CampusJsonRepository.

Keep the F1418 `ApplicationServices::saveDatabase()` wrapper void; F1445 typed
`WorkspaceStore` commit without changing that wrapper.

F1447 does not include the separately queued ClassRepository (F1448) or
TestingClassRepository (F1449).

## Batch 23 discovery — complete, no new queue accepted

The F1462 (candidate 19 of 20) trigger has been met. Independent scans found
no new eligible repository candidate; defer any new queue until the post-F1463
exit audit supplies new evidence. See the [progress log](04-Phase-3-Progress-Log.md)
for scan IDs and boundary findings.
