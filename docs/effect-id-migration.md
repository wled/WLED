# Effect ID Changes in WLED 0.16.0

## Overview
When upgrading from WLED 0.15.x to 0.16.0, presets that reference
certain effects by ID may load an unexpected effect. This is because
some effect IDs were reassigned in 0.16.0.

## Known ID Changes

| Old Effect (0.15.x) | Effect ID | New Effect (0.16.0) |
|---------------------|-----------|----------------------|
| Meteor Smooth       | 77        | Copy Segment         |

## Symptoms
If your preset was saved with "Meteor Smooth" in 0.15.x, it may
activate "Copy Segment" after upgrading to 0.16.0, which can cause
LEDs to strobe unexpectedly.

## Fix
Delete and recreate any affected presets after upgrading.

## Workaround for Meteor Smooth
The Meteor Smooth effect is still available in 0.16.0. Use the
"Meteor" effect (ID 76) and enable the new "Smooth" checkbox option
to replicate the previous behavior.
