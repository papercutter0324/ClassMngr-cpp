# Windows Memory Profiling Playbook

The developer Memory Usage Monitor is a lightweight, opt-in guide to a
reproducible investigation. Its process working-set and private-usage totals
are authoritative snapshots; its feature attribution is deliberately partial
and consists only of cheap, feature-owned retained-resource estimates.

## Capture a scenario

1. Build the desired revision and record its commit, build configuration, and
   the repeatable workflow you will exercise.
2. Start ClassMngr, open **Developer > Memory Usage Monitor**, reset the
   baseline, and perform the workflow. Add markers at meaningful boundaries
   such as startup complete, calendar opened, and PDF released.
3. Export the monitor JSON. It contains process samples, redacted lifecycle
   events, feature-attribution estimates, and page lifecycle state, but no
   student data or file paths.
4. Repeat the same workflow at least three times. Compare steady-state and
   post-release private usage rather than a single high-water mark.

## Windows Performance Recorder and Analyzer

Use WPR/WPA when an OS-level allocation investigation is needed:

1. Open **Windows Performance Recorder** and choose the **Memory** profile.
   Add CPU usage only when correlating allocation work with a slow operation.
2. Begin recording immediately before the marked scenario, then stop after
   the post-release or idle point. Save the `.etl` beside the exported monitor
   JSON using the same scenario name.
3. In **Windows Performance Analyzer**, inspect process working set,
   committed/private memory, virtual allocation, and heap/allocation stacks as
   applicable to the selected WPR profile. Filter to the ClassMngr process.
4. Align the trace timeline with the monitor's marker times and page-lifecycle
   states. Treat a feature attribution as a lead for inspection, not proof of
   an exact allocation owner.

## Visual Studio diagnostics alternative

For a development-time comparison, run the application under **Visual Studio
Diagnostic Tools > Memory Usage**, take a snapshot at each exported marker,
then compare the snapshots. Follow retained QObject/QPixmap/PDF objects back
to their feature owner before changing a retention policy.

## Reporting

Include the build revision, OS/Qt version, scenario steps, three-run summary,
monitor JSON, and (when captured) ETL or Visual Studio snapshot comparison.
State whether the conclusion comes from an authoritative process total, a
native allocation trace, or a partial feature-owned estimate.

## Reproducible health scenario

Use this same sequence for manual reports so timing and memory checkpoints can
be compared across revisions:

1. Launch with an empty settings profile, open **Developer > Memory Usage
   Monitor**, and capture a baseline marker.
2. Open Classes, then Calendar and wait for its visible range to finish.
   Record the Calendar cache/fetch and render timing events.
3. Open a representative PDF, wait for the PDF-open event, leave the page,
   and confirm the PDF-release event and retained-source estimate return to
   zero.
4. Visit Campus Maps, wait for decoded-image timing and attribution, return to
   the home page, then allow a short settling interval before exporting JSON.

The automated `--startup-performance-test` run writes
`classmngr-startup-profile-v2`. Its representative scenario uses the
synthetic, checked-in `Testing-copy.tps` profile and records structural,
native-memory, and timing metrics at `window-shown`, `startup-complete`, and
`settled-5s`. The root `peakMemory` object retains both sampled and
platform-reported peak memory values.

`ClassMngrStartupPerformanceTests` verifies that this representative route
keeps exactly one My Workspace page and one ScheduleWidget, with no Sub Prep,
PDF Viewer, or Campus Dashboard construction through the five-second
checkpoint. It prints each platform's measurements for trend comparison.
Treat those measurements as repeatable report artifacts, not cross-platform
CI limits, until several controlled baselines exist for that platform.
