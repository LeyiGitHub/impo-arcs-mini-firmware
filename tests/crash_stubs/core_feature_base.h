#pragma once
#define CSR_MCAUSE 0
#define CSR_MEPC 1
#define CSR_MTVAL 2
#define CSR_MDCAUSE 3
#define __RV_CSR_READ(csr) test_csr_read(csr)
