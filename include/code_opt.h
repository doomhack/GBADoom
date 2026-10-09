#ifndef CODE_OPT_H
#define CODE_OPT_H

//*****************************************
//Optimisation levels for the ARM code in
//IWRAM on the GBA.
//
//IWRAM holds this code and the main stack,
//and gbadoom.ld fails the link if less than
//2 KB is left for the stack, so code size
//matters. See IWRAM_BUDGET.md.
//
//HOT_CODE:  inner loops that run per pixel
//           or per sample.
//WARM_CODE: per column, span or seg work
//           around them (the BSP chain).
//Everything else in the .iwram.c files and
//TIMI_IWRAM: ARM_CODE_OPT_LEVEL, set with
//ARM_CODE_DEFAULT at the top of the file.
//
//GCC won't inline a function into one with
//a different optimise level, so functions
//that should inline into each other need
//the same level.
//*****************************************

#define HOT_CODE_OPT_LEVEL  "O3"
#define WARM_CODE_OPT_LEVEL "O3"
#define ARM_CODE_OPT_LEVEL  "Os"

#define CODE_OPT_PRAGMA_(x) _Pragma(#x)
#define CODE_OPT_PRAGMA(level) CODE_OPT_PRAGMA_(GCC optimize (level))

#ifdef GBA
    #define HOT_CODE __attribute__((optimize(HOT_CODE_OPT_LEVEL)))
    #define WARM_CODE __attribute__((optimize(WARM_CODE_OPT_LEVEL)))
    #define ARM_CODE_DEFAULT CODE_OPT_PRAGMA(ARM_CODE_OPT_LEVEL)
#else
    #define HOT_CODE
    #define WARM_CODE
    #define ARM_CODE_DEFAULT
#endif

#endif
