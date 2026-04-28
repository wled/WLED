# WLED License FAQ

WLED is licensed under the **European Union Public Licence v1.2 (EUPL-1.2)**.  
The full license text is in the [`LICENSE`](../LICENSE) file.

---

## Q: Is the EUPL-1.2 license "viral"?

**Yes, but only in a limited and well-defined way.**

"Viral" (or copyleft) means that if you distribute a *modified* version of WLED,
you must publish those modifications under a compatible open-source license.

However, the obligation is **narrowly scoped**:

- It applies **only when you distribute the software** (e.g., flash it onto devices
  you sell or otherwise hand out). Running WLED privately for yourself does not
  trigger it.
- It applies **only to the software itself** — not to anything else in your product
  (see the hardware question below).
- Distributing an **unmodified** WLED binary just requires attribution and a link to
  the source. You do not need to publish anything new.
- Distributing a **modified** WLED requires that your changes are made available as
  open source under a compatible license (EUPL-1.2 or one of the licenses listed in
  Appendix of the EUPL, such as GPL-2.0, GPL-3.0, LGPL, MPL-2.0, etc.).

So the license is viral in the standard copyleft sense, but it does **not** reach
further than the software derivative itself.

---

## Q: Does the "same licence" requirement also apply to my hardware design?

**No.**

The EUPL-1.2 is a *software* license. Its copyleft clause only covers the
**software** (the firmware source code and its derivatives). It has **no legal
effect on hardware**.

Your schematics, PCB layout files, bill of materials, enclosure designs, and any
other hardware-related work are entirely your own. You can keep them proprietary,
patent them, or license them however you like. The EUPL does not require you to
share or open-source any of that.

> **Practical example:** You design a custom PCB with a proprietary layout, write a
> custom WLED usermod, and sell the finished device. You must publish the usermod
> source code (because you are distributing modified WLED firmware), but your PCB
> files remain 100% yours to keep private.

This is consistent with the general principle that software copyleft licenses
(GPL, LGPL, MPL, EUPL, etc.) do not extend to hardware. Hardware is governed by
different legal frameworks — patents, design rights, trade secrets — not software
copyright licenses.
