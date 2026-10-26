# Physically qualified single-clear profile + borrowed publisher source

This is an OFFLINE composition, not a new GPU qualification. Root owns all physical tests. No boot/stage/SSH/X server/GPU operations were performed.

Base: `/home/gama/sgx535-offline/gears-depth-less-single-clear-20261026T160400Z`. Its separate physical qualifier passed COLOR/Z co-coverage, black/clear background, the stable analytical reference, full journal and negative gates. Its older footprint verifier failure remains preserved and is not reclassified.

Only three production files change: `remesa_stream_trusted.c`, `remesa_stream_trusted.h`, and `remesa_stream_owner_flush.c`. Their original S contents were verified equal to the K source baseline before copying the sealed borrowed-publisher source changes. All other module/observer sources, Makefile, S profile binary, five assets, profile header and canonical input header remain byte-exact S. No K geometry/profile data was copied.

FAST borrows six already-held provider CPU views and revokes the owner's loan pointers after unchanged publication checks, cache transfer and DMA barrier. This removes six vmap and six vunmap calls per frame, covering 4,399,104 bytes/1074 pages. No additional persistent mapping is created. LAB retains private aliases and its original cleanup. Clears, cache maintenance, address/PTE validation, leases, fences, retirement, errors and quarantine remain. This does not claim saved milliseconds or FPS.

Tests passed once in this composition: actual bind/consume/flush/quarantine/close helpers in native/UBSan/i386, plus nine existing data-aware regression suites. Tests load this S profile and canonical data; the cache fixture uses the real publisher helpers and actual attached-job release guards. Build and ELF comparison prove the embedded 5844-byte S profile, not physical completion. Private initrd identity is verified offline and remains unstaged.

`COMPOSITION_INPUTS.json` pins original S source/profile identities. `S_GOLDEN_INPUT_IDENTITIES.json` identifies the six physical S COLOR/Z captures by absolute path, without modifying them. Full Z hashes preserve original artifact identity only: future output comparison must use defined Z24 and treat high X8 as VOID.

Next root test: review the three diffs, stage a new isolated initrd/bundle, confirm identity and rollback, then six identical poses with full S COLOR byte equality and Z24 equality, all input/journal/negative/PTE/lease/retirement gates, and N/N/N cleanup. Only after that compare original 1280×800 GLXGears FPS using the same user libraries and assets. Do not mix the separate X11/SHM candidates into the first kernel comparison.
