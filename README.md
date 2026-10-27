# SGX535-reMESA
`SGX535-reMESA` an Open-source Mesa/Gallium 3D graphics driver for Intel Poulsbo (GMA 500 / PowerVR SGX535) on Linux

<table>
<tr>
<td width="115" valign="middle"><img src="assets/pipocahello.gif" width="100" alt="Pipoca saying hello"></td>
<td valign="middle">

**Introduction**

Hey! I'm Pipoca, the official SGX535-reMESA mascot.

Welcome to a project involving an old Atom processor, a rather mysterious GPU, far too much assembly, and a developer who apparently doesn't know when to give up.

</td>
</tr>
</table>

I started SGX535-reMESA on September 16, 2026, with one goal: understand the SGX535 well enough to write an independent, open-source Mesa/Gallium driver for it.

The SGX535 isn't exactly well documented. Most of what I've learned has come from studying old Windows drivers, Intel EMGD, historical DDK material, register definitions, disassembly, and lots of experiments on real hardware.

Some things worked on the first try. Most didn't.

## What works so far

The GPU is rendering again. I have verified triangles and an indexed square on the SGX535 itself, and on October 21 I got an OpenGL `glDrawArrays` call through Gallium and onto the hardware. That test produced 136 green pixels in a 32×32 target, exactly where they were supposed to be.

I've also displayed a GPU readback in an X11 window. That earlier test published pixels from a previous submission. Since then, the original `glxgears` has completed a qualified, bounded **600-frame / 600-GPU-submission** run at **1280×800** on the real SGX535. A separate six-operation physical test checked the new production transport and X11 presentation against the retained images.

Repeated submissions, synchronization and direct read-only GEM FD/X11 shared-memory presentation are now qualified for this restricted 1280×800 path. Unbounded continuity and general application compatibility still need qualification. OpenGL 2.1 is the target, not something I'm claiming to support already.

If you're looking for the actual evidence rather than my word for it, start with the [October 27 physical results](docs/phase9/PHYSICAL-GLX-STATUS-20261027.md), the [earlier development update](docs/phase9/DEVELOPMENT-UPDATE.md) and the [reproduction notes](docs/reproduction/TRIANGLE.md).

## Current status

<img src="assets/pipocajumping.gif" align="right" width="105" alt="Pipoca jumping">

My test machine is a Dell Inspiron Mini 12, powered by an Intel Atom Z520 and a PowerVR SGX535 rev121.

On October 6, I finally got my first triangle rendered by the actual GPU. That was followed by two indexed triangles forming a 16×16 square. Both experiments produced results I could verify rather than just hoping the GPU had done what I asked.

Since then, I've moved much further into Mesa/Gallium.

One of the biggest milestones came on October 21: an OpenGL-originated `glDrawArrays` call went through my Gallium path and reached the physical SGX535. The GPU completed the job, and the readback matched the expected result: **136 green pixels and 888 untouched pixels** in a 32×32 target.

That was a pretty big moment for me. I was no longer just submitting a diagnostic job by hand: I'd demonstrated a connection between an OpenGL operation, my driver code, and the GPU.

I've also been working on X11 presentation, front/back buffers, framebuffer binding, memory allocation, and the rather unpleasant details involved in moving from tiny test images to a real desktop resolution. I got a verified readback onto an X11 window too. That part worked, although it reused pixels from an earlier GPU run rather than submitting a new job.

And yes, **I finally got `glxgears` running at 1280×800!**

The latest completed qualification went further: **600 frames and 600 GPU submissions**, with **16.1345 FPS** averaged over six steady five-second windows (15.915–16.376 FPS, about 61.979 ms/frame). The run ended with no queued work, engine or presentation errors, legacy submissions or offline fallback, and its diagnostic capture stayed empty.

That's an observed **11.29% FPS improvement** over the earlier 14.4983 FPS snapshot run. The comparison combines the cached-Z/fault64 kernel, direct read-only GEM FD/XSHM presentation and paired GC work. CPU DVFS and workload were uncontrolled, so I can't attribute that gain to any one change.

A separate six-operation physical smoke test checked full COLOR, defined Z24 bits, all five input images and every X11 RGB pixel. It also checked completion before lease release and detach before FD/GEM close. Those readbacks belong to the separate LAB harness; the normal FAST run doesn't add per-frame hashes, pixel checks or dumps. This remains restricted qualification, not full GL conformance or LAB approval.

There's more to do before I can call this a usable everyday graphics driver. Still, considering where this project was a month ago, I'm pretty happy with the progress.

The next headache is reaching the **30–60 FPS goal**, qualifying longer continuous operation and expanding beyond this restricted path. The [current technical report](docs/phase9/PHYSICAL-GLX-STATUS-20261027.md) records the measurements, evidence and remaining limits.

<br clear="all">

## Reproducing GLXGears

The [GLXGears reproduction guide](docs/reproduction/GLXGEARS.md) separates public checkout/build checks from the qualified kernel, private bundle and actual GPU command used for the 1280×800 run. The current direct-FD build and boot artifacts are not yet published as a complete reproducible package; cloning this repository alone does not reproduce the measured 16.1345 FPS. The guide records the exact requirements and remaining publication gaps.

## The historic Git commit — `0x07000345`

<img src="assets/pipocaohno.gif" align="right" width="110" alt="Pipoca reacting to 0x07000345">

If you've followed this project for a while, you've probably seen me complain about `0x07000345`. A lot.

After roughly 26 days of investigating that one DWORD, I decided it deserved a permanent place in the repository's history.

So I made this commit: [**070003454e7f938661170abcc57a551ae68c931c**](https://github.com/gabrieelgama/SGX535-reMESA/commit/070003454e7f938661170abcc57a551ae68c931c).

Yes, the actual Git commit hash starts with `07000345`.

The commit preserves my independently written Mesa implementation and the offline qualification work around it. The hash itself is just a tribute to the hexadecimal number that has consumed an unreasonable amount of my time.

**ALWAYS.**

<br clear="all">

## Documentation

<img src="assets/pipocaconfering.gif" align="right" width="105" alt="Pipoca checking documentation">

I try to document the work as I go, including failures, unresolved questions, and experiments that didn't produce the result I expected.

Most of the technical documentation lives in [`docs/`](docs/).

If you want to reproduce some of the earlier hardware experiments, start here:

- [Triangle](docs/reproduction/TRIANGLE.md)
- [Square](docs/reproduction/SQUARE.md)
- [Display](docs/reproduction/DISPLAY.md)

The repository also contains historical raw results, hashes, and supporting evidence. For the latest technical breakdown and the retained local evidence paths, see the [October 27 physical results](docs/phase9/PHYSICAL-GLX-STATUS-20261027.md). The [October 14 development update](docs/phase9/DEVELOPMENT-UPDATE.md) records an earlier implementation checkpoint. I've also kept the [original triangle-era README](docs/history/README-TRIANGLE-07000345.md) for anyone interested in how this started.

I make an effort to distinguish CPU tests, offline validation, application integration, X11 publication, and actual GPU execution. Passing a thousand tests on the CPU is useful, but it doesn't mean the SGX535 has executed anything.

<br clear="all">

## OpenGL and future plans

<img src="assets/pipocarunningright.gif" align="right" width="105" alt="Pipoca running right">

My main compatibility target is **desktop OpenGL 2.1**.

I'm also researching OpenGL 3.0, although I don't want to promise support for something I haven't properly verified yet.

The first applications and games I want to test include **Minecraft Java**, Counter-Strike 1.6, SuperTuxKart, and some older browser games.

Minecraft is particularly interesting because I previously saw it average around 30 FPS with the original Windows 7 driver. I'd love to see what an independently written Linux driver can eventually do.

Wayland support is also part of the plan. KDE Plasma on an Atom Z520, though? That might be asking a little too much from a processor that already struggles with five browser tabs.

I'm not expecting miracles. The GMA 500 was slow even when it was new. And don't expect me to play GTA San Andreas on this thing. Even my cockatiel could probably run it faster.

The longer-term goal is to make this useful on supported 32-bit Intel hardware, not just on my own Dell Mini 12.

<br clear="all">

## Development setup

<img src="assets/pipocaidle.gif" align="right" width="105" alt="Pipoca resting">

Here's something that still makes me laugh: most of this driver is being developed on a **Samsung Galaxy Tab S7**.

I run Linux through Termux/PRoot and cross-compile the driver for i686. The resulting builds are then tested on the Mini 12.

I originally set up the tablet as a convenient development environment. Somehow, it became my main computer. At this point, I don't even question it anymore.

<br clear="all">

## Reverse engineering the USSE compiler

<img src="assets/pipocathinking.gif" align="right" width="110" alt="Pipoca thinking about USSE instructions">

One of the hardest parts of this project has been figuring out how the original SGX535 drivers generate shader instructions.

The SGX535 uses the USSE architecture, and understanding its instruction encoding has meant spending a lot of time in Ghidra, assembly listings, and old compiler code.

The infamous `0x07000345` was one of the starting points.

While tracing the original compiler, I recovered parts of its instruction encoding logic, including register field placement, MOV handling, instruction finalization, and some of the surrounding memory-management behavior.

That gave me enough information to begin implementing my own restricted USSE encoder and decoder rather than embedding proprietary shader binaries.

One finding was that assembler flag bit 0 maps to bit 23 of the second encoded instruction word. I can demonstrate that mapping in the CPU-side compiler logic, but its precise meaning during GPU execution still needs investigation.

I also traced a cleanup callback at offset `+0x630` through `_mesa_free` to the imported `free` function.

The identity-MOV cleanup routine releases 11 referenced allocations plus the instruction node itself. Two additional cleanup lists seem to manage recycled auxiliary records, although I haven't fully established their ownership relationships.

Not every mystery in the original compiler needs to be solved before my own restricted shader path can move forward.

At one checkpoint, the independent codec passed **447 offline tests** in each of three environments: native Linux, UBSan, and i386/QEMU.

Since then, the work has expanded into PDS encoding, program storage, relocations, shader preparation, and physical execution.

There's still plenty I don't know. Register-bank behavior, termination rules, PDS fetch details, and parts of the instruction encoding remain open questions. I'd rather leave those questions documented than pretend the entire architecture has been decoded.

And somehow, after all this work, `0x07000345` is still finding ways to annoy me.

<br clear="all">

## Contributing

<img src="assets/pipocawaitingyou.gif" align="right" width="110" alt="Pipoca waiting for contributors">

If you have old Poulsbo hardware, SGX535 documentation, historical Intel drivers, DDK information, or anything else that might help, feel free to open an issue. Testing on other compatible hardware would also be valuable.

### A note on earlier SGX535 driver projects

I was contacted by email about another project that reportedly had a working Mesa/Gallium driver for Poulsbo.

While looking into it, I found references to files whose origins I couldn't independently establish.

One example was an `sgxdefs.h` file reportedly containing more than 8,000 lines, whereas a historical distribution available to me contained a version of roughly 106 lines.

That difference raised questions for me, but it doesn't prove anything about the larger file's origin, licensing, or redistribution rights.

For SGX535-reMESA, I want to keep the approach straightforward: study the hardware and historical software behavior, document the evidence, and write original implementations without incorporating proprietary DDK code of uncertain provenance.

I'm open to collaboration and learning from earlier work. I just want the code's origins to remain clear, especially if this project eventually reaches upstream Mesa.

Information about other PowerVR Series5 GPUs is welcome too. The long-term goal is to contribute a maintainable Mesa driver and suitable mainline Linux kernel patches.

<br clear="all">

## Thanks

Thanks to **Simon Fenney** for pointing me toward Intel EMGD. That turned out to be extremely useful.

Thanks also to everyone who has read the documentation, shared information, tested something, or simply followed along while I tried to convince a nearly two-decade-old GPU to draw triangles again.

## Special thanks

<img src="assets/pipocajumping.gif" align="right" width="110" alt="Pipoca celebrating">

And finally, a special thanks to `0x07000345`.

I hate you.

Twenty days of reverse engineering, countless solver runs, and one mysterious hexadecimal value.

All that work eventually led me to `Xpsb_emit_pixel_shader` and a much better understanding of the surrounding compiler machinery.

And after all that...

**ALWAYS.**

Maybe the real `0x07000345` was the friends we made along the way 🥀

Fun fact: one of my important discoveries about this value wasn't even made on the Tab S7. I was using my phone. Apparently, the final boss wasn't prepared for that. 😂💀

<br clear="all">

## One last thing

<img src="assets/pipocarunningleft.gif" align="right" width="110" alt="Pipoca running left">

Thanks for stopping by!

This started as an attempt to understand a forgotten GPU and somehow turned into one of the most interesting projects I've ever worked on.

I'll keep documenting the progress, including the strange parts, the failures, and the occasional hexadecimal-induced breakdown.

The goal is serious. The documentation doesn't always have to be.

**And yes, I'll keep the cockatiel GIFs.**

— Gama
