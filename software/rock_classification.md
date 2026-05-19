# Rock Classification

## Classification Logic

Rocks are identified by combining three sensor readings: IR pulse rate, ultrasonic presence, and magnetic field direction.

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
