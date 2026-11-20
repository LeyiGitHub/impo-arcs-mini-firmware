# Code from elsewhere

Copied unchanged; fix bugs upstream or in Impo's own files around it, never
here, so a newer copy can replace these files as they are.

| Folder | Source | Version | Licence |
| --- | --- | --- | --- |
| `m5stack/hal/drivers/bmi270/` | [m5stack/StackChan](https://github.com/m5stack/StackChan) `firmware/main/hal/drivers/bmi270/` | firmware v1.6.2, commit `28b9f0c8bd6acd81cb3c70f1a93c3e373b39d400` | MIT (`m5stack/LICENSE`); Bosch's BMI270 SensorAPI inside it has its own (`BMI270_SensorAPI/LICENSE`, BSD-3-Clause) |
| `m5stack/hal/utils/motion_detector/` | the same, `firmware/main/hal/utils/motion_detector/` | the same | MIT |

Only the files the CoreS3 uses were copied: of Bosch's SensorAPI, `bmi2.*`
and `bmi270.*`, not its other feature variants.
