#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define REG_TENSOR_STREAM   0x00
#define REG_CONSENT_LATCH   0x01
#define REG_STATUS          0x02
#define REG_HW_CYCLE_LO     0x03
#define REG_EGRESS_STROBE   0x04
#define REG_AUDIT_HASH_BASE 0x08

static inline uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

int clinical_bar0_enforce_and_record(
    const char* pci_resource0_path,
    uint32_t consent_token,
    const uint32_t* payload_words,
    size_t word_count,
    uint32_t audit_digest_out[8],
    uint32_t* status_out,
    double* latency_ns_out
) {
    int fd = open(pci_resource0_path, O_RDWR | O_SYNC);
    if (fd < 0) return -1;

    void* map_base = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map_base == MAP_FAILED) {
        close(fd);
        return -2;
    }

    volatile uint32_t* reg = (volatile uint32_t*)map_base;
    uint64_t t0 = now_ns();
    reg[REG_CONSENT_LATCH] = consent_token;
    for (size_t i = 0; i < word_count; i++) {
        reg[REG_TENSOR_STREAM] = payload_words[i];
    }
    reg[REG_EGRESS_STROBE] = 0x00000001U;

    uint32_t status = reg[REG_STATUS];
    for (int h = 0; h < 8; h++) {
        audit_digest_out[h] = reg[REG_AUDIT_HASH_BASE + h];
    }
    uint64_t t1 = now_ns();

    *status_out = status;
    *latency_ns_out = (double)(t1 - t0);

    munmap(map_base, 4096);
    close(fd);
    return 0;
}