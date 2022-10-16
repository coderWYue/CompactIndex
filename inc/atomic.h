/*
 * Atomic operation library using gcc's features.
 */
#ifndef ___ATOMIC_H__
#define ___ATOMIC_H__

#if !defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1) || !defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_2) \
    || !defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4) || !defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_8)
# error "the gcc version has no atomic support!"
#else


#define ATOMIC_ADD(name, value) \
    __sync_add_and_fetch(&(name), value) 

#define ATOMIC_SUB(name, value) \
    __sync_sub_and_fetch(&(name), value) 

#define ATOMIC_AND(name, value) \
    __sync_fetch_and_and(&(name), value)

#define ATOMIC_OR(name, value) \
    __sync_fetch_and_or(&(name), value)

#define ATOMIC_XOR(name, value) \
    __sync_fetch_and_xor(&(name), value)

#define ATOMIC_NAND(name, value) \
    __sync_fetch_and_nand(&(name), value)

#define ATOMIC_CAS(name, cmpvalue, newvalue) \
    __sync_bool_compare_and_swap(&(name), cmpvalue, newvalue) 

#define ATOMIC_GET(name) \
    (name)

#define ATOMIC_SET(name, value) ({ \
    while (ATOMIC_CAS(name, ATOMIC_GET(name), value) == 0) \
        ; })

#endif

#endif
