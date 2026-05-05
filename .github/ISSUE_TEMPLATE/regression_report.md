---
name: Regression Report
about: Report something that worked on a previous branch or commit and is now broken
title: "[REGRESSION] "
labels: regression
assignees: rmuxnet
---

> A regression is when something that previously worked has stopped working. Please be as specific as possible about what changed — this is critical for bisecting.

---

## Console Information

| Field | Your answer |
|---|---|
| Model | e.g. CUH-1216A |
| Southbridge | Aeolia / Belize / Baikal / unknown |
| WiFi/BT chip | e.g. Marvell 88w8897, MediaTek 7668, unknown |
| UART available | yes / no |

---

## What broke

<!-- Describe clearly what is no longer working. -->

---

## Last known good

| Field | Your answer |
|---|---|
| Branch | e.g. 7.0-Stable, 6.18.21-Strawberry |
| Commit | paste the commit hash where it last worked |
| Build profile | Server / General |
| LTO | ThinLTO / FullLTO |

---

## First known bad

| Field | Your answer |
|---|---|
| Branch | e.g. 7.0-Stable |
| Commit | paste the commit hash where it first broke |
| Build profile | Server / General |
| LTO | ThinLTO / FullLTO |

---

## Have you bisected

- [ ] Yes — I have identified the commit that introduced this
- [ ] Partially — I have narrowed it down to a range
- [ ] No — I only know the branch where it broke

If you have bisected or narrowed it down, paste the relevant commit(s) here:

<!-- paste commit hash(es) or range -->

---

## Distro / Userspace

| Field | Your answer |
|---|---|
| Distro | e.g. Arch Linux, Gentoo, Debian |
| Mesa version | |
| Init system | e.g. systemd, OpenRC |

---

## Hardware status before and after

| Component | Last good | Current (broken) |
|---|---|---|
| HDMI display | ✔ / ✘ / - | ✔ / ✘ / - |
| WiFi | ✔ / ✘ / - | ✔ / ✘ / - |
| Bluetooth | ✔ / ✘ / - | ✔ / ✘ / - |
| Ethernet | ✔ / ✘ / - | ✔ / ✘ / - |
| Fan control | ✔ / ✘ / - | ✔ / ✘ / - |
| LEDs | ✔ / ✘ / - | ✔ / ✘ / - |
| Power button | ✔ / ✘ / - | ✔ / ✘ / - |
| Internal storage | ✔ / ✘ / - | ✔ / ✘ / - |

---

## Logs

> Please provide `dmesg` from both the last good and current broken kernel if possible. This makes bisecting much easier.

You can provide logs in any of the following ways:

- **Paste directly** into the code blocks below
- **Attach a file** — drag and drop a `.txt` or `.log` file directly into this text box
- **Pastebin** — paste at https://paste.rs or https://gist.github.com and link it here
- **Screenshot or photo** — attach images directly, GitHub accepts `.png`, `.jpg`, `.gif`, and `.webp`

<details>
<summary>dmesg — last known good</summary>

```
paste here
```

</details>

<details>
<summary>dmesg — current broken</summary>

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

---

## Additional context

<!-- Anything else that might help — config changes, userspace updates, hardware changes, things you already tried, etc. -->
