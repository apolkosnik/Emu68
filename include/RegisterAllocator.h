/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _REGISTER_ALLOCATOR_H
#define _REGISTER_ALLOCATOR_H

#include <stdint.h>

typedef struct {
    uint8_t m68k_arm_reg[16];
    uint8_t m68k_dirty[16];
    int8_t lru_table[8];
    uint16_t register_pool;
    uint16_t changed_mask;
    uint8_t fpu_alloc_state;
    uint8_t fpu_reg_state[8];
    uint8_t got_cc;
    uint8_t mod_cc;
    uint8_t reg_fpcr;
    uint8_t mod_fpcr;
    uint8_t reg_fpsr;
    uint8_t mod_fpsr;
} RAStateSnapshot;

void RA_TouchM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg);
void RA_SetDirtyM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg);
void RA_InsertM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg);
void RA_RemoveM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg);
void RA_DiscardM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg);
void RA_FlushM68kRegs(uint32_t **arm_stream);
void RA_StoreDirtyM68kRegs(uint32_t **arm_stream);

uint16_t RA_GetChangedMask();
void RA_ClearChangedMask();

uint8_t RA_AllocARMRegister(uint32_t **arm_stream);
void RA_FreeARMRegister(uint32_t **arm_stream, uint8_t arm_reg);
int RA_IsM68kRegister(uint8_t arm_reg);

uint8_t RA_GetMappedARMRegister(uint8_t m68k_reg);
uint8_t RA_IsARMRegisterMapped(uint8_t arm_reg);
void RA_AssignM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg, uint8_t arm_reg);
uint8_t RA_MapM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg);
uint8_t RA_MapM68kRegisterForWrite(uint32_t **arm_stream, uint8_t m68k_reg);
void RA_UnmapM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg);
uint8_t RA_CopyFromM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg);
uint16_t RA_GetTempAllocMask();

void RA_ResetFPUAllocator();
uint8_t RA_AllocFPURegister(uint32_t **arm_stream);
void RA_FreeFPURegister(uint32_t **arm_stream, uint8_t arm_reg);
uint8_t RA_MapFPURegister(uint32_t **arm_stream, uint8_t fpu_reg);
uint8_t RA_MapFPURegisterForWrite(uint32_t **arm_stream, uint8_t fpu_reg);
void RA_SetDirtyFPURegister(uint32_t **arm_stream, uint8_t fpu_reg);
void RA_FlushFPURegs(uint32_t **arm_stream);
void RA_StoreDirtyFPURegs(uint32_t **arm_stream);

uint8_t RA_TryCTX(uint32_t **ptr);
uint8_t RA_GetCTX(uint32_t **ptr);
void RA_FlushCTX(uint32_t **ptr);
int RA_IsCCLoaded();
int RA_IsCCModified();
uint8_t RA_GetCC(uint32_t **ptr);
uint8_t RA_ModifyCC(uint32_t **ptr);
void RA_FlushCC(uint32_t **ptr);
void RA_StoreCC(uint32_t **ptr);
uint8_t RA_GetFPCR(uint32_t **ptr);
uint8_t RA_ModifyFPCR(uint32_t **ptr);
void RA_FlushFPCR(uint32_t **ptr);
void RA_StoreFPCR(uint32_t **ptr);
uint8_t RA_GetFPSR(uint32_t **ptr);
uint8_t RA_ModifyFPSR(uint32_t **ptr);
void RA_FlushFPSR(uint32_t **ptr);
void RA_StoreFPSR(uint32_t **ptr);

uint32_t *EMIT_SaveRegFrame(uint32_t *ptr, uint32_t mask);
uint32_t *EMIT_RestoreRegFrame(uint32_t *ptr, uint32_t mask);

void RA_SaveState(RAStateSnapshot *snapshot);
void RA_RestoreState(const RAStateSnapshot *snapshot);

#endif /* _REGISTER_ALLOCATOR_H */
