# ClassMngr Startup Optimization — Final Architecture and Review

> **When to use this file:** Do not include this file in normal single-phase Codex context. Use it after the numbered phases are complete, or when checking that an individual phase still aligns with the intended end-state architecture.

# Recommended Implementation Order
| Order | Phase | Expected Impact |

|---:|---|---|

| 1 | Cross-platform startup instrumentation | Required for proof |

| 2 | Single-pass global settings | Very high |

| 3 | Demand-driven PageManager | Very high |

| 4 | Remove duplicate ScheduleWidgets | High |

| 5 | Remove refresh/navigation cascades | High |

| 6 | Shared page lifecycle | High / architectural |

| 7 | Schedule renderer optimization | High longer-term |

| 8 | Defer nonessential work | Medium |

| 9 | Resource-pack refinement | Low–medium |

| 10 | Startup query optimization | Low–medium memory, useful speed |

| 11 | Explicit startup completion | Architectural cleanup |

| 12 | Cross-platform validation | Required |

| 13 | Permanent regression coverage | Prevents recurrence |

---

# Target Cross-Platform Startup Sequence
The final shared startup sequence should conceptually be:
```text
process start

│

├─ create QApplication

│

├─ resolve startup preferences

│  ├─ locale

│  ├─ font

│  └─ theme

│

├─ apply locale once

├─ apply font once

├─ apply theme once

│

├─ initialize minimal resource registry

├─ show splash if enabled

│

├─ resolve startup database

├─ create core ApplicationServices

├─ open database

│

├─ construct MainWindow shell

│  ├─ actions/menus

│  ├─ navigation/sidebar shell

│  └─ register PageManager factories

│

├─ load startup navigation data once

│

├─ construct only requested initial page

│

├─ load/render only visible initial content

│

├─ show MainWindow

├─ close/destroy splash

├─ release startup-only resources

│

├─ STARTUP COMPLETE

│

└─ normal event loop

   │

   ├─ optional update check

   ├─ optional notifications

   │

   └─ feature initialization on first use

      ├─ Sub Prep

      ├─ standalone Schedule

      ├─ Calendar/QML

      ├─ PDF

      ├─ Campus

      ├─ Classes/Analytics

      ├─ Speaking Evaluations

      └─ other feature-specific resources
```
---

# Permanent Architectural Rules
## Rule 1 — Startup is shared across platforms
Windows, macOS, and Linux should use the same startup lifecycle unless the operating system genuinely requires different code.

Platform-specific memory accounting is not a reason for platform-specific page initialization.

## Rule 2 — Never perform startup work on a hidden page
Registering a page must not construct it.

A hidden page must not query data or render merely because the database opened.

## Rule 3 — Global visual state is established before the widget tree
Locale, font, palette, and stylesheet should be resolved before constructing most widgets.

Do not reapply unchanged global settings after the window is shown.

## Rule 4 — Activation does not mean full refresh
Returning to a page should not automatically reload or rebuild it.

Refresh only when relevant data or preferences changed.

## Rule 5 — Feature initialization cost belongs to that feature
PDF, Calendar, Sub Prep, Campus, analytics, speaking evaluations, reports, and similar features should initialize when first used.

## Rule 6 — Do not move eager startup work later
Deferred initialization is only useful if it is genuinely optional.

Do not initialize every feature a few seconds after startup.

## Rule 7 — Measure peak memory
A startup change is not successful merely because the process eventually settles.

Peak memory, startup duration, widget count, page count, and render count matter.

## Rule 8 — Fix platform differences at the correct layer
If Windows retains resident memory differently than macOS, investigate and measure that behavior.

Do not compensate by creating a Windows-only page lifecycle or startup architecture.

---

# Completion Criteria
This plan is complete when all of the following are true:

- Windows, macOS, and Linux use the same core startup lifecycle.

- Platform-specific startup code exists only where technically required.

- Startup profiling represents the actual production path.

- Locale is applied once.

- Startup font is applied once.

- Startup theme is applied once.

- No application-wide post-show font/style reapplication occurs.

- No `PageManager::refreshAll()` is used during normal startup.

- Only pages required for the initial view are instantiated.

- Exactly one ScheduleWidget exists during normal My Workspace/Schedule startup.

- The visible initial schedule renders once.

- Hidden schedules do not render.

- Sub Prep, standalone Schedule, PDF, Calendar, Campus, and other heavy features remain lazy.

- Sidebar/navigation data is populated once.

- The intended initial page/tab is selected directly rather than through temporary intermediate tabs.

- Expensive work is removed from constructors and redundant `showEvent()` paths.

- Startup has an explicit `startup-complete` checkpoint.

- Peak and steady memory are both measured.

- Windows no longer exhibits an unexplained 600–700 MB normal-startup spike, or any remaining peak is measured and clearly attributable.

- macOS and Linux show no startup regressions.

- Runtime theme/font changes remain correct.

- Lazy feature first-use behavior is correct.

- Cross-platform startup regression coverage exists.

- Structural startup assertions prevent hidden pages/features from becoming eager again.
