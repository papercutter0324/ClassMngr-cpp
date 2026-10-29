# Speaking Evaluation Reports — Candidate #40

Synthetic presentation and output evidence for the Next Speaking Evaluation
report UI. The set covers preview and batch-save screens in English and Korean,
light and dark themes. The Korean batch captures also show the PowerPoint
alternative-renderer choice, Korean help text, and action labels. These captures
do not claim Legacy/Next pixel parity.

## Synthetic input and states

The screenshots use synthetic evaluation data. The preview displays a sample
student, rubric scores, and report sheet; the batch screen selects all two sample
students. The included one-page PDF is synthetic English report content.

| State | Locale | Theme | Artifact |
|---|---|---|---|
| Report preview | English (`en_US`) | Light | [PNG](report-preview_en_US-light.png) |
| Report preview | English (`en_US`) | Dark | [PNG](report-preview_en_US-dark.png) |
| Batch save, internal template | English (`en_US`) | Light | [PNG](report-batch_en_US-light.png) |
| Batch save, internal template | English (`en_US`) | Dark | [PNG](report-batch_en_US-dark.png) |
| Report preview | Korean (`ko_KR`) | Light | [PNG](report-preview_ko_KR-light.png) |
| Report preview | Korean (`ko_KR`) | Dark | [PNG](report-preview_ko_KR-dark.png) |
| Batch save, internal template | Korean (`ko_KR`) | Light | [PNG](report-batch_ko_KR-light.png) |
| Batch save, internal template | Korean (`ko_KR`) | Dark | [PNG](report-batch_ko_KR-dark.png) |
| Batch save, PowerPoint alternative selected | Korean (`ko_KR`) | Light | [PNG](report-batch-powerpoint_ko_KR-light.png) |
| Batch save, PowerPoint alternative selected | Korean (`ko_KR`) | Dark | [PNG](report-batch-powerpoint_ko_KR-dark.png) |

## Artifact integrity

The portable [manifest](manifest.json) records relative filenames, dimensions,
PDF page count, and SHA-256 for each artifact. Preview PNGs are 940 × 900 px;
batch PNGs are 760 × 500 px.

| Artifact | Dimensions / pages | SHA-256 |
|---|---:|---|
| [report-preview_en_US-light.png](report-preview_en_US-light.png) | 940 × 900 | `17ba8bdf41f61c94905873d9c7d866444a9253bc421dfab34c8dcb0bd4eaa140` |
| [report-preview_en_US-dark.png](report-preview_en_US-dark.png) | 940 × 900 | `89faa3333ec3abaea0812d7c6ada62a7177cf560af637bf45f19135d786000e5` |
| [report-batch_en_US-light.png](report-batch_en_US-light.png) | 760 × 500 | `3871c1f79e4512bad576ff0858aaafee44ac821c4a63445e60bc73850e6bb951` |
| [report-batch_en_US-dark.png](report-batch_en_US-dark.png) | 760 × 500 | `4628d5fa3d87473b4248e32bf029ccad7318712e1ee0729914c0149844429c20` |
| [report-preview_ko_KR-light.png](report-preview_ko_KR-light.png) | 940 × 900 | `e934ca9d85ae591bf126f88a5f710b338cb71acf51b5f03b86361453ef9644c7` |
| [report-preview_ko_KR-dark.png](report-preview_ko_KR-dark.png) | 940 × 900 | `003c5c173be64900c438f29ffe570f619006141a0540e7f3a146ccbba9ccfeb0` |
| [report-batch_ko_KR-light.png](report-batch_ko_KR-light.png) | 760 × 500 | `7e2e2a4ace057b8ba7d968a9f6af6ef0691bd612acfbd36baa6dfb6f33dc37ea` |
| [report-batch_ko_KR-dark.png](report-batch_ko_KR-dark.png) | 760 × 500 | `6b18910599176d86024fb015736f98b763044a766632aa52bf9d5c1c3adc076a` |
| [report-batch-powerpoint_ko_KR-light.png](report-batch-powerpoint_ko_KR-light.png) | 760 × 500 | `7e059e008a1014a14b1afb90e13f7ab8a1073589cdc8e1a83bf357f26f5c761e` |
| [report-batch-powerpoint_ko_KR-dark.png](report-batch-powerpoint_ko_KR-dark.png) | 760 × 500 | `e6159d5ea7d29d0900520d122ca7b376adc59a14641dba66eb5a829b8a6455dc` |
| [synthetic-speaking-evaluation-report.pdf](synthetic-speaking-evaluation-report.pdf) | 1 page | `aec997874d2b3e1b4dd78a0cb84042505f0e16d147bd3541a111fc1744d3e6f6` |

## Verification evidence and limits

The independent C40 review reported the standard macOS Debug build at
`build/macos-clang-debug` and these focused CTests passing independently:

- `ClassMngrSpeakingEvalReportWidgetTests`
- `ClassMngrSpeakingEvalBatchReportServiceTests`
- `ClassMngrNextBootstrapTests`
- `ClassMngrNextSpeakingEvaluationPageTests`

The resource-inventory check and `git diff --check` also passed. The independent
Tester verified the ten PNGs, synthetic one-page PDF, manifest, and SHA-256 for
every artifact. This is synthetic presentation/output evidence only. It does not
establish Legacy/Next pixel parity, actual PowerPoint operation, the macOS
privacy flow, or a hosted Windows result.
