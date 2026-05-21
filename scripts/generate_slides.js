// EC7207 Group 48 — Project Presentation Generator
// Produces EC7207_Presentation.pptx (~12 slides, 6–8 min talk).
// Run with: node scripts/generate_slides.js

const pptxgen = require("pptxgenjs");

const OUT_PATH = "EC7207_Presentation.pptx";

// --- Palette (Ocean Gradient) -----------------------------------------------
const C = {
  primary:   "065A82",  // deep ocean blue
  secondary: "1C7293",  // teal
  dark:      "21295C",  // midnight (for dark backgrounds)
  light:     "F1F5F9",  // near-white wash
  card:      "FFFFFF",  // card surface
  muted:     "64748B",  // captions
  text:      "1E293B",  // body text
  accent:    "F59E0B",  // amber accent for key numbers
  good:      "10B981",  // green (good results)
  warn:      "EF4444",  // red (issues)
  border:    "CBD5E1",  // hairline border
};

const F = { head: "Cambria", body: "Calibri" };

// --- Pres -------------------------------------------------------------------
const pres = new pptxgen();
pres.layout = "LAYOUT_WIDE";          // 13.3 x 7.5
pres.title  = "EC7207 Group 48 — Tsunami HPC";
pres.author = "Group 48 (Surasinghe, Tamasha, Tharshihan)";

const W = 13.3, H = 7.5;

// helper to add a footer bar to content slides
function footer(slide, pageLabel) {
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 0, y: H - 0.4, w: W, h: 0.4, fill: { color: C.dark }, line: { color: C.dark },
  });
  slide.addText("EC7207 · Group 48 · Tsunami HPC Simulation", {
    x: 0.5, y: H - 0.4, w: 9, h: 0.4,
    fontFace: F.body, fontSize: 10, color: "CADCFC", valign: "middle", margin: 0,
  });
  slide.addText(pageLabel, {
    x: W - 1.0, y: H - 0.4, w: 0.5, h: 0.4,
    fontFace: F.body, fontSize: 10, color: "CADCFC", valign: "middle", align: "right", margin: 0,
  });
}

function sectionTitle(slide, title, subtitle) {
  // thick left bar
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 0.5, y: 0.5, w: 0.12, h: 0.9, fill: { color: C.primary }, line: { color: C.primary },
  });
  slide.addText(title, {
    x: 0.8, y: 0.4, w: W - 1.6, h: 0.65,
    fontFace: F.head, fontSize: 30, bold: true, color: C.text, valign: "middle", margin: 0,
  });
  if (subtitle) {
    slide.addText(subtitle, {
      x: 0.8, y: 1.0, w: W - 1.6, h: 0.4,
      fontFace: F.body, fontSize: 14, color: C.muted, valign: "top", margin: 0,
    });
  }
}

// =============================================================================
// SLIDE 1 — Title (dark)
// =============================================================================
{
  const s = pres.addSlide();
  s.background = { color: C.dark };

  // decorative bar (left)
  s.addShape(pres.shapes.RECTANGLE, {
    x: 0, y: 0, w: 0.35, h: H, fill: { color: C.primary }, line: { color: C.primary },
  });

  s.addText("EC7207 — High Performance Computing", {
    x: 1.0, y: 1.2, w: 11, h: 0.5,
    fontFace: F.body, fontSize: 16, color: "CADCFC", margin: 0, charSpacing: 4,
  });

  s.addText("High-Performance Simulation of\nTsunami Wave Propagation", {
    x: 1.0, y: 1.9, w: 11, h: 2.0,
    fontFace: F.head, fontSize: 44, bold: true, color: "FFFFFF", margin: 0,
  });

  s.addText("Using Parallel Computing Models — Serial, OpenMP, MPI, Hybrid", {
    x: 1.0, y: 3.9, w: 11, h: 0.5,
    fontFace: F.body, fontSize: 18, italic: true, color: "97BC62", margin: 0,
  });

  // divider line
  s.addShape(pres.shapes.LINE, {
    x: 1.0, y: 4.7, w: 4, h: 0,
    line: { color: C.accent, width: 3 },
  });

  s.addText([
    { text: "Group 48",                                              options: { bold: true, breakLine: true } },
    { text: "EG/2021/4820  SURASINGHE R.L.D.T.H.",                   options: { breakLine: true } },
    { text: "EG/2021/4823  TAMASHA A.P.D.",                          options: { breakLine: true } },
    { text: "EG/2021/4825  THARSHIHAN R.G",                          options: {} },
  ], {
    x: 1.0, y: 5.0, w: 11, h: 1.7,
    fontFace: F.body, fontSize: 16, color: "FFFFFF",
  });
}

// =============================================================================
// SLIDE 2 — Problem Statement
// =============================================================================
{
  const s = pres.addSlide();
  s.background = { color: C.light };

  sectionTitle(s, "The Problem", "Why tsunami simulation is computationally hard");

  // left: text
  s.addText([
    { text: "Tsunamis are dangerous.", options: { bold: true, fontSize: 18, color: C.primary, breakLine: true, paraSpaceAfter: 8 } },
    { text: "Accurate prediction saves lives, but high-resolution simulation is computationally expensive.", options: { fontSize: 14, color: C.text, breakLine: true, paraSpaceAfter: 16 } },

    { text: "The bottleneck.", options: { bold: true, fontSize: 18, color: C.primary, breakLine: true, paraSpaceAfter: 8 } },
    { text: "A 1000×1000 ocean grid = 1 million cells, each updated every timestep across thousands of timesteps. Serial execution doesn't finish in useful time.", options: { fontSize: 14, color: C.text, breakLine: true, paraSpaceAfter: 16 } },

    { text: "Our approach.", options: { bold: true, fontSize: 18, color: C.primary, breakLine: true, paraSpaceAfter: 8 } },
    { text: "Decompose the grid across CPU threads (OpenMP), processes (MPI), and both (Hybrid). Measure the speedup. Compare against a validated serial baseline.", options: { fontSize: 14, color: C.text } },
  ], {
    x: 0.8, y: 1.7, w: 6.4, h: 5.0,
    fontFace: F.body, color: C.text, valign: "top",
  });

  // right: stat callouts
  const cardX = 7.6, cardY = 1.7, cardW = 4.9, cardH = 1.55, gap = 0.2;

  const callouts = [
    { num: "1M",     label: "grid cells per timestep" },
    { num: "2000",   label: "timesteps per simulation" },
    { num: "4",      label: "parallel implementations compared" },
  ];

  callouts.forEach((c, i) => {
    const y = cardY + i * (cardH + gap);
    s.addShape(pres.shapes.RECTANGLE, {
      x: cardX, y, w: cardW, h: cardH,
      fill: { color: C.card }, line: { color: C.border, width: 0.5 },
      shadow: { type: "outer", color: "000000", blur: 6, offset: 2, angle: 135, opacity: 0.08 },
    });
    s.addShape(pres.shapes.RECTANGLE, {
      x: cardX, y, w: 0.1, h: cardH, fill: { color: C.primary }, line: { color: C.primary },
    });
    s.addText(c.num, {
      x: cardX + 0.3, y: y + 0.15, w: cardW - 0.5, h: 0.9,
      fontFace: F.head, fontSize: 44, bold: true, color: C.primary, valign: "middle", margin: 0,
    });
    s.addText(c.label, {
      x: cardX + 0.3, y: y + 1.0, w: cardW - 0.5, h: 0.45,
      fontFace: F.body, fontSize: 12, color: C.muted, valign: "middle", margin: 0,
    });
  });

  footer(s, "2");
}

// =============================================================================
// SLIDE 3 — Mathematical Model
// =============================================================================
{
  const s = pres.addSlide();
  s.background = { color: C.light };

  sectionTitle(s, "Mathematical Model", "Linear surrogate of the Shallow Water Equations");

  // formula box
  s.addShape(pres.shapes.RECTANGLE, {
    x: 0.8, y: 1.85, w: 7.5, h: 1.5,
    fill: { color: C.dark }, line: { color: C.dark },
  });
  s.addText("2D Linear Wave Equation", {
    x: 1.0, y: 1.95, w: 7.1, h: 0.4,
    fontFace: F.body, fontSize: 12, color: "CADCFC", margin: 0, charSpacing: 3,
  });
  s.addText("∂²h / ∂t² = c² ⋅ ∇²h", {
    x: 1.0, y: 2.35, w: 7.1, h: 0.8,
    fontFace: "Cambria Math", fontSize: 36, bold: true, color: "FFFFFF",
    italic: true, align: "left", valign: "middle", margin: 0,
  });

  // narrative under formula
  s.addText([
    { text: "where ", options: {} },
    { text: "h(x, y, t)", options: { italic: true, bold: true, color: C.primary } },
    { text: " = water surface height, and ", options: {} },
    { text: "c = √(gH)", options: { italic: true, bold: true, color: C.primary } },
    { text: " is the tsunami celerity (g = gravity, H = mean depth).", options: {} },
  ], {
    x: 0.8, y: 3.5, w: 7.5, h: 0.6,
    fontFace: F.body, fontSize: 13, color: C.text, valign: "top",
  });

  s.addText("This is the small-amplitude / constant-depth linearization of the Shallow Water Equations — the standard model for the deep-ocean phase of a tsunami, before it reaches the continental shelf.", {
    x: 0.8, y: 4.15, w: 7.5, h: 1.2,
    fontFace: F.body, fontSize: 13, color: C.text, valign: "top", italic: true,
  });

  // right: references
  s.addShape(pres.shapes.RECTANGLE, {
    x: 8.6, y: 1.85, w: 3.9, h: 5.0,
    fill: { color: C.card }, line: { color: C.border, width: 0.5 },
  });
  s.addShape(pres.shapes.RECTANGLE, {
    x: 8.6, y: 1.85, w: 3.9, h: 0.45, fill: { color: C.primary }, line: { color: C.primary },
  });
  s.addText("PUBLISHED REFERENCES", {
    x: 8.75, y: 1.85, w: 3.7, h: 0.45,
    fontFace: F.body, fontSize: 11, bold: true, color: "FFFFFF", valign: "middle", charSpacing: 4, margin: 0,
  });

  s.addText([
    { text: "Pedlosky, J. (1987)", options: { bold: true, fontSize: 12, color: C.text, breakLine: true } },
    { text: "Geophysical Fluid Dynamics, 2nd ed., Springer, §3.9 — derivation of the linear wave equation as the linearization of the SWE.", options: { fontSize: 10, color: C.muted, breakLine: true, paraSpaceAfter: 10 } },

    { text: "LeVeque, R. J. (2002)", options: { bold: true, fontSize: 12, color: C.text, breakLine: true } },
    { text: "Finite Volume Methods for Hyperbolic Problems, Cambridge — Ch. 13 covers SWE and its linear surrogates.", options: { fontSize: 10, color: C.muted, breakLine: true, paraSpaceAfter: 10 } },

    { text: "Titov & Synolakis (1998)", options: { bold: true, fontSize: 12, color: C.text, breakLine: true } },
    { text: "Numerical modeling of tidal wave runup. J. Waterway/Port/Coastal/Ocean Eng., 124(4) — MOST tsunami model.", options: { fontSize: 10, color: C.muted, breakLine: true, paraSpaceAfter: 10 } },

    { text: "Whitham, G. B. (1974)", options: { bold: true, fontSize: 12, color: C.text, breakLine: true } },
    { text: "Linear and Nonlinear Waves, Wiley — §1.2 derives the wave equation from conservation principles.", options: { fontSize: 10, color: C.muted } },
  ], {
    x: 8.85, y: 2.5, w: 3.5, h: 4.2,
    fontFace: F.body, valign: "top",
  });

  footer(s, "3");
}

// =============================================================================
// SLIDE 4 — Discretization (Numerical Scheme)
// =============================================================================
{
  const s = pres.addSlide();
  s.background = { color: C.light };

  sectionTitle(s, "Numerical Scheme", "Leap-frog in time, 5-point Laplacian in space");

  // big formula box
  s.addShape(pres.shapes.RECTANGLE, {
    x: 0.8, y: 1.85, w: W - 1.6, h: 1.7,
    fill: { color: C.dark }, line: { color: C.dark },
  });
  s.addText("EXPLICIT UPDATE RULE  (second-order accurate)", {
    x: 1.0, y: 1.95, w: W - 2.0, h: 0.35,
    fontFace: F.body, fontSize: 11, color: "CADCFC", charSpacing: 4, margin: 0,
  });
  s.addText([
    { text: "h", options: { italic: true } },
    { text: "i,j", options: { subscript: true } },
    { text: "  ", options: {} },
    { text: "n+1", options: { superscript: true } },
    { text: "  =  2 h", options: { italic: true } },
    { text: "i,j", options: { subscript: true } },
    { text: "  ", options: {} },
    { text: "n", options: { superscript: true } },
    { text: "  −  h", options: { italic: true } },
    { text: "i,j", options: { subscript: true } },
    { text: "  ", options: {} },
    { text: "n−1", options: { superscript: true } },
    { text: "  +  (c⋅Δt / Δx)²  ⋅  [ h", options: { italic: true } },
    { text: "i+1,j", options: { subscript: true } },
    { text: "  +  h", options: { italic: true } },
    { text: "i−1,j", options: { subscript: true } },
    { text: "  +  h", options: { italic: true } },
    { text: "i,j+1", options: { subscript: true } },
    { text: "  +  h", options: { italic: true } },
    { text: "i,j−1", options: { subscript: true } },
    { text: "  −  4 h", options: { italic: true } },
    { text: "i,j", options: { subscript: true } },
    { text: "  ]", options: { italic: true } },
  ], {
    x: 1.0, y: 2.4, w: W - 2.0, h: 0.95,
    fontFace: "Cambria Math", fontSize: 18, bold: true, color: "FFFFFF",
    valign: "middle", margin: 0,
  });

  // 3 info cards: accuracy / stability / scheme
  const cardY = 3.95, cardW = (W - 1.6 - 0.6) / 3, cardH = 2.6, gap = 0.3;
  const cards = [
    {
      head: "Accuracy",
      body: "Second-order in both space and time:\n   O(Δt² + Δx²)",
      tag:  "Central differences",
    },
    {
      head: "Stability (CFL)",
      body: "c⋅Δt / Δx  ≤  1/√2\n   ≈  0.707\n\nOur value: 0.50  ✓ stable",
      tag:  "Courant, Friedrichs, Lewy (1928)",
    },
    {
      head: "References",
      body: "Strikwerda (2004) Ch. 12\nLeVeque (2007) §10.2\nPress et al., Numerical Recipes §20.1\nCourant–Friedrichs–Lewy (1928) — original CFL paper",
      tag:  "Foundational since 1928",
    },
  ];

  cards.forEach((c, i) => {
    const x = 0.8 + i * (cardW + gap);
    s.addShape(pres.shapes.RECTANGLE, {
      x, y: cardY, w: cardW, h: cardH,
      fill: { color: C.card }, line: { color: C.border, width: 0.5 },
      shadow: { type: "outer", color: "000000", blur: 6, offset: 2, angle: 135, opacity: 0.08 },
    });
    s.addShape(pres.shapes.RECTANGLE, {
      x, y: cardY, w: cardW, h: 0.08, fill: { color: C.primary }, line: { color: C.primary },
    });
    s.addText(c.head, {
      x: x + 0.2, y: cardY + 0.2, w: cardW - 0.4, h: 0.45,
      fontFace: F.head, fontSize: 17, bold: true, color: C.primary, margin: 0,
    });
    s.addText(c.body, {
      x: x + 0.2, y: cardY + 0.75, w: cardW - 0.4, h: cardH - 1.3,
      fontFace: F.body, fontSize: 13, color: C.text, valign: "top",
    });
    s.addText(c.tag, {
      x: x + 0.2, y: cardY + cardH - 0.45, w: cardW - 0.4, h: 0.3,
      fontFace: F.body, fontSize: 10, color: C.muted, italic: true, margin: 0,
    });
  });

  footer(s, "4");
}

// =============================================================================
// SLIDE 5 — Methodology Overview (4 implementations)
// =============================================================================
{
  const s = pres.addSlide();
  s.background = { color: C.light };

  sectionTitle(s, "Four Parallel Implementations", "Same equation. Same wave. Different hardware utilization.");

  const impls = [
    {
      title: "Serial",
      sub:   "Single-core baseline",
      body:  "Standard nested loop over the 2D grid. Reflective boundaries. Used as the ground-truth reference for accuracy.",
      color: C.muted,
    },
    {
      title: "OpenMP",
      sub:   "Shared memory threads",
      body:  "#pragma omp parallel for collapse(2) across the y/x loops. Thread count varied from 1 to 8.",
      color: C.secondary,
    },
    {
      title: "MPI",
      sub:   "Distributed processes",
      body:  "Grid split into horizontal slices. Each process exchanges halo rows with its neighbours using MPI_Isend / MPI_Irecv (non-blocking).",
      color: C.primary,
    },
    {
      title: "Hybrid",
      sub:   "MPI + OpenMP",
      body:  "MPI ranks distribute grid slices across processes; OpenMP threads parallelize within each rank. Two-level parallelism.",
      color: C.accent,
    },
  ];

  const cardY = 1.85, cardH = 5.0, gap = 0.25;
  const cardW = (W - 1.6 - 3 * gap) / 4;

  impls.forEach((im, i) => {
    const x = 0.8 + i * (cardW + gap);
    s.addShape(pres.shapes.RECTANGLE, {
      x, y: cardY, w: cardW, h: cardH,
      fill: { color: C.card }, line: { color: C.border, width: 0.5 },
      shadow: { type: "outer", color: "000000", blur: 6, offset: 2, angle: 135, opacity: 0.08 },
    });
    // colored top header
    s.addShape(pres.shapes.RECTANGLE, {
      x, y: cardY, w: cardW, h: 1.0, fill: { color: im.color }, line: { color: im.color },
    });
    s.addText(im.title, {
      x: x + 0.2, y: cardY + 0.15, w: cardW - 0.4, h: 0.5,
      fontFace: F.head, fontSize: 22, bold: true, color: "FFFFFF", margin: 0,
    });
    s.addText(im.sub, {
      x: x + 0.2, y: cardY + 0.6, w: cardW - 0.4, h: 0.35,
      fontFace: F.body, fontSize: 11, color: "FFFFFF", margin: 0, charSpacing: 3,
    });
    s.addText(im.body, {
      x: x + 0.2, y: cardY + 1.2, w: cardW - 0.4, h: cardH - 1.5,
      fontFace: F.body, fontSize: 12, color: C.text, valign: "top",
    });
  });

  footer(s, "5");
}

// =============================================================================
// SLIDE 6 — Implementation Details (compile + run table)
// =============================================================================
{
  const s = pres.addSlide();
  s.background = { color: C.light };

  sectionTitle(s, "Build & Execution", "How each implementation is compiled and launched");

  const headerOpts = {
    fill: { color: C.primary }, color: "FFFFFF", bold: true,
    fontSize: 13, fontFace: F.body, align: "left", valign: "middle",
  };
  const cellOpts = {
    fontSize: 12, fontFace: F.body, color: C.text, valign: "middle",
  };
  const codeOpts = {
    fontSize: 11, fontFace: "Consolas", color: C.text, valign: "middle",
  };

  const rows = [
    [
      { text: "Implementation", options: headerOpts },
      { text: "Compile command", options: headerOpts },
      { text: "Run command", options: headerOpts },
    ],
    [
      { text: "Serial", options: { ...cellOpts, bold: true } },
      { text: "g++ -O3 -o tsunami_serial tsunami_serial.cpp", options: codeOpts },
      { text: "./tsunami_serial", options: codeOpts },
    ],
    [
      { text: "OpenMP", options: { ...cellOpts, bold: true } },
      { text: "g++ -O3 -fopenmp -o tsunami_omp tsunami_omp.cpp", options: codeOpts },
      { text: "./tsunami_omp <N_threads>", options: codeOpts },
    ],
    [
      { text: "MPI", options: { ...cellOpts, bold: true } },
      { text: "mpic++ -O3 -o tsunami_mpi tsunami_mpi.cpp", options: codeOpts },
      { text: "mpiexec -n <N_procs> ./tsunami_mpi", options: codeOpts },
    ],
    [
      { text: "Hybrid", options: { ...cellOpts, bold: true } },
      { text: "mpic++ -O3 -fopenmp -o tsunami_hybrid tsunami_hybrid.cpp", options: codeOpts },
      { text: "mpiexec -n <P> ./tsunami_hybrid <T>", options: codeOpts },
    ],
  ];

  s.addTable(rows, {
    x: 0.8, y: 2.0, w: W - 1.6,
    colW: [2.0, 5.6, 4.1],
    rowH: 0.7,
    border: { pt: 1, color: C.border },
    fill: { color: C.card },
  });

  // commentary block
  s.addShape(pres.shapes.RECTANGLE, {
    x: 0.8, y: 5.6, w: W - 1.6, h: 1.3,
    fill: { color: C.card }, line: { color: C.border, width: 0.5 },
  });
  s.addShape(pres.shapes.RECTANGLE, {
    x: 0.8, y: 5.6, w: 0.1, h: 1.3, fill: { color: C.accent }, line: { color: C.accent },
  });
  s.addText([
    { text: "Configuration we used for benchmarking. ", options: { bold: true, color: C.text } },
    { text: "Grid 1000×1000, 2000 timesteps, Δt = 0.0005, c = 1.0, Courant = 0.50 (stable). ", options: { color: C.text } },
    { text: "Run on a 4-physical-core x86 CPU (8 logical with hyperthreading) under WSL2 / Ubuntu / OpenMPI 4.1.6 / GCC 13.3.", options: { italic: true, color: C.muted } },
  ], {
    x: 1.0, y: 5.7, w: W - 2.0, h: 1.1,
    fontFace: F.body, fontSize: 12, valign: "middle",
  });

  footer(s, "6");
}

// =============================================================================
// SLIDE 7 — Live Demo segue (dark)
// =============================================================================
{
  const s = pres.addSlide();
  s.background = { color: C.dark };

  // left accent
  s.addShape(pres.shapes.RECTANGLE, {
    x: 0, y: 0, w: 0.35, h: H, fill: { color: C.accent }, line: { color: C.accent },
  });

  s.addText("LIVE DEMO", {
    x: 1.0, y: 1.8, w: 11, h: 0.6,
    fontFace: F.body, fontSize: 16, color: C.accent, charSpacing: 8, margin: 0,
  });

  s.addText("Compile. Run. Visualise.", {
    x: 1.0, y: 2.4, w: 11, h: 1.4,
    fontFace: F.head, fontSize: 56, bold: true, color: "FFFFFF", margin: 0,
  });

  s.addShape(pres.shapes.LINE, {
    x: 1.0, y: 4.1, w: 4, h: 0,
    line: { color: C.accent, width: 3 },
  });

  s.addText([
    { text: "1.  ", options: { color: C.accent, bold: true } },
    { text: "Compile all four implementations side-by-side.", options: { color: "FFFFFF", breakLine: true } },
    { text: "2.  ", options: { color: C.accent, bold: true } },
    { text: "Run each — observe execution time printed live.", options: { color: "FFFFFF", breakLine: true } },
    { text: "3.  ", options: { color: C.accent, bold: true } },
    { text: "Open dashboard — wave animation + full scaling study.", options: { color: "FFFFFF" } },
  ], {
    x: 1.0, y: 4.4, w: 11, h: 2.0,
    fontFace: F.body, fontSize: 20, paraSpaceAfter: 8,
  });
}

// =============================================================================
// SLIDE 8 — Results: Execution Times (bar chart)
// =============================================================================
{
  const s = pres.addSlide();
  s.background = { color: C.light };

  sectionTitle(s, "Results — Execution Time", "Measured wall-clock seconds, N = 1000, 2000 timesteps");

  // bar chart
  const labels = [
    "Serial",
    "OMP 1t", "OMP 2t", "OMP 4t", "OMP 8t",
    "MPI 1p", "MPI 2p", "MPI 4p", "MPI 8p",
    "Hyb 2×2", "Hyb 2×4", "Hyb 4×2",
  ];
  const values = [
    4.27,
    5.64, 4.06, 4.12, 5.48,
    4.51, 3.91, 4.13, 4.85,
    4.11, 4.31, 4.58,
  ];

  s.addChart(pres.charts.BAR, [{
    name: "Execution time (s)",
    labels, values,
  }], {
    x: 0.8, y: 1.85, w: 8.5, h: 5.1,
    barDir: "col",
    chartColors: [C.primary],
    chartArea: { fill: { color: C.card }, roundedCorners: false },
    plotArea:  { fill: { color: C.card } },
    catAxisLabelColor: C.text, catAxisLabelFontFace: F.body, catAxisLabelFontSize: 10,
    valAxisLabelColor: C.muted, valAxisLabelFontFace: F.body, valAxisLabelFontSize: 10,
    valGridLine: { color: C.border, size: 0.5 },
    catGridLine: { style: "none" },
    showValue: true,
    dataLabelPosition: "outEnd",
    dataLabelColor: C.text,
    dataLabelFontFace: F.body,
    dataLabelFontSize: 9,
    dataLabelFormatCode: "0.00",
    showLegend: false,
    showTitle: false,
  });

  // takeaways panel
  s.addShape(pres.shapes.RECTANGLE, {
    x: 9.6, y: 1.85, w: 2.9, h: 5.1,
    fill: { color: C.card }, line: { color: C.border, width: 0.5 },
  });
  s.addShape(pres.shapes.RECTANGLE, {
    x: 9.6, y: 1.85, w: 2.9, h: 0.5, fill: { color: C.primary }, line: { color: C.primary },
  });
  s.addText("KEY READINGS", {
    x: 9.75, y: 1.85, w: 2.7, h: 0.5,
    fontFace: F.body, fontSize: 11, bold: true, color: "FFFFFF", valign: "middle", charSpacing: 4, margin: 0,
  });

  s.addText([
    { text: "Baseline", options: { bold: true, fontSize: 11, color: C.muted, charSpacing: 2, breakLine: true } },
    { text: "Serial:  4.27 s", options: { fontSize: 13, color: C.text, breakLine: true, paraSpaceAfter: 12 } },

    { text: "Fastest", options: { bold: true, fontSize: 11, color: C.muted, charSpacing: 2, breakLine: true } },
    { text: "MPI 2p:  3.91 s", options: { fontSize: 13, color: C.good, bold: true, breakLine: true } },
    { text: "Speedup:  1.09×", options: { fontSize: 13, color: C.good, breakLine: true, paraSpaceAfter: 12 } },

    { text: "Diagnostic", options: { bold: true, fontSize: 11, color: C.muted, charSpacing: 2, breakLine: true } },
    { text: "All parallel times cluster near 4 s.", options: { fontSize: 12, color: C.text, breakLine: true } },
    { text: "Single thread already saturates memory bandwidth — adding cores cannot make memory go faster (Roofline ceiling).", options: { fontSize: 10, color: C.muted, italic: true } },
  ], {
    x: 9.85, y: 2.5, w: 2.5, h: 4.3,
    fontFace: F.body, valign: "top",
  });

  footer(s, "8");
}

// =============================================================================
// SLIDE 9 — Results: Speedup Analysis
// =============================================================================
{
  const s = pres.addSlide();
  s.background = { color: C.light };

  sectionTitle(s, "Results — Scaling Analysis", "Speedup and parallel efficiency vs. number of cores");

  // speedup line chart
  s.addChart(pres.charts.LINE, [
    { name: "Ideal",  labels: [1, 2, 4, 8], values: [1, 2, 4, 8] },
    { name: "OpenMP", labels: [1, 2, 4, 8], values: [0.76, 1.05, 1.04, 0.78] },
    { name: "MPI",    labels: [1, 2, 4, 8], values: [0.95, 1.09, 1.03, 0.88] },
    { name: "Hybrid", labels: [4, 8, 8],    values: [1.04, 0.99, 0.93] },
  ], {
    x: 0.8, y: 1.85, w: 7.5, h: 5.1,
    chartColors: [C.muted, C.secondary, C.primary, C.accent],
    lineSize: 3, lineSmooth: false,
    chartArea: { fill: { color: C.card }, roundedCorners: false },
    plotArea:  { fill: { color: C.card } },
    catAxisLabelColor: C.text, catAxisLabelFontFace: F.body, catAxisLabelFontSize: 11,
    valAxisLabelColor: C.muted, valAxisLabelFontFace: F.body, valAxisLabelFontSize: 11,
    valGridLine: { color: C.border, size: 0.5 },
    catGridLine: { style: "none" },
    showLegend: true,
    legendPos: "b",
    legendFontFace: F.body,
    legendFontSize: 11,
    legendColor: C.text,
    catAxisTitle: "Cores (processes × threads)",
    catAxisTitleColor: C.muted,
    catAxisTitleFontFace: F.body,
    catAxisTitleFontSize: 11,
    showCatAxisTitle: true,
    valAxisTitle: "Speedup (T_serial / T)",
    valAxisTitleColor: C.muted,
    valAxisTitleFontFace: F.body,
    valAxisTitleFontSize: 11,
    showValAxisTitle: true,
  });

  // analysis panel
  s.addShape(pres.shapes.RECTANGLE, {
    x: 8.6, y: 1.85, w: 3.9, h: 5.1,
    fill: { color: C.card }, line: { color: C.border, width: 0.5 },
  });
  s.addShape(pres.shapes.RECTANGLE, {
    x: 8.6, y: 1.85, w: 3.9, h: 0.5, fill: { color: C.primary }, line: { color: C.primary },
  });
  s.addText("WHAT THE CURVES TELL US", {
    x: 8.75, y: 1.85, w: 3.7, h: 0.5,
    fontFace: F.body, fontSize: 11, bold: true, color: "FFFFFF", valign: "middle", charSpacing: 4, margin: 0,
  });

  s.addText([
    { text: "Roofline ceiling.", options: { bold: true, fontSize: 13, color: C.warn, breakLine: true } },
    { text: "All curves cluster near 1× — a single thread already saturates this CPU's memory bandwidth at N = 1000.", options: { fontSize: 11, color: C.muted, breakLine: true, paraSpaceAfter: 10 } },

    { text: "Peak at MPI 2p.", options: { bold: true, fontSize: 13, color: C.good, breakLine: true } },
    { text: "1.09× — best result. Separate memory spaces avoid OpenMP's cache-coherence traffic, but cannot exceed the bandwidth ceiling.", options: { fontSize: 11, color: C.muted, breakLine: true, paraSpaceAfter: 10 } },

    { text: "OpenMP overhead visible.", options: { bold: true, fontSize: 13, color: C.text, breakLine: true } },
    { text: "OMP-1 (0.76×) is slower than serial — pure runtime overhead with no parallelism to amortize it.", options: { fontSize: 11, color: C.muted, breakLine: true, paraSpaceAfter: 10 } },

    { text: "Hyperthreading regression.", options: { bold: true, fontSize: 13, color: C.text, breakLine: true } },
    { text: "8-thread / 8-process cases regress — hyperthreads share FP units in this stencil-heavy workload.", options: { fontSize: 11, color: C.muted } },
  ], {
    x: 8.85, y: 2.5, w: 3.5, h: 4.4,
    fontFace: F.body, valign: "top",
  });

  footer(s, "9");
}

// =============================================================================
// SLIDE 10 — Limitations & Honest Findings
// =============================================================================
{
  const s = pres.addSlide();
  s.background = { color: C.light };

  sectionTitle(s, "Limitations & Honest Findings", "What the numbers don't show, and what we'd do differently");

  const items = [
    {
      head: "Memory-bandwidth ceiling (dominant)",
      body: "Stencil reads ~40 B/cell, performs ~5 FLOPs → arithmetic intensity ≈ 0.1 FLOP/B. A single thread already saturates ~20 GB/s of memory bandwidth, so multi-threading cannot accelerate data movement. Roofline confirms this is the hardware limit.",
    },
    {
      head: "Hyperthreading hurts FP-bound code",
      body: "Two hyperthreads share one core's floating-point unit. 8-thread / 8-process configurations all regress below their 4-core counterparts — confirmed across every implementation in our sweep.",
    },
    {
      head: "Single-node measurement only",
      body: "Experiments ran on one 4-physical-core CPU under WSL2. MPI's true strength — distributing across multiple machines over a network — is not exercised. On a real cluster MPI and Hybrid would scale substantially further before hitting their ceilings.",
    },
    {
      head: "No cache-blocking optimization",
      body: "Breaking through the Roofline ceiling requires increasing arithmetic intensity via temporal cache-blocking (compute multiple timesteps per data load). Identified as the next step.",
    },
  ];

  const cardY = 1.85, gap = 0.2, cardH = (H - cardY - 0.4 - gap * 3) / 4 - 0.05;

  items.forEach((it, i) => {
    const y = cardY + i * (cardH + gap);
    s.addShape(pres.shapes.RECTANGLE, {
      x: 0.8, y, w: W - 1.6, h: cardH,
      fill: { color: C.card }, line: { color: C.border, width: 0.5 },
      shadow: { type: "outer", color: "000000", blur: 6, offset: 2, angle: 135, opacity: 0.06 },
    });
    s.addShape(pres.shapes.RECTANGLE, {
      x: 0.8, y, w: 0.1, h: cardH, fill: { color: C.warn }, line: { color: C.warn },
    });
    s.addText(it.head, {
      x: 1.05, y: y + 0.1, w: 4.2, h: cardH - 0.2,
      fontFace: F.head, fontSize: 15, bold: true, color: C.text, valign: "middle", margin: 0,
    });
    s.addText(it.body, {
      x: 5.4, y: y + 0.1, w: W - 1.6 - 5.0, h: cardH - 0.2,
      fontFace: F.body, fontSize: 12, color: C.muted, valign: "middle",
    });
  });

  footer(s, "10");
}

// =============================================================================
// SLIDE 11 — Conclusion + Future Work
// =============================================================================
{
  const s = pres.addSlide();
  s.background = { color: C.light };

  sectionTitle(s, "Conclusion", "");

  // left: what we accomplished
  s.addShape(pres.shapes.RECTANGLE, {
    x: 0.8, y: 1.7, w: 5.9, h: 5.2,
    fill: { color: C.card }, line: { color: C.border, width: 0.5 },
  });
  s.addShape(pres.shapes.RECTANGLE, {
    x: 0.8, y: 1.7, w: 5.9, h: 0.5, fill: { color: C.good }, line: { color: C.good },
  });
  s.addText("WHAT WE DELIVERED", {
    x: 0.95, y: 1.7, w: 5.7, h: 0.5,
    fontFace: F.body, fontSize: 12, bold: true, color: "FFFFFF", valign: "middle", charSpacing: 4, margin: 0,
  });

  s.addText([
    { text: "Implemented and benchmarked four versions of a 2D wave-equation solver:  Serial, OpenMP, MPI, Hybrid.", options: { bullet: true, breakLine: true, paraSpaceAfter: 8 } },
    { text: "Validated all parallel implementations against the serial baseline — wave output is bit-identical to floating-point round-off.", options: { bullet: true, breakLine: true, paraSpaceAfter: 8 } },
    { text: "Measured execution time across 12 thread/process configurations, computed speedup and parallel efficiency.", options: { bullet: true, breakLine: true, paraSpaceAfter: 8 } },
    { text: "Built an interactive Streamlit dashboard for visualisation and reproducible reporting.", options: { bullet: true, breakLine: true, paraSpaceAfter: 8 } },
    { text: "Best observed: MPI with 2 processes — 1.09× speedup. The flat scaling reveals the Roofline ceiling: a single thread already saturates memory bandwidth.", options: { bullet: true, bold: true, color: C.good } },
  ], {
    x: 1.0, y: 2.35, w: 5.5, h: 4.5,
    fontFace: F.body, fontSize: 13, color: C.text, valign: "top",
  });

  // right: future work
  s.addShape(pres.shapes.RECTANGLE, {
    x: 6.9, y: 1.7, w: 5.6, h: 5.2,
    fill: { color: C.card }, line: { color: C.border, width: 0.5 },
  });
  s.addShape(pres.shapes.RECTANGLE, {
    x: 6.9, y: 1.7, w: 5.6, h: 0.5, fill: { color: C.primary }, line: { color: C.primary },
  });
  s.addText("FUTURE WORK", {
    x: 7.05, y: 1.7, w: 5.4, h: 0.5,
    fontFace: F.body, fontSize: 12, bold: true, color: "FFFFFF", valign: "middle", charSpacing: 4, margin: 0,
  });

  s.addText([
    { text: "Move the OpenMP parallel region outside the time loop to amortize fork-join overhead.", options: { bullet: true, breakLine: true, paraSpaceAfter: 8 } },
    { text: "Add SIMD vectorization (#pragma omp simd on the inner x-loop) and cache blocking.", options: { bullet: true, breakLine: true, paraSpaceAfter: 8 } },
    { text: "Pin processes with taskset / numactl to remove scheduler noise; report 3-run medians.", options: { bullet: true, breakLine: true, paraSpaceAfter: 8 } },
    { text: "Scale the study to a multi-node cluster to measure true MPI-over-network behaviour.", options: { bullet: true, breakLine: true, paraSpaceAfter: 8 } },
    { text: "Extend to the full nonlinear Shallow Water Equations with bathymetry and runup.", options: { bullet: true } },
  ], {
    x: 7.1, y: 2.35, w: 5.2, h: 4.5,
    fontFace: F.body, fontSize: 13, color: C.text, valign: "top",
  });

  footer(s, "11");
}

// =============================================================================
// SLIDE 12 — References + Thank You
// =============================================================================
{
  const s = pres.addSlide();
  s.background = { color: C.dark };

  s.addShape(pres.shapes.RECTANGLE, {
    x: 0, y: 0, w: 0.35, h: H, fill: { color: C.primary }, line: { color: C.primary },
  });

  s.addText("THANK YOU", {
    x: 1.0, y: 0.6, w: 11, h: 0.6,
    fontFace: F.body, fontSize: 14, color: C.accent, charSpacing: 8, margin: 0,
  });
  s.addText("Questions?", {
    x: 1.0, y: 1.1, w: 11, h: 1.0,
    fontFace: F.head, fontSize: 48, bold: true, color: "FFFFFF", margin: 0,
  });
  s.addShape(pres.shapes.LINE, {
    x: 1.0, y: 2.3, w: 3, h: 0, line: { color: C.accent, width: 2 },
  });

  s.addText("KEY REFERENCES", {
    x: 1.0, y: 2.6, w: 11, h: 0.4,
    fontFace: F.body, fontSize: 12, bold: true, color: C.accent, charSpacing: 4, margin: 0,
  });

  s.addText([
    { text: "Courant, R., Friedrichs, K., Lewy, H. (1928). ", options: { bold: true } },
    { text: "Über die partiellen Differenzengleichungen der mathematischen Physik. ", options: { italic: true } },
    { text: "Math. Ann. 100, 32–74.   (Original CFL stability paper.)", options: { breakLine: true } },

    { text: "Whitham, G. B. (1974). ", options: { bold: true } },
    { text: "Linear and Nonlinear Waves. ", options: { italic: true } },
    { text: "Wiley.", options: { breakLine: true } },

    { text: "Pedlosky, J. (1987). ", options: { bold: true } },
    { text: "Geophysical Fluid Dynamics, 2nd ed. ", options: { italic: true } },
    { text: "Springer.", options: { breakLine: true } },

    { text: "Titov, V. V. & Synolakis, C. E. (1998). Numerical modeling of tidal wave runup. ", options: { bold: true } },
    { text: "J. Waterway, Port, Coastal & Ocean Eng. 124(4), 157–171.", options: { breakLine: true } },

    { text: "LeVeque, R. J. (2002). ", options: { bold: true } },
    { text: "Finite Volume Methods for Hyperbolic Problems. ", options: { italic: true } },
    { text: "Cambridge.", options: { breakLine: true } },

    { text: "Strikwerda, J. C. (2004). ", options: { bold: true } },
    { text: "Finite Difference Schemes and Partial Differential Equations, 2nd ed. ", options: { italic: true } },
    { text: "SIAM.", options: { breakLine: true } },

    { text: "Hager, G. & Wellein, G. (2010). ", options: { bold: true } },
    { text: "Introduction to High Performance Computing for Scientists and Engineers. ", options: { italic: true } },
    { text: "CRC Press.", options: {} },
  ], {
    x: 1.0, y: 3.1, w: 11.3, h: 3.6,
    fontFace: F.body, fontSize: 12, color: "CADCFC", valign: "top", paraSpaceAfter: 4,
  });

  s.addText("EC7207 · Group 48 — Surasinghe · Tamasha · Tharshihan", {
    x: 1.0, y: 6.9, w: 11, h: 0.4,
    fontFace: F.body, fontSize: 11, color: "97BC62", italic: true, margin: 0,
  });
}

// --- write file -------------------------------------------------------------
pres.writeFile({ fileName: OUT_PATH }).then((p) => {
  console.log("WROTE: " + p);
});
