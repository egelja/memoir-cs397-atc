#include <iostream>

#include "cmemoir/cmemoir.h"

#define SEQLEN 10

using namespace memoir;

void add(memoir::Collection *seq1,
         memoir::Collection *seq2,
         memoir::Collection *seq3) {

  for (size_t i = 0; i < SEQLEN; ++i) {
    memoir_index_write(
        u64,
        memoir_index_read(u64, seq1, i) + memoir_index_read(u64, seq2, i),
        seq3,
        i);
  }

  return;
}

int main(int argc, char *argv[]) {
  printf("\nInitializing sequence\n");

  auto *seq1 = memoir_allocate_sequence(memoir_u64_t, SEQLEN);
  auto *seq2 = memoir_allocate_sequence(memoir_u64_t, SEQLEN);
  auto *seq3 = memoir_allocate_sequence(memoir_u64_t, SEQLEN);

  for (size_t i = 0; i < SEQLEN; i++) {
    memoir_index_write(u64, i, seq1, i);
    memoir_index_write(u64, i * 10, seq2, i);
  }

  printf("\nAdding\n");

  add(seq1, seq2, seq3);

  printf("\nResult: \n");
  for (auto i = 0; i < SEQLEN; i++) {
    auto read = memoir_index_read(u64, seq3, i);
    printf("%02lu, ", read);
  }
  printf("\n");

  return 0;
}
