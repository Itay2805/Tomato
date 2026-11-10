#pragma once

#include <stdint.h>

typedef union {
    struct {
        uint64_t EL0 : 4;
        uint64_t EL1 : 4;
        uint64_t EL2 : 4;
        uint64_t EL3 : 4;
        int64_t FP : 4;
        int64_t ADVSIMD : 4;
        uint64_t GIC : 4;
        uint64_t RAS : 4;
        uint64_t SVE : 4;
        uint64_t SEL2 : 4;
        uint64_t MPAM : 4;
        uint64_t AMU : 4;
        uint64_t DIT : 4;
        uint64_t RME : 4;
        uint64_t CSV2 : 4;
        uint64_t CSV3 : 4;
    };
    uint64_t raw;
} ID_AA64PFR0_EL1;

typedef union {
    struct {
        uint64_t BT : 4;
        uint64_t SSBS : 4;
        uint64_t MTE : 4;
        uint64_t RAS_FRAC : 4;
        uint64_t MPAM_FRAC : 4;
        uint64_t : 4;
        uint64_t SME : 4;
        uint64_t RNDR_TRAP : 4;
        uint64_t CSV2_FRAC : 4;
        uint64_t NMI : 4;
        uint64_t MTE_FRAC : 4;
        uint64_t GCS : 4;
        uint64_t THE : 4;
        uint64_t MTEX : 4;
        uint64_t DF2 : 4;
        uint64_t PFAR : 4;
    };
    uint64_t raw;
} ID_AA64PFR1_EL1;

typedef union {
    struct {
        uint64_t : 4;
        uint64_t AES : 4;
        uint64_t SHA1 : 4;
        uint64_t SHA2 : 4;
        uint64_t CRC32 : 4;
        uint64_t ATOMIC : 4;
        uint64_t : 4;
        uint64_t RDM : 4;
        uint64_t SHA3 : 4;
        uint64_t SM3 : 4;
        uint64_t SM4 : 4;
        uint64_t DP : 4;
        uint64_t FHM : 4;
        uint64_t TS : 4;
        uint64_t TLB : 4;
        uint64_t RNDR : 4;
    };
    uint64_t raw;
} ID_AA64ISAR0_EL1;

typedef union {
    struct {
        uint64_t DPB : 4;
        uint64_t APA : 4;
        uint64_t API : 4;
        uint64_t JSCVT : 4;
        uint64_t FCMA : 4;
        uint64_t LRCPC : 4;
        uint64_t GPA : 4;
        uint64_t GPI : 4;
        uint64_t FRINTTS : 4;
        uint64_t SB : 4;
        uint64_t SPECRES : 4;
        uint64_t BF16 : 4;
        uint64_t DGH : 4;
        uint64_t I8MM : 4;
        uint64_t XS : 4;
        uint64_t LS64 : 4;
    };
    uint64_t raw;
} ID_AA64ISAR1_EL1;

typedef union {
    struct {
        uint64_t WFXT : 4;
        uint64_t RPRES : 4;
        uint64_t GPA3 : 4;
        uint64_t APA3 : 4;
        uint64_t MOPS : 4;
        uint64_t BC : 4;
        uint64_t PAC_FRAC : 4;
        uint64_t CLRBHB : 4;
        uint64_t SYSREG_128 : 4;
        uint64_t SYSINSTR_128 : 4;
        uint64_t PRFMSLC : 4;
        uint64_t PCDPHINT : 4;
        uint64_t RPRFM : 4;
        uint64_t CSSC : 4;
        uint64_t LUT : 4;
        uint64_t ATS1A : 4;
    };
    uint64_t raw;
} ID_AA64ISAR2_EL1;

typedef union {
    struct {
        uint64_t PARANGE : 4;
        uint64_t ASIDBITS : 4;
        uint64_t BIGEND : 4;
        uint64_t SNSMEM : 4;
        uint64_t BIGENDEL0 : 4;
        uint64_t TGRAN16 : 4;
        int64_t TGRAN64 : 4;
        int64_t TGRAN4 : 4;
        uint64_t TGRAN16_2 : 4;
        uint64_t TGRAN64_2 : 4;
        uint64_t TGRAN4_2 : 4;
        uint64_t EXS : 4;
        uint64_t : 8;
        uint64_t FGT : 4;
        uint64_t ECV : 4;
    };
    uint64_t raw;
} ID_AA64MMFR0_EL1;

typedef union {
    struct {
        uint64_t HAFDBS : 4;
        uint64_t VMIDBITS : 4;
        uint64_t VH : 4;
        uint64_t HPDS : 4;
        uint64_t LO : 4;
        uint64_t PAN : 4;
        uint64_t SPECSEI : 4;
        uint64_t XNX : 4;
        uint64_t TWED : 4;
        uint64_t ETS : 4;
        uint64_t HCX : 4;
        uint64_t AFP : 4;
        uint64_t NTLBPA : 4;
        uint64_t TIDCP1 : 4;
        uint64_t CMOW : 4;
        uint64_t ECBHB : 4;
    };
    uint64_t raw;
} ID_AA64MMFR1_EL1;

typedef union {
    struct {
        uint64_t CNP : 4;
        uint64_t UAO : 4;
        uint64_t LSM : 4;
        uint64_t IESB : 4;
        uint64_t VARANGE : 4;
        uint64_t CCIDX : 4;
        uint64_t NV : 4;
        uint64_t ST : 4;
        uint64_t AT : 4;
        uint64_t IDS : 4;
        uint64_t FWB : 4;
        uint64_t : 4;
        uint64_t TTL : 4;
        uint64_t BBM : 4;
        uint64_t EVT : 4;
        uint64_t E0PD : 4;
    };
    uint64_t raw;
} ID_AA64MMFR2_EL1;
