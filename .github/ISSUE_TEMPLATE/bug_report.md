---
name: Bug Report
about: Report a problem with ps4-linux-12xx
title: "[BUG] "
labels: bug
assignees: rmuxnet
---

## Console Information

| Field | Your answer |
|---|---|
| Model | e.g. CUH-1216A |
| Southbridge | Aeolia / Belize / Baikal / unknown |
| WiFi/BT chip | e.g. Marvell 88w8897, MediaTek 7668, unknown |
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

## What is broken

<!-- Describe the problem clearly. What did you expect to happen, and what actually happened? -->

---

## Hardware status

| Component | Status |
|---|---|
| HDMI display | ✔ working / ✘ broken / - untested |
| WiFi | ✔ working / ✘ broken / - untested |
| Bluetooth | ✔ working / ✘ broken / - untested |
| Ethernet | ✔ working / ✘ broken / - untested |
| Fan control | ✔ working / ✘ broken / - untested |
| LEDs | ✔ working / ✘ broken / - untested |
| Power button | ✔ working / ✘ broken / - untested |
| Internal storage | ✔ working / ✘ broken / - untested |

---

## Steps to reproduce

1. 
2. 
3. 

---

## Logs

> The more information you provide here, the faster this can be diagnosed. Please include as much as you can.

You can provide logs in any of the following ways:

- **Paste directly** into the code block below
- **Attach a file** — drag and drop a `.txt` or `.log` file directly into this text box, or click the attachment button at the bottom of the editor
- **Pastebin** — if the output is very long, paste it at https://paste.rs or https://gist.github.com and drop the link here
- **Screenshot or photo** — if you have a UART setup or a camera pointed at the screen, attach the image directly to this issue. GitHub accepts `.png`, `.jpg`, `.gif`, and `.webp`

<details>
<summary>dmesg</summary>

```
paste here
```

</details>

<details>
<summary>Additional logs (optional)</summary>

```
paste here
```

</details>

<!-- To attach a file or image: drag it into this text area, or click the paperclip icon at the bottom of the editor when writing your issue. -->

---

## Additional context

<!-- Anything else that might help — previous kernels that worked, things you've already tried, links to related issues or discussions, etc. -->

---

> **Before submitting**, please check:
> - [ ] I have read the [README](https://github.com/rmuxnet/ps4-linux-12xx/blob/7.0-Stable/README.md), especially the compatibility table and release notes
> - [ ] I have searched existing [Issues](https://github.com/rmuxnet/ps4-linux-12xx/issues) and [Discussions](https://github.com/rmuxnet/ps4-linux-12xx/discussions) for this problem
> - [ ] I am running a supported branch (`7.0-Stable` recommended)
> - [ ] I have included logs or attachments
