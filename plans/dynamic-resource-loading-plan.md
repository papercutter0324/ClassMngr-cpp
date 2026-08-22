# Feature-Scoped Dynamic Resource Loading

## Goal

Reduce the process memory retained by bundled assets by moving the selected
asset trees into local RCC packs that are mounted only while a feature needs
them. A mounted pack is reference-counted through an RAII lease and is
unregistered after its last owner releases all decoded resources.

## Pack Boundaries

The build produces local, installed packs for `campuses`, `documents`,
`files`, `images`, `splash`, and `templates` (the runtime
`templates/speaking-eval` subset). Fonts, icons, styles, translations, QML,
roster designs, and other templates remain embedded. The speaking-evaluation
source-artwork directory remains excluded from distributable runtime assets.

## Lifecycle Rules

- Page transitions acquire the target page's required packs before releasing
  the leaving page. Calendar, Personal Details, Campus Dashboard, and Sub Prep
  own a campus lease only while active.
- The document catalog briefly leases `documents` to parse its metadata at
  startup, then releases it. Document viewing and document-backed operations
  retain a lease for their complete operation; the PDF viewer drops it after
  its document has been closed.
- Splash resources are released after the splash widget is destroyed once the
  main window is visible.
- The Classes Evaluations section owns the speaking-evaluation template lease.
  Leaving that section or the Classes page clears report images and
  template-specific font registrations before releasing the lease.
- `files` and `images` are available through lease-aware path helpers but are
  not mounted by current production code because they have no consumers.

## Verification

Cover pack construction, reference counting, unmounting, updated-pack
fallback, startup catalog release, page transitions, document-viewer release,
and speaking-evaluation cache recreation. Record mount/unmount events in the
developer memory diagnostics and compare cold-start and post-navigation
working-set/private-byte snapshots as report-only measurements.
