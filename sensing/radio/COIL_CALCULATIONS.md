# Ferrite Core Coil — Radio Wave Detector

A hand-wound ferrite core coil built for detecting radio waves. A ferrite core was chosen over a plain air core coil as it significantly increases inductance for the same number of turns and physical size, improving sensitivity to weak radio signals. The higher permeability of the ferrite concentrates the magnetic field more effectively, which helps when picking up low-amplitude radio waves.

## Coil Specifications

| Parameter | Value |
|---|---|
| Core material | Ferrite |
| Core diameter | 1 cm |
| Core length | 5.5 cm |
| Number of turns | 120 |
| Wire | 0.40 mm copper |

---

## Theoretical Calculations

### Inductance

The cross-sectional area of the core:

$$A = \pi r^2 = \pi \times (0.005)^2 = 7.854 \times 10^{-5} \text{ m}^2$$

Baseline air core inductance using the solenoid formula:

$$L_0 = \frac{\mu_0 \cdot N^2 \cdot A}{\ell} = \frac{1.2566 \times 10^{-6} \times 14400 \times 7.854 \times 10^{-5}}{0.055} = 25.84 \text{ μH}$$

Since the coil is relatively short, an end-effect correction factor $K$ is applied to account for flux leakage at the ends:

$$K = \frac{1}{1 + 0.9 \times \frac{2r}{\ell}} = \frac{1}{1 + 0.9 \times 0.182} = 0.859$$

$$L_0^{\text{corrected}} = 0.859 \times 25.84 = 22.2 \text{ μH}$$

For a ferrite rod, the effective permeability $\mu_{eff}$ is lower than the bulk material value due to magnetic flux leaking at the ends of the rod (demagnetisation effect). Using a typical bulk permeability of $\mu_{bulk} = 125$ and a demagnetisation factor $N_d \approx 0.044$ for this rod's aspect ratio ($L/D = 5.5$):

$$\mu_{eff} = \frac{\mu_{bulk}}{1 + N_d \cdot (\mu_{bulk} - 1)} = \frac{125}{1 + 0.044 \times 124} = \frac{125}{6.46} \approx 19.3$$

$$\boxed{L_{theoretical} = \mu_{eff} \times L_0^{corrected} = 19.3 \times 22.2 \approx \textbf{429 μH}}$$

---

### Resistance

Total wire length, accounting for the wire sitting on top of the core:

$$\ell_{wire} = N \times \pi \times (d_{core} + d_{wire}) = 120 \times \pi \times (0.010 + 0.0004) = \textbf{3.92 m}$$

DC resistance using copper resistivity $\rho = 1.68 \times 10^{-8}$ Ω·m:

$$A_{wire} = \pi \times (0.0002)^2 = 1.257 \times 10^{-7} \text{ m}^2$$

$$\boxed{R_{theoretical} = \frac{\rho \cdot \ell_{wire}}{A_{wire}} = \frac{1.68 \times 10^{-8} \times 3.92}{1.257 \times 10^{-7}} \approx \textbf{0.52 Ω}}$$

---

## Measured Values & Comparison

| | Theoretical | Measured | Difference |
|---|---|---|---|
| Inductance | 429 μH | 514 μH | ~85 μH (~17%) |
| Resistance | 0.52 Ω | — | — |


---

*Part of a radio wave detector project.*