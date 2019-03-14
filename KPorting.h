#ifndef KPORTING_H
#define KPORTING_H

#ifdef HAVE_VISIBILITY_ATTRIBUTE
    #define MOZ_EXPORT       __attribute__((visibility("default")))
#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
    #define MOZ_EXPORT      __global
#else
    #define MOZ_EXPORT /* nothing */
#endif

//#define ANDROID_EMULATOR 1

#ifdef DEBUG
#  define MOZ_ASSERT(e...) __android_log_assert(e, "TAG", #e)
#else
#  define MOZ_ASSERT(...) do { } while (0)
#endif /* DEBUG */


#endif /* KPORTING_H */