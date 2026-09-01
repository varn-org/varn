/* SPDX-License-Identifier: MIT */
/* Minimal POSIX-ish typedefs for lua-ffi (ffi.c) when building with MSVC. */
#pragma once

#if defined(_MSC_VER)

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifndef _GID_T_DEFINED
typedef int gid_t;
#define _GID_T_DEFINED
#endif
#ifndef _UID_T_DEFINED
typedef int uid_t;
#define _UID_T_DEFINED
#endif
#ifndef _PID_T_DEFINED
typedef int pid_t;
#define _PID_T_DEFINED
#endif
#ifndef _MODE_T_DEFINED
typedef unsigned int mode_t;
#define _MODE_T_DEFINED
#endif
#ifndef _NLINK_T_DEFINED
typedef unsigned short nlink_t;
#define _NLINK_T_DEFINED
#endif
#ifndef _SSIZE_T_DEFINED
#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef long ssize_t;
#endif
#define _SSIZE_T_DEFINED
#endif
#ifndef _USECONDS_T_DEFINED
typedef unsigned long useconds_t;
#define _USECONDS_T_DEFINED
#endif
#ifndef _SUSECONDS_T_DEFINED
typedef long suseconds_t;
#define _SUSECONDS_T_DEFINED
#endif
#ifndef _BLKSIZE_T_DEFINED
typedef long blksize_t;
#define _BLKSIZE_T_DEFINED
#endif
#ifndef _BLKCNT_T_DEFINED
typedef __int64 blkcnt_t;
#define _BLKCNT_T_DEFINED
#endif
#ifndef _OFF_T_DEFINED
typedef __int64 off_t;
#define _OFF_T_DEFINED
#endif
#ifndef _INO_T_DEFINED
typedef unsigned short ino_t;
#define _INO_T_DEFINED
#endif
#ifndef _DEV_T_DEFINED
typedef unsigned int dev_t;
#define _DEV_T_DEFINED
#endif

#endif /* _MSC_VER */
