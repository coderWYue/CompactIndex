/*
 * Atomic operation library using gcc's features.
 */
#ifndef __TDMS_ATOMIC_H__
#define __TDMS_ATOMIC_H__

#if !defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1) || !defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_2) \
    || !defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4) || !defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_8)
# error "the gcc version has no atomic support!"
#else

#define TDMS_ATOMIC_DECLARE(type, name) \
    type name ## _atomic_

#define TDMS_ATOMIC_EXTERN(type, name) \
    extern type name ## _atomic_

#define TDMS_ATOMIC_INIT(name) \
    (name ## _atomic_) = 0

#define TDMS_ATOMIC_RESET(name) \
    (name ## _atomic_) = 0

#define TDMS_ATOMIC_DECL_AND_INIT(type, name) \
    type (name ## _atomic_) = 0

#define TDMS_ATOMIC_ADD(name, value) \
    __sync_add_and_fetch(&(name ## _atomic_), value) 

#define TDMS_ATOMIC_SUB(name, value) \
    __sync_sub_and_fetch(&(name ## _atomic_), value) 

#define TDMS_ATOMIC_AND(name, value) \
    __sync_fetch_and_and(&(name ## _atomic_), value)

#define TDMS_ATOMIC_OR(name, value) \
    __sync_fetch_and_or(&(name ## _atomic_), value)

#define TDMS_ATOMIC_XOR(name, value) \
    __sync_fetch_and_xor(&(name ## _atomic_), value)

#define TDMS_ATOMIC_NAND(name, value) \
    __sync_fetch_and_nand(&(name ## _atomic_), value)

#define TDMS_ATOMIC_CAS(name, cmpvalue, newvalue) \
    __sync_bool_compare_and_swap(&(name ## _atomic_), cmpvalue, newvalue) 

#define TDMS_ATOMIC_GET(name) \
    (name ## _atomic_)

#define TDMS_ATOMIC_SET(name, value) ({ \
    while (TDMS_ATOMIC_CAS(name, TDMS_ATOMIC_GET(name), value) == 0) \
        ; })

#endif

#endif
