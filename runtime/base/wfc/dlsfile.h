/* DLS1 files of Dragon Quest IX (quests, DQVC stock, news):
 *   file = RC4(key, head || payload) || seal
 * head: 8 little-endian words (number, payload length, then 27, 2, 0x10065,
 * 0, 0x10064, 20); payload: elements, then the timestamp trailer (32 bytes);
 * seal: 4 big-endian bytes that make the whole file, read as one big-endian
 * number, a multiple of 0xC2A030D4. */
#pragma once
#include "wfc_ip.h"

#if defined(__ELF__) && defined(__GNUC__)
#pragma GCC visibility push(hidden)
#endif

#define DLS_KEY "XENLONPROJECTKEY"

void dls_rc4(const uint8_t* key, size_t klen, uint8_t* data, size_t n);   /* in place */
uint32_t dls_seal(const uint8_t* enc, size_t n);
/* a whole file: head, the elements, the trailer for time t, encrypted and sealed */
void dls_build(WfcBuf* out, uint32_t number, const uint8_t* elements, size_t n, int64_t t);

#if defined(__ELF__) && defined(__GNUC__)
#pragma GCC visibility pop
#endif
