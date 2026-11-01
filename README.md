# SGX535-reMESA

**An open-source Mesa/Gallium 3D graphics driver for Intel Poulsbo (GMA 500 / PowerVR SGX535) on Linux.**

<table>
<tr>
<td width="115" valign="middle"><img src="assets/pipocahello.gif" width="100" alt="Pipoca saying hello"></td>
<td valign="middle">

**Introduction**

Hey! I'm Pipoca, the official SGX535-reMESA mascot.

Welcome to a project involving an old Atom processor, a mysterious GPU, far too much assembly, and a developer who apparently doesn't know when to give up.

</td>
</tr>
</table>

I started SGX535-reMESA on September 16, 2026. The idea was simple: figure out how the PowerVR SGX535 works and write an independent Mesa/Gallium driver for it.

The actual work? Not so simple.

There's very little public documentation for this GPU. I've spent most of my time digging through old Windows drivers, Intel EMGD, historical DDK material, disassembly, register definitions, and whatever I could learn by experimenting on the hardware itself.

Some experiments worked. Others sent me back to Ghidra for another few days.

But after weeks of fighting with this thing, **the GPU is finally rendering again.**

## What works so far

<img src="assets/pipocajumping.gif" align="right" width="105" alt="Pipoca jumping">

My test machine is a **Dell Inspiron Mini 12**, with an Intel Atom Z520 and a PowerVR SGX535 rev121.

Here's how things have gone so far:

- **October 6:** First triangle rendered by the actual GPU, followed by two indexed triangles forming a 16×16 square (I set off a firecracker)
- **October 21:** An OpenGL `glDrawArrays` call went through my Gallium implementation and reached the SGX535. The readback contained exactly 136 green pixels and 888 untouched pixels in a 32×32 target.
- **October 25:** I finally got the original `glxgears` running at the Mini 12's native 1280×800 resolution.
- **October 27:** A qualified run completed 600 frames with 600 real GPU submissions, averaging **16.1345 FPS** across six steady measurement windows.

Yes, that's about 16 FPS. On an Atom Z520 from 2008, I'll take it.

The latest run improved FPS by **11.29%** compared with the previous qualified snapshot path, which averaged 14.4983 FPS. That improvement includes several changes to the kernel path, memory handling and X11 presentation, so I can't credit any single optimization.

I've also qualified a restricted direct read-only GEM FD/XSHM presentation path. A separate six-operation physical test verified GPU COLOR, defined Z24 depth bits, the input images and the resulting X11 pixels.

The original application run finished without reported engine or presentation errors, queued work, legacy submissions or offline fallback.

**This is real GPU execution, but it isn't a finished OpenGL driver.**

The current results apply to a restricted, tested path. General application compatibility, longer continuous operation and OpenGL conformance are still work in progress.

For the exact measurements, test conditions and evidence, see the [October 27 physical results](docs/phase9/PHYSICAL-GLX-STATUS-20261027.md).

<br clear="all">

## Reproducing GLXGears

I've documented the reproduction process in the [GLXGears guide](docs/reproduction/GLXGEARS.md).

One important detail: the qualified 1280×800 run uses a specific kernel, build and private test bundle. Those artifacts aren't all published as a complete reproducible package yet.

**Cloning this repository alone won't reproduce the measured 16.1345 FPS.**

The guide explains what's available, what was used for the physical test and what still needs to be published.

## The historic Git commit — `0x07000345`

<img src="assets/pipocaohno.gif" align="right" width="110" alt="Pipoca reacting to 0x07000345">

Oh, this number.

If you've been following the project, you've probably seen me complain about `0x07000345`.

After roughly 26 days investigating that one DWORD, I decided to give it a permanent place in Git history.

Meet commit [**070003454e7f938661170abcc57a551ae68c931c**](https://github.com/gabrieelgama/SGX535-reMESA/commit/070003454e7f938661170abcc57a551ae68c931c).

Yes, I actually made a commit whose hash starts with `07000345`.

It contains my independently written Mesa implementation and the offline qualification work around it. The hash is just a tribute to the hexadecimal value that consumed an unreasonable amount of my life.

**ALWAYS.**

<br clear="all">

## Documentation

<img src="assets/pipocaconfering.gif" align="right" width="105" alt="Pipoca checking documentation">

Most of the technical material lives in [`docs/`](docs/).

I document failures and dead ends too. If a test only ran on the CPU, I say so. If a GPU result was physically verified, I keep the evidence. Those are very different things, especially when reverse engineering hardware with so little documentation.

For the early experiments, start here:

- [Triangle](docs/reproduction/TRIANGLE.md)
- [Square](docs/reproduction/SQUARE.md)
- [Display](docs/reproduction/DISPLAY.md)

For newer work, see the [October 27 physical results](docs/phase9/PHYSICAL-GLX-STATUS-20261027.md) and the [development update](docs/phase9/DEVELOPMENT-UPDATE.md). The [original triangle-era README](docs/history/README-TRIANGLE-07000345.md) is still around if you want to see where this started.

Passing hundreds of offline tests is useful. It doesn't mean the GPU ran anything. I try not to mix those up.

<br clear="all">

## OpenGL and future plans

<img src="assets/pipocarunningright.gif" align="right" width="105" alt="Pipoca running right">

The main target is **desktop OpenGL 2.1**. I'm investigating OpenGL 3.0 too, but I'm not promising support before I can actually prove it works.

My first application targets are **Minecraft Java**, Counter-Strike 1.6, SuperTuxKart and a few older browser games.

Minecraft is especially interesting. I once saw it average around 30 FPS using the original Windows 7 driver, and I'd love to find out what a new Linux implementation can do.

Before that, I need to expand beyond the current restricted rendering path, qualify longer runs and work toward **30–60 FPS**. General shader behavior, resource handling and application compatibility are still big jobs.

I'd also like to explore Wayland support eventually. KDE Plasma on an Atom Z520, though? That processor already struggles with five browser tabs.

I'm not expecting miracles. The GMA 500 wasn't fast when it was new. And please don't ask me to run GTA San Andreas on it. My cockatiel could probably do better.

The long-term goal is to support compatible 32-bit Intel hardware beyond my own Mini 12.

<br clear="all">

## Development setup

<img src="assets/pipocaidle.gif" align="right" width="105" alt="Pipoca resting">

Most of this driver is being developed on a **Samsung Galaxy Tab S7**. Yes, a tablet from 2020.

I run Linux through Termux/PRoot, cross-compile for i686 and then test the builds on the Mini 12.

The tablet was supposed to be a convenient development machine. Somehow it became my main computer. I haven't questioned that decision in a while.

<br clear="all">

## Reverse engineering the USSE compiler

<img src="assets/pipocathinking.gif" align="right" width="110" alt="Pipoca thinking about USSE instructions">

One of the biggest headaches has been figuring out how the original drivers encode USSE shader instructions.

That meant a lot of time in Ghidra, staring at disassembly and tracing old compiler routines. `0x07000345` was one of the clues that kept coming back.

I've recovered parts of the original compiler's behavior: register-field placement, MOV encoding, instruction finalization and some memory-management details. That work helped me build a restricted, independent USSE encoder and decoder instead of shipping proprietary shader binaries.

One finding: assembler flag bit 0 maps to bit 23 of the second encoded instruction word. I can demonstrate the mapping in CPU-side compiler logic. What that bit means during actual GPU execution is still unresolved.

I also traced a cleanup callback at offset `+0x630` through `_mesa_free` to the imported `free` function. The identity-MOV cleanup routine releases 11 referenced allocations plus the instruction node. Two other cleanup lists appear to handle recycled auxiliary records, though their ownership relationships aren't fully understood yet.

At one checkpoint, the independent codec passed **447 offline tests** in each of three environments: native Linux, UBSan and i386/QEMU.

Since then, I've worked on PDS encoding, program storage, relocations, shader preparation and physical execution. There are still plenty of unanswered questions about register banks, termination, PDS fetches and instruction encodings.

I'm not going to pretend I've decoded the whole architecture. I just want to understand enough of it to keep moving forward, and document what I find along the way.

And yes, `0x07000345` still annoys me.

<br clear="all">

## Contributing

<img src="assets/pipocawaitingyou.gif" align="right" width="110" alt="Pipoca waiting for contributors">

Got old Poulsbo hardware, historical Intel drivers, SGX535 documentation, DDK references or useful test results? Feel free to open an issue.

Testing on other compatible machines would be especially helpful. I'd like this driver to work beyond the one Mini 12 sitting on my desk.

The code is intended to remain independently implemented. Historical proprietary software can help explain hardware behavior, but I don't want to incorporate code with unclear redistribution rights.

<br clear="all">

## Thanks

Thanks to **Simon Fenney** for pointing me toward Intel EMGD. That turned out to be extremely useful.

Thanks to **e1zo** and **Alex (Poulsback)** for helping with parts of the driver!

Thank you **Garnet (SGX540reversing)** for USSE disassembler! This helped a lot😀

Thanks to **monocasa (Reddit)** and **nicman24 (Reddit)** for the optimization tips! 

Thank you **intelminer (Reddit)** for liking the photos and GIFs of my cockatiel! :D

Thank you to my Cockatiel too😂

And thanks to everyone who has read the documentation, shared a reference, tested something or watched me try to convince a nearly two-decade-old GPU to draw triangles again :D

## Special thanks

<img src="assets/pipocajumping.gif" align="right" width="110" alt="Pipoca celebrating">

And finally, a special thanks to `0x07000345`.

I hate you.

Twenty days of reverse engineering, countless solver runs and one mysterious hexadecimal value.

Somehow, all of that led me to `Xpsb_emit_pixel_shader` and a much better understanding of the compiler around it.

And after all that...

**ALWAYS.**

Maybe the real `0x07000345` was the friends we made along the way 🥀

Fun fact: one of my important discoveries about this value wasn't even made on the Tab S7. I was using my phone. Apparently, the final boss wasn't prepared for that. 😂💀

<br clear="all">

## One last thing

<img src="assets/pipocarunningleft.gif" align="right" width="110" alt="Pipoca running left">

This started as an attempt to understand a forgotten GPU. Somehow it turned into one of the most interesting projects I've ever worked on.

I'll keep documenting the progress, including the failures, weird discoveries and occasional hexadecimal-induced breakdowns.

**The goal is serious. The documentation doesn't always have to be.**

And yes, I'll keep the cockatiel GIFs.

— Gama
