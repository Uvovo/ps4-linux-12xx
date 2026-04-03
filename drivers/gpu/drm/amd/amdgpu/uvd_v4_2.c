/*
 * Copyright 2013 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Christian König <christian.koenig@amd.com>
 */

#include <linux/firmware.h>

#include "amdgpu.h"
#include "amdgpu_uvd.h"
#include "cikd.h"

#include "uvd/uvd_4_2_d.h"
#include "uvd/uvd_4_2_sh_mask.h"

#include "oss/oss_2_0_d.h"
#include "oss/oss_2_0_sh_mask.h"

#include "bif/bif_4_1_d.h"

#include "smu/smu_7_0_1_d.h"
#include "smu/smu_7_0_1_sh_mask.h"

#ifndef mmUVD_GP_SCRATCH4
#define mmUVD_GP_SCRATCH4 0x3D38
#endif

#ifndef mmUVD_MIF_CURR_ADDR_CONFIG
#define mmUVD_MIF_CURR_ADDR_CONFIG 0x3992
#endif

#ifndef mmUVD_MIF_REF_ADDR_CONFIG
#define mmUVD_MIF_REF_ADDR_CONFIG 0x3993
#endif

#ifndef mmUVD_MIF_RECON1_ADDR_CONFIG
#define mmUVD_MIF_RECON1_ADDR_CONFIG 0x39C5
#endif

#ifndef mmUVD_LMI_VCPU_CACHE_64BIT_BAR_HIGH
#define mmUVD_LMI_VCPU_CACHE_64BIT_BAR_HIGH 0x3C5E
#endif

#ifndef mmUVD_LMI_VCPU_CACHE_64BIT_BAR_LOW
#define mmUVD_LMI_VCPU_CACHE_64BIT_BAR_LOW 0x3C5F
#endif

#ifndef mmUVD_LMI_RBC_RB_64BIT_BAR_HIGH
#define mmUVD_LMI_RBC_RB_64BIT_BAR_HIGH 0x3C68
#endif

#ifndef mmUVD_LMI_RBC_RB_64BIT_BAR_LOW
#define mmUVD_LMI_RBC_RB_64BIT_BAR_LOW 0x3C69
#endif

#ifndef mmUVD_SUVD_CGC_GATE
#define mmUVD_SUVD_CGC_GATE 0x3BE4
#endif

#ifndef mmUVD_LMI_VCPU_CACHE1_64BIT_BAR_LOW
#define mmUVD_LMI_VCPU_CACHE1_64BIT_BAR_LOW 0x3BEC
#endif

#ifndef mmUVD_LMI_VCPU_CACHE1_64BIT_BAR_HIGH
#define mmUVD_LMI_VCPU_CACHE1_64BIT_BAR_HIGH 0x3BED
#endif

#ifndef mmUVD_LMI_VCPU_CACHE2_64BIT_BAR_LOW
#define mmUVD_LMI_VCPU_CACHE2_64BIT_BAR_LOW 0x3BF0
#endif

#ifndef mmUVD_LMI_VCPU_CACHE2_64BIT_BAR_HIGH
#define mmUVD_LMI_VCPU_CACHE2_64BIT_BAR_HIGH 0x3BF1
#endif

#ifndef mmGARLIC_FLUSH_CNTL
#define mmGARLIC_FLUSH_CNTL 0x1401
#endif

#ifndef GARLIC_FLUSH_CNTL__UVD_RBC_RB_WPTR_MASK
#define GARLIC_FLUSH_CNTL__UVD_RBC_RB_WPTR_MASK 0x8
#endif

#define mmUVD_GP_SCRATCH0_LIVERPOOL 0x3DAC
#define mmUVD_GP_SCRATCH4_LIVERPOOL 0x3DC0
#define mmUVD_RBC_RB_RPTR_ADDR_ALT 0x3DAB
#define mmUVD_LMI_MISC_LIVERPOOL 0x3D68

#ifndef ixUVD_LMI_VMID_INTERNAL
#define ixUVD_LMI_VMID_INTERNAL 0x99
#endif

#ifndef ixUVD_LMI_VMID_INTERNAL2
#define ixUVD_LMI_VMID_INTERNAL2 0x9A
#endif

#ifndef ixUVD_LMI_VMID_INTERNAL3
#define ixUVD_LMI_VMID_INTERNAL3 0x162
#endif

static void uvd_v4_2_mc_resume(struct amdgpu_device *adev);
static void uvd_v4_2_set_ring_funcs(struct amdgpu_device *adev);
static void uvd_v4_2_set_irq_funcs(struct amdgpu_device *adev);
static int uvd_v4_2_start(struct amdgpu_device *adev);
static void uvd_v4_2_stop(struct amdgpu_device *adev);
static int uvd_v4_2_set_clockgating_state(struct amdgpu_ip_block *ip_block,
				enum amd_clockgating_state state);
static void uvd_v4_2_set_dcm(struct amdgpu_device *adev,
			     bool sw_mode);
static void uvd_v4_2_log_boot_state(struct amdgpu_device *adev, const char *tag);

static bool uvd_v4_2_use_legacy_boot(struct amdgpu_device *adev)
{
	return adev->asic_type == CHIP_LIVERPOOL ||
	       adev->asic_type == CHIP_GLADIUS;
}

static bool uvd_v4_2_use_legacy_fw_layout(struct amdgpu_device *adev)
{
	return adev->asic_type == CHIP_LIVERPOOL ||
	       adev->asic_type == CHIP_GLADIUS;
}

static bool uvd_v4_2_use_legacy_bar_layout(struct amdgpu_device *adev)
{
	return adev->asic_type == CHIP_LIVERPOOL ||
	       adev->asic_type == CHIP_GLADIUS;
}

static u32 uvd_v4_2_addr_config(struct amdgpu_device *adev)
{
	if (uvd_v4_2_use_legacy_fw_layout(adev))
		return 0x02011002;

	return adev->gfx.config.gb_addr_config;
}

static u32 uvd_v4_2_fw_window_size(struct amdgpu_device *adev)
{
	u32 size = AMDGPU_GPU_PAGE_ALIGN(adev->uvd.fw->size + 4);

	if (uvd_v4_2_use_legacy_fw_layout(adev) &&
	    size < AMDGPU_UVD_LEGACY_VCPU_CACHE_SIZE0)
		size = AMDGPU_UVD_LEGACY_VCPU_CACHE_SIZE0;

	return size;
}

static u32 uvd_v4_2_heap_window_size(struct amdgpu_device *adev)
{
	if (uvd_v4_2_use_legacy_fw_layout(adev))
		return AMDGPU_UVD_LEGACY_VCPU_CACHE_SIZE1;

	return AMDGPU_UVD_HEAP_SIZE;
}

static u32 uvd_v4_2_stack_session_window_size(struct amdgpu_device *adev)
{
	if (uvd_v4_2_use_legacy_fw_layout(adev))
		return AMDGPU_UVD_LEGACY_VCPU_CACHE_SIZE2;

	return AMDGPU_UVD_STACK_SIZE +
	       (AMDGPU_UVD_SESSION_SIZE * adev->uvd.max_handles);
}

static u32 uvd_v4_2_legacy_cache_offset(u32 bank, u32 byte_offset)
{
	return (bank << 21) | (byte_offset >> 3);
}

static u32 uvd_v4_2_legacy_bar_low(u64 addr)
{
	/*
	 * Liverpool/Gladius traces do not use a raw MC-address split for the
	 * legacy cache BARs. The VCPU windows are banked in 2 MiB chunks, so
	 * only the sub-2 MiB bits live in the low BAR register.
	 */
	return lower_32_bits(addr & ((1ULL << 21) - 1));
}

static u32 uvd_v4_2_legacy_bar_high(u64 addr)
{
	return upper_32_bits(addr >> 2);
}

static u32 uvd_v4_2_legacy_ext40_addr(u64 addr)
{
	return (u32)((addr >> 34) & 0xff);
}

static u32 uvd_v4_2_legacy_boot_rb_base(void)
{
	return AMDGPU_UVD_LEGACY_RBC_RB_OFFSET;
}

static u64 uvd_v4_2_legacy_boot_rb_gpu_addr(struct amdgpu_device *adev)
{
	return adev->uvd.inst->gpu_addr + uvd_v4_2_legacy_boot_rb_base();
}

static void uvd_v4_2_program_legacy_cache_bars(struct amdgpu_device *adev)
{
	u32 bar_low = uvd_v4_2_legacy_bar_low(adev->uvd.inst->gpu_addr);
	u32 bar_high = uvd_v4_2_legacy_bar_high(adev->uvd.inst->gpu_addr);

	if (!uvd_v4_2_use_legacy_bar_layout(adev))
		return;

	WREG32(mmUVD_LMI_VCPU_CACHE_64BIT_BAR_LOW, bar_low);
	WREG32(mmUVD_LMI_VCPU_CACHE_64BIT_BAR_HIGH, bar_high);
	WREG32(mmUVD_LMI_VCPU_CACHE1_64BIT_BAR_LOW, bar_low);
	WREG32(mmUVD_LMI_VCPU_CACHE1_64BIT_BAR_HIGH, bar_high);
	WREG32(mmUVD_LMI_VCPU_CACHE2_64BIT_BAR_LOW, bar_low);
	WREG32(mmUVD_LMI_VCPU_CACHE2_64BIT_BAR_HIGH, bar_high);
}

static void uvd_v4_2_program_legacy_rbc_bar(struct amdgpu_device *adev)
{
	if (!uvd_v4_2_use_legacy_bar_layout(adev))
		return;

	WREG32(mmUVD_LMI_RBC_RB_64BIT_BAR_LOW,
	       uvd_v4_2_legacy_bar_low(adev->uvd.inst->gpu_addr));
	WREG32(mmUVD_LMI_RBC_RB_64BIT_BAR_HIGH,
	       uvd_v4_2_legacy_bar_high(adev->uvd.inst->gpu_addr));
}

static void uvd_v4_2_program_internal_vmids(struct amdgpu_device *adev)
{
	/*
	 * Liverpool/Gladius can arrive here after an Orbis->kexec handoff with
	 * UVD internal VM routing still pointing at a stale non-zero VMID.
	 * Force the internal clients back onto VMID 0 before the VCPU boots.
	 */
	WREG32_UVD_CTX(ixUVD_LMI_VMID_INTERNAL, 0);
	WREG32_UVD_CTX(ixUVD_LMI_VMID_INTERNAL2, 0);
	WREG32_UVD_CTX(ixUVD_LMI_VMID_INTERNAL3, 0);
}

static void uvd_v4_2_program_mif_addr_config(struct amdgpu_device *adev)
{
	u32 addr_config = uvd_v4_2_addr_config(adev);

	if (!uvd_v4_2_use_legacy_fw_layout(adev))
		return;

	/*
	 * Liverpool/Gladius traces show both the direct UVD_MIF_* MMIO path and
	 * the indexed UVD context path being used during bring-up. Program both
	 * so a post-kexec reset does not leave the media tiling state half-reset.
	 */
	WREG32(mmUVD_MIF_CURR_ADDR_CONFIG, addr_config);
	WREG32(mmUVD_MIF_REF_ADDR_CONFIG, addr_config);
	WREG32(mmUVD_MIF_RECON1_ADDR_CONFIG, addr_config);
	WREG32_UVD_CTX(ixUVD_MIF_CURR_ADDR_CONFIG, addr_config);
	WREG32_UVD_CTX(ixUVD_MIF_REF_ADDR_CONFIG, addr_config);
	WREG32_UVD_CTX(ixUVD_MIF_RECON1_ADDR_CONFIG, addr_config);
}

static void uvd_v4_2_flush_legacy_rbc_garlic(struct amdgpu_device *adev)
{
	u32 tmp;

	if (!uvd_v4_2_use_legacy_boot(adev))
		return;

	/*
	 * Orbis flushes the UVD RBC writeback path through GARLIC right before
	 * final ring-base/bar programming. Mirror that here so a post-kexec
	 * handoff does not leave stale write-combine state behind.
	 */
	tmp = RREG32(mmGARLIC_FLUSH_CNTL);
	WREG32(mmGARLIC_FLUSH_CNTL,
	       tmp | GARLIC_FLUSH_CNTL__UVD_RBC_RB_WPTR_MASK);
}

static void uvd_v4_2_program_legacy_rbc_boot(struct amdgpu_device *adev)
{
	struct amdgpu_ring *ring = &adev->uvd.inst->ring;
	u8 *cpu_addr = adev->uvd.inst->cpu_addr;
	u32 rb_bufsz, tmp;

	if (!uvd_v4_2_use_legacy_boot(adev))
		return;

	rb_bufsz = order_base_2(ring->ring_size);
	tmp = REG_SET_FIELD(0, UVD_RBC_RB_CNTL, RB_BUFSZ, rb_bufsz);
	tmp = REG_SET_FIELD(tmp, UVD_RBC_RB_CNTL, RB_BLKSZ, 1);
	tmp = REG_SET_FIELD(tmp, UVD_RBC_RB_CNTL, RB_NO_FETCH, 1);
	tmp = REG_SET_FIELD(tmp, UVD_RBC_RB_CNTL, RB_WPTR_POLL_EN, 0);
	tmp = REG_SET_FIELD(tmp, UVD_RBC_RB_CNTL, RB_NO_UPDATE, 1);
	tmp = REG_SET_FIELD(tmp, UVD_RBC_RB_CNTL, RB_RPTR_WR_EN, 1);
	WREG32(mmUVD_RBC_RB_CNTL, tmp);
	WREG32(mmUVD_RBC_RB_WPTR_CNTL, 0);

	if (cpu_addr)
		memset(cpu_addr + AMDGPU_UVD_LEGACY_RBC_RB_OFFSET, 0,
		       min_t(u32, ring->ring_size, AMDGPU_UVD_LEGACY_RBC_RB_SIZE));

	WREG32(mmUVD_RBC_RB_BASE, uvd_v4_2_legacy_boot_rb_base());
	WREG32(mmUVD_RBC_IB_BASE, uvd_v4_2_legacy_boot_rb_base());
	WREG32(mmUVD_GP_SCRATCH4_LIVERPOOL, 0);
	WREG32(mmUVD_RBC_RB_RPTR_ADDR_ALT, 0x1);
	uvd_v4_2_flush_legacy_rbc_garlic(adev);
	uvd_v4_2_program_legacy_rbc_bar(adev);
	WREG32(mmUVD_RBC_RB_BASE, uvd_v4_2_legacy_boot_rb_base());
	ring->wptr = 0;
	WREG32(mmUVD_RBC_RB_WPTR, 0);
	WREG32(mmUVD_RBC_RB_RPTR, 0);
	WREG32_P(mmUVD_RBC_RB_CNTL, 0, ~UVD_RBC_RB_CNTL__RB_NO_FETCH_MASK);

	/*
	 * Liverpool/Gladius traces revisit the alternate RPTR and scratch
	 * registers after the ring has been primed and fetching is enabled.
	 */
	WREG32(mmUVD_RBC_RB_RPTR_ADDR_ALT, 0x1);
	WREG32(mmUVD_GP_SCRATCH0_LIVERPOOL, 0x10);
}

static void uvd_v4_2_program_legacy_misc_init(struct amdgpu_device *adev)
{
	if (!uvd_v4_2_use_legacy_boot(adev))
		return;

	/*
	 * The Liverpool/Gladius boot trace toggles an undocumented UVD-side
	 * LMI helper register around memory-controller init. The values are
	 * stable across the recovered boots: 0 before LMI programming, then
	 * 0x3dff after the MPC mux setup.
	 */
	WREG32(mmUVD_LMI_MISC_LIVERPOOL, 0);
}

static void uvd_v4_2_program_legacy_misc_enable(struct amdgpu_device *adev)
{
	if (!uvd_v4_2_use_legacy_boot(adev))
		return;

	WREG32(mmUVD_LMI_MISC_LIVERPOOL, 0x3dff);
}
/**
 * uvd_v4_2_ring_get_rptr - get read pointer
 *
 * @ring: amdgpu_ring pointer
 *
 * Returns the current hardware read pointer
 */
static uint64_t uvd_v4_2_ring_get_rptr(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	return RREG32(mmUVD_RBC_RB_RPTR);
}

/**
 * uvd_v4_2_ring_get_wptr - get write pointer
 *
 * @ring: amdgpu_ring pointer
 *
 * Returns the current hardware write pointer
 */
static uint64_t uvd_v4_2_ring_get_wptr(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	return RREG32(mmUVD_RBC_RB_WPTR);
}

/**
 * uvd_v4_2_ring_set_wptr - set write pointer
 *
 * @ring: amdgpu_ring pointer
 *
 * Commits the write pointer to the hardware
 */
static void uvd_v4_2_ring_set_wptr(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	WREG32(mmUVD_RBC_RB_WPTR, lower_32_bits(ring->wptr));
}

static int uvd_v4_2_early_init(struct amdgpu_ip_block *ip_block)
{
	struct amdgpu_device *adev = ip_block->adev;
	adev->uvd.num_uvd_inst = 1;

	uvd_v4_2_set_ring_funcs(adev);
	uvd_v4_2_set_irq_funcs(adev);

	return 0;
}

static int uvd_v4_2_sw_init(struct amdgpu_ip_block *ip_block)
{
	struct amdgpu_ring *ring;
	struct amdgpu_device *adev = ip_block->adev;
	int r;

	/* UVD TRAP */
	r = amdgpu_irq_add_id(adev, AMDGPU_IRQ_CLIENTID_LEGACY, 124, &adev->uvd.inst->irq);
	if (r)
		return r;

	r = amdgpu_uvd_sw_init(adev);
	if (r)
		return r;

	ring = &adev->uvd.inst->ring;
	sprintf(ring->name, "uvd");
	r = amdgpu_ring_init(adev, ring, 512, &adev->uvd.inst->irq, 0,
			     AMDGPU_RING_PRIO_DEFAULT, NULL);
	if (r)
		return r;

	r = amdgpu_uvd_resume(adev);
	if (r)
		return r;

	return r;
}

static int uvd_v4_2_sw_fini(struct amdgpu_ip_block *ip_block)
{
	int r;
	struct amdgpu_device *adev = ip_block->adev;

	r = amdgpu_uvd_suspend(adev);
	if (r)
		return r;

	return amdgpu_uvd_sw_fini(adev);
}

static void uvd_v4_2_enable_mgcg(struct amdgpu_device *adev,
				 bool enable);
/**
 * uvd_v4_2_hw_init - start and test UVD block
 *
 * @ip_block: Pointer to the amdgpu_ip_block for this hw instance.
 *
 * Initialize the hardware, boot up the VCPU and do some testing
 */
static int uvd_v4_2_hw_init(struct amdgpu_ip_block *ip_block)
{
	struct amdgpu_device *adev = ip_block->adev;
	struct amdgpu_ring *ring = &adev->uvd.inst->ring;
	uint32_t tmp;
	int r;

	uvd_v4_2_enable_mgcg(adev, true);
	amdgpu_asic_set_uvd_clocks(adev, 10000, 10000);

	r = amdgpu_ring_test_helper(ring);
	if (r)
		goto done;

	r = amdgpu_ring_alloc(ring, 10);
	if (r) {
		DRM_ERROR("amdgpu: ring failed to lock UVD ring (%d).\n", r);
		goto done;
	}

	tmp = PACKET0(mmUVD_SEMA_WAIT_FAULT_TIMEOUT_CNTL, 0);
	amdgpu_ring_write(ring, tmp);
	amdgpu_ring_write(ring, 0xFFFFF);

	tmp = PACKET0(mmUVD_SEMA_WAIT_INCOMPLETE_TIMEOUT_CNTL, 0);
	amdgpu_ring_write(ring, tmp);
	amdgpu_ring_write(ring, 0xFFFFF);

	tmp = PACKET0(mmUVD_SEMA_SIGNAL_INCOMPLETE_TIMEOUT_CNTL, 0);
	amdgpu_ring_write(ring, tmp);
	amdgpu_ring_write(ring, 0xFFFFF);

	/* Clear timeout status bits */
	amdgpu_ring_write(ring, PACKET0(mmUVD_SEMA_TIMEOUT_STATUS, 0));
	amdgpu_ring_write(ring, 0x8);

	amdgpu_ring_write(ring, PACKET0(mmUVD_SEMA_CNTL, 0));
	amdgpu_ring_write(ring, 3);

	amdgpu_ring_commit(ring);

done:
	if (!r)
		DRM_INFO("UVD initialized successfully.\n");

	return r;
}

/**
 * uvd_v4_2_hw_fini - stop the hardware block
 *
 * @ip_block: Pointer to the amdgpu_ip_block for this hw instance.
 *
 * Stop the UVD block, mark ring as not ready any more
 */
static int uvd_v4_2_hw_fini(struct amdgpu_ip_block *ip_block)
{
	struct amdgpu_device *adev = ip_block->adev;

	cancel_delayed_work_sync(&adev->uvd.idle_work);

	if (RREG32(mmUVD_STATUS) != 0)
		uvd_v4_2_stop(adev);

	return 0;
}

static int uvd_v4_2_prepare_suspend(struct amdgpu_ip_block *ip_block)
{
	struct amdgpu_device *adev = ip_block->adev;

	return amdgpu_uvd_prepare_suspend(adev);
}

static int uvd_v4_2_suspend(struct amdgpu_ip_block *ip_block)
{
	int r;
	struct amdgpu_device *adev = ip_block->adev;

	/*
	 * Proper cleanups before halting the HW engine:
	 *   - cancel the delayed idle work
	 *   - enable powergating
	 *   - enable clockgating
	 *   - disable dpm
	 *
	 * TODO: to align with the VCN implementation, move the
	 * jobs for clockgating/powergating/dpm setting to
	 * ->set_powergating_state().
	 */
	cancel_delayed_work_sync(&adev->uvd.idle_work);

	if (adev->pm.dpm_enabled) {
		amdgpu_dpm_enable_uvd(adev, false);
	} else {
		amdgpu_asic_set_uvd_clocks(adev, 0, 0);
		/* shutdown the UVD block */
		amdgpu_device_ip_set_powergating_state(adev, AMD_IP_BLOCK_TYPE_UVD,
						       AMD_PG_STATE_GATE);
		amdgpu_device_ip_set_clockgating_state(adev, AMD_IP_BLOCK_TYPE_UVD,
						       AMD_CG_STATE_GATE);
	}

	r = uvd_v4_2_hw_fini(ip_block);
	if (r)
		return r;

	return amdgpu_uvd_suspend(adev);
}

static int uvd_v4_2_resume(struct amdgpu_ip_block *ip_block)
{
	int r;

	r = amdgpu_uvd_resume(ip_block->adev);
	if (r)
		return r;

	return uvd_v4_2_hw_init(ip_block);
}

/**
 * uvd_v4_2_start - start UVD block
 *
 * @adev: amdgpu_device pointer
 *
 * Setup and start the UVD block
 */
static int uvd_v4_2_start(struct amdgpu_device *adev)
{
	struct amdgpu_ring *ring = &adev->uvd.inst->ring;
	uint32_t rb_bufsz;
	int i, j, r;
	u32 tmp;
	bool legacy_boot = uvd_v4_2_use_legacy_boot(adev);
	/* disable byte swapping */
	u32 lmi_swap_cntl = 0;
	u32 mp_swap_cntl = 0;

	/* set uvd busy */
	WREG32_P(mmUVD_STATUS, 1<<2, ~(1<<2));

	uvd_v4_2_set_dcm(adev, true);
	WREG32(mmUVD_CGC_GATE, 0);
	if (legacy_boot)
		WREG32(mmUVD_SUVD_CGC_GATE, 0);

#ifdef __BIG_ENDIAN
	/* swap (8 in 32) RB and IB */
	lmi_swap_cntl = 0xa;
	mp_swap_cntl = 0;
#endif

	if (legacy_boot) {
		uvd_v4_2_mc_resume(adev);
		uvd_v4_2_log_boot_state(adev, "after_mc_resume");

		/* disable interrupt */
		WREG32_P(mmUVD_MASTINT_EN, 0, ~(1 << 1));

		/* stall UMC before taking the block through reset */
		WREG32_P(mmUVD_LMI_CTRL2, 1 << 8, ~(1 << 8));
		mdelay(1);

		WREG32(mmUVD_SOFT_RESET, UVD_SOFT_RESET__LMI_SOFT_RESET_MASK |
			UVD_SOFT_RESET__VCPU_SOFT_RESET_MASK |
			UVD_SOFT_RESET__LBSI_SOFT_RESET_MASK |
			UVD_SOFT_RESET__RBC_SOFT_RESET_MASK |
			UVD_SOFT_RESET__CSM_SOFT_RESET_MASK |
			UVD_SOFT_RESET__CXW_SOFT_RESET_MASK |
			UVD_SOFT_RESET__TAP_SOFT_RESET_MASK |
			UVD_SOFT_RESET__LMI_UMC_SOFT_RESET_MASK);
		mdelay(5);

		/* take UVD block out of reset */
		WREG32_P(mmSRBM_SOFT_RESET, 0, ~SRBM_SOFT_RESET__SOFT_RESET_UVD_MASK);
		mdelay(5);

		/* initialize UVD memory controller */
		WREG32(mmUVD_LMI_SWAP_CNTL, lmi_swap_cntl);
		WREG32(mmUVD_MP_SWAP_CNTL, mp_swap_cntl);
		uvd_v4_2_program_legacy_misc_init(adev);
		WREG32(mmUVD_LMI_CTRL, 0x203108);

		tmp = RREG32(mmUVD_MPC_CNTL);
		WREG32(mmUVD_MPC_CNTL, tmp | 0x10);

		WREG32(mmUVD_MPC_SET_MUXA0, 0x40c2040);
		WREG32(mmUVD_MPC_SET_MUXA1, 0x0);
		WREG32(mmUVD_MPC_SET_MUXB0, 0x40c2040);
		WREG32(mmUVD_MPC_SET_MUXB1, 0x0);
		WREG32(mmUVD_MPC_SET_ALU, 0);
		WREG32(mmUVD_MPC_SET_MUX, 0x88);
		uvd_v4_2_program_legacy_misc_enable(adev);

		uvd_v4_2_program_internal_vmids(adev);

		tmp = RREG32_UVD_CTX(ixUVD_LMI_CACHE_CTRL);
		WREG32_UVD_CTX(ixUVD_LMI_CACHE_CTRL, tmp & (~0x10));

		/* take all subblocks out of reset, except VCPU */
		WREG32(mmUVD_SOFT_RESET, UVD_SOFT_RESET__VCPU_SOFT_RESET_MASK);
		mdelay(5);

		/*
		 * The Liverpool/Gladius boot trace re-applies the legacy cache
		 * BARs after reset release, then primes the RBC path immediately
		 * before the VCPU leaves reset.
		 */
		uvd_v4_2_program_legacy_cache_bars(adev);
		uvd_v4_2_program_mif_addr_config(adev);
		uvd_v4_2_program_legacy_rbc_boot(adev);

		/*
		 * Orbis touches the cache control register again in the final
		 * pre-boot window, right after the RBC state is primed.
		 */
		tmp = RREG32_UVD_CTX(ixUVD_LMI_CACHE_CTRL);
		WREG32_UVD_CTX(ixUVD_LMI_CACHE_CTRL, tmp & (~0x10));
		WREG32(mmUVD_VCPU_CNTL, 1 << 9);

		/* enable UMC */
		WREG32_P(mmUVD_LMI_CTRL2, 0, ~(1 << 8));

		/*
		 * Drop reset in the same staged read-modify-write style as the
		 * legacy trace instead of blasting SOFT_RESET straight to zero.
		 */
		WREG32_P(mmUVD_SOFT_RESET, 0, ~UVD_SOFT_RESET__LMI_SOFT_RESET_MASK);
		WREG32_P(mmUVD_SOFT_RESET, 0, ~UVD_SOFT_RESET__LMI_UMC_SOFT_RESET_MASK);
		WREG32_P(mmUVD_SOFT_RESET, 0, ~UVD_SOFT_RESET__VCPU_SOFT_RESET_MASK);
		mdelay(10);

		/*
		 * Some legacy address-config state gets cleared as reset drops, so
		 * rewrite it once more before polling STATUS.
		 */
		uvd_v4_2_program_legacy_cache_bars(adev);
		uvd_v4_2_program_mif_addr_config(adev);
	} else {
		/* take UVD block out of reset */
		WREG32_P(mmSRBM_SOFT_RESET, 0, ~SRBM_SOFT_RESET__SOFT_RESET_UVD_MASK);
		mdelay(5);

		/* enable VCPU clock */
		WREG32(mmUVD_VCPU_CNTL,  1 << 9);

		/* disable interrupt */
		WREG32_P(mmUVD_MASTINT_EN, 0, ~(1 << 1));

		WREG32(mmUVD_LMI_SWAP_CNTL, lmi_swap_cntl);
		WREG32(mmUVD_MP_SWAP_CNTL, mp_swap_cntl);

		/* initialize UVD memory controller */
		WREG32(mmUVD_LMI_CTRL, 0x203108);

		tmp = RREG32(mmUVD_MPC_CNTL);
		WREG32(mmUVD_MPC_CNTL, tmp | 0x10);

		WREG32(mmUVD_MPC_SET_MUXA0, 0x40c2040);
		WREG32(mmUVD_MPC_SET_MUXA1, 0x0);
		WREG32(mmUVD_MPC_SET_MUXB0, 0x40c2040);
		WREG32(mmUVD_MPC_SET_MUXB1, 0x0);
		WREG32(mmUVD_MPC_SET_ALU, 0);
		WREG32(mmUVD_MPC_SET_MUX, 0x88);

		uvd_v4_2_program_internal_vmids(adev);

		uvd_v4_2_mc_resume(adev);
		uvd_v4_2_log_boot_state(adev, "after_mc_resume");

		tmp = RREG32_UVD_CTX(ixUVD_LMI_CACHE_CTRL);
		WREG32_UVD_CTX(ixUVD_LMI_CACHE_CTRL, tmp & (~0x10));

		/* enable UMC */
		WREG32_P(mmUVD_LMI_CTRL2, 0, ~(1 << 8));

		WREG32_P(mmUVD_SOFT_RESET, 0, ~UVD_SOFT_RESET__LMI_SOFT_RESET_MASK);

		WREG32_P(mmUVD_SOFT_RESET, 0, ~UVD_SOFT_RESET__LMI_UMC_SOFT_RESET_MASK);

		WREG32_P(mmUVD_SOFT_RESET, 0, ~UVD_SOFT_RESET__VCPU_SOFT_RESET_MASK);

		mdelay(10);
	}
	uvd_v4_2_log_boot_state(adev, "after_reset_release");

	for (i = 0; i < 10; ++i) {
		uint32_t status;
		for (j = 0; j < 100; ++j) {
			status = RREG32(mmUVD_STATUS);
			if (status & 2)
				break;
			mdelay(10);
		}
		r = 0;
		if (status & 2)
			break;

		uvd_v4_2_log_boot_state(adev, "boot_retry");
		DRM_ERROR("UVD not responding, trying to reset the VCPU!!!\n");
		WREG32_P(mmUVD_SOFT_RESET, UVD_SOFT_RESET__VCPU_SOFT_RESET_MASK,
				~UVD_SOFT_RESET__VCPU_SOFT_RESET_MASK);
		mdelay(10);
		WREG32_P(mmUVD_SOFT_RESET, 0, ~UVD_SOFT_RESET__VCPU_SOFT_RESET_MASK);
		mdelay(10);
		r = -1;
	}

	if (r) {
		uvd_v4_2_log_boot_state(adev, "boot_failed");
		DRM_ERROR("UVD not responding, giving up!!!\n");
		return r;
	}

	/* enable interupt */
	WREG32_P(mmUVD_MASTINT_EN, 3<<1, ~(3 << 1));

	WREG32_P(mmUVD_STATUS, 0, ~(1<<2));

	/* force RBC into idle state */
	WREG32(mmUVD_RBC_RB_CNTL, 0x11010101);

	/* Set the write pointer delay */
	WREG32(mmUVD_RBC_RB_WPTR_CNTL, 0);

	/* program the 4GB memory segment for rptr and ring buffer */
	WREG32(mmUVD_LMI_EXT40_ADDR, upper_32_bits(ring->gpu_addr) |
				   (0x7 << 16) | (0x1 << 31));

	/* Initialize the ring buffer's read and write pointers */
	WREG32(mmUVD_RBC_RB_RPTR, 0x0);

	ring->wptr = RREG32(mmUVD_RBC_RB_RPTR);
	WREG32(mmUVD_RBC_RB_WPTR, lower_32_bits(ring->wptr));

	/* set the ring address */
	WREG32(mmUVD_RBC_RB_BASE, ring->gpu_addr);

	/* Set ring buffer size */
	rb_bufsz = order_base_2(ring->ring_size);
	rb_bufsz = (0x1 << 8) | rb_bufsz;
	WREG32_P(mmUVD_RBC_RB_CNTL, rb_bufsz, ~0x11f1f);

	return 0;
}

/**
 * uvd_v4_2_stop - stop UVD block
 *
 * @adev: amdgpu_device pointer
 *
 * stop the UVD block
 */
static void uvd_v4_2_stop(struct amdgpu_device *adev)
{
	uint32_t i, j;
	uint32_t status;

	WREG32(mmUVD_RBC_RB_CNTL, 0x11010101);

	for (i = 0; i < 10; ++i) {
		for (j = 0; j < 100; ++j) {
			status = RREG32(mmUVD_STATUS);
			if (status & 2)
				break;
			mdelay(1);
		}
		if (status & 2)
			break;
	}

	for (i = 0; i < 10; ++i) {
		for (j = 0; j < 100; ++j) {
			status = RREG32(mmUVD_LMI_STATUS);
			if (status & 0xf)
				break;
			mdelay(1);
		}
		if (status & 0xf)
			break;
	}

	/* Stall UMC and register bus before resetting VCPU */
	WREG32_P(mmUVD_LMI_CTRL2, 1 << 8, ~(1 << 8));

	for (i = 0; i < 10; ++i) {
		for (j = 0; j < 100; ++j) {
			status = RREG32(mmUVD_LMI_STATUS);
			if (status & 0x240)
				break;
			mdelay(1);
		}
		if (status & 0x240)
			break;
	}

	WREG32_P(0x3D49, 0, ~(1 << 2));

	WREG32_P(mmUVD_VCPU_CNTL, 0, ~(1 << 9));

	/* put LMI, VCPU, RBC etc... into reset */
	WREG32(mmUVD_SOFT_RESET, UVD_SOFT_RESET__LMI_SOFT_RESET_MASK |
		UVD_SOFT_RESET__VCPU_SOFT_RESET_MASK |
		UVD_SOFT_RESET__LMI_UMC_SOFT_RESET_MASK);

	WREG32(mmUVD_STATUS, 0);

	uvd_v4_2_set_dcm(adev, false);
}

/**
 * uvd_v4_2_ring_emit_fence - emit an fence & trap command
 *
 * @ring: amdgpu_ring pointer
 * @addr: address
 * @seq: sequence number
 * @flags: fence related flags
 *
 * Write a fence and a trap command to the ring.
 */
static void uvd_v4_2_ring_emit_fence(struct amdgpu_ring *ring, u64 addr, u64 seq,
				     unsigned flags)
{
	WARN_ON(flags & AMDGPU_FENCE_FLAG_64BIT);

	amdgpu_ring_write(ring, PACKET0(mmUVD_CONTEXT_ID, 0));
	amdgpu_ring_write(ring, seq);
	amdgpu_ring_write(ring, PACKET0(mmUVD_GPCOM_VCPU_DATA0, 0));
	amdgpu_ring_write(ring, addr & 0xffffffff);
	amdgpu_ring_write(ring, PACKET0(mmUVD_GPCOM_VCPU_DATA1, 0));
	amdgpu_ring_write(ring, upper_32_bits(addr) & 0xff);
	amdgpu_ring_write(ring, PACKET0(mmUVD_GPCOM_VCPU_CMD, 0));
	amdgpu_ring_write(ring, 0);

	amdgpu_ring_write(ring, PACKET0(mmUVD_GPCOM_VCPU_DATA0, 0));
	amdgpu_ring_write(ring, 0);
	amdgpu_ring_write(ring, PACKET0(mmUVD_GPCOM_VCPU_DATA1, 0));
	amdgpu_ring_write(ring, 0);
	amdgpu_ring_write(ring, PACKET0(mmUVD_GPCOM_VCPU_CMD, 0));
	amdgpu_ring_write(ring, 2);
}

/**
 * uvd_v4_2_ring_test_ring - register write test
 *
 * @ring: amdgpu_ring pointer
 *
 * Test if we can successfully write to the context register
 */
static int uvd_v4_2_ring_test_ring(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;
	uint32_t tmp = 0;
	unsigned i;
	int r;

	WREG32(mmUVD_CONTEXT_ID, 0xCAFEDEAD);
	r = amdgpu_ring_alloc(ring, 3);
	if (r)
		return r;

	amdgpu_ring_write(ring, PACKET0(mmUVD_CONTEXT_ID, 0));
	amdgpu_ring_write(ring, 0xDEADBEEF);
	amdgpu_ring_commit(ring);
	for (i = 0; i < adev->usec_timeout; i++) {
		tmp = RREG32(mmUVD_CONTEXT_ID);
		if (tmp == 0xDEADBEEF)
			break;
		udelay(1);
	}

	if (i >= adev->usec_timeout)
		r = -ETIMEDOUT;

	return r;
}

/**
 * uvd_v4_2_ring_emit_ib - execute indirect buffer
 *
 * @ring: amdgpu_ring pointer
 * @job: iob associated with the indirect buffer
 * @ib: indirect buffer to execute
 * @flags: flags associated with the indirect buffer
 *
 * Write ring commands to execute the indirect buffer
 */
static void uvd_v4_2_ring_emit_ib(struct amdgpu_ring *ring,
				  struct amdgpu_job *job,
				  struct amdgpu_ib *ib,
				  uint32_t flags)
{
	amdgpu_ring_write(ring, PACKET0(mmUVD_RBC_IB_BASE, 0));
	amdgpu_ring_write(ring, ib->gpu_addr);
	amdgpu_ring_write(ring, PACKET0(mmUVD_RBC_IB_SIZE, 0));
	amdgpu_ring_write(ring, ib->length_dw);
}

static void uvd_v4_2_ring_insert_nop(struct amdgpu_ring *ring, uint32_t count)
{
	int i;

	WARN_ON(ring->wptr % 2 || count % 2);

	for (i = 0; i < count / 2; i++) {
		amdgpu_ring_write(ring, PACKET0(mmUVD_NO_OP, 0));
		amdgpu_ring_write(ring, 0);
	}
}

/**
 * uvd_v4_2_mc_resume - memory controller programming
 *
 * @adev: amdgpu_device pointer
 *
 * Let the UVD memory controller know it's offsets
 */
static void uvd_v4_2_mc_resume(struct amdgpu_device *adev)
{
	uint64_t addr;
	uint32_t size;
	bool legacy_layout = uvd_v4_2_use_legacy_fw_layout(adev);
	u32 addr_config = uvd_v4_2_addr_config(adev);

	/* program the VCPU memory controller bits 0-27 */
	if (legacy_layout) {
		/*
		 * Liverpool/Gladius use three banked 64-bit cache BARs. The low
		 * 21 offset bits are still qword-addressed, while bits 21+ select
		 * the bank, which matches the recovered Orbis trace.
		 */
		u32 fw_size = uvd_v4_2_fw_window_size(adev);
		u32 heap_size = uvd_v4_2_heap_window_size(adev);
		u32 stack_size = uvd_v4_2_stack_session_window_size(adev);

		uvd_v4_2_program_legacy_cache_bars(adev);

		addr = 0;
		size = uvd_v4_2_fw_window_size(adev);
		WREG32(mmUVD_VCPU_CACHE_OFFSET0, uvd_v4_2_legacy_cache_offset(0, addr));
		WREG32(mmUVD_VCPU_CACHE_SIZE0, size);

		addr = fw_size;
		size = heap_size;
		WREG32(mmUVD_VCPU_CACHE_OFFSET1, uvd_v4_2_legacy_cache_offset(1, addr));
		WREG32(mmUVD_VCPU_CACHE_SIZE1, size);

		addr = fw_size + heap_size;
		size = stack_size;
		WREG32(mmUVD_VCPU_CACHE_OFFSET2, uvd_v4_2_legacy_cache_offset(2, addr));
		WREG32(mmUVD_VCPU_CACHE_SIZE2, size);
	} else {
		addr = (adev->uvd.inst->gpu_addr + AMDGPU_UVD_FIRMWARE_OFFSET) >> 3;
		size = AMDGPU_UVD_FIRMWARE_SIZE(adev) >> 3;
		WREG32(mmUVD_VCPU_CACHE_OFFSET0, addr);
		WREG32(mmUVD_VCPU_CACHE_SIZE0, size);

		addr += size;
		size = AMDGPU_UVD_HEAP_SIZE >> 3;
		WREG32(mmUVD_VCPU_CACHE_OFFSET1, addr);
		WREG32(mmUVD_VCPU_CACHE_SIZE1, size);

		addr += size;
		size = (AMDGPU_UVD_STACK_SIZE +
		       (AMDGPU_UVD_SESSION_SIZE * adev->uvd.max_handles)) >> 3;
		WREG32(mmUVD_VCPU_CACHE_OFFSET2, addr);
		WREG32(mmUVD_VCPU_CACHE_SIZE2, size);
	}

	/* bits 28-31 */
	addr = (adev->uvd.inst->gpu_addr >> 28) & 0xF;
	WREG32(mmUVD_LMI_ADDR_EXT, (addr << 12) | (addr << 0));

	/* bits 32-39 */
	if (legacy_layout)
		addr = uvd_v4_2_legacy_ext40_addr(adev->uvd.inst->gpu_addr);
	else
		addr = (adev->uvd.inst->gpu_addr >> 32) & 0xFF;
	WREG32(mmUVD_LMI_EXT40_ADDR, addr | (0x9 << 16) | (0x1 << 31));

	WREG32(mmUVD_UDEC_ADDR_CONFIG, addr_config);
	WREG32(mmUVD_UDEC_DB_ADDR_CONFIG, addr_config);
	WREG32(mmUVD_UDEC_DBW_ADDR_CONFIG, addr_config);
	uvd_v4_2_program_mif_addr_config(adev);
	if (legacy_layout)
		WREG32(mmUVD_GP_SCRATCH4_LIVERPOOL, 0);
	else
		WREG32(mmUVD_GP_SCRATCH4, adev->uvd.max_handles);
}

static void uvd_v4_2_log_boot_state(struct amdgpu_device *adev, const char *tag)
{
	DRM_INFO("UVD %s: gpu_addr=0x%016llx STATUS=0x%08x VCPU_CNTL=0x%08x SOFT_RESET=0x%08x SRBM_SOFT_RESET=0x%08x LMI_CTRL=0x%08x LMI_CTRL2=0x%08x\n",
		 tag, adev->uvd.inst->gpu_addr, RREG32(mmUVD_STATUS),
		 RREG32(mmUVD_VCPU_CNTL), RREG32(mmUVD_SOFT_RESET),
		 RREG32(mmSRBM_SOFT_RESET), RREG32(mmUVD_LMI_CTRL),
		 RREG32(mmUVD_LMI_CTRL2));
	DRM_INFO("UVD %s: CACHE0=0x%08x/%x CACHE1=0x%08x/%x CACHE2=0x%08x/%x ADDR_EXT=0x%08x EXT40=0x%08x MPC_CNTL=0x%08x SUVD_CGC=0x%08x LMI_MISC=0x%08x\n",
		 tag, RREG32(mmUVD_VCPU_CACHE_OFFSET0),
		 RREG32(mmUVD_VCPU_CACHE_SIZE0), RREG32(mmUVD_VCPU_CACHE_OFFSET1),
		 RREG32(mmUVD_VCPU_CACHE_SIZE1), RREG32(mmUVD_VCPU_CACHE_OFFSET2),
		 RREG32(mmUVD_VCPU_CACHE_SIZE2), RREG32(mmUVD_LMI_ADDR_EXT),
		 RREG32(mmUVD_LMI_EXT40_ADDR), RREG32(mmUVD_MPC_CNTL),
		 RREG32(mmUVD_SUVD_CGC_GATE), RREG32(mmUVD_LMI_MISC_LIVERPOOL));
	DRM_INFO("UVD %s: VMID_INT=0x%08x VMID_INT2=0x%08x VMID_INT3=0x%08x LMI_CACHE_CTRL=0x%08x\n",
		 tag, RREG32_UVD_CTX(ixUVD_LMI_VMID_INTERNAL),
		 RREG32_UVD_CTX(ixUVD_LMI_VMID_INTERNAL2),
		 RREG32_UVD_CTX(ixUVD_LMI_VMID_INTERNAL3),
		 RREG32_UVD_CTX(ixUVD_LMI_CACHE_CTRL));
	if (uvd_v4_2_use_legacy_bar_layout(adev))
		DRM_INFO("UVD %s: BAR0=%08x/%08x BAR1=%08x/%08x BAR2=%08x/%08x RBC_BAR=%08x/%08x RB_BASE=%08x IB_BASE=%08x BOOT_RB=0x%016llx RB_CNTL=%08x GARLIC=%08x RPTR_ADDR=%08x MIF_CTX=%08x/%08x/%08x\n",
			 tag,
			 RREG32(mmUVD_LMI_VCPU_CACHE_64BIT_BAR_LOW),
			 RREG32(mmUVD_LMI_VCPU_CACHE_64BIT_BAR_HIGH),
			 RREG32(mmUVD_LMI_VCPU_CACHE1_64BIT_BAR_LOW),
			 RREG32(mmUVD_LMI_VCPU_CACHE1_64BIT_BAR_HIGH),
			 RREG32(mmUVD_LMI_VCPU_CACHE2_64BIT_BAR_LOW),
			 RREG32(mmUVD_LMI_VCPU_CACHE2_64BIT_BAR_HIGH),
			 RREG32(mmUVD_LMI_RBC_RB_64BIT_BAR_LOW),
			 RREG32(mmUVD_LMI_RBC_RB_64BIT_BAR_HIGH),
			 RREG32(mmUVD_RBC_RB_BASE),
			 RREG32(mmUVD_RBC_IB_BASE),
			 uvd_v4_2_legacy_boot_rb_gpu_addr(adev),
			 RREG32(mmUVD_RBC_RB_CNTL),
			 RREG32(mmGARLIC_FLUSH_CNTL),
			 RREG32(mmUVD_RBC_RB_RPTR_ADDR),
			 RREG32_UVD_CTX(ixUVD_MIF_CURR_ADDR_CONFIG),
			 RREG32_UVD_CTX(ixUVD_MIF_REF_ADDR_CONFIG),
			 RREG32_UVD_CTX(ixUVD_MIF_RECON1_ADDR_CONFIG));
	if (uvd_v4_2_use_legacy_boot(adev))
		DRM_INFO("UVD %s: SCR0=%08x SCR4=%08x RPTR_ALT=%08x ADDR_CFG=%08x\n",
			 tag, RREG32(mmUVD_GP_SCRATCH0_LIVERPOOL),
			 RREG32(mmUVD_GP_SCRATCH4_LIVERPOOL),
			 RREG32(mmUVD_RBC_RB_RPTR_ADDR_ALT),
			 uvd_v4_2_addr_config(adev));
}

static void uvd_v4_2_enable_mgcg(struct amdgpu_device *adev,
				 bool enable)
{
	u32 orig, data;

	if (enable && (adev->cg_flags & AMD_CG_SUPPORT_UVD_MGCG)) {
		data = RREG32_UVD_CTX(ixUVD_CGC_MEM_CTRL);
		data |= 0xfff;
		WREG32_UVD_CTX(ixUVD_CGC_MEM_CTRL, data);

		orig = data = RREG32(mmUVD_CGC_CTRL);
		data |= UVD_CGC_CTRL__DYN_CLOCK_MODE_MASK;
		if (orig != data)
			WREG32(mmUVD_CGC_CTRL, data);
	} else {
		data = RREG32_UVD_CTX(ixUVD_CGC_MEM_CTRL);
		data &= ~0xfff;
		WREG32_UVD_CTX(ixUVD_CGC_MEM_CTRL, data);

		orig = data = RREG32(mmUVD_CGC_CTRL);
		data &= ~UVD_CGC_CTRL__DYN_CLOCK_MODE_MASK;
		if (orig != data)
			WREG32(mmUVD_CGC_CTRL, data);
	}
}

static void uvd_v4_2_set_dcm(struct amdgpu_device *adev,
			     bool sw_mode)
{
	u32 tmp, tmp2;

	WREG32_FIELD(UVD_CGC_GATE, REGS, 0);

	tmp = RREG32(mmUVD_CGC_CTRL);
	tmp &= ~(UVD_CGC_CTRL__CLK_OFF_DELAY_MASK | UVD_CGC_CTRL__CLK_GATE_DLY_TIMER_MASK);
	tmp |= UVD_CGC_CTRL__DYN_CLOCK_MODE_MASK |
		(1 << UVD_CGC_CTRL__CLK_GATE_DLY_TIMER__SHIFT) |
		(4 << UVD_CGC_CTRL__CLK_OFF_DELAY__SHIFT);

	if (sw_mode) {
		tmp &= ~0x7ffff800;
		tmp2 = UVD_CGC_CTRL2__DYN_OCLK_RAMP_EN_MASK |
			UVD_CGC_CTRL2__DYN_RCLK_RAMP_EN_MASK |
			(7 << UVD_CGC_CTRL2__GATER_DIV_ID__SHIFT);
	} else {
		tmp |= 0x7ffff800;
		tmp2 = 0;
	}

	WREG32(mmUVD_CGC_CTRL, tmp);
	WREG32_UVD_CTX(ixUVD_CGC_CTRL2, tmp2);
}

static bool uvd_v4_2_is_idle(struct amdgpu_ip_block *ip_block)
{
	struct amdgpu_device *adev = ip_block->adev;

	return !(RREG32(mmSRBM_STATUS) & SRBM_STATUS__UVD_BUSY_MASK);
}

static int uvd_v4_2_wait_for_idle(struct amdgpu_ip_block *ip_block)
{
	unsigned i;
	struct amdgpu_device *adev = ip_block->adev;

	for (i = 0; i < adev->usec_timeout; i++) {
		if (!(RREG32(mmSRBM_STATUS) & SRBM_STATUS__UVD_BUSY_MASK))
			return 0;
	}
	return -ETIMEDOUT;
}

static int uvd_v4_2_soft_reset(struct amdgpu_ip_block *ip_block)
{
	struct amdgpu_device *adev = ip_block->adev;

	uvd_v4_2_stop(adev);

	WREG32_P(mmSRBM_SOFT_RESET, SRBM_SOFT_RESET__SOFT_RESET_UVD_MASK,
			~SRBM_SOFT_RESET__SOFT_RESET_UVD_MASK);
	mdelay(5);

	return uvd_v4_2_start(adev);
}

static int uvd_v4_2_set_interrupt_state(struct amdgpu_device *adev,
					struct amdgpu_irq_src *source,
					unsigned type,
					enum amdgpu_interrupt_state state)
{
	// TODO
	return 0;
}

static int uvd_v4_2_process_interrupt(struct amdgpu_device *adev,
				      struct amdgpu_irq_src *source,
				      struct amdgpu_iv_entry *entry)
{
	DRM_DEBUG("IH: UVD TRAP\n");
	amdgpu_fence_process(&adev->uvd.inst->ring);
	return 0;
}

static int uvd_v4_2_set_clockgating_state(struct amdgpu_ip_block *ip_block,
					  enum amd_clockgating_state state)
{
	return 0;
}

static int uvd_v4_2_set_powergating_state(struct amdgpu_ip_block *ip_block,
					  enum amd_powergating_state state)
{
	/* This doesn't actually powergate the UVD block.
	 * That's done in the dpm code via the SMC.  This
	 * just re-inits the block as necessary.  The actual
	 * gating still happens in the dpm code.  We should
	 * revisit this when there is a cleaner line between
	 * the smc and the hw blocks
	 */
	struct amdgpu_device *adev = ip_block->adev;

	if (state == AMD_PG_STATE_GATE) {
		uvd_v4_2_stop(adev);
		if (adev->pg_flags & AMD_PG_SUPPORT_UVD && !adev->pm.dpm_enabled) {
			if (!(RREG32_SMC(ixCURRENT_PG_STATUS) &
				CURRENT_PG_STATUS__UVD_PG_STATUS_MASK)) {
				WREG32(mmUVD_PGFSM_CONFIG, (UVD_PGFSM_CONFIG__UVD_PGFSM_FSM_ADDR_MASK   |
							UVD_PGFSM_CONFIG__UVD_PGFSM_POWER_DOWN_MASK |
							UVD_PGFSM_CONFIG__UVD_PGFSM_P1_SELECT_MASK));
				mdelay(20);
			}
		}
		return 0;
	} else {
		if (adev->pg_flags & AMD_PG_SUPPORT_UVD && !adev->pm.dpm_enabled) {
			if (RREG32_SMC(ixCURRENT_PG_STATUS) &
				CURRENT_PG_STATUS__UVD_PG_STATUS_MASK) {
				WREG32(mmUVD_PGFSM_CONFIG, (UVD_PGFSM_CONFIG__UVD_PGFSM_FSM_ADDR_MASK   |
						UVD_PGFSM_CONFIG__UVD_PGFSM_POWER_UP_MASK |
						UVD_PGFSM_CONFIG__UVD_PGFSM_P1_SELECT_MASK));
				mdelay(30);
			}
		}
		return uvd_v4_2_start(adev);
	}
}

static const struct amd_ip_funcs uvd_v4_2_ip_funcs = {
	.name = "uvd_v4_2",
	.early_init = uvd_v4_2_early_init,
	.sw_init = uvd_v4_2_sw_init,
	.sw_fini = uvd_v4_2_sw_fini,
	.hw_init = uvd_v4_2_hw_init,
	.hw_fini = uvd_v4_2_hw_fini,
	.prepare_suspend = uvd_v4_2_prepare_suspend,
	.suspend = uvd_v4_2_suspend,
	.resume = uvd_v4_2_resume,
	.is_idle = uvd_v4_2_is_idle,
	.wait_for_idle = uvd_v4_2_wait_for_idle,
	.soft_reset = uvd_v4_2_soft_reset,
	.set_clockgating_state = uvd_v4_2_set_clockgating_state,
	.set_powergating_state = uvd_v4_2_set_powergating_state,
};

static const struct amdgpu_ring_funcs uvd_v4_2_ring_funcs = {
	.type = AMDGPU_RING_TYPE_UVD,
	.align_mask = 0xf,
	.support_64bit_ptrs = false,
	.no_user_fence = true,
	.get_rptr = uvd_v4_2_ring_get_rptr,
	.get_wptr = uvd_v4_2_ring_get_wptr,
	.set_wptr = uvd_v4_2_ring_set_wptr,
	.parse_cs = amdgpu_uvd_ring_parse_cs,
	.emit_frame_size =
		14, /* uvd_v4_2_ring_emit_fence  x1 no user fence */
	.emit_ib_size = 4, /* uvd_v4_2_ring_emit_ib */
	.emit_ib = uvd_v4_2_ring_emit_ib,
	.emit_fence = uvd_v4_2_ring_emit_fence,
	.test_ring = uvd_v4_2_ring_test_ring,
	.test_ib = amdgpu_uvd_ring_test_ib,
	.insert_nop = uvd_v4_2_ring_insert_nop,
	.pad_ib = amdgpu_ring_generic_pad_ib,
	.begin_use = amdgpu_uvd_ring_begin_use,
	.end_use = amdgpu_uvd_ring_end_use,
};

static void uvd_v4_2_set_ring_funcs(struct amdgpu_device *adev)
{
	adev->uvd.inst->ring.funcs = &uvd_v4_2_ring_funcs;
}

static const struct amdgpu_irq_src_funcs uvd_v4_2_irq_funcs = {
	.set = uvd_v4_2_set_interrupt_state,
	.process = uvd_v4_2_process_interrupt,
};

static void uvd_v4_2_set_irq_funcs(struct amdgpu_device *adev)
{
	adev->uvd.inst->irq.num_types = 1;
	adev->uvd.inst->irq.funcs = &uvd_v4_2_irq_funcs;
}

const struct amdgpu_ip_block_version uvd_v4_2_ip_block =
{
		.type = AMD_IP_BLOCK_TYPE_UVD,
		.major = 4,
		.minor = 2,
		.rev = 0,
		.funcs = &uvd_v4_2_ip_funcs,
};
