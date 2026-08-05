# Rock Classification

> Reference implementation of the lookup. For how a scan is actually run on the rover — the 5-second window, majority voting over ~25 samples, and the confidence score — see [docs/10-dual-board-architecture.md § 10.7](../docs/10-dual-board-architecture.md#107-rock-classification).

## Classification Logic

Rocks are identified by combining three sensor readings: IR pulse rate, ultrasonic presence, and magnetic field direction.

The two IR rates emitted by the rocks are **312 Hz** and **547 Hz**; 450 Hz is the midpoint used as the decision boundary. Magnetic direction is decided from the sign of the Z-axis field with a ±170 µT dead band.

| Rock Type | IR Rate (λ s⁻¹) | Ultrasonic | Magnetic Direction |
|-----------|-----------------|------------|--------------------|
| Basaltoid | > 450           | Present    | Down               |
| Gravion   | < 450           | Absent     | Down               |
| Regolix   | < 450           | Present    | Up                 |
| Lunarite  | > 450           | Absent     | Up                 |

## Code

```cpp
String classifyRock(int irRate, bool ultrasonicPresent, String magDirection) {
  if (irRate > 450 && ultrasonicPresent && magDirection == "DOWN")
    return "Basaltoid";
  if (irRate < 450 && !ultrasonicPresent && magDirection == "DOWN")
    return "Gravion";
  if (irRate < 450 && ultrasonicPresent && magDirection == "UP")
    return "Regolix";
  if (irRate > 450 && !ultrasonicPresent && magDirection == "UP")
    return "Lunarite";
  return "Unknown";
}
```
