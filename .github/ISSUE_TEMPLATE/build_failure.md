---
name: Build Failure
about: Report a failure when compiling ps4-linux-12xx
title: "[BUILD] "
labels: build
assignees: rmuxnet
---

## Build Environment

| Field | Your answer |
|---|---|
| Host OS / distro | e.g. Arch Linux, Ubuntu 24.04 |
| Host architecture | e.g. x86_64 |
| Clang/LLVM version | paste `clang --version` |
| GNU Make version | paste `make --version` |
| Python version | paste `python3 --version` |
| Available RAM | e.g. 16 GB |
| Available disk space | e.g. 50 GB |

---

## Kernel / Build Information

| Field | Your answer |
|---|---|
| Branch | e.g. 7.0-Stable |
| Commit | paste `git rev-parse HEAD` |
| Build profile | Server / General |
| LTO | ThinLTO / FullLTO |
| Build method | build.sh / manual / GitHub Actions |

---

## Build command used

<!-- Paste the exact command you ran. -->

```
paste here
```

---

## Where did it fail

- [ ] `build.sh` pre-checks (missing firmware, config issues)
- [ ] `olddefconfig`
- [ ] `prepare`
- [ ] `bzImage`
- [ ] `modules`
- [ ] GitHub Actions workflow
- [ ] Other

---

## Error output

> Paste the full error output. If it is very long, attach a `.txt` file or use https://paste.rs or https://gist.github.com and link it here.

You can also attach screenshots or photos of the terminal output directly to this issue. GitHub accepts `.png`, `.jpg`, `.gif`, and `.webp`.

<details>
<summary>Build error output</summary>

```
paste here
```

</details>

<details>
<summary>Full build log (optional)</summary>

```
paste here
```

</details>

---

## Additional context

<!-- Anything else that might help — things you already tried, whether it built on a previous commit, etc. -->

---

> **Before submitting**, please check:
> - [ ] I have read the build instructions in the [README](https://github.com/rmuxnet/ps4-linux-12xx/blob/7.0-Stable/README.md)
> - [ ] I have placed the SD8797 firmware at `extra_firmware/mrvl/sd8797_uapsta.bin` if required by my config
> - [ ] I have searched existing [Issues](https://github.com/rmuxnet/ps4-linux-12xx/issues) for this error
> - [ ] I am on a supported branch (`7.0-Stable` recommended)
