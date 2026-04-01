#!/usr/bin/env bash

# PS4-Linux Strawberry Builder
# Supports two PS4-focused ThinLTO build profiles:
#   server  — headless/services, HZ=250, PREEMPT_VOLUNTARY, performance governor
#   general — desktop/gaming, HZ=1000, PREEMPT=y, BORE, schedutil/reflex
#
# Usage:
#   ./build.sh                        Interactive menu
#   ./build.sh --option N             Non-interactive (1=build, 2=fetch, 3=both)
#   ./build.sh --option N use=Server  Force profile (Server or General)
#   ./build.sh --option 7             Show/switch build profile

set -euo pipefail

OUTPUT_DIR="${PWD}/out"
FIRMWARE_DIR="${PWD}/extra_firmware"
BASE_URL="https://gitlab.com/kernel-firmware/linux-firmware/-/raw/main"

export KCFLAGS="-march=btver2 -mtune=btver2 -O3"
export KAFLAGS="-march=btver2 -mtune=btver2 -O3"
export HOSTCFLAGS="-Wno-error=incompatible-pointer-types-discards-qualifiers"

PROFILE="server"
JOBS=$(nproc)
MAX_JOBS=$(nproc)

# Parse optional build profile: use=Server or use=General
if [[ $# -ge 3 && "$3" =~ ^use= ]]; then
    PROFILE_ARG="${3#use=}"
    if [[ "${PROFILE_ARG,,}" == "server" ]]; then
        PROFILE="server"
    elif [[ "${PROFILE_ARG,,}" == "general" ]]; then
        PROFILE="general"
    else
        echo "Unknown build profile: ${PROFILE_ARG}. Valid: Server, General"
        exit 1
    fi
fi

if [[ $# -ge 2 && "$1" == "--option" ]]; then
    CHOICE="$2"
    case "$CHOICE" in
        1) DO_BUILD=1; DO_FETCH=0 ;;
        2) DO_BUILD=0; DO_FETCH=1 ;;
        3) DO_BUILD=1; DO_FETCH=1 ;;
        4|5|6)
            echo "--option $CHOICE is not supported in non-interactive mode."
            exit 1
            ;;
        7)
            echo "Current build profile: ${PROFILE}"
            read -p "Switch profile? (y/n): " SWITCH
            if [[ "$SWITCH" =~ ^[Yy]$ ]]; then
                [[ "$PROFILE" == "server" ]] && PROFILE="general" || PROFILE="server"
                echo "Profile switched to: ${PROFILE}"
            fi
            exit 0
            ;;
        *)
            echo "Invalid --option argument: $CHOICE"
            exit 1
            ;;
    esac
    SKIP_MENU=1
else
    SKIP_MENU=0
fi

if [[ "$SKIP_MENU" == "0" ]]; then
    while true; do
        clear
        echo -e "\e[1;35m╔══════════════════════════════════════════════════╗\e[0m"
        echo -e "\e[1;35m║\e[0m \e[1;37mPS4-Linux Strawberry Builder\e[0m                     \e[1;35m║\e[0m"
        echo -e "\e[1;35m╠══════════════════════════════════════════════════╣\e[0m"
        echo -e "\e[1;35m║\e[0m \e[1;32m1)\e[0m Build bzImage                                 \e[1;35m║\e[0m"
        echo -e "\e[1;35m║\e[0m \e[1;32m2)\e[0m Fetch firmware blobs                          \e[1;35m║\e[0m"
        echo -e "\e[1;35m║\e[0m \e[1;32m3)\e[0m Both (fetch + build)                          \e[1;35m║\e[0m"
        echo -e "\e[1;35m║\e[0m \e[1;32m4)\e[0m Threads to use: \e[1;33m$(printf "%-29s" "${JOBS} / ${MAX_JOBS}")\e[0m \e[1;35m║\e[0m"
        echo -e "\e[1;35m║\e[0m \e[1;32m6)\e[0m Build profile: \e[1;33m${PROFILE}\e[0m$(printf "%-22s" "")\e[1;35m║\e[0m"
        echo -e "\e[1;35m║\e[0m \e[1;31m5)\e[0m Quit                                          \e[1;35m║\e[0m"
        echo -e "\e[1;35m║\e[0m \e[1;36m7)\e[0m Show/switch build profile                      \e[1;35m║\e[0m"
        echo -e "\e[1;35m╚══════════════════════════════════════════════════╝\e[0m"
        echo ""
        read -p "Select option [1-7]: " CHOICE

        case "$CHOICE" in
            1) DO_BUILD=1; DO_FETCH=0; break ;;
            2) DO_BUILD=0; DO_FETCH=1; break ;;
            3) DO_BUILD=1; DO_FETCH=1; break ;;
            4)
                read -p "Enter number of threads (1-${MAX_JOBS}): " NEW_JOBS
                if [[ "$NEW_JOBS" =~ ^[0-9]+$ ]] && \
                   [ "$NEW_JOBS" -ge 1 ] && [ "$NEW_JOBS" -le "$MAX_JOBS" ]; then
                    JOBS=$NEW_JOBS
                else
                    echo -e "\e[1;31m[!] Invalid input.\e[0m Press enter to continue."
                    read -r
                fi
                ;;
            5) echo "Exiting."; exit 0 ;;
            6)
                echo ""
                echo "Select build profile:"
                echo "  1) Server  (max throughput, headless)"
                echo "  2) General (gaming/desktop latency)"
                read -p "Profile [1-2]: " PROFILE_CHOICE
                if [[ "$PROFILE_CHOICE" == "1" ]]; then
                    PROFILE="server"
                elif [[ "$PROFILE_CHOICE" == "2" ]]; then
                    PROFILE="general"
                else
                    echo -e "\e[1;31m[!] Invalid input.\e[0m Press enter to continue."
                    read -r
                fi
                ;;
            7)
                echo ""
                echo "Current build profile: ${PROFILE}"
                read -p "Switch profile? (y/n): " SWITCH
                if [[ "$SWITCH" =~ ^[Yy]$ ]]; then
                    [[ "$PROFILE" == "server" ]] && PROFILE="general" || PROFILE="server"
                    echo "Profile switched to: ${PROFILE}"
                    sleep 1
                fi
                ;;
            *) echo -e "\e[1;31m[!] Invalid option.\e[0m"; sleep 1 ;;
        esac
    done
fi

MAKE_OPTS=(
    -j"${JOBS}"
    LLVM=1
    ARCH=x86_64
    HOSTCFLAGS="${HOSTCFLAGS}"
)

if [[ ! -f Makefile ]] || ! grep -q "KERNELRELEASE" Makefile 2>/dev/null; then
    echo -e "\e[1;31mERROR:\e[0m Run this from the kernel source root (ps4-linux-12xx/)." >&2
    exit 1
fi

if [[ ! -f .config ]]; then
    if [[ -f config ]]; then
        echo -e "\e[1;34m[*]\e[0m Moving 'config' -> '.config'"
        mv config .config
    else
        echo -e "\e[1;31mERROR:\e[0m No .config found." >&2
        exit 1
    fi
fi

if [[ "$DO_FETCH" == "1" ]]; then
    CONFIG_LINE=$(grep -E '^CONFIG_EXTRA_FIRMWARE=' .config 2>/dev/null || true)
    if [[ -z "${CONFIG_LINE}" ]]; then
        echo -e "\e[1;31mERROR:\e[0m CONFIG_EXTRA_FIRMWARE not found in .config" >&2
        exit 1
    fi

    BLOBS=$(echo "${CONFIG_LINE}" \
        | sed 's/CONFIG_EXTRA_FIRMWARE="\(.*\)"/\1/' \
        | tr ' ' '\n' \
        | grep -v '^$')

    if [[ -z "${BLOBS}" ]]; then
        echo "CONFIG_EXTRA_FIRMWARE is empty -- nothing to fetch."
    else
        echo -e "\e[1;34m[*]\e[0m Blobs required by CONFIG_EXTRA_FIRMWARE:"
        echo "${BLOBS}" | sed 's/^/    /'
        echo ""
        mkdir -p "${FIRMWARE_DIR}"
        FAILED=()
        while IFS= read -r blob; do
            dest="${FIRMWARE_DIR}/${blob}"
            if [[ -f "${dest}" ]]; then
                echo -e "  \e[1;32m[=]\e[0m Already exists: ${blob}"
                continue
            fi
            mkdir -p "$(dirname "${dest}")"
            echo -e "  \e[1;34m[↓]\e[0m Fetching: ${blob}"
            if curl -fsSL --retry 3 --retry-delay 2 \
                    "${BASE_URL}/${blob}" -o "${dest}"; then
                echo -e "  \e[1;32m[✓]\e[0m ${blob}"
            else
                echo -e "  \e[1;31m[✗]\e[0m FAILED: ${blob}" >&2
                FAILED+=("${blob}")
                rm -f "${dest}"
            fi
        done <<< "${BLOBS}"

        echo ""
        if [[ ${#FAILED[@]} -eq 0 ]]; then
            echo -e "\e[1;32mAll firmware blobs fetched -> ${FIRMWARE_DIR}\e[0m"
        else
            echo -e "\e[1;31mThe following blobs could not be fetched:\e[0m"
            printf '  %s\n' "${FAILED[@]}"
            exit 1
        fi
    fi
fi

if [[ "$DO_BUILD" == "1" ]]; then

    echo -e "\e[1;34m[*]\e[0m Applying invariant config (both profiles)..."

    # ── Build system ─────────────────────────────────────────────────────
    # ThinLTO only: on PS4, FullLTO's extra link time does not buy enough
    # runtime speed to justify the heavier build cost.
    scripts/config --enable  CONFIG_LTO_CLANG_THIN
    scripts/config --disable CONFIG_LTO_CLANG_FULL
    scripts/config --disable CONFIG_LOCALVERSION_AUTO

    # ── Kernel compression ───────────────────────────────────────────────
    # ZSTD decompresses ~3x faster than XZ at boot, negligible size diff.
    scripts/config --disable CONFIG_KERNEL_XZ
    scripts/config --enable  CONFIG_KERNEL_ZSTD

    # ── NUMA removal ─────────────────────────────────────────────────────
    # PS4 is single-node UMA. NUMA=y adds node-aware indirection to every
    # alloc_pages call, zone accounting, and scheduler wake path.
    # Unconditional overhead on this hardware -- remove it entirely.
    scripts/config --disable CONFIG_NUMA
    scripts/config --disable CONFIG_AMD_NUMA
    scripts/config --disable CONFIG_X86_64_ACPI_NUMA
    scripts/config --disable CONFIG_ACPI_NUMA
    scripts/config --disable CONFIG_NUMA_MEMBLKS
    scripts/config --disable CONFIG_NUMA_BALANCING

    # ── Bare-metal PS4 target ────────────────────────────────────────────
    # Both profiles target a native PS4, so trim guest/hypervisor overhead.
    scripts/config --disable CONFIG_HYPERVISOR_GUEST
    scripts/config --disable CONFIG_PARAVIRT
    scripts/config --disable CONFIG_PARAVIRT_XXL
    scripts/config --disable CONFIG_KVM
    scripts/config --disable CONFIG_KVM_AMD
    scripts/config --disable CONFIG_KVM_INTEL

    # ── Memory management ────────────────────────────────────────────────
    # MGLRU: better page reclaim under memory pressure. Mixed anon+file
    # workloads (games loading assets while running) benefit most.
    scripts/config --enable  CONFIG_LRU_GEN
    scripts/config --enable  CONFIG_LRU_GEN_ENABLED
    scripts/config --enable  CONFIG_LRU_GEN_STATS

    # Enable THP support globally; each profile chooses its default mode.
    scripts/config --enable  CONFIG_TRANSPARENT_HUGEPAGE

    # SLUB per-cpu partial lists: reduces slab lock contention under
    # concurrent allocation workloads (games, servers, containers).
    scripts/config --enable  CONFIG_SLUB_CPU_PARTIAL

    # ZSWAP/ZRAM: zstd gives better compression ratio than LZO/LZ4
    # at comparable throughput on Jaguar -- more effective usable RAM.
    scripts/config --disable CONFIG_ZSWAP_COMPRESSOR_DEFAULT_LZO
    scripts/config --enable  CONFIG_ZSWAP_COMPRESSOR_DEFAULT_ZSTD
    scripts/config --set-str CONFIG_ZSWAP_COMPRESSOR_DEFAULT "zstd"
    scripts/config --disable CONFIG_ZRAM_DEF_COMP_LZ4
    scripts/config --enable  CONFIG_ZRAM_DEF_COMP_ZSTD
    scripts/config --set-str CONFIG_ZRAM_DEF_COMP "zstd"
    scripts/config --enable  CONFIG_ZSWAP
    scripts/config --enable  CONFIG_ZRAM

    # ── Async I/O ────────────────────────────────────────────────────────
    # io_uring: was disabled. Used by modern server daemons and game
    # shader compilation pipelines (dxvk/vkd3d async workers).
    scripts/config --enable  CONFIG_IO_URING

    # ── Network ──────────────────────────────────────────────────────────
    # BBR: model-based congestion control. Better throughput/latency than
    # CUBIC under concurrent connections and non-ideal links.
    # FQ: per-flow pacing qdisc. BBR computes a target sending rate; FQ
    # enforces it at the transmit path. Without FQ, BBR's pacing is
    # calculated but never applied. Required pairing.
    scripts/config --enable  CONFIG_TCP_CONG_BBR
    scripts/config --set-str CONFIG_DEFAULT_TCP_CONG "bbr"
    scripts/config --enable  CONFIG_NET_SCH_DEFAULT
    scripts/config --enable  CONFIG_NET_SCH_FQ
    scripts/config --enable  CONFIG_NET_SCH_FQ_CODEL
    scripts/config --enable  CONFIG_NET_SCH_CAKE

    # ── Crypto acceleration ──────────────────────────────────────────────
    # Jaguar has AES-NI + PCLMULQDQ. Hardware paths for AES-GCM used by
    # TLS 1.3, WireGuard, dm-crypt. No software fallback needed.
    scripts/config --enable  CONFIG_CRYPTO_AES_NI_INTEL
    scripts/config --enable  CONFIG_CRYPTO_GHASH_CLMUL_NI_INTEL
    scripts/config --enable  CONFIG_CRYPTO_POLYVAL_CLMUL_NI
    scripts/config --enable  CONFIG_CRYPTO_LIB_SHA256

    # ── Futex ────────────────────────────────────────────────────────────
    # Private hash + MPOL: lower latency mutex/condvar operations.
    # Affects every mutex in every application -- game engines, Wine,
    # system daemons.
    scripts/config --enable  CONFIG_FUTEX
    scripts/config --enable  CONFIG_FUTEX_PI
    scripts/config --enable  CONFIG_FUTEX_PRIVATE_HASH
    scripts/config --enable  CONFIG_FUTEX_MPOL

    # ── NTSYNC ───────────────────────────────────────────────────────────
    # In-kernel NT synchronization primitives for Wine/Proton.
    # Replaces esync/fsync fd-based workarounds entirely.
    # Significantly lower latency NT mutex/event/semaphore for games.
    scripts/config --enable  CONFIG_NTSYNC

    # ── Scheduler ────────────────────────────────────────────────────────
    scripts/config --enable  CONFIG_SCHED_CLASS_EXT
    scripts/config --enable  CONFIG_SCHED_EXT
    # Autogroup: kernel groups tasks by session automatically.
    # Background compilers, package managers get deprioritized as a
    # group vs the active foreground application.
    scripts/config --enable  CONFIG_SCHED_AUTOGROUP

    # ── BPF ──────────────────────────────────────────────────────────────
    # BTF required for sched_ext kfunc resolution at BPF verifier time.
    scripts/config --enable  CONFIG_BPF_SYSCALL
    scripts/config --enable  CONFIG_BPF_JIT
    scripts/config --enable  CONFIG_BPF_JIT_DEFAULT_ON
    scripts/config --enable  CONFIG_DEBUG_INFO_BTF

    # ── I/O schedulers ───────────────────────────────────────────────────
    # Build all in; profile selects the default.
    scripts/config --enable  CONFIG_MQ_IOSCHED_DEADLINE
    scripts/config --enable  CONFIG_MQ_IOSCHED_KYBER
    scripts/config --enable  CONFIG_IOSCHED_BFQ
    scripts/config --enable  CONFIG_BFQ_GROUP_IOSCHED
    scripts/config --enable  CONFIG_BLK_WBT
    scripts/config --enable  CONFIG_BLK_WBT_MQ

    # ── Strip debug overhead ─────────────────────────────────────────────
    # All of these log on hot paths and have no production value.
    scripts/config --disable CONFIG_DMADEVICES_DEBUG
    scripts/config --disable CONFIG_DMADEVICES_VDEBUG
    scripts/config --disable CONFIG_IOMMU_DEBUG
    scripts/config --disable CONFIG_I2C_DEBUG_CORE
    scripts/config --disable CONFIG_I2C_DEBUG_ALGO
    scripts/config --disable CONFIG_I2C_DEBUG_BUS
    scripts/config --disable CONFIG_DM_DEBUG
    scripts/config --disable CONFIG_BLK_DEBUG_FS

    # ── Profile-specific ─────────────────────────────────────────────────
    if [[ "$PROFILE" == "server" ]]; then
        echo -e "\e[1;34m[*]\e[0m Applying server profile..."

        # BORE off: burst-aware interactive bias is irrelevant for
        # server batch/throughput workloads.
        scripts/config --disable CONFIG_SCHED_BORE
        scripts/config --disable CONFIG_SCHED_AUTOGROUP
        scripts/config --disable CONFIG_CPU_FREQ_GOV_REFLEX

        # Keep the server profile on the safer side for exposed services.
        scripts/config --enable  CONFIG_CPU_MITIGATIONS

        # Performance governor: clocks at max, zero scaling latency.
        scripts/config --disable CONFIG_CPU_FREQ_DEFAULT_GOV_SCHEDUTIL
        scripts/config --disable CONFIG_CPU_FREQ_GOV_SCHEDUTIL
        scripts/config --enable  CONFIG_CPU_FREQ_DEFAULT_GOV_PERFORMANCE
        scripts/config --enable  CONFIG_CPU_FREQ_GOV_PERFORMANCE

        # HZ=250: 750 fewer timer interrupts/sec/CPU vs HZ=1000.
        # ~1-2% CPU saving on compute-bound workloads.
        scripts/config --disable CONFIG_HZ_1000
        scripts/config --disable CONFIG_HZ_300
        scripts/config --disable CONFIG_HZ_100
        scripts/config --enable  CONFIG_HZ_250
        scripts/config --set-val CONFIG_HZ 250

        # Tickless idle avoids wasted timer interrupts on parked CPUs
        # without the syscall/interrupt tradeoffs of full dynticks.
        scripts/config --enable  CONFIG_NO_HZ_IDLE
        scripts/config --disable CONFIG_NO_HZ_FULL

        # PREEMPT_VOLUNTARY: better throughput than full preemption.
        # Yields only at explicit schedule points.
        scripts/config --disable CONFIG_PREEMPT
        scripts/config --disable CONFIG_PREEMPT_NONE
        scripts/config --enable  CONFIG_PREEMPT_VOLUNTARY

        # Keep memory/cpu controllers for services and light containers.
        scripts/config --enable  CONFIG_MEMCG
        scripts/config --enable  CONFIG_CGROUP_SCHED
        scripts/config --enable  CONFIG_FAIR_GROUP_SCHED
        scripts/config --disable CONFIG_RT_GROUP_SCHED

        # CFS bandwidth: CPU quota enforcement for containers/cgroups.
        scripts/config --enable  CONFIG_CFS_BANDWIDTH

        # PSI: pressure stall info for systemd-oomd/cgroup2 monitoring.
        # Near-zero overhead when not actively read.
        scripts/config --enable  CONFIG_PSI
        scripts/config --enable  CONFIG_PSI_DEFAULT_DISABLED

        # THP madvise: keeps the TLB win for opted-in workloads without
        # the memory bloat and compaction spikes of always-on THP.
        scripts/config --enable  CONFIG_TRANSPARENT_HUGEPAGE
        scripts/config --disable CONFIG_TRANSPARENT_HUGEPAGE_ALWAYS
        scripts/config --enable  CONFIG_TRANSPARENT_HUGEPAGE_MADVISE

        # mq-deadline: predictable latency under queue depth, better for
        # server HDD/SSD throughput than BFQ.
        scripts/config --set-str CONFIG_DEFAULT_IOSCHED "mq-deadline"

        # FQ is the cleanest default partner for BBR pacing on servers.
        scripts/config --enable  CONFIG_DEFAULT_FQ
        scripts/config --disable CONFIG_DEFAULT_FQ_CODEL
        scripts/config --disable CONFIG_DEFAULT_FQ_PIE
        scripts/config --disable CONFIG_DEFAULT_SFQ
        scripts/config --disable CONFIG_DEFAULT_PFIFO_FAST
        scripts/config --set-str CONFIG_DEFAULT_NET_SCH "fq"

    else
        echo -e "\e[1;34m[*]\e[0m Applying general/gaming profile..."

        # ── Mitigations ──────────────────────────────────────────────────
        # Dedicated desktop/gaming box: strip x86 mitigation overhead for
        # the lowest syscall and context-switch latency on 6.18 LTS.
        scripts/config --disable CONFIG_CPU_MITIGATIONS

        # ── Cgroup / memcg ───────────────────────────────────────────────
        # MEMCG hooks into every alloc_pages. No containers on this box.
        scripts/config --disable CONFIG_MEMCG
        scripts/config --disable CONFIG_CGROUP_SCHED
        scripts/config --disable CONFIG_FAIR_GROUP_SCHED
        scripts/config --disable CONFIG_RT_GROUP_SCHED
        scripts/config --disable CONFIG_CFS_BANDWIDTH

        # ── BORE ─────────────────────────────────────────────────────────
        scripts/config --enable  CONFIG_SCHED_BORE

        # ── CPU frequency ─────────────────────────────────────────────────
        scripts/config --enable  CONFIG_CPU_FREQ_GOV_REFLEX
        scripts/config --enable  CONFIG_CPU_FREQ_DEFAULT_GOV_SCHEDUTIL
        scripts/config --enable  CONFIG_CPU_FREQ_GOV_SCHEDUTIL
        scripts/config --disable CONFIG_CPU_FREQ_DEFAULT_GOV_PERFORMANCE

        # ── Timer / preemption ────────────────────────────────────────────
        # HZ=1000 + NO_HZ_FULL: 1ms resolution + tickless on game cores.
        scripts/config --disable CONFIG_HZ_250
        scripts/config --disable CONFIG_HZ_300
        scripts/config --disable CONFIG_HZ_100
        scripts/config --enable  CONFIG_HZ_1000
        scripts/config --set-val CONFIG_HZ 1000
        scripts/config --disable CONFIG_NO_HZ_IDLE
        scripts/config --enable  CONFIG_NO_HZ_FULL

        # Full preemption: kernel preemptible anywhere safe.
        scripts/config --enable  CONFIG_PREEMPT
        scripts/config --disable CONFIG_PREEMPT_VOLUNTARY
        scripts/config --disable CONFIG_PREEMPT_NONE

        # Always-on THP fits desktop/gaming better than server duty:
        # shader caches, Wine/Proton, and larger userspace heaps benefit.
        scripts/config --enable  CONFIG_TRANSPARENT_HUGEPAGE
        scripts/config --enable  CONFIG_TRANSPARENT_HUGEPAGE_ALWAYS
        scripts/config --disable CONFIG_TRANSPARENT_HUGEPAGE_MADVISE

        # ── I/O ───────────────────────────────────────────────────────────
        # BFQ: isolates game I/O from background noise.
        scripts/config --disable CONFIG_PSI
        scripts/config --set-str CONFIG_DEFAULT_IOSCHED "bfq"

        # fq_codel is the better desktop default for mixed latency traffic.
        scripts/config --enable  CONFIG_DEFAULT_FQ_CODEL
        scripts/config --disable CONFIG_DEFAULT_FQ
        scripts/config --disable CONFIG_DEFAULT_FQ_PIE
        scripts/config --disable CONFIG_DEFAULT_SFQ
        scripts/config --disable CONFIG_DEFAULT_PFIFO_FAST
        scripts/config --set-str CONFIG_DEFAULT_NET_SCH "fq_codel"

    fi

    if [[ -d "${FIRMWARE_DIR}" ]] && [[ -n "$(ls -A "${FIRMWARE_DIR}" 2>/dev/null)" ]]; then
        echo -e "\e[1;34m[*]\e[0m Setting CONFIG_EXTRA_FIRMWARE_DIR=${FIRMWARE_DIR}"
        scripts/config --set-str CONFIG_EXTRA_FIRMWARE_DIR "${FIRMWARE_DIR}"
    else
        echo -e "\e[1;33m[!]\e[0m WARNING: extra_firmware/ missing or empty -- run fetch firmware first." >&2
    fi

    echo -e "\e[1;34m[*]\e[0m Running olddefconfig..."
    make "${MAKE_OPTS[@]}" olddefconfig

    echo -e "\e[1;34m[*]\e[0m Running prepare..."
    make "${MAKE_OPTS[@]}" prepare

    echo -e "\e[1;34m[*]\e[0m Building bzImage [profile: ${PROFILE}] with ${JOBS} jobs..."
    time make "${MAKE_OPTS[@]}" bzImage

    BZIMAGE="arch/x86/boot/bzImage"
    if [[ ! -f "${BZIMAGE}" ]]; then
        echo -e "\e[1;31mERROR:\e[0m bzImage not found after build." >&2
        exit 1
    fi

    mkdir -p "${OUTPUT_DIR}"
    cp "${BZIMAGE}" "${OUTPUT_DIR}/bzImage"
    cp .config "${OUTPUT_DIR}/.config"

    KVER=$(cat include/config/kernel.release 2>/dev/null || echo "unknown")
    LTO_FLAVOR="ThinLTO"

    PROFILE_LABEL="Server"
    if [[ "$PROFILE" == "general" ]]; then
        PROFILE_LABEL="Desktop"
    fi

    KVER_BASE="${KVER%%-*}"
    RELEASE_TRACK="Mainline"
    if [[ "$KVER_BASE" == 6.18.* ]]; then
        RELEASE_TRACK="LTS"
    fi

    ARTIFACT_BASENAME="Strawberry-${LTO_FLAVOR}-${PROFILE_LABEL}-${RELEASE_TRACK}-${KVER}"
    printf '%s\n' "${ARTIFACT_BASENAME}" > "${OUTPUT_DIR}/artifact_name.txt"
    echo ""
    echo -e "\e[1;32m╔══════════════════════════════════════════════════╗\e[0m"
    echo -e "\e[1;32m║\e[0m  Build complete! [${PROFILE}]$(printf "%-26s" "")\e[1;32m║\e[0m"
    echo -e "\e[1;32m║\e[0m  Kernel : $(printf "%-39s" "${KVER}")\e[1;32m║\e[0m"
    echo -e "\e[1;32m║\e[0m  bzImage: $(printf "%-39s" "${OUTPUT_DIR}/bzImage")\e[1;32m║\e[0m"
    echo -e "\e[1;32m╚══════════════════════════════════════════════════╝\e[0m"
    echo ""
    if [[ "$PROFILE" == "general" ]]; then
        echo "Kernel cmdline (add to your kexec invocation):"
        echo "  mitigations=off pti=off spectre_v2=off"
        echo "  isolcpus=2-7 nohz_full=2-7 rcu_nocbs=2-7 irqaffinity=0-1 threadirqs"
        echo ""
        echo "Post-boot sysctl (add to /etc/sysctl.d/99-ps4-gaming.conf):"
        echo "  vm.swappiness = 10"
        echo "  vm.dirty_ratio = 15"
        echo "  vm.dirty_background_ratio = 5"
        echo "  vm.compaction_proactiveness = 1"
        echo ""
        echo "Force GPU to max SCLK:"
        echo "  echo manual > /sys/class/drm/card0/device/power_dpm_force_performance_level"
        echo "  echo 2      > /sys/class/drm/card0/device/pp_dpm_sclk"
        echo ""
    fi
    echo "Deploy to PS4:"
    echo "  scp ${OUTPUT_DIR}/bzImage root@<ps4-ip>:/boot/bzImage"
fi
