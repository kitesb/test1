#ifndef _GENERAL_H
#define _GENERAL_H

#include <stdint.h>


// 符号可见性控制: 对外的接口函数需要携带RES_API
// 编译时携带  gcc -fvisibility=hidden
#ifdef _MSC_VER
#define RES_API
#else
#define RES_API __attribute__((visibility("default")))
#endif




#ifndef  PTR_NULL
#define PTR_NULL    NULL
#endif



#ifndef  uint8_t
#define uint8_t     unsigned char
#define uint16_t    unsigned short int
#define uint32_t    unsigned int
#define uint64_t    unsigned long long

#define int8_t      char
#define int16_t     short int
#define int32_t     int
#define int64_t     long long
#endif




#endif

