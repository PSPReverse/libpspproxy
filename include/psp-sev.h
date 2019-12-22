/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Userspace interface for AMD Secure Encrypted Virtualization (SEV)
 * platform management commands.
 *
 * Copyright (C) 2016-2017 Advanced Micro Devices, Inc.
 *
 * Author: Brijesh Singh <brijesh.singh@amd.com>
 *
 * SEV API specification is available at: https://developer.amd.com/sev/
 */

#ifndef __PSP_SEV_USER_H__
#define __PSP_SEV_USER_H__

#include <linux/types.h>

/**
 * SEV platform commands
 */
enum {
    SEV_FACTORY_RESET = 0,
    SEV_PLATFORM_STATUS,
    SEV_PEK_GEN,
    SEV_PEK_CSR,
    SEV_PDH_GEN,
    SEV_PDH_CERT_EXPORT,
    SEV_PEK_CERT_IMPORT,
    SEV_GET_ID,    /* This command is deprecated, use SEV_GET_ID2 */
    SEV_GET_ID2,

    SEV_PSP_STUB_LOAD_BIN = 0xf0,
    SEV_PSP_STUB_EXEC_BIN,
    SEV_PSP_STUB_SMN_READ,
    SEV_PSP_STUB_SMN_WRITE,
    SEV_PSP_STUB_PSP_READ,
    SEV_PSP_STUB_PSP_WRITE,
    SEV_PSP_STUB_PSP_X86_READ,
    SEV_PSP_STUB_PSP_X86_WRITE,
    SEV_PSP_STUB_CALL_SVC,
    SEV_X86_SMN_READ,
    SEV_X86_SMN_WRITE,
    SEV_X86_MEM_ALLOC,
    SEV_X86_MEM_FREE,
    SEV_X86_MEM_READ,
    SEV_X86_MEM_WRITE,
    SEV_EMU_WAIT_FOR_WORK,
    SEV_EMU_SET_RESULT,

    SEV_MAX,
};

/**
 * SEV Firmware status code
 */
typedef enum {
    SEV_RET_SUCCESS = 0,
    SEV_RET_INVALID_PLATFORM_STATE,
    SEV_RET_INVALID_GUEST_STATE,
    SEV_RET_INAVLID_CONFIG,
    SEV_RET_INVALID_LEN,
    SEV_RET_ALREADY_OWNED,
    SEV_RET_INVALID_CERTIFICATE,
    SEV_RET_POLICY_FAILURE,
    SEV_RET_INACTIVE,
    SEV_RET_INVALID_ADDRESS,
    SEV_RET_BAD_SIGNATURE,
    SEV_RET_BAD_MEASUREMENT,
    SEV_RET_ASID_OWNED,
    SEV_RET_INVALID_ASID,
    SEV_RET_WBINVD_REQUIRED,
    SEV_RET_DFFLUSH_REQUIRED,
    SEV_RET_INVALID_GUEST,
    SEV_RET_INVALID_COMMAND,
    SEV_RET_ACTIVE,
    SEV_RET_HWSEV_RET_PLATFORM,
    SEV_RET_HWSEV_RET_UNSAFE,
    SEV_RET_UNSUPPORTED,
    SEV_RET_MAX,
} sev_ret_code;

/**
 * struct sev_user_data_status - PLATFORM_STATUS command parameters
 *
 * @major: major API version
 * @minor: minor API version
 * @state: platform state
 * @flags: platform config flags
 * @build: firmware build id for API version
 * @guest_count: number of active guests
 */
struct sev_user_data_status {
    __u8 api_major;                /* Out */
    __u8 api_minor;                /* Out */
    __u8 state;                    /* Out */
    __u32 flags;                   /* Out */
    __u8 build;                    /* Out */
    __u32 guest_count;             /* Out */
} __attribute__((packed));

/**
 * struct sev_user_data_pek_csr - PEK_CSR command parameters
 *
 * @address: PEK certificate chain
 * @length: length of certificate
 */
struct sev_user_data_pek_csr {
    __u64 address;                 /* In */
    __u32 length;                  /* In/Out */
} __attribute__((packed));

/**
 * struct sev_user_data_cert_import - PEK_CERT_IMPORT command parameters
 *
 * @pek_address: PEK certificate chain
 * @pek_len: length of PEK certificate
 * @oca_address: OCA certificate chain
 * @oca_len: length of OCA certificate
 */
struct sev_user_data_pek_cert_import {
    __u64 pek_cert_address;        /* In */
    __u32 pek_cert_len;            /* In */
    __u64 oca_cert_address;        /* In */
    __u32 oca_cert_len;            /* In */
} __attribute__((packed));

/**
 * struct sev_user_data_pdh_cert_export - PDH_CERT_EXPORT command parameters
 *
 * @pdh_address: PDH certificate address
 * @pdh_len: length of PDH certificate
 * @cert_chain_address: PDH certificate chain
 * @cert_chain_len: length of PDH certificate chain
 */
struct sev_user_data_pdh_cert_export {
    __u64 pdh_cert_address;        /* In */
    __u32 pdh_cert_len;            /* In/Out */
    __u64 cert_chain_address;      /* In */
    __u32 cert_chain_len;          /* In/Out */
} __attribute__((packed));

/**
 * struct sev_user_data_get_id - GET_ID command parameters (deprecated)
 *
 * @socket1: Buffer to pass unique ID of first socket
 * @socket2: Buffer to pass unique ID of second socket
 */
struct sev_user_data_get_id {
    __u8 socket1[64];              /* Out */
    __u8 socket2[64];              /* Out */
} __attribute__((packed));

/**
 * struct sev_user_data_get_id2 - GET_ID command parameters
 * @address: Buffer to store unique ID
 * @length: length of the unique ID
 */
struct sev_user_data_get_id2 {
    __u64 address;                /* In */
    __u32 length;                /* In/Out */
} __attribute__((packed));

/**
 * struct sev_user_data_psp_stub_load_bin - SEV_PSP_STUB_LOAD_BIN command parameters
 *
 * @binary_address: Binary buffer address
 * @binary_size: Size of the binary in bytes
 * @ccd_id: The CCD ID to load the binary on
 * @status: The status code returned by the request
 */
struct sev_user_data_psp_stub_load_bin {
    __u64 binary_address;          /* In */
    __u32 binary_size;             /* In */
    __u32 ccd_id;                  /* In */
    __s32 status;                  /* Out */
} __attribute__((packed));

/**
 * struct sev_user_data_psp_stub_exec_bin - SEV_PSP_STUB_EXEC_BIN command parameters
 *
 * @req_address: Request buffer address
 * @ccd_id: The CCD ID to execute the binary on
 * @status: The status code returned by the request
 */
struct sev_user_data_psp_stub_exec_bin {
    __u64 req_address;             /* In */
    __u32 ccd_id;                  /* In */
    __s32 status;                  /* Out */
} __attribute__((packed));

/**
 * struct sev_user_data_psp_stub_smn_read - SEV_PSP_STUB_SMN_READ/SEV_PSP_STUB_SMN_WRITE command parameters
 *
 * @ccd_id: The CCD ID to execute the request on
 * @ccd_tgt_id: The target CCD to read a register from
 * @smn_addr: SMN address to operate on
 * @size: Value size (1, 2, 4 or 8)
 * @value: Contains the value on successful read or value to write
 * @status: The status code returned by the request
 */
struct sev_user_data_psp_stub_smn_rw {
    __u32 ccd_id;                  /* In */
    __u32 ccd_id_tgt;              /* In */
    __u32 smn_addr;                /* In */
    __u32 size;                    /* In */
    __u64 value;                   /* Out */
    __s32 status;                  /* Out */
} __attribute__((packed));

/**
 * struct sev_user_data_psp_stub_psp_rw - SEV_PSP_STUB_PSP_READ/SEV_PSP_STUB_PSP_WRITE command parameters
 *
 * @ccd_id: The CCD ID to execute the request on
 * @psp_addr: PSP address to read/write from/to
 * @buf: Userspace buffer to read the data into or write data from
 * @size: Number of bytes to copy
 * @status: The status code returned by the request
 */
struct sev_user_data_psp_stub_psp_rw {
    __u32 ccd_id;                  /* In */
    __u32 psp_addr;                /* In */
    __u64 buf;                     /* In */
    __u32 size;                    /* In */
    __s32 status;                  /* Out */
} __attribute__((packed));

/**
 * struct sev_user_data_psp_stub_psp_x86_rw - SEV_PSP_STUB_PSP_X86_READ/SEV_PSP_STUB_PSP_X86_WRITE command parameters
 *
 * @ccd_id: The CCD ID to execute the request on
 * @size: Number of bytes to copy
 * @x86_phys: x86 physical address to read/write from/to
 * @buf: Userspace buffer to read the data into or write data from
 * @status: The status code returned by the request
 */
struct sev_user_data_psp_stub_psp_x86_rw {
    __u32 ccd_id;                  /* In */
    __u32 size;                    /* In */
    __u64 x86_phys;                /* In */
    __u64 buf;                     /* In */
    __s32 status;                  /* Out */
} __attribute__((packed));

/**
 * struct sev_user_data_psp_stub_svc_call - SEV_PSP_STUB_CALL_SVC command parameters
 *
 * @ccd_id: The CCD ID to execute the request on
 * @syscall: The syscall to execute
 * @r0: R0 argument
 * @r1: R1 argument
 * @r2: R2 argument
 * @r3: R3 argument
 * @r0_return: R0 content upon syscall return
 * @status: The status code returned by the request
 */
struct sev_user_data_psp_stub_svc_call {
    __u32 ccd_id;                  /* In */
    __u32 syscall;                 /* In */
    __u32 r0;                      /* In */
    __u32 r1;                      /* In */
    __u32 r2;                      /* In */
    __u32 r3;                      /* In */
    __u32 r0_return;               /* Out */
    __s32 status;                  /* Out */
} __attribute__((packed));

/**
 * struct sev_user_data_x86_smn_rw - SEV_X86_SMN_READ/SEV_X86_SMN_WRITE ioctl parameters
 *
 * @cmd: SEV commands to execute
 * @opaque: pointer to the command structure
 * @error: SEV FW return code on failure
 */
struct sev_user_data_x86_smn_rw {
    __u16 node;                    /* In */
    __u16 rsvd;                    /* Reserved/Padding */
    __u32 addr;                    /* In */
    __u32 value;                   /* In/Out */
} __attribute__((packed));

/**
 * struct sev_user_data_x86_mem_alloc - SEV_X86_MEM_ALLOC ioctl parameters
 *
 * @size: Size of the region to allocate in bytes
 * @addr_virtual: Virtual address of the allocated memory on return
 * @addr_physical: Physical address of the allocated memory on return
 */
struct sev_user_data_x86_mem_alloc {
    __u32 size;                    /* In */
    __u32 rsvd;                    /* Padding */
    __u64 addr_virtual;            /* Out */
    __u64 addr_physical;           /* Out */
} __attribute__((packed));

/**
 * struct sev_user_data_x86_mem_free - SEV_X86_MEM_FREE ioctl parameters
 *
 * @addr_virtual: Address to free
 */
struct sev_user_data_x86_mem_free {
    __u64 addr_virtual;            /* In */
} __attribute__((packed));

/**
 * struct sev_user_data_x86_mem_rw - SEV_X86_MEM_READ/SEV_X86_MEM_WRITE command parameters
 *
 * @kern_buf: Kernel virtual address of the source/destination
 * @user_buf: Userspace virtual address of the destination/source
 * @size: Number of bytes to copy
 */
struct sev_user_data_x86_mem_rw {
    __u64 kern_buf;                /* In */
    __u64 user_buf;                /* In */
    __u32 size;                    /* In */
} __attribute__((packed));

struct sev_user_data_emu_wait_for_work {
    __u32 timeout;                 /* In */
    __u32 cmd;                     /* Out */
    __u32 phys_lsb;                /* Out */
    __u32 phys_msb;                /* Out */
} __attribute__((packed));

struct sev_user_data_emu_set_result {
    __u32 result;
} __attribute__((packed));

/**
 * struct sev_issue_cmd - SEV ioctl parameters
 *
 * @cmd: SEV commands to execute
 * @opaque: pointer to the command structure
 * @error: SEV FW return code on failure
 */
struct sev_issue_cmd {
    __u32 cmd;                     /* In */
    __u64 data;                    /* In */
    __u32 error;                   /* Out */
} __attribute__((packed));

#define SEV_IOC_TYPE     'S'
#define SEV_ISSUE_CMD    _IOWR(SEV_IOC_TYPE, 0x0, struct sev_issue_cmd)

#endif /* __PSP_USER_SEV_H */
