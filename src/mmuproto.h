/* UNIVERSIDADE FEDERAL DE MINAS GERAIS     *
 * DEPARTAMENTO DE CIENCIA DA COMPUTACAO    *
 * Copyright (c) Italo Fernando Scota Cunha */

/* MMU protocol operation
 *
 * Each message is signaled with a `uint32_t` code.  Message codes
 * are defined below.  Message with codes ending in `REQ` are sent
 * from client (uvm.c) to MMU (mmu.c); message codes ending in `REP`
 * are sent from the MMU (mmu.c) to clients.
 *
 * The `CREATE` message and its reply are exchanged before the
 * `vmu_thread` starts.  Clients send their PID to the MMU, and
 * receive the path to the memory-mapped file representing physical
 * memory.
 *
 * The `EXTEND` and `SEGV` messages are generated by the client when
 * they allocate memory and experience a segmentation fault,
 * respectively.  The request functions (`uvm_extend` and
 * `uvm_segv_action`) wait on a condition variable for the request
 * to be serviced.
 *
 * The `REMAP` and `CHPROT` messages are generated by the MMU and
 * are processed by `uvm_thread` asynchronously.  These messages are
 * used to service sergmentation faults and whenever the pager pages
 * some of the processes pages to disk. */

#ifndef __MMUPROTO_HEADER__
#define __MMUPROTO_HEADER__

/* From UNIX_PATH_MAX, see man (7) unix: */
#define MMU_PROTO_PATH_MAX 108
#define MMU_PROTO_UNIX_PATH "mmu.sock"

#define MMU_PROTO_CREATE_REQ 1
#define MMU_PROTO_CREATE_REP 2
#define MMU_PROTO_EXTEND_REQ 3
#define MMU_PROTO_EXTEND_REP 4
#define MMU_PROTO_SYSLOG_REQ 5
#define MMU_PROTO_SYSLOG_REP 6
#define MMU_PROTO_SEGV_REQ 7
#define MMU_PROTO_SEGV_REP 8
#define MMU_PROTO_REMAP_REQ 9
#define MMU_PROTO_REMAP_REP 10
#define MMU_PROTO_CHPROT_REQ 11
#define MMU_PROTO_CHPROT_REP 12
#define MMU_PROTO_EXIT_REQ 32
#define MMU_PROTO_EXIT_REP 33

struct mmu_proto_create_req {
	uint32_t type;
	uint32_t pid;
} __attribute__((packed));
struct mmu_proto_create_rep {
	uint32_t type;
	char pmem_fn[MMU_PROTO_PATH_MAX];
} __attribute__((packed));

struct mmu_proto_extend_req {
	uint32_t type;
} __attribute__((packed));
struct mmu_proto_extend_rep {
	uint32_t type;
	uint64_t vaddr;
} __attribute__((packed));

struct mmu_proto_syslog_req {
	uint32_t type;
	uint32_t len;
	uint64_t addr;
} __attribute__((packed));
struct mmu_proto_syslog_rep {
	uint32_t type;
	uint32_t retcode;
} __attribute__((packed));

struct mmu_proto_segv_req {
	uint32_t type;
	int32_t code;
	uint64_t addr;
} __attribute__((packed));
struct mmu_proto_segv_rep {
	uint32_t type;
} __attribute__((packed));
// segv causes remap and chprot to happen

struct mmu_proto_remap_req {
	uint32_t type;
} __attribute__((packed));
struct mmu_proto_remap_rep {
	uint32_t type;
	int32_t prot;
	uint64_t offset;
	uint64_t vaddr;
} __attribute__((packed));

struct mmu_proto_chprot_req {
	uint32_t type;
} __attribute__((packed));
struct mmu_proto_chprot_rep {
	uint32_t type;
	int32_t prot;
	uint64_t vaddr;
} __attribute__((packed));

struct mmu_proto_exit_req {
	uint32_t type;
} __attribute__((packed));
struct mmu_proto_exit_rep {
	uint32_t type;
} __attribute__((packed));

#endif
