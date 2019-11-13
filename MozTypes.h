#ifndef MOZTYPES_H
#define MOZTYPES_H

#ifdef HAVE_VISIBILITY_ATTRIBUTE
    #define MOZ_EXPORT       __attribute__((visibility("default")))
#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
    #define MOZ_EXPORT      __global
#else
    #define MOZ_EXPORT /* nothing */
#endif

#define EMULATOR_DISPLAY_PRIMARY 1

#ifndef MOZ_ASSERT
#ifdef DEBUG
#  define MOZ_ASSERT(e...) __android_log_assert(e, "TAG", #e);
#else
#  define MOZ_ASSERT(...) do { } while (0)
#endif /* DEBUG */
#endif

#endif /* MOZTYPES_H */
