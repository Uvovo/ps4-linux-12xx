---
name: Compatibility Report
about: Report what works and what does not on your specific console model
title: "[COMPAT] "
labels: compatibility
assignees: rmuxnet
---

> This is not a bug report. Use this to document your console's compatibility with ps4-linux-12xx so it can be added to the compatibility table in the README.

---

## Console Information

| Field | Your answer |
|---|---|
| Model | e.g. CUH-1216A |
| Southbridge | Aeolia / Belize / Baikal / unknown |
| Southbridge revision | e.g. B0, B1, unknown |
| WiFi/BT chip | e.g. Marvell 88w8897, MediaTek 7668, unknown |
| Hard drive size | 500 GB / 1000 GB |
| UART available | yes / no |

---

## Kernel Information

| Field | Your answer |
|---|---|
| Branch | e.g. 7.0-Stable |
| Commit | paste `git rev-parse HEAD` or the hash from the release page |
| Build profile | Server / General |
| LTO | ThinLTO / FullLTO |
| Pre-built or self-compiled | pre-built release / self-compiled |

---

## Distro / Userspace

| Field | Your answer |
|---|---|
| Distro | e.g. Arch Linux, Gentoo, Debian |
| Mesa version | |
| Init system | e.g. systemd, OpenRC |

---

## Hardware status

| Component | Status | Notes |
|---|---|---|
| HDMI display | ✔ working / ✘ broken / - untested | |
| WiFi | ✔ working / ✘ broken / - untested | |
| Bluetooth | ✔ working / ✘ broken / - untested | |
| Ethernet | ✔ working / ✘ broken / - untested | |
| Fan control | ✔ working / ✘ broken / - untested | |
| LEDs | ✔ working / ✘ broken / - untested | |
| Power button | ✔ working / ✘ broken / - untested | |
| Internal storage | ✔ working / ✘ broken / - untested | |
| Audio | ✔ working / ✘ broken / - untested | |

---

## Overall status

- [ ] Fully working — daily driver
- [ ] Mostly working — minor issues
- [ ] Partially working — significant issues
- [ ] Barely working — boots but not usable
- [ ] Does not boot

---

## Notes

<!-- Any extra detail about your setup, quirks, workarounds you needed, or things that surprised you. -->

---

## Logs (optional but appreciated)

> Attach a `dmesg` log if you can. It helps even if everything works — it gives us a baseline for your model.

You can provide logs in any of the following ways:

- **Paste directly** into the code block below
- **Attach a file** — drag and drop a `.txt` or `.log` file directly into this text box
- **Pastebin** — paste at https://paste.rs or https://gist.github.com and link it here
- **Screenshot or photo** — attach images directly, GitHub accepts `.png`, `.jpg`, `.gif`, and `.webp`

<details>
<summary>dmesg (optional)</summary>

```
paste here
```

</details>
