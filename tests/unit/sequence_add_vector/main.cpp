#include <iostream>

#include "cmemoir/cmemoir.h"

using namespace memoir;

void add(memoir::Collection *seq1,
         memoir::Collection *seq2,
         memoir::Collection *seq3) {
  memoir_index_write(
      u64,
      memoir_index_read(u64, seq1, 0) + memoir_index_read(u64, seq2, 0),
      seq3,
      0);
  memoir_index_write(
      u64,
      memoir_index_read(u64, seq1, 1) + memoir_index_read(u64, seq2, 1),
      seq3,
      1);
  memoir_index_write(
      u64,
      memoir_index_read(u64, seq1, 2) + memoir_index_read(u64, seq2, 2),
      seq3,
      2);
  return;
}

int main(int argc, char *argv[]) {
  printf("\nInitializing sequence\n");

  auto *seq1 = memoir_allocate_sequence(memoir_u64_t, 3);
  auto *seq2 = memoir_allocate_sequence(memoir_u64_t, 3);
  auto *seq3 = memoir_allocate_sequence(memoir_u64_t, 3);

  for (auto i = 0; i < 3; i++) {
    memoir_index_write(u64, i, seq1, i);
    memoir_index_write(u64, i, seq2, i + 3);
  }

  printf("\nAdding\n");

  add(seq1, seq2, seq3);

  printf("\nResult: \n");
  for (auto i = 0; i < 3; i++) {
    auto read = memoir_index_read(u64, seq3, i);
    printf("%lu, ", read);
  }
  printf("\n");

  return 0;
}
