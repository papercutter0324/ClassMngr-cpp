# Current Slice Batches

Phase 3 is In Progress on the mandatory Heavy route. Its exit gate remains
open while the accepted §3.3 structured-error queue is incomplete; the
completion audit and accepted contract are in the [Phase 3 progress
log](04-Phase-3-Progress-Log.md).

## Batch 22 — Active

F1444 implementation and independent focused verification are complete; its
slice commit is pending. F1445 scope review is underway, with no implementation
started. The accepted queue is:

1. **F1444** — shared `RepositoryError`/`RepositoryResult`/status contract and SQL error mapping.
2. **F1445** — Workspace/database transaction, schema, lifecycle, and file-operation errors.
3. **F1446** — Teacher Import repository/apply-port error preservation.
4. **F1447** — TestingBlock/assignment errors, including occupied-slot `Conflict` currently classified `Technical`.
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

Keep the F1418 `ApplicationServices::saveDatabase()` wrapper void; only type
`WorkspaceStore` commit at F1445.

## Batch 23 discovery reminder

Start discovery when F1462 (candidate 19 of 20, the second-to-last candidate)
begins. The independent API scan found likely eligible DocumentCatalog and
DataService compatibility-conversion surfaces beyond Batch 22's 20-candidate
limit; search for these and any other candidates then.
