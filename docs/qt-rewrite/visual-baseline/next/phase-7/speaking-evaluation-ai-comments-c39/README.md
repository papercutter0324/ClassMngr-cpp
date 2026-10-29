# Speaking Evaluation AI comment captures

Candidate #39 presentation evidence for the Next Speaking Evaluation page and
its AI comment dialogs. The 16 synthetic captures cover English (`en_US`) and
Korean (`ko_KR`) in light and dark themes. They show the page, single-student
prompt, batch selection, and batch review. The review uses a deterministic
synthetic response; no real student data is included. These images support
presentation review and do not claim pixel parity.

Candidate #27 already migrated class analytics. Candidate #39 adds the
AI-assisted comment workflow and does not duplicate that analytics screen.
The prompt redacts student names before opening the configured external AI
website and restores names locally when applying the response. The workflow
does not call a hosted AI API.

| Capture | Dimensions | SHA-256 |
|---|---:|---|
| [page_en_US-light.png](page_en_US-light.png) | 1440×880 | `5277098c3943fe4cd0184c39a23c79bb2e08452aec865e7c58769c37d061e84f` |
| [page_en_US-dark.png](page_en_US-dark.png) | 1440×880 | `9e0c55f9bf23e2277b00ee2bdb74fa1260dcdb054be4ac2f46d203b4f6bf074b` |
| [page_ko_KR-light.png](page_ko_KR-light.png) | 1440×880 | `68ecc439321c38222270aa26542deeb2fa2a833dc06f59f35967cf83dbb7065b` |
| [page_ko_KR-dark.png](page_ko_KR-dark.png) | 1440×880 | `7421552cf83da0da38455b23480eff1bb14f3e59e140a60bd161513a907ea064` |
| [ai-prompt_en_US-light.png](ai-prompt_en_US-light.png) | 840×600 | `f0c4fb255316a5f6a656a71f1f7be062523424c7e9e1a2e661916bfe556f13db` |
| [ai-prompt_en_US-dark.png](ai-prompt_en_US-dark.png) | 840×600 | `7578b05405f616b24ff33409afd348f1da648e311fb2b7de1d5634484e1ee163` |
| [ai-prompt_ko_KR-light.png](ai-prompt_ko_KR-light.png) | 840×600 | `a85f90ada97ae20539019d9ef96088246a4d7cf193a4b12c857bb8bba5e6b45a` |
| [ai-prompt_ko_KR-dark.png](ai-prompt_ko_KR-dark.png) | 840×600 | `cb7de967b8cb237e83e8f6ba58d08a5bf86e0c9e9e8f0ba6756ce50284747090` |
| [ai-batch_en_US-light.png](ai-batch_en_US-light.png) | 1080×760 | `1c9888269d10ffd33e8a4ff632ba97cc1ada8d395c3a7554bce421c11122c67c` |
| [ai-batch_en_US-dark.png](ai-batch_en_US-dark.png) | 1080×760 | `12d21165cc1167214edd5f89b480b1e212e64d6830d341502a1d8add307c8953` |
| [ai-batch_ko_KR-light.png](ai-batch_ko_KR-light.png) | 1080×760 | `5dd838e3b40ddb0f4a0cec4819ceaa598afb471da5c495d99e487a3b46294fdc` |
| [ai-batch_ko_KR-dark.png](ai-batch_ko_KR-dark.png) | 1080×760 | `d98297b968ed34702605f4d888733ce1d55fc995dbd56d8f25195bb42ae24092` |
| [ai-batch-review_en_US-light.png](ai-batch-review_en_US-light.png) | 1080×760 | `ac8216930b92a6ea1909e8de06fec4516869f62585e8963e38cb081714bda5c5` |
| [ai-batch-review_en_US-dark.png](ai-batch-review_en_US-dark.png) | 1080×760 | `52dc6fb60af3aa62bfa855afd87d342e1b0f1ed387ad87cfc973bc258815c4f8` |
| [ai-batch-review_ko_KR-light.png](ai-batch-review_ko_KR-light.png) | 1080×760 | `a20564594048632c0f4b5ec58c5c348e5ba7883930e30ada7a115a3eb6e522ba` |
| [ai-batch-review_ko_KR-dark.png](ai-batch-review_ko_KR-dark.png) | 1080×760 | `9a5caf119092139a64b866f6f9828ed4d5e1332342dbc5a057e3995b9d2abc17` |

Independent acceptance review passed. A fresh macOS Debug configure validated
one owner for 1,911 handwritten sources and built the affected targets; focused
CTest passed 9/9. Direct offscreen QtTest verified batch-row selection by mouse
and Space, plus WorkspaceStore batch apply, save, page recreation, and persisted
readback. Resource inventory and diff checks passed. All 16 captures match the
fresh output, and the accepted #38 captures remain unchanged. This is focused
presentation, interaction, and persistence evidence; the images make no
pixel-parity claim. No full-suite, Windows, hosted memory, or phase-gate result
is claimed.
