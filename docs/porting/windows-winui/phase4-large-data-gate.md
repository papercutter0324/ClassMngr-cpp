# Phase 4 large-data gate

## Representative workloads

| Surface | Source rows | Visible-region assertion |
| --- | ---: | --- |
| Class list | 10,000 | Realized containers remain below 3x the rows that fit in the viewport. |
| Roster | 5,000 | Only visible row containers and the selected editor are retained. |
| Schedule slots | 2,000 | `ItemsRepeater` realizes the viewport and a bounded cache only. |
| Speaking scores | 10,000 x 8 logical cells | Only visible row/cell presentations are realized; editing one cell cannot materialize the grid. |

## Required capture

For each workload, record the following after initial render, a page-down
sequence, and a selection/edit operation:

- source item count and viewport row count;
- realized container count and live row/cell view-model count;
- 95th-percentile frame time and managed/native allocation sample;
- private working-set delta and reclaimed-memory observation;
- UI Automation availability for the selected row/cell.

The control fails the gate if realized visual or view-model counts track the
total data size. The implementation must use the first-party primitive selected
in `phase4-virtualization-decision.md`; a performance failure is not an
implicit approval for an external grid.
