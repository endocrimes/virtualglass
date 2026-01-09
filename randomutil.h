#ifndef RANDOMUTIL_H
#define RANDOMUTIL_H

#include <cstdint>

// Qt 6 compatible random functions (replaces qsrand/qrand)
void vg_srand(uint64_t seed);
int vg_rand();

#endif // RANDOMUTIL_H
