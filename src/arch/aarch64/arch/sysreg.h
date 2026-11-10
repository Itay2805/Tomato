#pragma once

#include "util/defs.h"

#define CURRENT_EL_EL1 (1 << 2)
#define CURRENT_EL_EL2 (2 << 2)

/**
 * The HCR_EL2 bits that make EL2 the host of EL0 (VHE)
 */
#define HCR_TGE BIT27
#define HCR_E2H BIT34

#define SCTLR_M    BIT0
#define SCTLR_A    BIT1
#define SCTLR_C    BIT2
#define SCTLR_SA   BIT3
#define SCTLR_SA0  BIT4
#define SCTLR_I    BIT12
#define SCTLR_WXN  BIT19
#define SCTLR_SPAN BIT23

/**
 * The PSTATE.DAIF bits, also the layout of the DAIF register
 */
#define DAIF_F BIT6
#define DAIF_I BIT7
#define DAIF_A BIT8
#define DAIF_D BIT9

#define SPSR_M_MASK 0b1111
#define SPSR_M_SP   BIT0
#define SPSR_F      DAIF_F
#define SPSR_I      DAIF_I
#define SPSR_A      DAIF_A
#define SPSR_D      DAIF_D
#define SPSR_PAN    BIT22
#define SPSR_V      BIT28
#define SPSR_C      BIT29
#define SPSR_Z      BIT30
#define SPSR_N      BIT31

#define PAR_F BIT0

/**
 * Exception syndrome
 */
#define ESR_EC(esr)  (((esr) >> 26) & 0x3F)
#define ESR_ISS(esr) ((esr) & 0x1FFFFFF)

#define ESR_EC_UNKNOWN       0x00
#define ESR_EC_WFX           0x01
#define ESR_EC_FP_ASIMD      0x07
#define ESR_EC_ILLEGAL_STATE 0x0E
#define ESR_EC_SVC64         0x15
#define ESR_EC_SYS64         0x18
#define ESR_EC_SVE           0x19
#define ESR_EC_IABT_LOW      0x20
#define ESR_EC_IABT_CUR      0x21
#define ESR_EC_PC_ALIGN      0x22
#define ESR_EC_DABT_LOW      0x24
#define ESR_EC_DABT_CUR      0x25
#define ESR_EC_SP_ALIGN      0x26
#define ESR_EC_FP_EXC64      0x2C
#define ESR_EC_SERROR        0x2F
#define ESR_EC_BREAKPT_LOW   0x30
#define ESR_EC_BREAKPT_CUR   0x31
#define ESR_EC_SOFTSTP_LOW   0x32
#define ESR_EC_SOFTSTP_CUR   0x33
#define ESR_EC_WATCHPT_LOW   0x34
#define ESR_EC_WATCHPT_CUR   0x35
#define ESR_EC_BRK64         0x3C

/**
 * Instruction and data abort syndrome
 */
#define ESR_ISS_ABT_FSC_MASK 0x3F
#define ESR_ISS_ABT_WNR      BIT6
#define ESR_ISS_ABT_CM       BIT8
#define ESR_ISS_ABT_FNV      BIT10

/**
 * Fault status codes, without the level in the low 2 bits
 */
#define ESR_FSC_TYPE(fsc)    ((fsc) & 0x3C)
#define ESR_FSC_LEVEL(fsc)   ((fsc) & 0b11)
#define ESR_FSC_TRANSLATION  0x04
#define ESR_FSC_ACCESS_FLAG  0x08
#define ESR_FSC_PERMISSION   0x0C
#define ESR_FSC_ALIGNMENT    0x21
#define ESR_FSC_EXTERNAL     0x10
#define ESR_FSC_TLB_CONFLICT 0x30
