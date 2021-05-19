#ifndef MIN
#define MIN(_x, _y) ((_x) <= (_y) ? (_x) : (_y))
#endif

#ifndef ARRLEN
#define ARRLEN(_x) (sizeof((_x)) / sizeof((_x)[0]))
#endif
