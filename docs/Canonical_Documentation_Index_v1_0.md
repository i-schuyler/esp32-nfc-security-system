ARTIFACT — docs_index.md (Canonical Documentation Index v1.0)

# ESP32 NFC Security System — Canonical Documentation Index

Version: v1.0  
Session: SEC-2026-01-23-esp32-nfc-security-system  
Purpose:
- Single entry point for all canonical documentation.
- Defines the order of authority and prevents “which doc is truth?” ambiguity.
- Makes the firmware/UI/storage contracts explicit before code exists.

------------------------------------------------------------
1) Canonical Hierarchy (Order of Authority)
------------------------------------------------------------

When documents appear to conflict, resolve in this order:

1. **State Machine** (`State_Machine_v1_0.md`)
2. **Security + Access Model** (`Networking_and_Security_v1_0.md`, `NFC_Data_Contracts_v1_0.md`)
3. **Logging & Evidence Contracts** (`Event_Log_Schema_v1_0.md`)
4. **Feature Specification** (`Product_Specification_v1_0.md`)
5. **Hardware & Wiring Contract** (`Hardware_and_Wiring_v1_0.md`, `Wiring_Instructions_DevKitV1_v1_0.md`)
6. **UI Contract** (`Web_UI_Spec_v1_0.md`, `Web_UI_Style_Language_ModeC_v1_0.md`) — includes Setup Wizard + Factory Restore contracts
7. **OTA/Update Contract** (`OTA_Update_Contract_v1_0.md`)
8. **Configuration Registry** (`Configuration_Registry_v1_0.md`)
9. **Bench Test Checklist** (`Bench_Test_Checklist_v1_0.md`)

Implementation (code) must conform to the above.

------------------------------------------------------------
2) Recommended Reading Order
------------------------------------------------------------

1) Decisions Needed Before Code  
   - `Decisions_Needed_Before_Code_v1_0.md`

2) State Machine  
   - `State_Machine_v1_0.md`

3) NFC Data Contracts  
   - `NFC_Data_Contracts_v1_0.md`

4) Event Log Schema  
   - `Event_Log_Schema_v1_0.md`

5) Product Specification  
   - `Product_Specification_v1_0.md`

6) Hardware & Wiring  
   - `Hardware_and_Wiring_v1_0.md`
   - `Wiring_Instructions_DevKitV1_v1_0.md`

7) Networking & Security  
   - `Networking_and_Security_v1_0.md`

8) Web UI Spec (includes Setup Wizard + Factory Restore)  
   - `Web_UI_Spec_v1_0.md`
   - `Web_UI_Style_Language_ModeC_v1_0.md`
   - `/setup` routing + gating: Section 8 "Routing + gating (M7.1)" in `Web_UI_Spec_v1_0.md`
   - Setup Wizard optimization: Section 8 "M7.2 Setup Wizard Optimization Contract (M7.2)" in `Web_UI_Spec_v1_0.md`

9) OTA Update Contract  
   - `OTA_Update_Contract_v1_0.md`

10) Configuration Registry  
   - `Configuration_Registry_v1_0.md`

11) Bench Test Checklist  
   - `Bench_Test_Checklist_v1_0.md`

12) Implementation Plan  
   - `Implementation_Plan_v1_0.md`

13) Troubleshooting  
   - `Troubleshooting_v1_0.md`

14) Release Channels & Stability Promise  
   - `Release_Channels_and_Stability_Promise.md`

15) Platform choice note (optional, recommended)  
   - `Platform_Choice_IDF_Note_v1_0.md`

------------------------------------------------------------
3) Versioning Rules (Contracts First)
------------------------------------------------------------

- Canonical docs use semantic versions (vMAJOR.MINOR).
- If a contract changes in a non-backwards-compatible way, bump MAJOR and define a migration plan.
- Firmware must expose:
  - `firmware_version`
  - `config_schema_version`
  - `log_schema_version`
  - `nfc_record_version`

------------------------------------------------------------
4) “No Hidden Features” Rule
------------------------------------------------------------

If behavior is not in the canonical docs, it is not allowed to “appear” in code.

All user-visible behavior must have:
- a state machine transition reason
- a log event
- a UI representation


------------------------------------------------------------
5) Supporting Docs (Non-Canonical, Required Reading)
------------------------------------------------------------

The following documents are not part of the canonical “conflict resolver” hierarchy, but they are required reading for implementation and release discipline:

- `Implementation_Plan_v1_0.md` — build sequence, milestone exit criteria, negative-path tests, and contract coverage ledger
- `Diagnostics_Board_Bringup_Firmware_v1_0.md` — board bring-up diagnostics firmware (non-customer)
- `Troubleshooting_v1_0.md` — user-facing fault symptoms, recovery steps, and UI health indicator expectations
- `Release_Channels_and_Stability_Promise.md` — release discipline and guarantees for product-grade stability
- `Platform_Choice_IDF_Note_v1_0.md` — platform rationale and constraints for future migration decisions
- `safety-appendix.md` — security boundaries, offline assumptions, and safety defaults (public release)
- `SECURITY.md` — security reporting guidance (public release)
- `CONTRIBUTING.md` — contribution policy and issue guidance (public release)
- `LICENSE` — Apache-2.0 license text

---

## Codex + CI Foundation Additions (2026-01-28)

- `AGENTS.md` — Codex guardrails and working rules for safe repo changes.
- `tools/toolchain_sanity_check.sh` — local environment sanity checks (developer helper).
- `.github/workflows/firmware-build.yml` — CI compile-check workflow (fast default; optional artifacts as explicitly configured).


- Codex CLI: `docs/Codex_CLI_Workflow_and_Safety_v1_0.md` — Repo-scoped guardrails and usage patterns for safe Codex-assisted development.

------------------------------------------------------------
6) Investigation Notes (Non-Canonical)
------------------------------------------------------------

These notes do not override canonical docs and are for planning only:

- `docs/_investigation/esp32-s3-devkit-n32r16v-m_support_report.md`
- `docs/_investigation/esp32-s3-dev-kit-n32r16v-m_pin_roles_and_allowlist_notes.md`
