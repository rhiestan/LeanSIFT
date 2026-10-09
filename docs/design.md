# LeanSift design

This document explains how LeanSift reproduces VLFeat's SIFT and where its speed
comes from. Source files: `src/sift.cpp` (pipeline), `src/kernels.cpp` (SIMD kernels),
`src/executor.cpp` (thread pool).

## Reference: VLFeat's SIFT

LeanSift follows VLFeat's `vl/sift.c` step by step, with the same parameters:

| Step | VLFeat function | Notes |
|---|---|---|
| Base image | `vl_sift_process_first_octave` | Optional doubling by linear interpolation (rows, then columns), then smoothing from the nominal blur (0.5) to 1.6 |
| Gaussian levels | `_vl_sift_smooth` | S + 3 levels per octave (s = -1..S+1), each blurring the previous one; kernel truncated at 4 sigma; replicated borders |
| Next octave | `vl_sift_process_next_octave` | Level s = S - 1 subsampled by two |
| Extrema | `vl_sift_detect` | Strict 26-neighbour extrema of the DoG with \|D\| >= 0.8 peak threshold, levels s = 0..S-1 |
| Refinement | `vl_sift_detect` | Up to 5 Newton steps in double precision, peak and edge thresholds |
| Orientation | `vl_sift_calc_keypoint_orientations` | 36-bin histogram with linear interpolation, Gaussian window 1.5 sigma, six box smoothings, peaks above 80 %, up to 4 |
| Descriptor | `vl_sift_calc_keypoint_descriptor` | 4x4x8 trilinear histogram, window sigma 2 bins, magnification 3, normalize / clamp 0.2 / normalize |

Gradients use VLFeat's formulas: central differences inside the image, one-sided
differences on the border, and VLFeat's rational `atan2` approximation (error below
0.005 rad), so orientations match VLFeat's closely.

Deliberate differences, all within floating-point noise:

- Accurate `exp` and `sqrt` instead of VLFeat's table and Newton approximations.
- Separable Gaussian windows: exp(-(dx^2 + dy^2) c) = exp(-dx^2 c) exp(-dy^2 c),
  computed with a recurrence instead of one `exp` per pixel.
- Single-precision accumulation of the orientation histogram (VLFeat uses double).
- Correct image doubling for `first_octave < -1`; VLFeat swaps width and height
  there, which corrupts non-square images.

The tests (`tests/test_vlfeat_agreement.cpp`) run VLFeat and LeanSift on the same
images with several parameter sets and require 99 % of the keypoints to match
(in practice 99.8 % to 100 %), a median descriptor distance below 0.001 and a 99th
percentile below 0.01 (in practice about 0.0003). `tests/test_matching.cpp` checks that
matching a rotated and scaled copy of an image is as accurate as with VLFeat.

## Where the time goes

On one core, VLFeat spends roughly half its time building the scale space and half
on orientations and descriptors. Its octave buffers (S + 3 Gaussian levels, S + 2 DoG
levels and 2 S gradient images of the full octave size) are far larger than the
caches, so each stage streams whole images through memory. With several threads,
memory bandwidth is the limit long before the cores are.

LeanSift's design follows from that.

### Streaming scale space (tiles and ring buffers)

An octave is processed in one pass per tile. A tile is about 1024 columns wide and
tall enough to give each thread about two tiles. For each tile, the pipeline keeps:

- one ring buffer of rows per Gaussian level, holding only the rows still needed by
  the next level's blur and by the detector;
- ring buffers of DoG rows around the current detection row.

Rows are produced on demand: detecting extrema on row y needs every level up to row
y + 7 (the detector's 3x3x3 neighbourhood plus the refinement, which can move a
keypoint by up to four pixels), which pulls the rows each level needs from the level
below, down to the (optionally doubled) input. The doubling itself is also done row by
row. While the rows are in cache the pass also

- subtracts adjacent levels into the DoG rings,
- finds the extrema and refines them (the refinement reads the DoG rings),
- writes the next octave's base image (level S - 1 subsampled),
- copies the tile's part of levels s = 0..S-1 to full-size planes with non-temporal
  stores; orientations and descriptors read these later.

Tiles overlap: each level is computed for the tile plus a margin (the sum of the blur
radii above it plus the refinement margin, about 50 pixels at the base level with the
default parameters) so tiles are independent. Tiles are at least twice the margin in
each direction, which keeps the recomputation small. The detections of the tiles are
merged in VLFeat's order (level, row, column), so the output does not depend on the
number of threads.

Compared with VLFeat's layout this removes most memory traffic: per octave only the
S kept levels and the next base image are written, instead of about 4 S + 5 full
images.

### Orientations and descriptors from the Gaussian levels

Storing gradient magnitude and angle (two floats per pixel per level, as VLFeat does)
doubles the memory traffic of the levels, and descriptors read it back from DRAM.
LeanSift stores the Gaussian levels instead (one float per pixel) and computes the
gradients of each window row on the fly with SIMD (square root and VLFeat's atan2
approximation). The orientation and descriptor kernels then work row by row:

1. compute gradients for the part of the row inside the window (for descriptors,
   the part inside the rotated 5 x 5 bin square, so pixels that cannot contribute
   are skipped);
2. compute, with SIMD, each sample's cell, angle bins and interpolation weights;
3. accumulate the samples. For descriptors, successive samples mostly fall into the
   same 2 x 2 block of spatial cells, so the four 8-bin angle histograms of that block
   stay in SIMD registers (one AVX2 vector each) and go to memory only when the block
   changes; a sample costs one one-hot table lookup and four FMAs. Targets with less
   than 8 floats per vector use scalar accumulation.

Orientations accumulate into four interleaved histogram copies so that successive
samples do not wait on each other.

### SIMD kernels

All per-pixel work is in `src/kernels.cpp`, written once with Highway and compiled for
every x86 and Arm instruction set Highway supports; `HWY_DYNAMIC_DISPATCH` picks the
best at run time. Highlights:

- **Gaussian blur:** separable, using the kernel's symmetry (one multiply per pair of
  taps), four vectors per step to amortise loads of row pointers and coefficients,
  and replicated borders written into a padded row so the inner loops have no
  checks.
- **Extrema:** vectors of 8 or 16 pixels are compared against the threshold first,
  then against the 8 neighbours on their level, then the 9 on each adjacent level,
  leaving early when no lane can still be an extremum.
- **Upsampling, subsampling, DoG, input conversion:** straightforward vector loops.

Rows are padded to a multiple of 16 floats with guard space around every buffer, so
vectors can run past the ends of rows without masking.

### Threads

`lsift::Executor` has a single operation, `parallel_for(n, grain, fn)`. The default
`ThreadPool` uses a shared atomic counter for dynamic load balancing (keypoints differ
a lot in cost). Threads spin for up to 10 ms before sleeping, because the pipeline's
phases follow each other quickly and a sleeping core takes milliseconds to wake up and
raise its clock. A job is complete when all its items are done; the owner never waits
for workers that have not started, so a worker that wakes up late cannot delay it.

## Research used

The `Papers` folder held three papers on parallel SIFT. What each contributed:

**Wu, Wu, Tan, Sun: "Speeding up SIFT algorithm by multi-core processor supporting
SIMD instruction sets" (CAD/Graphics 2013).** OpenMP and SIMD SIFT on Xeon. Two ideas:
(1) a vectorized Gaussian blur that first pads the image by replicating border pixels
so that every pixel is processed alike, giving 8-11x over the plain loop; (2) keypoint
work is unbalanced, and its cost follows the keypoint scale, so they split keypoints
by the sum of sigma instead of by count. *Used:* border replication into a padded
buffer in the blur kernel; dynamic scheduling of keypoints in small chunks (which
balances load without needing a cost model).

**Le Tran Su, Jong-Soo Lee: "Novel Parallel Approach for SIFT Algorithm
Implementation" (JICCE 2013).** SIFT on a simulated SIMD pixel-processor array, with
4x4 "block filtering" that exploits the symmetry of the Gaussian kernel. Their profile
of Lowe's code shows the scale space as the largest single stage (about 45 %),
followed by orientation and descriptor (about 25 % each). *Used:* the symmetric-kernel
idea (pairs of taps share a multiply). The hardware-specific parts do not apply to
CPUs.

**Feng, Li, Chen, Zhang: "Parallelization and Characterization of SIFT on Multi-Core
Systems" (Intel, 2008).** OpenMP parallelisation, SIMD (1.7-1.85x) and cache
optimisations (20-25 %) on 16 cores, plus a detailed analysis. Findings: load
imbalance limits scaling, especially in detection where keypoint counts vary between
scales; barriers between the row and column passes of the blur cost time; the
memory-bound steps (subtraction, resampling) have burst bandwidth demand that stops
them from scaling beyond 8 threads; per-frame working sets are 12-32 MB. *Used:* this
is the reason for the streaming tile design (no barrier between blur passes, no
full-size DoG or intermediate images, working set per tile in the low megabytes),
dynamic scheduling of keypoints, padding to avoid false sharing, and fine-grained
tasks for detection (tiles rather than scales).

## Possible next steps

- Overlap an octave's orientations and descriptors with the next octave's scale space
  to fill the gaps between phases (needs double-buffered kept levels).
- Blur: compute several output rows per pass over the input rows to cut cache traffic
  in the vertical pass.
- Store the kept levels in 16-bit fixed point to halve their memory traffic (a small
  deviation from VLFeat).
- 8-float register accumulation for 128-bit targets (two vectors per cell) for NEON.
- A GPU backend behind the same API.
