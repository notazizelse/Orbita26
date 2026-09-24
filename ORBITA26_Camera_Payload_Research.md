# ORBITA 2026 — Camera-as-Instrument Payload Dossier
### Second focused pass for the technician: turning a $10 camera into a metrology instrument

**Prepared:** 18 September 2026 · **Hard deadline:** 26 October 2026 — **38 days**
**Builds on:** `ORBITA26_Technician_Mission_Research.md` (constraints, rubric, platform, environment — not repeated here)
**New evidence this pass:** OV2640 datasheet, esp32-camera driver docs, STM32H750 datasheet, 6 papers on CMOS particle detection / low-cost DIC / vision-inertial fusion, plus 8 new computed budgets.

---

## THE ONE-PARAGRAPH ANSWER

The decisive constraint I found is in the OV2640 datasheet: **"Operation −30 °C to 70 °C, Stable Image 0 °C to 50 °C."** An externally-pointed camera on a −55 °C balloon flight is a coin flip — and the ORBITA rubric puts 27 of 60 points on returning a complete dataset. So the entire design pivots on one decision: **put the camera inside the heated payload bay, looking inward at a controlled target.** That single choice converts the camera from a weather-dependent Earth-observation device into a **laboratory instrument flying through a controlled environmental sweep** — thermally survivable, perfectly repeatable on a desk, independent of cloud cover, pointing, or where the balloon drifts. Everything good in this document follows from it.

The three concepts that survive every filter are: an **optical dilatometer** (camera + fiducials = simultaneous CTE measurement of 5 materials from +20 °C to −55 °C), a **dark-frame cosmic-ray counter** (the same sensor, time-multiplexed, counting particle hits through the Regener–Pfotzer maximum), and **optical/strain sensor fusion** (camera measures absolute displacement at 0.2 Hz, strain gauges measure relative displacement at 80 Hz, a complementary filter produces one drift-free high-bandwidth signal — and the disagreement between them is itself the scientific result).

All three run on **one $14 camera board, one lens, one LED, and one printed target**, share one firmware, and produce numbers — micrometres, counts per minute, coefficients of thermal expansion — not photographs.

---

## PART 1 — WHAT MAKES A GOOD ORBITA CAMERA EXPERIMENT

Seven tests. An idea must pass all seven. I applied these ruthlessly in Part 4.

### Test 1 — The thermal test
**Can the camera survive and keep working at the temperature it will actually see?**
The OV2640 — the sensor in essentially every cheap module — is rated **−30…+70 °C for operation and only 0…+50 °C for stable image quality**. Outside the stable band you get gain/offset drift, colour shift, increased fixed-pattern noise, and eventually nothing. Ambient at 20–24 km is **−52 to −57 °C**.

Consequence: **an outward-facing camera is an unmanaged risk; an inward-facing camera in a warm bay is a managed one.** An inward camera also removes lens fogging and frost on descent, which is a documented and common balloon-payload failure.

### Test 2 — The geometry test
**Does the thing being measured move in the direction the camera measures well?**
A camera measures **in-plane** motion superbly (sub-pixel) and **out-of-plane** motion badly (as a scale/defocus change). Computed sensitivity for an oblique view:

| Camera angle from surface normal | Image sensitivity to out-of-plane motion w |
|---|---|
| 15° | 0.26 × w |
| 30° | 0.50 × w |
| 45° | 0.71 × w |
| **60°** | **0.87 × w** |
| 75° | 0.97 × w |

So: **either arrange the phenomenon to move in-plane, or view it at ≥45°.** Any concept that ignores this is dead on arrival.

### Test 3 — The differential test
**Is there a reference fiducial in the same image?**
This is the single most important design rule in the whole document. Every optical measurement must be *the position of a target relative to a fiducial in the same frame*, never an absolute pixel coordinate. Doing this cancels, to first order:
- camera and lens thermal drift (focal length of a plastic lens changes measurably over 70 K)
- mount expansion and camera-to-target distance change
- illumination changes
- vibration of the camera itself

Additionally, put **two fiducials a known distance apart** in every frame — that gives you the **pixel scale in every single image**, self-calibrating against lens thermal drift at zero cost. This turns "a camera that got cold" from a problem into a non-issue.

### Test 4 — The continuous-signal test
**Does the measurand vary continuously through the flight, or is it a one-off event?**
The rubric's analysis band wants *"the full array of data correctly processed… conclusions confirming or refuting the hypothesis."* An event-detection mission ("did it crack?") returns a single bit and risks returning nothing. A continuous mission returns a curve against altitude. **Prefer curves.**

### Test 5 — The determinism test
**Is the image-processing algorithm deterministic and explainable?**
Thresholding, centroiding, normalised cross-correlation, connected-component labelling and FFT are all deterministic, provable on synthetic images, and defensible line-by-line in front of a jury. Neural networks are none of those things on a 38-day schedule with no training data. **No ML anywhere in this document.**

### Test 6 — The bandwidth test
**Does the science survive if no image is ever downlinked?**
LoRa at 2400 baud gives ~58 bytes/second. A single QVGA grayscale frame is 75 kB — **21 minutes of airtime.** So the architecture is fixed: **images stay on SD; numbers go down the radio.** Every concept must yield a small numeric vector per frame.

### Test 7 — The ground-truth test
**Can I reproduce the entire experiment on a desk with a freezer?**
If the answer is yes, the *Ground Testing* rubric line (8 points) is free, the calibration is real, and the flight is a confirmation rather than a gamble. If the phenomenon only exists at 24 km, you cannot validate anything before flight.

### A corollary worth stating plainly

> A camera that photographs the Earth is a **passenger**. A camera that photographs a controlled target inside your own spacecraft is an **instrument**. Only the second one can be calibrated, and only a calibrated device produces a measurement.

---

## PART 2 — THIRTY-EIGHT CAMERA-CENTRED CONCEPTS

Each entry: the measurement principle, what number comes out, and the immediate objection. **[KILL]** marks ideas I reject in Part 4 — they are listed so you can defend not doing them.

### A. Structural / mechanical vision

**1. Optical dilatometer — multi-material CTE comparator** ⭐⭐⭐
Five coupons (PLA, PETG, FR4, aluminium, CFRP or nylon) anchored to a common baseplate, free ends carrying high-contrast fiducials. Camera tracks each marker's position against a reference fiducial on the baseplate.
*Output:* ΔL(T) in µm for each material → **α(T) in ppm/K**, 1 curve per material, ~1 sample/2 s.
*Objection:* needs a rigid, repeatable mount. Solvable with an aluminium L-bracket.

**2. Oblique-view panel deflection mapping** ⭐⭐⭐
Camera at 60° views 6–9 fiducials on the inner face of the instrumented side panel. Sub-pixel centroids → out-of-plane deflection field.
*Output:* w(x,y) in mm at 6–9 points, 0.2–1 Hz. Computed SNR: deflections of 0.66–1.99 mm against a 9 µm noise floor = **70–210 σ**.
*Objection:* perspective must be calibrated (homography). One afternoon's work; deterministic.

**3. Camera + strain-gauge complementary fusion** ⭐⭐⭐
Camera gives absolute, drift-free, low-rate displacement; strain gauges give relative, high-rate, drifting displacement. Fuse with a complementary filter at a 0.05 Hz crossover.
*Output:* one drift-corrected 80 Hz displacement signal **plus** the residual between the two methods — which is the calibration-drift curve of the strain system, a result in itself.
*Objection:* requires both subsystems to work. Mitigated because each is independently useful.

**4. Optical lever — panel slope via mirror** ⭐⭐
A 10 mm mirror glued to the panel; an LED+pinhole beam reflects onto a white screen 80 mm away; the camera watches the spot.
*Output:* panel **slope** in degrees. Computed: 0.25° of slope moves the spot 0.70 mm = 3.7 px; 1° moves it 14.9 px.
*Why it's interesting:* strain gauges give curvature κ, the mirror gives slope θ, the fiducials give deflection w. **Three independent measurements linked by calculus: w′ = θ, θ′ = κ. Testing whether the calculus closes is a rigorous, falsifiable metrology result.**
*Objection:* one more alignment. Tolerance is loose (spot has ±20 mm of screen to wander in).

**5. Moiré-amplified displacement sensor** ⭐⭐
Two laser-printed line gratings (0.2 mm pitch) overlapped at ~2°; relative displacement of one pitch moves the fringe pattern by one fringe spacing — a computed amplification of **~28×**. Camera measures fringe phase by FFT.
*Output:* displacement to ~4 µm with no sub-pixel algorithm at all.
*Objection:* two gratings must stay parallel and within ~0.5 mm gap over 70 K. Medium mechanical risk. **Excellent upgrade, poor primary.**

**6. Bi-material cantilever thermal actuator** ⭐
PLA bonded to aluminium; tip deflection ∝ Δα·ΔT. Timoshenko formula with equal 0.5 mm layers, L = 50 mm gives **≈2.9 mm tip motion over 70 K = ~26 px**. Huge signal.
*Output:* effective Δα and the modulus ratio E₁/E₂ in flight.
*Objection:* it is mostly a fancy thermometer. Good as a high-SNR sanity channel, weak as a mission.

**7. Fastener / joint slip detection**
Fiducials either side of a bolted joint; relative motion = joint slip.
*Objection:* likely null over 2 hours. **Good passive bonus channel, bad primary.** [KILL as primary]

**8. Crack propagation in a pre-notched coupon**
*Objection:* classic binary-outcome trap. If it doesn't crack, you have nothing. [KILL]

**9. 2D Digital Image Correlation on a speckled panel** ⭐⭐
Spray-speckle the panel; correlate 32×32 subsets between frames (ZNCC) → full-field displacement, then strain.
*Published accuracy (Cambridge, low-cost DIC):* displacement noise floor **1×10⁻³ to 1×10⁻² px**, strain noise floor **37.5–468 µε**.
*Objection:* the strain noise floor is **20–900× worse than your strain gauges**. DIC is excellent for *displacement fields*, not for competing with a bridge on strain. Use it for what it's good at, and do the correlation **on the ground from stored frames**, not onboard.

**10. Vibration mode shape from marker motion**
Track fiducials at 25–30 fps during burst/parachute transients; FFT each marker's trajectory; relative phases give the mode shape.
*Objection:* frame rate limits you to <15 Hz modes, but the panel's first mode is 60–70 Hz. **Aliased.** Only works with the rolling-shutter trick (#11). [KILL in this naive form]

**11. Rolling-shutter vibrometry** ⭐
Exploit the fact that each image row is exposed at a different time: a vibrating edge imaged with a rolling shutter produces a spatial wave whose wavelength encodes the temporal frequency, up to ~row-rate/2 (kHz-class).
*Output:* vibration frequency in Hz from a **single frame**.
*Objection:* needs the exact row readout time, which is not documented for OV2640 and must be measured. Doable (image a known LED strobe) but adds a calibration step. **Strong stretch goal.**

**12. Panel buckling mode visualisation** — same family as #2; fold into it.

**13. Structural "before/after" difference imaging**
Identical illuminated frames at launch and landing, pixel-differenced.
*Objection:* qualitative. Keep as a free by-product of #2.

### B. Material characterisation

**14. Optical measurement of polymer glass-transition / stiffening** ⭐⭐
Extension of #1: look for a **change in slope** of ΔL(T) for PLA/PETG as they approach their low-temperature stiffening region. A kink in α(T) is a real materials result.
*Output:* α(T) piecewise slope, with the kink temperature if it exists.
*Null-result-proof:* if there is no kink, you still have the complete α(T) curve.

**15. Shrinkage anisotropy of FDM prints** ⭐⭐
Print the same coupon in three raster orientations (0°, 45°, 90°) and measure all three simultaneously.
*Output:* α_∥ vs α_⊥ for FDM PLA at stratospheric temperatures — **genuinely scarce data**, directly useful to every 3D-printed CubeSat.

**16. Adhesive / bond-line creep**
Fiducials across a glued joint under a small dead load.
*Objection:* creep over 2 h at −50 °C is tiny. Likely null. [KILL]

**17. Elastomer (O-ring/silicone) stiffening**
A rubber band under constant load; camera measures length. Below the glass transition it stops stretching.
*Objection:* attractive and cheap; risk is a spring/tension fixture. **Keep as a bonus coupon in #1** — it costs one more marker.

**18. Colour/reflectance change of a material under UV+cold**
*Objection:* needs an external view or a UV source; effects over 2 h are negligible. [KILL]

### C. Thermal experiments

**19. Camera watching a thermochromic / liquid-crystal sheet**
A thermochromic strip changes colour with temperature; camera reads the colour front.
*Output:* a 1D temperature field along the strip, from one image.
*Objection:* thermochromic pigments operate over narrow bands (typically 25–45 °C) and are useless at −50 °C. [KILL]

**20. Visualising a controlled thermal gradient via bi-material strips** — folded into #6.

**21. Camera + heater: transient thermal response of a coupon** ⭐
Pulse a heater under a coupon; camera watches the marker move as the coupon expands, giving the **thermal time constant** τ(altitude). Because convection collapses to ~19% by 24 km, τ should lengthen measurably.
*Output:* τ in seconds vs altitude — an optical cross-check of the convective-collapse experiment from the first dossier.
*Objection:* needs careful power control. Cheap and clean; **strong secondary.**

**22. Frost / condensation growth on a cooled window**
*Objection:* requires a Peltier, a sealed humid cell, and the phenomenon may not occur. [KILL] — but see #23.

**23. Condensation on descent (opportunistic)**
If the payload has any window, condensation on re-entry into humid air is near-certain. Log it as a by-product.
*Objection:* uncontrolled, uncalibratable. **Free bonus only.**

### D. Optical experiments

**24. Camera as a photometer: LED intensity vs temperature** ⭐
Image a set of LEDs at fixed drive current; total pixel intensity ∝ LED output. LED efficiency rises as it cools (roughly +0.3–1 %/K for some emitters).
*Output:* relative LED output vs junction temperature — a real optoelectronics characterisation.
*Objection:* camera AGC must be locked off. Trivial once you know to do it.

**25. Lens focal-length drift with temperature** ⭐⭐
Image two fiducials a known distance apart; their pixel separation gives the pixel scale, which gives the effective focal length.
*Output:* **f(T) for a plastic M12 lens over 70 K** — a genuine, useful, and almost never published number for cheap optics.
*Why it matters:* it is simultaneously a *result* and the *self-calibration* that makes every other optical measurement trustworthy. **This is the single cheapest high-value channel in the document: it is already in every frame.**

**26. Optical attenuation through a sample cell**
LED through a gel/liquid; camera measures transmitted intensity as the sample cools and possibly clouds.
*Objection:* liquids, sealing, freezing, possible organiser restrictions. [KILL]

**27. Scattering from a suspended-particle cell**
*Objection:* same liquid problems, plus settling under g. [KILL]

**28. Speckle decorrelation as a vibration/temperature sensor**
Laser speckle from a rough surface decorrelates with sub-micron motion.
*Objection:* extremely sensitive — which here means it saturates instantly and is uninterpretable. Also needs a laser at −40 °C. [KILL]

### E. Atmospheric optics

**29. Sky brightness / blue-to-dark transition vs altitude**
*Objection:* requires an outward camera (Test 1 fails), and it is the single most-done balloon camera measurement in existence. Class A. [KILL]

**30. Sun-shadow gnomon for attitude and spin rate**
A pin casting a shadow on a printed graticule; camera reads shadow angle → sun vector → spin rate.
*Objection:* needs sunlight inside the bay — i.e., a window, which reintroduces frost and thermal leaks. **[KILL]** — but a variant using **internal photodiodes only** was already covered in the first dossier.

**31. Optical air-density measurement via Schlieren**
*Objection:* requires precise optics and a density gradient you cannot create in a 100 mm box. [KILL]

### F. Sensor calibration and self-monitoring

**32. Dark-current / hot-pixel mapping vs temperature** ⭐⭐⭐
With the illuminator off, capture long-exposure dark frames. Compute mean level, RMS noise, and hot-pixel count above threshold.
*Output:* dark signal (DN/s) and hot-pixel count vs sensor temperature — a textbook exponential falling by roughly a factor of 2 per 6–8 K of cooling.
*Why it is the strongest "cannot fail" channel:* it requires **no target, no illumination, no mechanism, and no phenomenon to occur**. If the camera is powered, you have data. It is also the necessary calibration for #33.

**33. Camera as a cosmic-ray detector** ⭐⭐⭐
The same dark frames, but now looking for **isolated bright clusters** that are not in the hot-pixel map — ionising particles depositing charge in the silicon.
*Computed rates (scaled from the published Raspberry Pi HQ ground rate of 17.1 events/hour on 29.6 mm²):*

| Sensor | Active area | Ground rate | At Pfotzer max (×50) | (×100) | (×150) |
|---|---|---|---|---|---|
| OV2640 (1/4″) | 9.6 mm² | 5.6 /h | 278 /h = 4.6/min | 556 /h = 9.3/min | 834 /h = 13.9/min |
| IMX477 (Pi HQ) | 29.6 mm² | 17.1 /h | 855 /h | 1710 /h | 2565 /h |

*Flight-integrated estimate for an OV2640:* **≈900 events**, enough for **10 altitude bins at ±10% Poisson error**.
*The documented gap:* the one published balloon attempt (DePaul AHAC) used a GoPro and states plainly that *"due to the lack of GPS on GoPros it was not possible to correlate altitude to rate of events detected."* **You have a barometer, an IMU and a synchronised clock. That is exactly the gap.**
*Objection:* JPEG compression destroys single-pixel events (the same paper cites camera post-processing "blurring out" events). **You must capture uncompressed grayscale.** This drives the hardware choice in Part 8.

**34. ADC/exposure linearity self-test with a stepped LED ladder** ⭐
Drive 4 LEDs at known, different currents; the camera's response to a known intensity ladder gives its transfer curve, tracked through the flight.
*Output:* camera linearity and gain vs temperature — turns the camera into a *calibrated* photometer.

### G. Electronics self-monitoring

**35. Camera reads the CubeSat's own status LEDs**
*Objection:* cute, useless — you already have the data electronically. [KILL]

**36. Camera watching an analogue indicator (e.g. a moving-coil meter)**
*Objection:* adds a mechanism to measure something a wire already measures. [KILL]

### H. Camera + IMU

**37. Camera-IMU fusion for payload motion**
Track a fiducial fixed to the frame while the IMU logs motion; the residual isolates camera-mount vibration.
*Prior art:* vision+acceleration displacement fusion is a mature field (multiple 2023–2026 papers). Class A/B.
*Use:* not a mission, but a **necessary correction term** — it tells you how much of the marker motion in #2 is real panel bending and how much is camera shake.

### I/N. Fluids and biology

**38. Bubble-size optical barometer**
A sealed cell with a trapped gas bubble in a low-freezing-point liquid; bubble volume ∝ 1/P, so volume grows ~26× from ground to 24 km (diameter ~3×).
*Output:* pressure from bubble diameter, cross-checked against the MS5611. Delightful physics; Boyle's law made visible.
*Objection:* liquid, sealing, freezing, leak risk in a shared payload, and possible organiser restrictions on liquids. **[KILL for flight, excellent for the ground demo.]**

**Also considered and killed:** camera + biological specimen (needs the bio module's environment and adds nothing the biologist cannot photograph post-flight); camera watching a deployable mechanism (mechanism risk, first dossier §20); star/sun tracking (pointing we do not control); stereo camera pair (doubles cost, power and failure modes for a measurement an oblique single view already makes with 70σ margin).

---

## PART 3 — NOVELTY CHECK

Same four classes as the first dossier. **A** = common · **B** = exists but uncommon · **C** = novel combination · **D** = potentially novel implementation (stated cautiously).

| Concept | Class | Prior art found | What would actually be new |
|---|---|---|---|
| Earth/sky/horizon photography from a balloon | **A** | Thousands of flights; the default student payload | Nothing |
| Vision-based structural displacement measurement | **A/B** | Mature civil-engineering field; many 2020–2026 papers on vision+acceleration fusion for bridges | The *method* is not new |
| Digital image correlation on low-cost cameras | **B** | Cambridge/Eichhorn 2020 low-cost DIC (Raspberry Pi camera, 10⁻³ px displacement floor, 37.5 µε strain floor) | Applying it **inside a flying CubeSat during a 70 K thermal sweep** |
| CMOS sensor as a particle detector | **B** | DECO, CREDO, SORAMAME (2026, Raspberry Pi HQ, 17.1 events/h at sea level); smartphone-muon-efficiency papers | Not the technique |
| CMOS particle counting **on a balloon, correlated with altitude** | **B/C** | One documented attempt (DePaul AHAC, GoPro) that **explicitly failed to correlate altitude** for lack of GPS | **Doing it properly**: uncompressed grayscale, a hot-pixel map built in flight, and every frame timestamped against a barometer and IMU — producing an actual Pfotzer profile from a $10 camera |
| Optical dilatometry (CTE from image tracking) | **B** | Standard optical dilatometry exists in materials labs with far better instruments; video extensometers are commercial | Doing it **on five materials at once, in flight, from +20 to −55 °C, with a self-calibrating in-frame scale reference, for under $20** |
| Low-temperature CTE of FDM-printed polymers, in as-printed raster orientations | **C** | Room- and high-temperature data exist; low-temperature FDM data is sparse and rarely orientation-resolved | The dataset itself. Directly useful to every 3D-printed CubeSat |
| Lens focal-length drift f(T) for cheap M12 plastic optics | **C/D** | Thermal defocus of optics is well known in principle; I found no published f(T) characterisation for commodity M12 modules over a 70 K span | A small, honest, genuinely useful number — and it is free, because it is already in every frame |
| **Camera + resistive strain gauge complementary fusion, with the residual reported as the strain system's drift** | **C** | Vision+**accelerometer** fusion is well established; vision+**strain-gauge** fusion for *in-flight drift correction of the strain channel* is much rarer | The inversion: the camera is used to **calibrate the strain gauges in flight**, not the other way round |
| **Closure test w″ = κ using three independent optical/electrical measurements of one beam** | **C/D** | Each measurement is textbook; I found no instance of all three being flown together on a student vehicle as a consistency test | The test itself: a falsifiable, quantitative metrology result with error bars, independent of whether the panel bends much or little |
| Rolling-shutter vibrometry | **B** | Published technique (single-frame frequency extraction, laser-speckle variants) | Applying it to a CubeSat panel's first mode in flight |
| Moiré-amplified displacement with printed gratings | **B** | Moiré metrology dates to the 1960s | The cost point and the flight application |

**The sentence you can defend under cross-examination:**

> *"Every individual technique here is established — image-based displacement measurement, CMOS particle counting, and complementary-filter sensor fusion are all textbook. What we have not found published is the combination: a single low-cost camera time-multiplexed between an illuminated metrology mode and a dark particle-counting mode inside a CubeSat, producing a multi-material CTE dataset, an altitude-resolved particle-rate profile, and an in-flight drift calibration of a resistive strain system, all from the same sensor and the same flight. We searched CubeSat camera literature, high-altitude balloon payload reports, low-cost DIC papers and CMOS-particle-detection publications; we cannot prove nobody has done it."*

**What you must NOT claim:** that image-based deformation measurement is new, that CMOS cosmic-ray detection is new, or that anything here is a "world first."

---

## PART 4 — RELIABILITY CHECK: THE KILL LIST

Reliability is your stated top priority, so here is the explicit reasoning for every idea I removed.

| Killed idea | Reason |
|---|---|
| Any outward-facing camera as the primary instrument | **Test 1.** OV2640 stable-image band is 0…+50 °C; ambient is −55 °C. Add cloud cover, lens frost on descent, and uncontrolled pointing |
| Sky-brightness vs altitude | Class A, outward-facing, and weather-dependent |
| Crack detection | Binary outcome. No crack → no data → the 12-point analysis line collapses |
| Adhesive creep | Effect over 2 h at −50 °C is below the noise floor |
| Thermochromic temperature imaging | Pigments work over ~25–45 °C. Useless at −50 °C |
| Peltier-cooled frost cell | Adds a mechanism, a sealed humid volume, and a phenomenon that may not occur |
| Liquid-based cells (optical attenuation, scattering, bubble barometer) | Sealing, freezing, leak risk inside a shared payload, likely organiser restrictions. **Build the bubble barometer as a ground demo instead — it is a wonderful teaching object and costs nothing** |
| Laser speckle decorrelation | Over-sensitive (saturates), needs a laser diode at −40 °C |
| Schlieren imaging | Requires precision optics and a density gradient you cannot generate in a 100 mm box |
| Sun gnomon inside the bay | Requires a window → frost, thermal leak, and pointing you do not control |
| Stereo camera pair | Doubles cost, power, wiring and failure modes for a measurement the oblique single view already makes with 70σ of margin |
| Naive marker-tracking vibration measurement | Frame rate ≈ 25 fps; the panel's first mode is 60–70 Hz. **Aliased.** Only the rolling-shutter variant escapes this |
| Onboard DIC | The STM32H750 has 128 kB flash and no FPU-friendly correlation library ready to go. Do DIC **on the ground** from stored frames |
| Any machine learning | No training data, no time, not explainable to a jury |
| Raspberry Pi Zero as the camera computer | Linux boot time, SD-corruption-on-power-loss, 1.5–3 W, and a 38-day schedule. **This is the most tempting and most dangerous option on the table** |

**Two reliability principles that survived and shaped everything downstream:**

1. **Every optical measurement is differential against a fiducial in the same frame.** This makes camera drift, lens drift, mount expansion and illumination change second-order.
2. **The camera has two modes, and one of them needs nothing to happen.** Dark-frame characterisation produces a complete dataset whether or not the panel bends, whether or not the LEDs work, whether or not the targets are in focus. It is the floor under the score.

---

## PART 5 — TOP NINE CANDIDATES

### C-1. OPTICAL DILATOMETER — multi-material CTE from +20 °C to −55 °C
- **Mission question:** What is the coefficient of thermal expansion of the materials a student CubeSat is actually built from, measured in flight across the full stratospheric temperature range, in the orientations they are actually used?
- **Hypothesis:** ΔL/L = α·ΔT will be linear for aluminium and FR4 (α ≈ 23.6 and 16 ppm/K) and **non-linear for FDM-printed PLA and PETG**, whose α falls as the polymer stiffens on cooling; measured α at −50 °C will be **10–30% below** the room-temperature value.
- **Why it matters:** 3D-printed structural parts are now standard in student CubeSats, and low-temperature CTE data for FDM prints in as-printed raster orientations is scarce. Get this wrong and your panels preload your fasteners — which is exactly what the first dossier predicted (0.97 mm of differential shrinkage over a 300 mm panel).
- **What makes it different:** five materials measured *simultaneously* by one sensor, self-calibrated by an in-frame scale reference, with an aluminium coupon of known α as the built-in accuracy check.
- **Camera:** ESP32-S3 camera board, OV2640, **QVGA grayscale, fixed exposure/gain/AWB off**, 6 mm M12 lens at 60 mm → 112 µm/px.
- **Other sensors:** 6 × DS18B20 (one per coupon + baseplate), MS5611 (altitude), the platform's LM75A.
- **Numbers (computed, 150 mm coupons, 112 µm/px, 0.05 px centroid noise = 5.6 µm):**

| Material | α (ppm/K) | ΔL over −70 K | In pixels | SNR |
|---|---|---|---|---|
| PLA (FDM) | 70 | 735 µm | 6.6 px | **131σ** |
| PETG (FDM) | 60 | 630 µm | 5.6 px | 113σ |
| Aluminium 6061 | 23.6 | 248 µm | 2.2 px | **44σ** (accuracy check) |
| FR4 | 16 | 168 µm | 1.5 px | 30σ |
| CFRP | 2 | 21 µm | 0.19 px | 3.7σ (near-zero reference) |

- **What gets logged:** every frame (QVGA grayscale, ~75 kB) + a per-frame record of 6 marker centroids, 2 scale fiducials, 6 temperatures, pressure, exposure settings.
- **What gets transmitted:** 6 × int16 displacements (µm), 6 × int16 temperatures, scale factor, frame counter. **~30 bytes.**
- **Ground experiment:** the whole thing in a chest freezer (−20 °C) and then a dry-ice cooler (−78 °C), with a dial indicator on the PLA coupon as independent truth.
- **Expected graph:** ΔL vs T, five curves, with the aluminium line's slope matching the handbook value to within a few per cent — which is your proof the instrument works.
- **Dev time:** 8–10 days. **Cost:** ~$25. **Biggest failure mode:** camera stops in the cold (mitigated by the warm bay + heater). **Reliability: very high.** **Novelty: B/C.**

### C-2. DARK-FRAME CAMERA AS PARTICLE DETECTOR + SENSOR CHARACTERISER
- **Mission question:** Can a $10 CMOS camera, used as a silicon particle detector, resolve the altitude profile of secondary cosmic radiation through the Regener–Pfotzer maximum — and simultaneously characterise its own dark current and hot-pixel population as it cools by 70 K?
- **Hypothesis:** Cluster rate will rise from ~6/h at ground to **several hundred per hour above 18 km**, peaking in the 15–26 km band; dark signal will fall roughly a factor of 2 per 6–8 K of cooling, so the detector gets *cleaner* as it climbs.
- **Why it matters:** it converts an ordinary camera into a radiation instrument with **no high voltage, no Geiger tube, no exotic parts** — and it fills a documented gap (the one published balloon attempt could not correlate rate with altitude).
- **What makes it different:** uncompressed grayscale (so single-pixel events survive), an in-flight-built hot-pixel map, and rigorous timestamping against the barometer.
- **Camera:** same board, same sensor, illuminator off, **long exposure**, lens capped or bay light-sealed.
- **Other sensors:** sensor die temperature (from the camera), bay temperature, MS5611.
- **Numbers:** ≈900 events per flight → 10 altitude bins at **±10.4%** Poisson error; 5 bins at ±7.4%.
- **What gets transmitted:** per 60 s — cluster count, mean cluster size, mean cluster intensity, dark level, RMS noise, hot-pixel count. **~16 bytes/minute.**
- **Ground experiment:** run for 12 h on a desk to measure the ground rate and build the hot-pixel map; put it in a freezer and watch dark current fall; compare QVGA vs full-resolution rates to check whether the sensor bins or crops.
- **Expected graph:** counts/minute vs altitude with Poisson error bars, overlaid on the standard Pfotzer curve shape.
- **Dev time:** 4–6 days. **Cost:** $0 extra (same camera). **Biggest failure mode:** light leaks into the bay (mitigated: a light-tight baffle + verify with a ground dark test). **Reliability: extremely high** — it needs no phenomenon to occur. **Novelty: B/C.**

### C-3. OBLIQUE-VIEW PANEL DEFLECTION + STRAIN-GAUGE FUSION
- **Mission question:** Do two physically independent measurements of the same panel deflection — optical and resistive — agree, and what does their disagreement reveal about the drift of a cheap strain system in a 70 K thermal sweep?
- **Hypothesis:** The camera (absolute, drift-free, 0.2–1 Hz) and the strain gauges (relative, 80 Hz, drifting) will agree to **within 15%** in the first 20 minutes, then diverge by a slowly growing offset that tracks temperature — and that offset *is* the strain system's thermal drift curve.
- **Why it matters:** it is a real metrology result, it produces a fused signal better than either sensor alone, and it is **immune to null results**: if the panel barely bends, you still measure both systems' agreement near zero, which is still a calibration.
- **What makes it different:** vision+*accelerometer* fusion is a mature published field; vision used to **calibrate a strain channel in flight** is not.
- **Camera:** same board, at 60° to the panel → **0.87× sensitivity** to out-of-plane motion.
- **Other sensors:** the 4-station strain array from the first dossier, LSM6DS3 (to separate camera shake from panel motion), panel temperatures.
- **Numbers:** predicted deflections 0.66–1.99 mm against a 9.4 µm optical noise floor = **70–210σ**.
- **Fusion:** complementary filter, crossover 0.05 Hz — *camera sets the DC level, strain gauges carry the dynamics.*
- **What gets transmitted:** w_cam[4] (µm), w_strain[4] (µm), w_fused[4] (µm), residual[4]. **~32 bytes.**
- **Ground experiment:** three-point bend with a dial indicator as truth for both systems simultaneously; then repeat inside the freezer to see the strain drift appear while the camera stays honest.
- **Expected graph:** three traces on one axis (camera, strain, fused) plus the residual vs temperature.
- **Dev time:** 6–8 days **on top of** the strain payload. **Cost:** ~$4 (fiducials + bracket). **Biggest failure mode:** camera mount vibrates (mitigated by the in-frame reference fiducial). **Reliability: high.** **Novelty: C.**

### C-4. IN-FRAME OPTICAL SELF-CALIBRATION: lens f(T) and camera transfer curve
- **Mission question:** How much does a commodity M12 plastic lens change focal length over 70 K, and how does the camera's photometric response drift with it?
- **Why it matters:** it is both a *result* and the *validity proof* for every other optical measurement. A jury asking "how do you know your camera didn't just shrink?" gets a measured answer.
- **Method:** two fiducials a known distance apart on an aluminium bar in every frame → pixel scale per frame → effective focal length. Plus a 4-LED intensity ladder at fixed current → transfer curve.
- **Transmitted:** scale factor (ppm), 4 LED intensities. **8 bytes.** **Dev time:** 1–2 days (it is already in the frames). **Reliability: certain.** **Novelty: C/D.**

### C-5. FDM PRINT ANISOTROPY COMPARATOR
Same hardware as C-1, different coupons: the same geometry printed at 0°, 45° and 90° raster.
**Output:** α_∥ vs α_45 vs α_⊥ for FDM PLA at stratospheric temperatures. **Dev time:** +1 day (just print three more coupons). **Novelty: C.** **This is the cheapest novelty upgrade available.**

### C-6. OPTICAL LEVER SLOPE CHANNEL AND THE w″ = κ CLOSURE TEST
Mirror on the panel + LED/pinhole + diffusing screen + the same camera.
**Output:** panel slope θ(t). Combined with camera deflection w and strain curvature κ: **test whether w′ = θ and θ′ = κ**, with propagated error bars.
**Numbers:** 0.25° slope → 3.7 px; 1° → 14.9 px. Loose alignment tolerance (±20 mm of screen).
**Dev time:** 3–4 days. **Reliability: medium-high.** **Novelty: C/D.** **This is the most intellectually satisfying concept in the document.**

### C-7. TRANSIENT THERMAL RESPONSE τ(altitude), measured optically
Pulse a heater under one coupon; the camera watches the marker move; the exponential time constant τ lengthens as convection collapses.
**Output:** τ vs altitude — an optical cross-check of the convective-collapse experiment (THERMOS) from the first dossier. **Dev time:** 2–3 days. **Novelty: C.**

### C-8. ROLLING-SHUTTER VIBROMETRY
Single-frame extraction of the panel's vibration frequency using the row-exposure time offset.
**Output:** f_vib in Hz, from one image, at frequencies far above the frame rate.
**Prerequisite:** measure the row readout time by imaging a strobed LED of known frequency. **Dev time:** 4–5 days. **Reliability: medium.** **Stretch goal only.**

### C-9. MOIRÉ-AMPLIFIED DISPLACEMENT CHANNEL
Two printed 0.2 mm gratings at ~2° → **28× amplification**; FFT the fringe pattern for phase → displacement to ~4 µm with no sub-pixel algorithm.
**Dev time:** 4–5 days. **Reliability: medium** (gap and parallelism must hold over 70 K). **Upgrade, not primary.**

---

## PART 6 — DETAILED COMPARISON

### Scoring methodology (stated first)

Each criterion scored **1–5**. For *Build time*, *Failure risk* and *Hardware availability*, **5 = better** (faster / safer / easier to obtain). These are engineering estimates against *your* stated constraints, not objective measurements.

| Score | Scientific value | Novelty | Reliability | Camera dependence | Electronics depth | Programming depth | Data richness | Ground-testability | Build time | HW availability | Env. robustness | Telemetry efficiency |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **5** | answers a real open question | class C/D | ~certain to return data | camera is the only way | real analogue + timing design | non-trivial deterministic CV + fusion | dense continuous curves | fully reproducible on a desk | ≤5 days | buy locally today | unaffected by cold | <50 B/s carries the science |
| **3** | useful | class B | probably works | camera is best option | moderate | moderate | discrete points | partially testable | 1–2 wk | 1–2 wk shipping | needs heating | needs thumbnails |
| **1** | little transfer | class A | may return nothing | a sensor would do better | wiring only | scripting only | one before/after | untestable | >3 wk | long lead | fails cold | needs images downlinked |

### The table

| Criterion | **C-1 Dilatometer** | **C-2 Dark-frame** | **C-3 Cam+strain fusion** | C-4 Self-calib | C-5 Anisotropy | C-6 Optical lever | C-7 Thermal τ | C-8 Rolling shutter | C-9 Moiré |
|---|---|---|---|---|---|---|---|---|---|
| Scientific value | **5** | 4 | **5** | 4 | 4 | 4 | 3 | 3 | 2 |
| Novelty | 4 | 4 | **5** | **5** | 4 | **5** | 4 | 3 | 3 |
| **Reliability** | **5** | **5** | 4 | **5** | **5** | 3 | 4 | 2 | 3 |
| Camera dependence | **5** | **5** | 4 | **5** | **5** | **5** | 3 | **5** | **5** |
| Electronics depth | 4 | 4 | **5** | 3 | 3 | 4 | **5** | 3 | 3 |
| Programming depth | 4 | **5** | **5** | 3 | 3 | 4 | 3 | **5** | 4 |
| Data richness | **5** | **5** | **5** | 4 | 4 | 4 | 3 | 2 | 4 |
| Ground-testability | **5** | **5** | **5** | **5** | **5** | 4 | 4 | 3 | 4 |
| Build time (5 = fast) | 3 | **5** | 3 | **5** | **5** | 4 | 4 | 3 | 3 |
| HW availability | 4 | **5** | 4 | **5** | 4 | 4 | 4 | **5** | **5** |
| Env. robustness | 4 | **5** | 4 | 4 | 4 | 3 | 4 | 3 | 2 |
| Telemetry efficiency | **5** | **5** | **5** | **5** | **5** | **5** | **5** | **5** | **5** |
| Failure risk (5 = low) | 4 | **5** | 3 | **5** | **5** | 3 | 4 | 2 | 3 |
| **TOTAL (of 65)** | **57** | **62** | **57** | **58** | **56** | **52** | **50** | **44** | **46** |

### How to read this table

**C-2 wins on points and is not the most interesting concept.** That is the whole lesson of the ORBITA rubric: the dark-frame experiment scores highest because it is nearly impossible for it to fail, not because it is the most impressive thing you could build. It is the floor.

**C-1 and C-3 tie at 57** and are the concepts a judge will remember. C-1 produces the clean materials dataset; C-3 produces the cross-validation story.

**C-4 scores 58 on almost no work** — it is already inside every frame of C-1 and C-2. Ignoring it would be leaving points on the ground.

**C-8 and C-9 are the ideas to admire and not build this year.** Both are genuinely clever; both have reliability scores of 2–3; both are worth mentioning in your presentation as "considered and deferred, for these reasons" — which itself demonstrates engineering judgement.

**The right answer is not one concept.** C-1, C-2 and C-4 share **one camera, one lens, one firmware and one target assembly**, differ only in exposure settings and which LEDs are lit, and together score all seven rubric lines. C-3 adds the strain payload you already designed. That combination is Part 7.

---

## PART 7 — TOP THREE TECHNICAL DESIGNS

The three are designed as **one payload**, called **OPTIC-24**, with three measurement modes multiplexed in firmware. What follows is the real implementation.

### 7.1 The physical layout

```
        ┌──────────────────────────────── 2U payload bay (100 × 100 × 200 mm) ───┐
        │  ╔════════════ insulated, heated inner box (XPS 15 mm) ═══════════╗    │
        │  ║                                                                ║    │
        │  ║   [CAM]───────── 60 mm ─────────►  ┌──────────────────────┐    ║    │
        │  ║     │  6 mm M12 lens, 36 mm FOV    │  TARGET ASSEMBLY     │    ║    │
        │  ║     │                              │                      │    ║    │
        │  ║   4× LED ring (diffused, 850 nm    │  ▣ scale fiducial A  │    ║    │
        │  ║     or white, current-controlled)  │  ▣ scale fiducial B  │    ║    │
        │  ║                                    │   (known 25.00 mm    │    ║    │
        │  ║   light-tight baffle ─────────►    │    apart on Al bar)  │    ║    │
        │  ║                                    │                      │    ║    │
        │  ║                                    │  ● PLA-0°  marker    │    ║    │
        │  ║                                    │  ● PLA-90° marker    │    ║    │
        │  ║                                    │  ● PETG    marker    │    ║    │
        │  ║                                    │  ● FR4     marker    │    ║    │
        │  ║                                    │  ● Al      marker    │    ║    │
        │  ║                                    │  ● CFRP    marker    │    ║    │
        │  ║                                    └──────────┬───────────┘    ║    │
        │  ║        6× DS18B20 bonded to the coupons ──────┘                ║    │
        │  ║        coupons 150 mm long, anchored at the far end to the     ║    │
        │  ║        common aluminium baseplate that also carries the camera ║    │
        │  ╚════════════════════════════════════════════════════════════════╝    │
        │                                                                        │
        │   instrumented side panel (strain gauges, from dossier 1)               │
        │        viewed at 60° by the SAME camera in MODE C via a folding mirror  │
        └────────────────────────────────────────────────────────────────────────┘
```

**The single most important mechanical fact:** the camera, the scale fiducials and the coupon anchors are all bolted to **one aluminium baseplate**. Everything that could drift — camera position, lens focal length, baseplate expansion — appears identically in the scale fiducials and cancels.

### 7.2 Design A — OPTIC-24 / MODE M (Metrology): the optical dilatometer

**Hardware block diagram**

```
 coupon expands/contracts (physical)
        │
        ▼
 printed fiducial marker moves in the image plane (in-plane — optimal geometry)
        │
        ▼
 4× LED ring, constant-current, ON only during MODE M  ◄── MOSFET ◄── STM32 GPIO
        │
        ▼
 OV2640 on ESP32-S3 camera board
   · QVGA 320×240 GRAYSCALE, uncompressed
   · AEC/AGC/AWB **OFF**, exposure and gain fixed by firmware
   · 6 mm M12 lens, object distance 60 mm → 36 mm field → 112 µm/px
        │
        ▼
 ESP32-S3 in-frame processing (C, deterministic):
   1. subtract stored dark frame
   2. threshold at (background + 6σ)
   3. connected-component label → 8 blobs expected
   4. intensity-weighted centroid of each blob, sub-pixel:
         cx = Σ(x·I)/ΣI ,  cy = Σ(y·I)/ΣI
   5. scale = 25.00 mm / |fiducialA − fiducialB|   [mm per pixel, THIS FRAME]
   6. for each coupon marker: d_i = (cy_i − cy_ref) × scale
        │
        ▼
 UART 115200 8N1 ──► STM32H750 (UART3 on PD9/PD8)
        │
        ├──► timestamp (µs), attach 6× DS18B20 temps, MS5611 P/T, IMU
        ├──► microSD: full frame (75 kB) + the numeric record
        └──► 58-byte LoRa packet, 1 Hz
```

**Wiring concept (real signals)**

| From | To | Signal | Notes |
|---|---|---|---|
| IntroBus_L `+5V_IS` | camera board 5 V in | power | ESP32-S3 board's own LDO; 470 µF bulk cap at the board |
| IntroBus_L `GND` | camera board GND | ground | **single point, star from the payload PCB** |
| IntroBus_L `PD9 (RX3)` | camera board TX | telemetry from camera | 3.3 V logic both sides |
| IntroBus_L `PD8 (TX3)` | camera board RX | commands to camera | mode select, exposure, trigger |
| IntroBus_L `PC6` | camera board GPIO | **hardware trigger / sync pulse** | STM32 raises this at t₀; camera timestamps its capture against it |
| IntroBus_L `PC7` | LED driver MOSFET gate | illuminator on/off | AO3400, 4 LEDs at 20 mA each via 150 Ω |
| IntroBus_L `PC0` | DS18B20 1-Wire bus | 6 sensors | 4.7 kΩ pull-up to 3.3 V |
| IntroBus_L `PC1` | heater MOSFET gate | bay heater | 2 × 47 Ω on the baseplate |
| IntroBus_L `PC3_C` | thermistor divider | bay temperature (analogue backup) | independent of the 1-Wire bus |

Everything terminates in **JST B2B-EH-A connectors or 2-pin terminal blocks** — flight-legal per the IntroSat guide. No PLS-2 headers.

**Firmware (STM32 side, the flight computer)**
```
MODE_M cycle, every 2 s:
  1. GPIO PC7 HIGH            → LEDs on
  2. wait 50 ms               → LED thermal settle
  3. GPIO PC6 pulse           → sync mark; latch micros()
  4. UART3 TX: 'M' <exposure> → camera captures and processes
  5. read DS18B20 ×6 (async, started earlier — 750 ms conversion)
  6. read MS5611, LSM6DS3
  7. UART3 RX: 8 centroids + scale + frame stats  (~40 bytes)
  8. GPIO PC7 LOW             → LEDs off
  9. compute d_i, append to SD, build telemetry packet
```

**Data format — MODE M telemetry (58 bytes, one radio fragment)**

| Offset | Field | Type | Encoding |
|---|---|---|---|
| 0–1 | sync | u8×2 | `0xAA 0x55` |
| 2 | type | u8 | `0x10` = TM_OPTIC_M |
| 3–6 | t_ms | u32 | ms since boot |
| 7–8 | altitude | i16 | m |
| 9–10 | d_PLA0 | i16 | µm, vs reference fiducial |
| 11–12 | d_PLA90 | i16 | µm |
| 13–14 | d_PETG | i16 | µm |
| 15–16 | d_FR4 | i16 | µm |
| 17–18 | d_Al | i16 | µm |
| 19–20 | d_CFRP | i16 | µm |
| 21–22 | T_PLA0 | i16 | °C×8 |
| 23–24 | T_PETG | i16 | °C×8 |
| 25–26 | T_Al | i16 | °C×8 |
| 27–28 | T_baseplate | i16 | °C×8 |
| 29–30 | T_camera | i16 | °C×8 |
| 31–32 | **scale_ppm** | i16 | pixel scale deviation from calibration, ppm → **this is C-4, lens f(T)** |
| 33–34 | blob_SNR_min | u16 | weakest marker's SNR — data-quality flag |
| 35 | blobs_found | u8 | should be 8; anything else flags degraded tracking |
| 36–37 | exposure_us | u16 | as commanded |
| 38–39 | LED_current_mA | u16 | |
| 40–41 | frame_mean | u16 | DN — illumination health |
| 42–43 | frame_rms | u16 | DN |
| 44–45 | focus_metric | u16 | Tenengrad sharpness — detects fogging or defocus |
| 46–47 | P_hPa×10 | u16 | |
| 48 | flight_phase | u8 | |
| 49 | cam_status | u8 | bit flags |
| 50–51 | frame_index | u16 | ties telemetry to the SD frame |
| 52–54 | reserved | — | |
| 55–56 | CRC16 | u16 | |
| 57 | end | u8 | `0x0D` |

**Expected result plot**
Six ΔL(T) curves on one axis, each fitted with a straight line whose slope is α. The aluminium line should land on 23.6 ± 1 ppm/K — **that is the plot that proves the instrument is real**, and it is the first thing to show a judge.

---

### 7.3 Design B — OPTIC-24 / MODE D (Dark): the particle detector

**The elegance:** the same camera, the same wiring, the same firmware. Only the LEDs, the exposure and the processing change.

```
 MODE_D cycle, every 2 s (interleaved with MODE M):
   LEDs OFF, baffle keeps the bay light-tight
   exposure = 500–1000 ms (long), gain fixed low
        │
        ▼
 ESP32-S3 processing:
   1. frame − running dark reference
   2. threshold at (mean + 8σ)
   3. connected-component label
   4. for each cluster: (x, y, n_pixels, peak_DN, total_DN)
   5. reject any cluster whose centroid matches the HOT-PIXEL MAP
      (map built during the first 10 minutes on the pad, and updated:
       any pixel firing in >20 % of frames is hot, not a particle)
   6. emit: cluster count, size histogram (4 bins), mean intensity,
            dark level, RMS, hot-pixel count
        │
        ▼
 STM32: accumulate over 60 s, timestamp, tie to altitude, log, downlink
```

**Why the hot-pixel map is the whole experiment.** A warm CMOS sensor produces bright pixels indistinguishable from a particle hit in a single frame. They are distinguishable by **persistence**: a hot pixel is at the same coordinate every frame; a particle is at a random coordinate once. The algorithm is five lines, deterministic, and completely defensible. This is also why the dark-current channel (C-4) is not a side-effect but a prerequisite.

**MODE D telemetry (accumulated per 60 s, 58 bytes)**

| Offset | Field | Type |
|---|---|---|
| 0–2 | sync + type `0x11` | u8×3 |
| 3–6 | t_ms | u32 |
| 7–8 | altitude | i16 |
| 9–10 | frames_in_window | u16 |
| 11–12 | **cluster_count** | u16 |
| 13–14 | clusters_size1 | u16 |
| 15–16 | clusters_size2_3 | u16 |
| 17–18 | clusters_size4_8 | u16 |
| 19–20 | clusters_size9plus | u16 |
| 21–22 | mean_peak_DN | u16 |
| 23–24 | max_peak_DN | u16 |
| 25–26 | dark_level_DN×100 | u16 |
| 27–28 | dark_rms_DN×100 | u16 |
| 29–30 | hot_pixel_count | u16 |
| 31–32 | T_sensor | i16 (°C×8) |
| 33–34 | exposure_ms | u16 |
| 35–50 | 8 × brightest-cluster (x,y) as u8 pairs | — |
| 51–52 | window_index | u16 |
| 53–54 | reserved | |
| 55–56 | CRC16 | u16 |
| 57 | end | u8 |

**Expected result plot**
Clusters/minute vs altitude with Poisson error bars (±10% in 10 bins), showing the rise through the Pfotzer region — overlaid with the dark-current curve falling as the sensor cools, which demonstrates the rise is not a thermal artefact. **That overlay is the scientific argument.**

---

### 7.4 Design C — OPTIC-24 / MODE C (Cross-check): camera + strain fusion

The camera views the instrumented side panel at 60° through a small folding mirror (so the camera stays in the warm box and only a mirror is exposed). Four fiducials on the panel's inner face.

**The fusion, concretely**

```
 camera:   w_cam(t)    every 2 s   absolute, σ ≈ 10 µm, no drift
 strain:   w_str(t)    80 Hz       relative, σ ≈ 3 µm, drifts with temperature

 complementary filter, crossover f_c = 0.05 Hz (τ = 20 s):
     w_fused[k] = a · (w_fused[k-1] + Δw_str[k]) + (1 − a) · w_cam[k]
     with a = τ / (τ + Δt)

 residual r(t) = w_str(t) − w_cam(t)   ← plotted against panel temperature,
                                         this IS the strain system's drift curve
```

**Why this is the most defensible concept in the document:** it produces a result *whether or not the panel bends*. If deflection is large, you get agreement (or disagreement) at high SNR. If deflection is near zero, you get both systems' noise floors and drift rates measured against each other in flight. There is no outcome in which you have nothing to say.

**MODE C telemetry** — 4× w_cam, 4× w_str, 4× w_fused, 4× residual, all i16 µm = 32 bytes, plus header/temps/CRC = 58 bytes.

**Expected plots**
1. Three traces (camera / strain / fused) vs time, with altitude on a second axis.
2. Residual vs panel temperature — the drift calibration curve.
3. A Bland–Altman style agreement plot: mean of the two methods vs their difference.

---

## PART 8 — CAMERA AND ELECTRONICS ARCHITECTURE

### 8.1 The interface question, answered

**The STM32H750's DCMI (parallel camera interface) is not usable on this platform.** DCMI requires 11 dedicated pins simultaneously (D0–D7, PIXCLK, HSYNC, VSYNC) plus an I²C for SCCB. IntroBus_L exposes roughly 24 signal pins in total, several already committed, and — decisively — **the IntroSat guide's own list of payload-accessible MCU peripherals (ADC, DAC, COMP, OPAMP, DFSDM, HRTIM; I²C, SPI, UART×3, RS485, CAN) does not include DCMI**, while several standard DCMI pins (PC8–PC11, PB6/PB7) are already consumed by the SD card and the service I²C bus.

> ⚠️ **Verify before you commit:** check the DCMI alternate-function table in the STM32H750VB datasheet against the IntroBus_L pin list yourself. I am confident DCMI is unavailable, but this is a claim you should confirm rather than inherit.

So the camera must connect over **SPI or UART**, which leaves three real architectures.

### 8.2 The three architectures

| | **Option 1: ESP32-S3 camera board as co-processor** ✅ | Option 2: ArduCAM on SPI2 | Option 3: Raspberry Pi Zero 2 W |
|---|---|---|---|
| Link to STM32 | UART3 + GPIO sync | SPI2 + I²C4 | UART |
| Raw grayscale? | **Yes** (`PIXFORMAT_GRAYSCALE`) | Limited; module is JPEG-oriented | Yes |
| Max non-JPEG size | **QVGA on ESP32**; VGA+ on ESP32-S3 with PSRAM | VGA into 1 MB STM32 RAM | Full res |
| Where CV runs | On the ESP32-S3 (240 MHz dual-core + PSRAM) | On the STM32H750 (480 MHz M7) | On Linux |
| Power | ~250 mA capture, ~20 mA idle → **256 mW at 25% duty** | ~120 mA | **300–900 mA, 1.5–3 W** |
| Temperature | ESP32-S3 −40…+85; OV2640 −30…+70 | OV2640 −30…+70 | Same + Linux |
| Boot/robustness | Instant boot, no filesystem to corrupt | Instant | **Linux boot ~20 s; SD corruption on power loss** |
| Availability in Tashkent | **ESP32-CAM widely stocked**; XIAO ESP32S3 Sense by order | AliExpress only, 2–4 wk | Scarce, expensive |
| Cost | **$6–14** | ~$25 | ~$30 + camera |
| Firmware you write | **Two** (ESP32 CV + STM32 flight) | One | One + OS wrangling |
| **Verdict** | **RECOMMENDED** | Good single-MCU fallback | **REJECT on reliability** |

**Why Option 1 wins, stated plainly:** the measurement needs *uncompressed* pixels (JPEG destroys single-pixel particle events and degrades sub-pixel centroiding), the ESP32-S3 has the RAM and the mature `esp32-camera` driver to deliver them, it costs $14, it boots instantly, and it gives you a second embedded platform to program — which is more of the engineering work you said you want, not less. The cost is a second firmware and a UART protocol to define, both of which are squarely in your role.

> ⚠️ **Board choice matters.** The plain **ESP32-CAM** (AI-Thinker) is limited by the driver to **QVGA for non-JPEG formats**. An **ESP32-S3** board (XIAO ESP32S3 Sense, or a Freenove ESP32-S3-WROOM CAM) can hold larger uncompressed grayscale frames. Buy an ESP32-CAM locally **today** to start developing, and order an ESP32-S3 board as the flight article. The firmware is nearly identical.

### 8.3 Camera configuration — the settings that make it an instrument

These are not defaults. Every one of them must be set explicitly, and getting them wrong quietly ruins the science:

| Setting | Value | Why |
|---|---|---|
| Pixel format | **GRAYSCALE** | No Bayer interpolation (which smears single-pixel events and blurs marker edges); no JPEG artefacts |
| AEC (auto exposure) | **OFF**, fixed | Photometric determinism. An auto-exposing camera changes its own transfer function mid-experiment |
| AGC (auto gain) | **OFF**, fixed | Same |
| AWB (auto white balance) | **OFF** | Same |
| Lens correction / BPC / WPC | **OFF** | On-chip "bad pixel correction" would delete your cosmic-ray events |
| Downsize/scaling | as needed | Verify whether the sensor **bins or crops** for QVGA — it changes the effective detector area for MODE D. Test on the ground by comparing hit rates at two resolutions |
| Exposure MODE M | 5–20 ms | Short, LED-lit, freeze any vibration |
| Exposure MODE D | 500–1000 ms | Long, dark, maximise particle collection time |
| Lens | 6 mm M12, fixed focus, locked with thread-lock | 112 µm/px at 60 mm; no autofocus to drift |

**"Turn off every automatic feature" is the single most important instruction in this section.** The features that make a camera good at taking pictures are exactly the features that make it bad at taking measurements.

### 8.4 Illumination

4 × 5 mm LEDs in a ring around the lens, behind a diffuser (a piece of baking paper works), driven at a **constant current** (150 Ω series from 3.3 V ≈ 20 mA each) and switched by one MOSFET. Total 80 mA for ~100 ms per capture → negligible energy.

Use **white or 850 nm IR**. IR has one advantage worth noting: it makes the metrology immune to any stray visible light leaking into the bay, and the OV2640 without an IR-cut filter is sensitive there. But it also makes the bay less light-tight for MODE D. **Recommendation: white LEDs + a proper light-tight baffle**, tested on the ground by taking a dark frame with the bay closed and confirming the mean level equals the lens-capped level.

### 8.5 Thermal design — the part that decides whether you get data

| Element | Requirement | Solution |
|---|---|---|
| OV2640 sensor | > 0 °C for stable image; > −30 °C to function | Inside the insulated box, on the aluminium baseplate |
| ESP32-S3 | > −40 °C | Same box; it dissipates ~0.8 W when capturing, which **is itself a heater** |
| microSD | consumer cards often only 0…+70 °C | **Industrial-grade card (−40…+85 °C)**, inside the box |
| Lens | plastic M12 focal length drifts with T | **Measured, not fought** — that is channel C-4 |
| Bay | keep above 0 °C for 3 h | 15 mm XPS foam + the electronics' own ~1.2 W + a 2 W resistive heater on thermostat |

**Heat budget (computed).** Two decisions make this easy, and both are worth stating in your defence because they show you sized the problem rather than guessed at it.

*Decision 1 — set the right target temperature.* The camera does not need to be at room temperature. It needs to be **above −30 °C to function**, and image drift above that is handled by the in-frame reference fiducial (Test 3), not by thermal control. So the target is **bay ≥ −20 °C**, i.e. ΔT ≈ 35 K above ambient, not 60 K. That alone cuts the heat load by 42%.

*Decision 2 — keep the box small.* Conduction loss scales with surface area, and the internal width available inside a 3U is only about 90 mm, which caps the insulation thickness. Computed options (XPS, k ≈ 0.034 W/m·K at ground; ≈0.025 at 30 hPa because gas conduction in the foam pores drops):

| Inner box | Insulation | Outer | Loss at ΔT=35 K (ground / altitude) | Energy over 3 h |
|---|---|---|---|---|
| 90 × 90 × 90 mm | 15 mm | 120 mm ✗ | 5.9 W / 4.3 W | *does not fit a 3U* |
| 60 × 60 × 150 mm | 15 mm | 90 mm | 3.4 W / 2.5 W | 7.6 Wh |
| **55 × 55 × 120 mm** | **15 mm** | **85 mm** | **2.6 W / 1.9 W** | **5.7 Wh** |
| 50 × 50 × 100 mm | 20 mm | 90 mm | 1.5 W / 1.1 W | 3.3 Wh |

The **55 × 55 × 120 mm box** is the recommendation: it holds a camera, a 60 mm object distance and the target assembly, it fits inside a 3U with 15 mm of foam, and it loses under 2 W at altitude. Since the electronics inside already dissipate **~1.2 W**, the thermostatted trim heater only has to supply **~0.7–1 W ≈ 2–3 Wh** over the whole flight — a few per cent of the pack.

**Run this calculation for your actual box before you cut any foam.** The camera bay is the one place where the first dossier's "everything is rated to −40 °C" warning becomes mission-critical — and it is also the place where a five-minute spreadsheet turns a mission-ending risk into a rounding error in the power budget.

---

## PART 9 — FIRMWARE AND COMPUTER-VISION ARCHITECTURE

### 9.1 Division of labour between the two processors

| | **ESP32-S3 (vision co-processor)** | **STM32H750 (flight computer)** |
|---|---|---|
| Owns | camera, LEDs, frame buffer, CV | time, sensors, SD, radio, mission state |
| Does | capture, dark subtract, threshold, label, centroid, cluster stats | timestamp, fuse, log, compress, transmit, fault-handle |
| Never does | mission decisions, telemetry formatting | pixel arithmetic |
| If it dies | STM32 detects UART silence, flags it, **continues the whole strain/thermal mission** | camera keeps logging to its own SD as a fallback |

**That last row is the reliability argument for the two-processor design:** neither processor's failure takes the mission with it.

### 9.2 ESP32-S3 firmware

```c
setup():
  camera_config: GRAYSCALE, FRAMESIZE_QVGA (or VGA on S3), fb_location=PSRAM, fb_count=1
  esp_camera_init()
  s = esp_camera_sensor_get()
  s->set_gain_ctrl(s,0); s->set_exposure_ctrl(s,0); s->set_whitebal(s,0);
  s->set_aec2(s,0); s->set_bpc(s,0); s->set_wpc(s,0); s->set_raw_gma(s,0); s->set_lenc(s,0);
  s->set_agc_gain(s, FIXED_GAIN); s->set_aec_value(s, FIXED_EXPOSURE);
  load_or_build_hot_pixel_map();
  uart_init(115200);

loop():
  cmd = wait_for_uart_command();        // 'M' metrology | 'D' dark | 'C' cross-check | 'S' status
  latch_sync_edge();                    // GPIO from STM32 — this is the timestamp anchor
  fb = esp_camera_fb_get();
  switch(cmd):
    'M': blobs = find_blobs(fb, THRESH_M);        // expect 8
         centroids = subpixel_centroid(blobs);     // intensity-weighted
         scale = 25.00f / dist(centroids[REF_A], centroids[REF_B]);
         emit_M_record(centroids, scale, frame_stats(fb));
         if (store_this_frame) write_frame_to_sd(fb);
    'D': clusters = find_clusters(fb, mean+8*sigma);
         clusters = reject_hot(clusters, hot_map);
         update_hot_map(clusters);
         emit_D_record(histogram(clusters), dark_stats(fb));
    'C': centroids = subpixel_centroid(find_blobs(fb, THRESH_C));
         emit_C_record(apply_homography(centroids));
  esp_camera_fb_return(fb);
```

### 9.3 The computer vision, algorithm by algorithm

All deterministic. All testable against synthetic images with known ground truth. **No machine learning anywhere.**

**(a) Sub-pixel centroid — the workhorse**
```
for each blob above threshold:
    ΣI = Σ p(x,y)
    cx  = Σ x·p(x,y) / ΣI
    cy  = Σ y·p(x,y) / ΣI
```
Intensity-weighted centroiding of a well-exposed circular marker reaches **0.02–0.05 px** routinely (the published low-cost DIC work reports displacement noise floors of 10⁻³–10⁻² px with larger subsets). At 112 µm/px that is **2–6 µm**. Use **filled circles 2–3 mm across (≈20–27 px)**, printed matte black on white, not crosses or squares — circles are rotation-invariant and their centroid is unbiased under defocus.

**(b) Per-frame scale from the reference pair**
```
scale_mm_per_px = 25.00 / ‖c_A − c_B‖
scale_ppm       = (scale − scale_cal) / scale_cal × 1e6
```
This is C-4 and it makes every other number in the frame immune to lens thermal drift.

**(c) Connected-component labelling** (one pass, union-find) for both blob finding and particle clustering. ~50 lines.

**(d) Cluster classification for MODE D**
```
size 1 px        → likely a low-energy hit or noise (report separately)
size 2–8 px      → typical normal-incidence particle
size >8, elongated → a glancing track (compute eccentricity; a real track discriminator)
coordinate in hot_map → REJECT
```

**(e) Homography for the oblique panel view (MODE C)**
Calibrate once on the ground using a printed grid: solve the 3×3 homography **H** mapping image coordinates to panel coordinates. Then out-of-plane deflection follows from the marker's displacement along the projected normal direction, divided by sin(60°) = 0.87.

**(f) Tenengrad focus metric** — `Σ(Gx² + Gy²)` over a small ROI. Detects fogging, defocus and condensation as a single number per frame. 6 lines, and it is your "is the optical path still healthy?" telltale.

**(g) What is NOT computed onboard:** full-field DIC. Correlating 32×32 subsets across a whole frame is the right tool for a deformation field, but it belongs on the ground station, run afterwards from the stored frames. Onboard you extract 8 centroids; on the ground you can run DIC on the same images and compare — a free second analysis path from the same data.

### 9.4 STM32 firmware additions (on top of the dossier-1 architecture)

```
scheduler additions:
  @0.5 Hz : alternate MODE M / MODE D            (so each gets 1 sample / 2 s)
  @0.5 Hz : MODE C when the strain payload is present
  @1 Hz   : build and transmit the appropriate 58-byte packet
  @1/60Hz : emit the accumulated MODE D window packet

camera supervision (this is the fault-tolerance that protects the 15 flight points):
  if no UART response within 1500 ms      → retry once
  if two consecutive timeouts             → pulse camera power (MOSFET on the 5 V feed), re-init
  if three power cycles fail              → set CAM_DEAD flag, stop polling, CONTINUE THE MISSION
  if blobs_found != 8                     → flag DEGRADED, still log what was found
  if focus_metric drops >40 % from baseline → flag OPTICAL_DEGRADED (fogging/frost)
  if scale_ppm exceeds ±5000              → flag SCALE_IMPLAUSIBLE, do not apply it
```

**Note the third line.** A camera that stops must not be able to stop the flight computer. That is a five-line policy that converts a likely partial failure into a graceful degradation, and it is worth more rubric points than any additional sensor.

### 9.5 Ground-station software

```
pyserial → frame sync → CRC16 → dispatch by packet type
   0x10 MODE M → CSV: t, alt, d[6], T[5], scale_ppm, quality flags
   0x11 MODE D → CSV: t, alt, counts, size histogram, dark stats
   0x12 MODE C → CSV: t, alt, w_cam[4], w_str[4], w_fused[4], residual[4]

live views (pyqtgraph):
   1. ΔL vs T, six live traces          ← the dilatometer, updating in flight
   2. clusters/min vs altitude, with Poisson bars
   3. dark level & hot-pixel count vs sensor temperature
   4. camera/strain/fused deflection + residual
   5. health strip: blobs_found, focus_metric, scale_ppm, cam_status

post-flight (offline, from the SD frames):
   6. full-field DIC on the stored frames → deformation field
   7. per-frame particle images, montaged → the "event gallery"
```

### 9.6 The dynamic visualisation

Requested explicitly, so here it is designed in full — **"The Instrument Panel"**, a single screen reconstructing what the camera sees as physics rather than as pictures:

```
┌──────────────────────────────────────────────────────────────────────┐
│  LEFT: synthetic target view, reconstructed from telemetry only      │
│    · 6 coupon markers drawn at their reported positions,             │
│      displacement exaggerated ×20 so motion is visible               │
│    · each marker coloured by its coupon's live temperature           │
│    · ghost outlines showing the launch positions                     │
│    · the 25.00 mm scale bar redrawn every frame from scale_ppm,      │
│      so the lens's thermal breathing is visible as the bar pulsing   │
│                                                                      │
│  CENTRE: ΔL vs T, six live traces, with the fitted slope (α) for     │
│    each material updating as points accumulate. The aluminium        │
│    trace has the handbook 23.6 ppm/K line drawn behind it —          │
│    watching the fit converge onto the known value IS the demo.       │
│                                                                      │
│  RIGHT: particle sky — a 320×240 canvas accumulating every detected  │
│    cluster as a dot, hot pixels in grey and rejected, real events    │
│    in white and sized by cluster size. It fills up visibly faster    │
│    as the altitude counter climbs. Below it, counts/min vs altitude  │
│    building live with Poisson error bars.                            │
│                                                                      │
│  BOTTOM: altitude/time scrubber — drag it and the whole screen       │
│    replays. Two hours of flight in ninety seconds.                   │
└──────────────────────────────────────────────────────────────────────┘
```

**The demonstration that wins the room:** bring the target assembly and a can of freeze spray to the defence. Chill one coupon; on screen, that marker moves, its trace bends, and its fitted α updates live. Then cap the lens and let the particle sky accumulate a few real cosmic-ray hits in front of the jury. **The same software does the live demo, the flight monitoring and the playback.** Build it once, use it three times.

---

## PART 10 — GROUND TESTING PLAN

The rubric gives **8 points for ground testing** and this payload is unusually easy to test completely, because the entire experiment lives inside a box you can put in a freezer. Use that.

| # | Test | Method | Pass criterion |
|---|---|---|---|
| **T1** | Basic camera + electronics function | ESP32-S3 captures a grayscale frame, sends 8 centroids over UART, STM32 timestamps and logs | Frame received, 8 blobs found, round-trip < 500 ms |
| **T2** | **Synthetic-image algorithm validation** | Feed the CV pipeline **generated images with known marker positions** shifted by exact sub-pixel amounts | Recovered displacement matches ground truth to **< 0.05 px RMS**. *Do this before you touch hardware — it separates algorithm bugs from optical problems forever* |
| **T3** | Optical calibration | Image a printed grid of known pitch; solve lens distortion (k1, k2, p1, p2) and the MODE C homography | Residual reprojection error < 0.3 px |
| **T4** | **Displacement calibration against independent truth** | Mount one coupon on a micrometer stage (or use stacked feeler gauges); command 0, 25, 50, 100, 200, 500 µm | Linear, R² > 0.995, slope within 2% of the optical scale factor |
| **T5** | **Freezer run (−18 °C, 2 h)** | Whole assembly in a chest freezer with a datalogger | All six ΔL curves produced; **aluminium coupon's fitted α within 10% of 23.6 ppm/K** — this is the instrument-validity test |
| **T6** | **Dry-ice run (−78 °C)** | Insulated cooler with dry ice; ramp over 60 min to mimic ascent | Camera still produces usable frames at bay temperature ≥ −20 °C; record the exact temperature at which each subsystem degrades |
| **T7** | Thermal cycling ×5 | +25 ↔ −40 °C | Calibration repeats within 5%; no marker debonds; no solder cracks |
| **T8** | Reduced pressure | Vacuum jar to ≤ 100 hPa (hand pump) or ≤ 30 hPa (rotary vane) | No fogging, no outgassing film on the lens, focus unchanged |
| **T9** | **Light-tightness** | Dark frame with the bay closed vs lens physically capped | Mean levels equal within 1 DN. **If they are not, MODE D is measuring light, not particles** |
| **T10** | **Long-duration dark run (12 h)** | Bench, room temperature, MODE D only | Ground cluster rate measured (expect ~5–6/h for a 1/4″ sensor); hot-pixel map converges and stops growing |
| **T11** | **Binning-vs-cropping check** | Compare MODE D rates at QVGA and at the largest available frame size, several hours each | Establishes the sensor's **effective detector area** — without this the MODE D absolute rate is uncalibrated |
| **T12** | Vibration | Mount on an orbital sander / speaker for 10 min, then run T1 | Markers still tracked; camera mount has not shifted (check the reference fiducial position) |
| **T13** | Power-loss and restart | Cut power mid-capture, 20 times | Both processors reboot, append to the log (never overwrite), resume in the correct mode |
| **T14** | **SD failure and corruption handling** | Remove the card mid-flight; insert a deliberately corrupted card | Firmware flags it and **continues**; no `while(!SD.begin())` anywhere |
| **T15** | Camera-death handling | Unplug the camera's UART mid-run | STM32 retries, power-cycles, flags `CAM_DEAD`, and the strain/thermal mission continues uninterrupted |
| **T16** | Telemetry failure | Disconnect the antenna load (into a dummy load, never open) | Main loop never blocks; SD log unaffected |
| **T17** | EMI | Key the LoRa at 30 dBm during capture | No corrupted frames; no shifted centroids. Blank the capture window during TX if needed |
| **T18** | **Full 3 h mission rehearsal** | Batteries only, freezer if possible, ground station running, one person acting as flight director | Complete dataset, zero CRC errors, all three modes cycling, and a plot produced from the received CSV alone |

**The three tests that actually decide the mission:** T2 (algorithm proven against synthetic truth), T5 (aluminium α recovered correctly — the instrument is real), and T9 (the bay is genuinely dark — MODE D is real). If those three pass, almost nothing else can go badly wrong.

---

## PART 11 — TELEMETRY AND DATA ARCHITECTURE

### 11.1 The bandwidth reality

One QVGA grayscale frame is **75 kB**. At the LoRa link's sustainable ~58 bytes/second, transmitting one frame takes **21 minutes**. The architecture is therefore not a choice:

| Option | Verdict |
|---|---|
| A — full images on SD, only extracted parameters downlinked | ✅ **This is the architecture** |
| B — occasional thumbnails | ⚠️ A 40×30 thumbnail is 1.2 kB ≈ 21 packets ≈ 21 s. **Send one every 10 minutes as a health check only** — never as science |
| C — feature vectors | ✅ Same as A; this *is* what the parameters are |
| D — event-triggered images | ⚠️ Only a 32×32 crop around an interesting particle cluster (1 kB). Occasionally, for the presentation |
| E — image statistics | ✅ Already in every packet (mean, RMS, focus metric) |

**The rule: the mission must be scientifically complete using only the downlinked numbers.** Every hypothesis in Part 5 can be tested from the 58-byte packets alone. The stored frames are an *upgrade* — they enable ground-side DIC, the particle-event gallery and the presentation imagery — not a dependency. This matters because the organisers explicitly warn of a possible water landing.

### 11.2 Storage budget

| Mode | Cadence | Per frame | Over 3 h |
|---|---|---|---|
| MODE M frames (metrology) | 1 per 2 s → 5400 frames | 75 kB raw | **405 MB** |
| MODE D frames | store only frames containing a cluster (~900 of them) | 75 kB | **68 MB** |
| MODE C frames | 1 per 2 s when enabled | 75 kB | 405 MB |
| Numeric records (all modes) | 1 Hz | 58 B | 0.6 MB |
| **Total** | | | **~0.9 GB** |

A 16 GB industrial microSD has 17× margin. **Store everything.** If the card fills or fails, the numeric stream continues regardless.

*Optimisation if you want it:* for MODE M you only care about eight small regions. Storing **8 × 64×64 px crops (32 kB) instead of the full frame** cuts storage by 60% and still allows full ground-side re-analysis of every marker. Implement this only if T-tests show an SD bottleneck.

### 11.3 Downlink schedule

```
t mod 4 s == 0 : MODE M packet   (type 0x10)  ← dilatometer
t mod 4 s == 1 : MODE C packet   (type 0x12)  ← camera/strain fusion
t mod 4 s == 2 : strain/thermal packet (type 0x01, from dossier 1)
t mod 4 s == 3 : housekeeping    (type 0x03)  ← power, temps, status bits
every 60 s     : MODE D window packet (type 0x11) replaces one housekeeping slot
every 600 s    : one 40×30 thumbnail (21 packets, sent during a quiet phase)
```
Sustained rate: **1 packet/second at 2400 baud = 19% duty cycle.** Five times the margin you need, which is exactly where you want to be on a link that has to reach 25–80 km.

---

## PART 12 — 38-DAY DEVELOPMENT PLAN

**Today: Friday 18 September 2026. Hard deadline: Monday 26 October 2026.**
Priorities: **[M]** must · **[S]** should · **[N]** nice.

### Week 0 — Unblock (Days 0–2, Fri 18 – Sun 20 Sep)
| Day | Task | Pri |
|---|---|---|
| **0 — Fri 18** | **Order today:** ESP32-S3 camera board ×2 (XIAO ESP32S3 Sense or Freenove ESP32-S3 CAM), 6 mm M12 lens ×2, industrial microSD ×2. *AliExpress to Tashkent is 2–4 weeks — this is the critical path, not the code* | **[M]** |
| 0 | **Buy locally today:** ESP32-CAM (AI-Thinker) ×2 for development, LEDs, MOSFETs, DS18B20, XPS foam, JST connectors | **[M]** |
| 1 — Sat 19 | Ask the organisers: are cameras permitted as payload, and are there restrictions on illuminators, lasers or optical sources? *(The IntroSat guide never mentions a camera — it is entirely your payload.)* | **[M]** |
| 2 — Sun 20 | **Start T2 today, with no hardware**: write the CV pipeline against synthetically generated marker images and prove < 0.05 px | **[M]** |

### Week 1 — Proof of concept (Days 3–9, Mon 21 – Sun 27 Sep)
| Day | Task | Pri |
|---|---|---|
| 3 — Mon 21 | ESP32-CAM: grayscale capture, **all auto-features disabled**, frame into PSRAM | **[M]** |
| 4 — Tue 22 | Blob finding + sub-pixel centroid on real frames; print the first target sheet | **[M]** |
| 5 — Wed 23 | **T4 displacement calibration** on a micrometer stage — prove micrometres, not pixels | **[M]** |
| 6 — Thu 24 | UART protocol ESP32 ↔ STM32; sync-pulse timestamping | **[M]** |
| 7 — Fri 25 | MODE D: long exposure, dark frame, cluster finder, hot-pixel map. Start the **12 h bench dark run (T10)** tonight | **[M]** |
| 8 — Sat 26 | Build the coupon/target assembly: 5–6 coupons, markers, scale-reference bar, aluminium baseplate | **[M]** |
| 9 — Sun 27 | **GATE: T5 freezer run.** If the aluminium coupon's α does not come out near 23.6 ppm/K, stop and fix the optics before going further | **[M]** |

### Week 2 — Electronics and integration (Days 10–16, Mon 28 Sep – Sun 4 Oct)
| Day | Task | Pri |
|---|---|---|
| 10 — Mon 28 | Payload wiring: LED driver, heater, DS18B20 bus, camera power switching via MOSFET | **[M]** |
| 11 — Tue 29 | Insulated bay built (55 × 55 × 120 mm inner, 15 mm XPS); thermostat loop working | **[M]** |
| 12 — Wed 30 | **T9 light-tightness** and **T11 binning check** | **[M]** |
| 13 — Thu 1 Oct | STM32 scheduler: mode alternation, packet building, SD logging | **[M]** |
| 14 — Fri 2 | Camera supervision and fault handling (retry → power-cycle → CAM_DEAD → continue) | **[M]** |
| 15 — Sat 3 | LoRa end-to-end: all four packet types decoding on the ground station | **[M]** |
| 16 — Sun 4 | Ground station v1: CSV logging + live plots. **Data must be landing on disk tonight** | **[M]** |

### Week 3 — Science modes and fusion (Days 17–23, Mon 5 – Sun 11 Oct)
| Day | Task | Pri |
|---|---|---|
| 17 — Mon 5 | MODE C: mirror, oblique view, **T3 homography calibration** | **[S]** |
| 18 — Tue 6 | Complementary-filter fusion of camera + strain; residual channel | **[S]** |
| 19 — Wed 7 | The dynamic visualisation ("Instrument Panel") | **[S]** |
| 20 — Thu 8 | Second complete target assembly (flight + spare/demo) | **[M]** |
| 21 — Fri 9 | Integrate into the 3U; **weigh everything** (3 kg limit) | **[M]** |
| 22 — Sat 10 | **T6 dry-ice run** to −78 °C | **[M]** |
| 23 — Sun 11 | Fix whatever T6 broke. *Something will. This day exists for that* | **[M]** |

### Week 4 — Qualification (Days 24–30, Mon 12 – Sun 18 Oct)
| Day | Task | Pri |
|---|---|---|
| 24 — Mon 12 | T7 thermal cycling ×5; re-verify calibration | **[M]** |
| 25 — Tue 13 | T8 reduced pressure; T12 vibration | **[M]** |
| 26 — Wed 14 | T13/T14/T15/T16 — power loss, SD failure, camera death, telemetry loss | **[M]** |
| 27 — Thu 15 | T17 EMI with the radio at 30 dBm | **[S]** |
| 28 — Fri 16 | Anisotropy coupons (C-5) added if schedule allows | **[N]** |
| 29 — Sat 17 | Optical lever / slope channel (C-6) if schedule allows | **[N]** |
| 30 — Sun 18 | **T18 full 3 h mission rehearsal** | **[M]** |

### Week 5 — Freeze and defend (Days 31–37, Mon 19 – Sun 25 Oct)
| Day | Task | Pri |
|---|---|---|
| **31 — Mon 19** | **HARDWARE AND FIRMWARE FREEZE** | **[M]** |
| 32 — Tue 20 | Test report: all 18 tests, methods, results, photographs | **[M]** |
| 33 — Wed 21 | Theory write-up: optical geometry, centroid error model, CTE theory, particle-rate expectation with Poisson statistics | **[M]** |
| 34 — Thu 22 | Presentation built around the **live demo**, not slides | **[M]** |
| 35 — Fri 23 | Screen-record the demo and a flight-playback as backup | **[S]** |
| 36 — Sat 24 | Full dress rehearsal of the defence | **[M]** |
| 37 — Sun 25 | Pack: flight unit, spare target assembly, spare camera, freeze spray for the demo, printed calibration record, laptop, LoRa dongle, antenna | **[M]** |
| **38 — Mon 26** | **Finals begin** | |

### Scope control
| Behind by | Cut | Keep |
|---|---|---|
| 2 days | C-6 optical lever, C-5 anisotropy coupons | Everything else |
| 4 days | MODE C fusion; fly the camera as a standalone dilatometer + particle counter | MODE M, MODE D, C-4 |
| 1 week | Reduce to 4 coupons; drop the dynamic visualisation to static plots | MODE M, MODE D |
| 2 weeks | **Fly MODE D alone.** A dark camera in a box counting particles needs no target, no LEDs, no calibration fixture — and still returns a complete altitude-resolved dataset | MODE D + dark-current characterisation |

---

## PART 13 — COMPONENTS AND COST

| Part | Function | Interface | V | Qty | $ ea | $ | Availability | Alternatives | Limitation |
|---|---|---|---|---|---|---|---|---|---|
| **XIAO ESP32S3 Sense** (OV2640 + PSRAM + microSD, 21 × 17.5 mm) | Camera + vision co-processor | UART + GPIO | 3.3/5 | 2 | 14 | 28 | Order online | Freenove ESP32-S3 CAM; ESP32-CAM (QVGA limit) | ESP32-S3 −40…+85; **OV2640 −30…+70, stable image 0…+50** |
| **ESP32-CAM (AI-Thinker)** | Development unit, flight backup | UART | 5 | 2 | 6 | 12 | **Stocked in Tashkent** | — | **QVGA max for non-JPEG** |
| M12 lens, 6 mm, fixed focus | 112 µm/px at 60 mm | — | — | 2 | 4 | 8 | Online | 3.6 mm (wider, 187 µm/px) | Plastic element → focal length drifts with T (**measure it: channel C-4**) |
| White 5 mm LED + 150 Ω | Controlled illumination | GPIO + MOSFET | 3.3 | 8 | 0.15 | 1.20 | Local | 850 nm IR LED | Must be **constant current**, not PWM-dimmed during capture |
| AO3400 N-MOSFET | LED / heater / camera power switching | GPIO | 3.3 | 6 | 0.10 | 0.60 | Local | IRLML2502 | Logic-level gate required |
| DS18B20 (TO-92) | Coupon + bay temperatures | 1-Wire, 1 GPIO | 3.3 | 8 | 1.00 | 8.00 | Local | LM75A on I²C4 | −55…+125 °C; 750 ms conversion |
| **Industrial microSD 16 GB (−40…+85 °C)** | Frame storage | SDMMC / SPI | 3.3 | 2 | 12 | 24 | Order online | SanDisk / Kingston Industrial | **Consumer cards are often 0…+70 °C — this is a real failure mode** |
| Coupon stock: PLA, PETG, FR4, Al 6061, CFRP | CTE specimens, 150 mm | — | — | 6 | ~1 | 6 | Local / print | any 3 of these | Document print orientation |
| Printed fiducial targets (matte photo paper or laser-printed film) | Markers + scale reference | — | — | — | — | 2 | Local | Laser-cut anodised aluminium | Filled circles 2–3 mm, matte black on white |
| Aluminium baseplate + L-brackets | Common reference structure | — | — | 1 | 6 | 6 | Local | 3 mm Al sheet | **Camera, coupon anchors and scale bar must share one plate** |
| 10 mm first-surface mirror | MODE C folding / optical lever | — | — | 2 | 2 | 4 | Local | polished Al shim | Ordinary mirrors give a double image; first-surface preferred |
| XPS foam 15 mm | Camera bay insulation | — | — | — | — | 4 | Local | PIR foam, Styrofoam | k ≈ 0.034 W/m·K |
| 47 Ω 2 W resistors ×2 | Trim heater | MOSFET | 3.3/5 | 2 | 0.20 | 0.40 | Local | Kapton heater pad | ~1 W total needed |
| JST B2B-EH-A + terminal blocks | Flight-legal connections | — | — | — | — | in kit | — | **PLS-2 headers are forbidden for flight** |
| 470 µF low-ESR capacitor | Camera supply decoupling | — | 16 V | 2 | 0.30 | 0.60 | Local | 220 µF ×2 | Keep at the camera board |
| **TOTAL** | | | | | | **≈ $105** | | | |

**Minimum viable set ≈ $45:** one ESP32-CAM bought locally, one lens, LEDs, 4 DS18B20, 4 coupons, printed targets, foam, one industrial SD. Everything else is an upgrade.

**Sourcing rule, repeated from the first dossier because it is still the thing most likely to sink you:** buy locally what you can (`electron.uz`, Uzum, the Tashkent radio market), order the rest **today**, and make sure the ESP32-CAM-based fallback works so the mission still flies if nothing arrives from abroad.

---

## PART 14 — FAILURE-MODE ANALYSIS

| # | Failure | Likelihood | Effect | Detection | Mitigation |
|---|---|---|---|---|---|
| F1 | **Camera stops below its rated temperature** | Medium | All optical channels lost | UART timeout; `T_camera` telemetry | **Heated, insulated bay (Part 8.5)**; target ≥ −20 °C; T6 establishes the real limit; ESP32 self-heating helps |
| F2 | **Lens fogs or frosts** | Low (inside a sealed warm bay) | Blurred markers | **Tenengrad focus metric** drops >40% | Sealed bay with a small silica-gel sachet; bay stays warmer than ambient so it is never the cold surface |
| F3 | **Auto-exposure left enabled** | **Medium — this is the classic own-goal** | Photometry meaningless; MODE D thresholds wander | `exposure_us` and `frame_mean` in telemetry are constant or not | Explicitly disable AEC/AGC/AWB/BPC/WPC/LENC at init and **verify by reading the registers back** |
| F4 | **JPEG compression left on** | Medium | Single-pixel particle events destroyed (documented failure in the prior balloon attempt) | Frame size in bytes | Force `PIXFORMAT_GRAYSCALE`; assert the frame length equals width × height |
| F5 | **Light leak into the bay** | Medium | MODE D counts photons, not particles | T9; dark level rises with external illumination or spin phase | Light-tight baffle; black interior; verify dark level against a lens-capped reference |
| F6 | **Hot pixels mistaken for particles** | **High if unhandled** | Inflated, meaningless count rate | Hot-pixel count channel; cluster coordinates repeating | Persistence-based hot-pixel map built on the pad and updated in flight; report rejected count separately |
| F7 | **Marker debonds or moves** | Medium | One coupon channel lost | `blobs_found` ≠ 8 | Redundant markers; graceful degradation to the remaining coupons; the aluminium reference is the one to protect most |
| F8 | **Camera mount vibrates** | Medium | Apparent marker motion that is really camera shake | Reference fiducial moves in image coordinates | **Everything is differential against the in-frame fiducial**; IMU cross-check isolates shake |
| F9 | **Lens focal length drifts** | **Certain** | Scale error on every measurement | `scale_ppm` channel | Per-frame scale from the 25.00 mm reference bar — the drift is measured and divided out (and reported as result C-4) |
| F10 | **microSD fails in the cold** | Medium | Frames lost | Write returns 0 | Industrial card; **numeric telemetry is scientifically complete without any frame** |
| F11 | **ESP32 brown-out during capture** | Medium | Frame lost, possible reboot loop | Boot counter in telemetry | 470 µF bulk cap; separate MOSFET power switch so the STM32 can power-cycle it; capture never overlaps LoRa TX |
| F12 | **UART desync between processors** | Medium | Garbled records | Framing bytes + CRC on the UART link too | Length-prefixed frames, CRC8, resync on the sync byte; timeout → flush |
| F13 | **Camera death takes the mission with it** | **Would be fatal** | Everything lost | — | **Explicit policy: three failed power cycles → `CAM_DEAD` → stop polling → continue the strain/thermal mission.** Five lines of code |
| F14 | **Not enough particles** | Medium | MODE D statistics weak | Live count rate | Estimated ~900 events → ±10% in 10 bins. **If the rate is low, the dark-current curve is still a complete result** |
| F15 | **Illumination changes between frames** | Low | Centroid bias | `frame_mean` channel | Constant-current LEDs, fixed exposure, differential measurement |
| F16 | **Storage fills** | Low | Frames stop | Free-space check each write | 16 GB vs 0.9 GB needed; fall back to ROI crops; numeric log always continues |
| F17 | **Condensation on descent** | Medium | Late-flight optical degradation | Focus metric | Sealed bay + desiccant; and if it happens, **it is logged and reportable rather than mysterious** |
| F18 | **You break the target assembly during integration** | Medium | Mission lost | Obvious | **Build two.** The spare is also the defence-room demo unit |

**Read the shape of this table:** almost every high-likelihood failure is a *configuration* failure (F3, F4, F6) or a *thermal* failure (F1, F10), not a physics failure. Configuration failures are defeated by reading registers back and asserting on frame size. Thermal failures are defeated by a foam box and a 1 W resistor. Both are cheap. That is why this payload is reliable.

---

## PART 15 — THE SAFEST HIGH-VALUE CONCEPT

**MODE D — the dark-frame particle counter and sensor characteriser.**

It is the safest thing in this document for one reason that is worth stating precisely: **it requires nothing to happen.** No target must stay bonded, no LED must light, no marker must stay in focus, no panel must bend, no mechanism must move. A powered camera in a dark box produces a complete, analysable dataset from the moment it boots.

And it is not a consolation prize:
- It produces **two** results, not one: the particle-rate profile through the Regener–Pfotzer region, and the dark-current/hot-pixel curve as the sensor cools — and the second is the control that proves the first is not a thermal artefact. That overlay is a genuinely strong piece of scientific reasoning.
- It fills a **documented gap**: the one published balloon attempt at CMOS particle counting states it could not correlate rate with altitude. You can.
- It costs **16 bytes per minute** of telemetry.
- It is **fully ground-testable**: a 12-hour bench run gives you the sea-level rate and the hot-pixel map before you ever fly.
- The honest weakness — statistics — is quantified up front: ~900 events, ±10% in 10 altitude bins. You can state that error bar in advance and then show you hit it.

If you build only one thing from this document, build this. Then add MODE M on top of it, because they share every component.

---

## PART 16 — THE MOST TECHNICALLY INTERESTING CONCEPT

**The closure test: measuring one beam three independent ways and checking whether the calculus holds.**

For a bending beam, the deflection w, the slope θ and the curvature κ are related by differentiation:
```
        dw/dx = θ          dθ/dx = κ
```
Three separate instruments can measure these three quantities independently:

| Quantity | Instrument | Physics | Noise | Drift |
|---|---|---|---|---|
| **w** (deflection) | camera, oblique fiducial tracking | geometric optics | ~10 µm | none (differential) |
| **θ** (slope) | mirror + LED + screen, optical lever | reflection angle doubling | ~0.01° | small |
| **κ** (curvature) | strain-gauge array | resistance change | ~0.5–2 µε | **significant, thermal** |

So you can measure all three and ask: **do they agree?** Differentiate the camera's w twice and compare with the gauges' κ. Integrate the gauges' κ twice and compare with the camera's w. Check the mirror's θ against both.

Why this is the best engineering question in the document:
- **It cannot return nothing.** If the panel bends a lot, you test agreement at high SNR. If it barely bends, you measure three instruments' noise floors and drift rates against each other in flight. Both are results.
- **It is quantitative and falsifiable**, with propagated error bars — exactly the language the analysis rubric band uses.
- **It produces a genuinely useful by-product**: the camera, being drift-free, calibrates the strain system's thermal drift *in flight*. That inverts the usual relationship (people normally calibrate cameras with contact sensors) and is the part I could not find published.
- **It is deeply a technician's project**: three different front ends, three different noise models, a complementary filter, a synchronisation problem, and an error-propagation analysis.

**Its cost:** it needs the strain payload from the first dossier *and* the camera payload *and* the optical lever — the most integration work of anything here, and the lowest reliability score of the top concepts (3/5 on failure risk, because it depends on three subsystems). That is the trade. It is the right one only if Weeks 1–2 go smoothly.

---

## PART 17 — THE BEST CAMERA + ELECTRONICS COMBINATION, AND THE TRADE-OFFS

You asked me not to just name a winner, so here are the four decisions that actually matter, each with the trade stated plainly.

### Decision 1 — Where does the camera point?

| | **Inward (at a controlled target)** | Outward (at the world) |
|---|---|---|
| Thermal survival | Camera in a warm box; manageable | −55 °C, outside the OV2640's stable band |
| Repeatability | Identical on a desk and at 24 km | Depends on weather, pointing, spin, time of day |
| Ground validation | Complete | Impossible |
| Novelty | The measurement is the novelty | Class A, thousands of prior flights |
| Risk of no data | Very low | Moderate to high |
| "Wow" factor for a lay audience | Lower | Higher |
| **Trade** | **You give up pretty pictures and gain a calibrated instrument** | |

**This is the decision everything else depends on**, and I think it is not close. Take pretty pictures with a second, cheap, expendable outward camera if you want them for the presentation — but do not let your score depend on it.

### Decision 2 — One processor or two?

| | **Two (ESP32-S3 + STM32H750)** | One (ArduCAM on STM32 SPI) |
|---|---|---|
| Uncompressed grayscale | Yes, well-supported driver | Possible but poorly supported |
| CV development speed | Fast (PSRAM, examples, plenty of RAM) | Slower (128 kB flash, hand-rolled) |
| Failure isolation | **Camera death cannot stop the flight computer** | A camera fault can hang the flight loop |
| Firmware you maintain | Two | One |
| Sync complexity | Needs a hardware sync pulse | Inherent |
| Power | 256 mW average at 25% duty | ~120 mW |
| Cost / availability | $14, or $6 locally | $25, AliExpress only |
| **Trade** | **More firmware, more robustness, more of the engineering work you said you want** | Simpler system, tighter coupling, more risk |

I recommend two — but note honestly that "two firmwares" is a real cost on a 38-day schedule, and if you are more comfortable staying entirely in STM32 C++, Option 2 is a legitimate choice that costs you image quality and buys you simplicity.

### Decision 3 — How many measurement modes?

Adding MODE D to MODE M costs **zero hardware and about four days of firmware**. Adding MODE C costs a mirror, a homography calibration and dependence on the strain payload. C-4 (lens f(T) and scale self-calibration) costs nothing at all — it is already in every frame.

**The efficient frontier is: MODE M + MODE D + C-4.** That is three results, one camera, one lens, one target, one firmware. MODE C is the upgrade you take only if the strain payload is already working by 5 October.

### Decision 4 — What do you optimise for?

Here is the honest tension, stated as plainly as I can:

- **If you optimise for the rubric**, build MODE D + MODE M. Near-certain complete dataset, two independent results, ground-testable end to end, ~$45, and the flight-performance and analysis lines (27 of 60 points) are close to locked.
- **If you optimise for the most interesting engineering**, build the closure test (Part 16). It is the project you will learn the most from and the one a technically strong judge will remember — but it depends on three subsystems, and three subsystems is how missions fail.
- **If you optimise for the demonstration**, build MODE M plus the Instrument Panel visualisation. Freeze spray on a coupon, a marker moving on screen, and a fitted α converging on the handbook value in real time is the single most persuasive ninety seconds available to you.

**My reading of your constraints** — reliability first, 38 days, substantial embedded work, a complete dataset, and something a judge will call an instrument rather than a camera — is that you should **build MODE D + MODE M + C-4 as the committed mission, treat MODE C as a stretch goal with a hard decision date of 5 October, and build the Instrument Panel because it is what turns the numbers into something a room full of people can see.**

But that is a judgement about risk appetite, and it is yours to make. The engineering in Parts 7–14 is the same either way.

---

### Two sentences to keep in front of you

> **Every automatic feature you leave enabled on that camera converts it back from an instrument into a camera.**
> **Every measurement referenced to a fiducial in the same frame is a measurement that cannot be ruined by the cold.**

---

## APPENDIX A — SOURCES AND COMPUTED VALUES

### New evidence used in this pass

**Camera hardware**
- [OV2640 datasheet (OmniVision, via UCTronics)](https://www.uctronics.com/download/cam_module/OV2640DS.pdf) — **"Operation −30 °C to 70 °C, Stable Image 0 °C to 50 °C"**; 1/4″ optical format; image area **3590 × 2684 µm**; 2.2 µm pixels; RAW RGB / RGB565 / YUV422 / JPEG; 15 fps UXGA, 30 fps SVGA, 60 fps CIF; 125 mW active, 600 µA standby
- [OV2640 datasheet mirror (Waveshare)](https://www.waveshare.com/w/upload/9/92/Ov2640_ds_1.8_.pdf)
- [espressif/esp32-camera driver (ESP Component Registry)](https://components.espressif.com/components/espressif/esp32-camera) — supported formats incl. **GRAYSCALE**; supported sensors (OV2640, OV5640, SC031GS global-shutter mono, HM0360 …); **"For ESP32, do not use sizes above QVGA when not JPEG"**; PSRAM required for most non-JPEG use
- [STM32H750VBT6 datasheet (DS12556)](https://www.tme.eu/Document/8f91193d73dea76fbd1a26bfe6d6bed7/STM32H750VBT6.pdf) — used to check DCMI availability against the IntroBus_L pin list
- [ESP32 camera board comparison: ESP32-CAM vs ESP32-S3-CAM vs XIAO Sense](https://circuitdigest.com/articles/esp32-camera-dev-board-comparison)
- [Seeed XIAO ESP32S3 Sense product page](https://www.seeedstudio.com/XIAO-ESP32S3-Sense-p-5639.html)
- [Arducam OV9281 / OV7251 global-shutter monochrome modules](https://www.arducam.com/arducam-ov9281-mipi-1mp-monochrome-global-shutter-camera-module-raspberry-pi.html) — considered and rejected (MIPI CSI-2, needs a Raspberry Pi host)

**CMOS sensors as particle detectors**
- [Development of a cosmic-ray detector using CMOS sensors in smartphones and Raspberry Pi devices (SORAMAME), *Experimental Astronomy* 2026](https://arxiv.org/html/2607.02106v1) — **ground reference rate 17.10 events/hour on a 29.6 mm² IMX477**; adaptive thresholding + OpenCV clustering + a hot-pixel "noise list" of up to 20,000 entries; *"no increase in thermal-noise-related event rates was observed"*
- [Detecting cosmic rays using CMOS sensors in consumer devices (Academic High Altitude Conference)](https://via.library.depaul.edu/cgi/viewcontent.cgi?httpsredir=1&article=1021&context=ahac) — the balloon attempt: GoPro, 0.24 cm² sensor, threshold cuts; **"due to the lack of GPS on GoPros it was not possible to correlate altitude to rate of events detected"**; camera post-processing "can lead to events being blurred out"
- [Recognition and classification of cosmic-ray events in images captured by CMOS/CCD cameras (CREDO, ICRC 2019)](https://arxiv.org/pdf/1909.01929)
- [Measurement of smartphone sensor efficiency to cosmic-ray muons (arXiv:2107.06332)](https://arxiv.org/abs/2107.06332)

**Optical metrology and vision-based structural measurement**
- [Low-cost digital image correlation and strain measurement for geotechnical applications, *Strain* 2020 (Cambridge)](https://api.repository.cam.ac.uk/server/api/core/bitstreams/e3b04f0c-d1e2-4e8a-8ab0-422318ae6a2f/content) — Raspberry Pi camera; **displacement noise floor 1×10⁻³ to 1×10⁻² px; strain noise floor 37.5–468 µε**; ZNCC with biquintic B-spline interpolation; grey-level noise 0.4%; ~$125/camera
- [Low-cost two-dimensional digital image correlation system](https://www.sciencedirect.com/org/science/article/pii/S0315897724000557)
- [Vision and vibration data fusion-based structural dynamic displacement measurement, *Sensors* 2023](https://www.mdpi.com/1424-8220/23/9/4547) — establishes vision+acceleration fusion as mature prior art
- [Real-time structural displacement estimation by fusing asynchronous acceleration and computer-vision measurements (2026)](https://www.sciencedirect.com/science/article/pii/S1093968726002495)
- [Displacement estimation based on optical and inertial sensor fusion (PMC)](https://pmc.ncbi.nlm.nih.gov/articles/PMC7922896/)
- [Measuring frequency of one-dimensional vibration with a video camera using the electronic rolling shutter](https://www.researchgate.net/publication/324484728_Measuring_frequency_of_one-dimensional_vibration_with_video_camera_using_electronic_rolling_shutter)
- [Extreme detectable vibration frequency limited by rolling-shutter imaging of laser speckles, *Optics Letters* 2023](https://opg.optica.org/ol/abstract.cfm?uri=ol-48-15-3837)
- [Accurate full-field optical displacement measurement using a digital camera and repeated patterns, *Optics Express* 2014](https://opg.optica.org/oe/fulltext.cfm?uri=oe-22-8-9693&id=283529)
- [Displacement measurement with CCD moiré considering relative rotation of gratings, *Optics Express* 2025](https://opg.optica.org/oe/fulltext.cfm?uri=oe-33-1-1111)

**CubeSat camera context**
- [A survey of camera modules for CubeSats — imaging payload of ICUBE-1](https://www.researchgate.net/publication/259532420_A_survey_of_camera_modules_for_CubeSats_-_Design_of_imaging_payload_of_ICUBE-1)
- [Design and environmental testing of the imaging payload for the KITSUNE 6U CubeSat, *Frontiers in Space Technologies* 2022](https://www.frontiersin.org/articles/10.3389/frspt.2022.1000219/full)
- [On-orbit smart camera system to observe illuminated and unilluminated space objects (arXiv:1809.02042)](https://arxiv.org/pdf/1809.02042)
- [Thermal analysis of high-altitude ballooning payloads (University of Virginia)](https://libraetd.lib.virginia.edu/downloads/cv43nx33b?filename=1_Hughes_Paul_2018_BS.pdf)

**Carried over from the first dossier** (competition rules, the 60-point rubric, IntroSat platform specification, flight profile, environment): see `ORBITA26_Technician_Mission_Research.md` §21.

### Values computed for this document

| Quantity | Result |
|---|---|
| Pixel scale, QVGA + 6 mm lens at 60 mm | 36 mm field, **112 µm/px**; 0.05 px = **5.6 µm** |
| Pixel scale, QVGA + 3.6 mm lens at 60 mm | 60 mm field, 187 µm/px; 0.05 px = 9.4 µm |
| Coupon ΔL over −70 K, 150 mm | PLA 735 µm (131σ) · PETG 630 µm · Al 248 µm (44σ) · FR4 168 µm (30σ) · CFRP 21 µm (3.7σ) |
| Oblique-view sensitivity to out-of-plane motion | 0.50× at 30°, **0.87× at 60°**, 0.97× at 75° |
| Optical lever, mirror to screen 80 mm | 0.25° → 0.70 mm = 3.7 px; 1° → 2.79 mm = 14.9 px |
| Moiré amplification, 0.2 mm gratings at 2° | **≈28×** |
| Bi-material cantilever (PLA/Al, 0.5+0.5 mm, L=50 mm, ΔT=70 K), Timoshenko | κ = 2.31 m⁻¹ → tip deflection **2.9 mm** |
| Particle rate, OV2640 (9.6 mm²), scaled from the published IMX477 ground rate | 5.6/h at ground; **278–834/h at the Pfotzer maximum (×50–150)** |
| Flight-integrated particle count | **≈900 events** → 10 altitude bins at **±10.4%** Poisson error (5 bins: ±7.4%) |
| Storage, QVGA grayscale | 75 kB/frame; 1 frame/2 s → **405 MB per 3 h**; full mission ≈ **0.9 GB** |
| LoRa airtime for one QVGA frame at 2400 baud | **≈21 minutes** — hence numbers-only downlink |
| Camera bay heat loss, 55 × 55 × 120 mm inner + 15 mm XPS, ΔT = 35 K | 2.6 W at ground, **1.9 W at altitude** → 5.7 Wh; electronics supply ~1.2 W of it |
| Payload power, camera duty-cycled 25% | **≈256 mW average** |

---

## APPENDIX B — WHAT TO DO IN THE NEXT 48 HOURS

1. **Order the ESP32-S3 camera boards, 6 mm M12 lenses and industrial microSD cards today.** Shipping is the critical path; firmware is not.
2. **Buy an ESP32-CAM locally today** so development starts tomorrow regardless of what ships.
3. **Ask the organisers whether cameras and internal illuminators are permitted**, since the IntroSat platform has no camera and the guides never mention one — this is entirely your payload, and Regulations §7.13 defers safety rules to the first day of finals.
4. **Start Test T2 with no hardware at all:** generate synthetic marker images shifted by exact sub-pixel amounts and prove your centroid algorithm recovers them to better than 0.05 px. This is the one piece of work that needs nothing to arrive, and it permanently separates algorithm bugs from optical problems.
5. **Print the first target sheet** — filled black circles 2–3 mm across on matte white, plus two scale fiducials exactly 25.00 mm apart.
6. **Put 27 September in the calendar as the gate.** If the aluminium coupon's measured α is not near 23.6 ppm/K by then, the optics need fixing before anything else proceeds.

---

*Prepared 18 September 2026. Every number is sourced in Appendix A or computed in the table above. Where the prior art is genuine, I have said so; where I could find no prior implementation, I have said that too — and said that absence of evidence is not proof of absence.*
