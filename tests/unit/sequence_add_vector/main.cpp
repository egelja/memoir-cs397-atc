#include <iostream>

#include "cmemoir/cmemoir.h"

#define SEQLEN 16 // must be multiple of 4 lol

#define DO_ADD(d, s1, s2, i)                                                   \
  memoir_index_write(                                                          \
      u64,                                                                     \
      memoir_index_read(u64, s1, i) + memoir_index_read(u64, s2, i),           \
      d,                                                                       \
      i)

using namespace memoir;

void add_loop(memoir::Collection *seq1,
              memoir::Collection *seq2,
              memoir::Collection *seq3) {
  for (size_t i = 0; i < SEQLEN; i += 4) {
    // manually unrolled
    DO_ADD(seq3, seq1, seq2, i);
    DO_ADD(seq3, seq1, seq2, i + 1);
    DO_ADD(seq3, seq1, seq2, i + 2);
    DO_ADD(seq3, seq1, seq2, i + 3);
  }

  return;
}

void add_4(memoir::Collection *seq1,
           memoir::Collection *seq2,
           memoir::Collection *seq3) {
  DO_ADD(seq3, seq1, seq2, 0);
  DO_ADD(seq3, seq1, seq2, 1);
  DO_ADD(seq3, seq1, seq2, 2);
  DO_ADD(seq3, seq1, seq2, 3);
}

void print_seq(memoir::Collection *seq) {
  printf(" Result: \n");
  for (auto i = 0; i < SEQLEN; i++) {
    auto read = memoir_index_read(u64, seq, i);
    printf("%02lu, ", read);
  }
  printf("\n");
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

  // add first 4
  printf("\nAdding first 4...");

  add_4(seq1, seq2, seq3);
  print_seq(seq3);

  // add in a loop
  printf("\nAdding in a loop...");

  add_loop(seq1, seq2, seq3);
  print_seq(seq3);

  return 0;
}
