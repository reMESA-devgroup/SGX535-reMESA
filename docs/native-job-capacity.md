# Complete retained resource closure up to32 allocations

The current generated fixed-event scene references17 GPU allocations. Retained
GEM jobs and their private MMU loans previously stopped at16, although the GEM
object table and MMU auditor already supported32. The maintained staging layer
now uses the shared kernel-only `REMESA_GEM_JOB_BINDINGS=32` for job IDs, CPU
views, rollback bookkeeping, temporary binding arrays and private rule copies.
No reference is truncated or omitted to fit the former limit.

The frozen checkpoint sources remain unchanged. `job_capacity_sources()` applies
an exact checked transformation while staging: every expected field/local/limit
anchor must occur the specified number of times. Missing or extra anchors refuse
the transformation. A new staged header carries the bound; compile-time checks
keep it within the backing-slot count and private MMU auditor capacity. The
kernel-only loan structure grows consistently within the rebuilt module. There
is no userspace ABI, new ioctl, executor registration or runtime gate change.

Address, page identity, permissions, caching, mapping generation, ownership,
lease exclusion, source/device exclusion and complete-MMU audit checks remain.
Jobs retain every allocation until cancellation or acknowledged retirement;
uncertain jobs retain all resources. The reserved private executor still returns
EOPNOTSUPP. Raising resource capacity alone does not qualify or enable native
execution, initialize scratch storage or establish complete desktop/OpenGL APIs.

`test_gem_job_capacity.py` compiles the actual retained base core with the same
capacity transformation and injected kernel APIs, the maintained private-loan
code, and the actual MMU auditor with modeled pages. Tests cover16/17/32 resources,
rejection33, all tail IDs/views/rules, sorted R/W/RW and cache settings, CPU-only
sidecars, last-map failure rollback, tampered last views/PTEs, physical aliases,
bound-root refusal, publication, cancellation and quarantine. A test-only
executor exercises the entire issue/completion/publication/acknowledgement/
retirement sequence for17 and32 resources; early release refuses and all holds
remain until retirement. This executor is not installed in production.

Evidence root: `/home/gama/sgx535-offline/native-job-capacity-20261031/v1`.
`tests-v3/RESULT.json` records native/UBSan/statici486 results. Earlier v2 passed
before additional lifecycle cases; `tests.stdout` preserves an initial test-only
anchor mutation error (the malformed fixture retained the original substring),
corrected before model compilation. No production rule was weakened.
`execution-regression/RESULT.json` records the existing private-execution models;
`controller/RESULT.json` records the existing LAB controller with UBSan.

`NORMAL_PROFILES.json` compares five staging profiles with the preceding stager:
all other guard/execution source text is identical, and reversing only capacity
and header edits restores the original GEM source. `full-module/RESULT.json`
records a complete antiX5.10.240 i486 module build in copied kernel output,
without installing it. `BUILD_WARNINGS.json` records no new warnings relative to
the previous full build. `FINAL_BINDING.json` binds final sources and receipts.
The modeled core is deliberately separate from the complete staged module,
whose additional integrations receive build/controller regression coverage.

No new physical kernel, GPU submission, retirement or rendering qualification
is claimed. The newly built module is unqualified for deployment. A trusted
general-scene consumer, scratch initialization/publication contracts and bounded
physical validation remain required before normal driver/API integration.
