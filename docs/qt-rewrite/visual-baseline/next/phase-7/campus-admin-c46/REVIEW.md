# Campus Admin and edit-permission review (C46)

This package records the Next Admin menu and Campus Directory Information surface in English and Korean, with light and dark styles. `campus_admin_c46_review.json` is the per-image manifest; it records each capture's state, route result, pixel dimensions, device-pixel ratio, and scale.

## Capture setup

- Qt platform plugin: `offscreen`; widget style: Fusion with the packaged light or dark stylesheet.
- Logical window viewport: 1024 × 768; `QT_SCALE_FACTOR=1`; all captures have device-pixel ratio 1.0 (1×).
- Campus data: packaged Bundang catalog; each capture uses isolated temporary application storage.
- The menu strips are 1024 × 50 pixels in English and 1024 × 47 pixels in Korean. Authorized and read-only route images are 1024 × 768 pixels.

## Review

| View | English captures | Korean captures | State and route result |
| --- | --- | --- | --- |
| Admin menu | `campus_admin_en_US_{light,dark}_menu.png` | `campus_admin_ko_KR_{light,dark}_menu.png` | The localized Admin menu contains Manage Campuses. Activating the action routes to `campus_information`. |
| Authorized Information route | `campus_admin_en_US_{light,dark}_route.png` | `campus_admin_ko_KR_{light,dark}_route.png` | The Information route is active through Manage Campuses. Save and legacy-import actions are visible; Save is disabled while the untouched form is clean. |
| Read-only Information route | `campus_readonly_en_US_{light,dark}_route.png` | `campus_readonly_ko_KR_{light,dark}_route.png` | Campus Directory remains reachable, shows a read-only notice, and hides editing actions. The Admin menu is absent. |

The English and Korean menu labels are readable in both themes. The authorized route keeps the existing directory navigation and information fields, while the read-only route explains its mode and leaves the normal Campus Directory navigation available. Read-only map-link buttons remain enabled; their availability is checked by `allFiveRoutesRenderAndReadOnlyDoesNotExposeEditing` rather than shown in these Information-route captures.

## Permission and navigation evidence

- `ClassMngrNextCampusDirectoryTests::productionAdminArgumentMapsToCampusEditCapabilityByPresence` verifies that the existing `--enable-admin` argument maps to the edit capability.
- `NavigationModelTests::shellAdminMenuIsCapabilityGatedAndManageCampusesUsesNavigationGuard` asserts that read-only mode has no Admin menu or Manage Campuses action, that enabled mode creates the menu entry, and that the action uses the normal navigation guard. Rejecting navigation leaves the current route and history unchanged; activation failure also leaves the route unchanged; allowing navigation opens `campus_information`.
- `ClassMngrNextBootstrapTests::dirtyPageBlocksNavigationOpenNewAndWindowClose` exercises Manage Campuses with a dirty page and Cancel, confirming that the route does not change and the draft remains.
- `CampusDirectoryTests::allFiveRoutesRenderAndReadOnlyDoesNotExposeEditing` checks all five directory routes in read-only mode, hidden Save/import controls, read-only fields, and enabled Naver/Kakao map-link buttons.
- `CampusDirectoryTests::composedCampusEditOperationsEnforceCapabilityAndAuthorizedParity` calls the composed save/import actions directly. Denied Create, Update, and import return non-recoverable `InvalidInput` before any save/import port call and preserve the campus data. Authorized Create, Update, and import persist and read back their results. Injected authorized save and import failures leave data unchanged.
- `CampusDirectoryTests::realQSaveFileFailureRetainsDirtyDraftAndRetrySucceeds` verifies the UI preserves the dirty form after a failed save and succeeds on retry.

These checks exercise the local feature capability derived from the existing command-line flag. They do not establish account authentication or role-based authorization.

## Verification

Built with:

```sh
cmake --build build/macos-clang-debug --target \
  ClassMngrNextCampusDirectoryTests \
  ClassMngrNextBootstrapTests \
  ClassMngrNextApplicationCampusDashboardCampusIdentitySavePolicyTests \
  ClassMngrCampusDashboardPageTests \
  ClassMngrMainWindowManageCampusesParityTests \
  ClassMngrNextCampusDirectoryVisualCapture -j2
```

Focused CTest command:

```sh
ctest --test-dir build/macos-clang-debug \
  -R '^(ClassMngrNextCampusDirectoryTests|ClassMngrNextBootstrapTests|ClassMngrNextApplicationCampusDashboardCampusIdentitySavePolicyTests|ClassMngrCampusDashboardPageTests|CampusDashboardCleanRefreshToEmpty|CampusDashboardDirtyRefreshToEmpty|ClassMngrNextCampusDirectoryVisualCapture)$' \
  --output-on-failure
```

Result: 7/7 registered tests passed. The Manage Campuses parity registration was run separately with `ctest --test-dir build/macos-clang-debug -R '^ClassMngrMainWindowManageCampusesParityTests$' --output-on-failure`; result: 1/1 passed. Direct QtTest totals were Campus Directory 13 passed, Next Bootstrap 78 passed, Legacy Campus Dashboard page 34 passed, and Manage Campuses parity 7 passed. The standalone Campus identity/save-policy executable exited successfully. The visual capture CTest passed, and the capture executable regenerated the 12 files in this package at 1×.

The C46 captures were generated with:

```sh
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 \
  build/macos-clang-debug/ClassMngrNextCampusDirectoryVisualCapture \
  build/macos-clang-debug/c46-campus-admin-review
```

The resource inventory check was run with `python3 scripts/phase4/generate_resource_inventory.py --check` and reported `stale inventory` for `docs/qt-rewrite/phase-4-resource-inventory.json`. Its recorded `resources` source tree ID is `d30662cf0549696384da4830485e34f07ee56d1a`; the current tracked tree ID is `cd415a1fc50124384e6ed75db7d1a29d8d7a4b9d`. That separate Phase 4 inventory was left unchanged.

## Limits

These are offscreen Fusion captures at 1× using the packaged Bundang campus data. They document the route and permission states, not native-window rendering or pixel parity with Legacy. Menu heights differ by locale because the localized labels have different metrics.
