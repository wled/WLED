# Preset Migration Guide

## Upgrading from WLED 0.13.x to 0.14.x

When upgrading from WLED 0.13.x to 0.14.x, presets that were saved
in the previous version may not work correctly due to a change in
how segments are indexed.

## What Changed

In WLED 0.13.x, segments were indexed starting from 1.
In WLED 0.14.x, segments are indexed starting from 0.

This means presets saved in 0.13.x that reference segment IDs will
have incorrect segment references after upgrading.

## How to Fix

After upgrading to 0.14.x, re-save all of your presets from the UI
to update the segment indexing. This will resolve any segment-related
issues caused by the migration.

## Example

In 0.13.x a preset segment entry looks like this:
{"id":1,"start":0,"stop":50,...}

In 0.14.x the same segment looks like this:
{"id":0,"start":0,"stop":50,...}

## Caution: Do Not Manually Edit Segment IDs

Segment IDs in WLED are ordinal and not stable identifiers. 
Do not manually edit segment IDs in your saved preset JSON files.
The order of segments in `strip.segments` can change at runtime,
and manual ID edits can cause presets to break after segment 
reorder, add, or remove events. Always re-save presets through 
the UI instead of editing JSON directly.
