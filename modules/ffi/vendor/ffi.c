/* SPDX-License-Identifier: MIT */
/*
 * Author: Jianhui Zhao <zhaojh329@gmail.com>
 */

#include <lauxlib.h>
#include <lualib.h>

#if defined(_MSC_VER)
#include "varn_msvc_ffi_compat.h"
#else
#include <sys/types.h>
#endif

#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <alloca.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <math.h>
#include <ffi.h>

#include "helper.h"
#include "config.h"
#include "token.h"
#include "lex.h"
#include "varn_ffi_host.h"

/* MSVC: arithmetic on void * is a GNU extension (error C2036). */
#define VARN_PTR_OFFSET(p, off) ((void *)((unsigned char *)(p) + (off)))

#define MAX_RECORD_FIELDS   30
#define MAX_FUNC_ARGS       30

/* A struct of bitfields is described to libffi in chunks of at most two eightbytes, the most any platform passes in registers. */
#define MAX_BITFIELD_ELEMENTS 16

#define CDATA_MT    "cdata"
#define CTYPE_MT    "ctype"
#define CLIB_MT     "clib"

enum {
    CTYPE_BOOL,

    CTYPE_CHAR,
    CTYPE_UCHAR,

    CTYPE_SHORT,
    CTYPE_USHORT,

    CTYPE_INT,
    CTYPE_UINT,

    CTYPE_LONG,
    CTYPE_ULONG,

    CTYPE_LONGLONG,
    CTYPE_ULONGLONG,

    CTYPE_INT8_T,
    CTYPE_INT16_T,
    CTYPE_INT32_T,
    CTYPE_INT64_T,
    CTYPE_UINT8_T,
    CTYPE_UINT16_T,
    CTYPE_UINT32_T,
    CTYPE_UINT64_T,

    CTYPE_INO_T,
    CTYPE_DEV_T,
    CTYPE_GID_T,
    CTYPE_MODE_T,
    CTYPE_NLINK_T,
    CTYPE_UID_T,
    CTYPE_OFF_T,
    CTYPE_PID_T,
    CTYPE_SIZE_T,
    CTYPE_SSIZE_T,
    CTYPE_USECONDS_T,
    CTYPE_SUSECONDS_T,
    CTYPE_BLKSIZE_T,
    CTYPE_BLKCNT_T,
    CTYPE_TIME_T,
    CTYPE_INTPTR_T,
    CTYPE_UINTPTR_T,

    CTYPE_FLOAT,
    CTYPE_DOUBLE,

    CTYPE_VOID,
    CTYPE_RECORD,
    CTYPE_ARRAY,
    CTYPE_PTR,
    CTYPE_FUNC,
};

enum {
    METATYPE_FLAG_INDEX    = 1 << 0,
    METATYPE_FLAG_TOSTRING = 1 << 1
};

struct crecord;
struct carray;
struct cfunc;

struct ctype {
    uint8_t type;
    uint8_t is_const:1;
    union {
        struct carray *array;
        struct crecord *rc;
        struct cfunc *func;
        struct ctype *ptr;
        ffi_type *ft;
    };
};

struct carray {
    size_t size;
    ffi_type ft;
    struct ctype *ct;
};

struct crecord_field {
    struct ctype *ct;
    size_t offset;
    uint8_t bit_offset;
    uint8_t bit_width;
    char name[0];
};

struct crecord {
    ffi_type ft;
    int mt_ref;
    uint8_t mflags;
    uint8_t nfield:5;
    uint8_t is_union:1;
    uint8_t anonymous:1;
    uint8_t packed:1;
    struct crecord_field *fields[0];
};

struct cfunc {
    uint8_t va:1;
    uint8_t narg:5;
    struct ctype *rtype;
    struct ctype *args[0];
};

/* What the binding keeps for a Lua state, which a callback reaches from any thread without touching the state. */
struct cstate {
    lua_State *main;
    lua_State *calling;
    const struct varn_ffi_host *host;
    int ncall;
    int failure_ref;
};

struct ccallback {
    struct cstate *state;
    struct cfunc *func;
    char *name;
    ffi_cif cif;
    ffi_type *args[MAX_FUNC_ARGS];
    ffi_closure *closure;
    void *code;
    int fn_ref;
};

static bool ctype_equal(const struct ctype *ct1, const struct ctype *ct2);

struct cdata {
    struct ctype *ct;
    int gc_ref;
    void *ptr;
    struct ccallback *cb;
};

struct clib {
    void *h;
};

static const char *crecord_registry;
static const char *carray_registry;
static const char *cfunc_registry;
static const char *ctype_registry;
static const char *ctdef_registry;
static const char *clib_registry;
static const char *cstate_registry;
static const char *cenum_registry;
static const char *cconst_registry;
static const char *cmem_registry;
static const char *cfunctype_registry;

#if LUA_VERSION_NUM < 503

/* LUA_TINT is defined in openwrt */
#ifndef LUA_TINT
static int lua_isinteger(lua_State *L, int idx)
{
    double number;

    if (!lua_isnumber(L, idx))
        return 0;

    number = lua_tonumber(L, idx);

    return floor(number) == number;
}
#endif

#if LUA_VERSION_NUM < 502
static void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup)
{
    luaL_checkstack(L, nup, "too many upvalues");
    for (; l->name != NULL; l++) {  /* fill the table with given functions */
        int i;
        for (i = 0; i < nup; i++)  /* copy upvalues to the top */
            lua_pushvalue(L, -nup);
        lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
        lua_setfield(L, -(nup + 2), l->name);
    }
    lua_pop(L, nup);  /* remove upvalues */
}

#define luaL_newlibtable(L, l) lua_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)
#define luaL_newlib(L, l) (luaL_newlibtable(L, l), luaL_setfuncs(L, l, 0))

#define ispseudo(i) ((i) <= LUA_REGISTRYINDEX)

static int lua_absindex(lua_State *L, int idx)
{
    return (idx > 0 || ispseudo(idx)) ? idx : lua_gettop(L) + idx + 1;
}

static int lua_rawgetp(lua_State *L, int idx, const void *p)
{
    lua_pushlightuserdata(L, (void *)p);
    lua_rawget(L, idx);
    return lua_type(L, -1);
}

static void lua_rawsetp(lua_State *L, int idx, const void *p)
{
    idx = lua_absindex(L, idx);
    lua_pushlightuserdata(L, (void *)p);
    lua_pushvalue(L, -2);
    lua_rawset(L, idx);
    lua_pop(L, 1);
}

static void *luaL_testudata (lua_State *L, int ud, const char *tname)
{
    void *p = lua_touserdata(L, ud);
    if (p != NULL) {  /* value is a userdata? */
        if (lua_getmetatable(L, ud)) {  /* does it have a metatable? */
            luaL_getmetatable(L, tname);  /* get correct metatable */
            if (!lua_rawequal(L, -1, -2))  /* not the same? */
                p = NULL;  /* value is a userdata with wrong metatable */
            lua_pop(L, 2);  /* remove both metatables */
            return p;
        }
    }
    return NULL;  /* value is not a userdata with a metatable */
}
#endif
#endif

#if LUA_VERSION_NUM > 501
#ifndef lua_equal
#define lua_equal(L,idx1,idx2) lua_compare(L,(idx1),(idx2),LUA_OPEQ)
#endif
#endif

/* Allocates zeroed memory the Lua state owns, so a type, a record, a field or a name the parser keeps lives as long as the state and is released when it closes. */
static void *cmem_alloc(lua_State *L, size_t size)
{
    void *p;

    lua_rawgetp(L, LUA_REGISTRYINDEX, &cmem_registry);
    p = lua_newuserdata(L, size);
    memset(p, 0, size);
    lua_rawsetp(L, -2, p);
    lua_pop(L, 1);

    return p;
}

/* Copies a name into memory the Lua state owns, which `cmem_release` gives back once the parser no longer needs it. */
static char *cmem_strdup(lua_State *L, const char *text)
{
    size_t len = strlen(text);
    char *copy = (char *)cmem_alloc(L, len + 1);

    memcpy(copy, text, len);

    return copy;
}

/* Lets the state collect memory from `cmem_alloc` before it closes. */
static void cmem_release(lua_State *L, void *p)
{
    lua_rawgetp(L, LUA_REGISTRYINDEX, &cmem_registry);
    lua_pushnil(L);
    lua_rawsetp(L, -2, p);
    lua_pop(L, 1);
}

static int lua_type_error(lua_State *L, int narg, char const *tname)
{
    lua_pushfstring(L, "%s expected, got %s", tname, luaL_typename(L, narg));
    luaL_argcheck(L, false, narg, lua_tostring(L, -1));
    return 0;
}

static ffi_type *ffi_type_of(size_t size, bool s)
{
    switch (size) {
    case 8:
        return s ? &ffi_type_sint64 : &ffi_type_uint64;
    case 4:
        return s ? &ffi_type_sint32 : &ffi_type_uint32;
    case 2:
        return s ? &ffi_type_sint16 : &ffi_type_uint16;
    default:
        return s ? &ffi_type_sint8 : &ffi_type_uint8;
    }
}

static const char *ctype_name(struct ctype *ct)
{
    switch (ct->type) {
    case CTYPE_BOOL:
        return "bool";

    case CTYPE_CHAR:
        return "char";
    case CTYPE_SHORT:
        return "short";
    case CTYPE_INT:
        return "int";
    case CTYPE_LONG:
        return "long";
    case CTYPE_LONGLONG:
        return "long long";

    case CTYPE_UCHAR:
        return "unsigned char";
    case CTYPE_USHORT:
        return "unsigned short";
    case CTYPE_UINT:
        return "unsigned int";
    case CTYPE_ULONG:
        return "unsigned long";
    case CTYPE_ULONGLONG:
        return "unsigned long long";

    case CTYPE_FLOAT:
        return "float";
    case CTYPE_DOUBLE:
        return "double";

    case CTYPE_INT8_T:
        return "int8_t";
    case CTYPE_INT16_T:
        return "int16_t";
    case CTYPE_INT32_T:
        return "int32_t";
    case CTYPE_INT64_T:
        return "int64_t";
    case CTYPE_UINT8_T:
        return "uint8_t";
    case CTYPE_UINT16_T:
        return "uint16_t";
    case CTYPE_UINT32_T:
        return "uint32_t";
    case CTYPE_UINT64_T:
        return "uint64_t";

    case CTYPE_INO_T:
        return "ino_t";
    case CTYPE_DEV_T:
        return "dev_t";
    case CTYPE_GID_T:
        return "gid_t";
    case CTYPE_MODE_T:
        return "mode_t";
    case CTYPE_NLINK_T:
        return "nlink_t";
    case CTYPE_UID_T:
        return "uid_t";
    case CTYPE_OFF_T:
        return "off_t";
    case CTYPE_PID_T:
        return "pid_t";
    case CTYPE_SIZE_T:
        return "size_t";
    case CTYPE_SSIZE_T:
        return "ssize_t";
    case CTYPE_USECONDS_T:
        return "useconds_t";
    case CTYPE_SUSECONDS_T:
        return "suseconds_t";
    case CTYPE_BLKSIZE_T:
        return "blksize_t";
    case CTYPE_BLKCNT_T:
        return "blkcnt_t";
    case CTYPE_TIME_T:
        return "time_t";
    case CTYPE_INTPTR_T:
        return "intptr_t";
    case CTYPE_UINTPTR_T:
        return "uintptr_t";

    case CTYPE_VOID:
        return "void";
    case CTYPE_RECORD:
        return ct->rc->is_union ? "union" : "struct";
    case CTYPE_ARRAY:
        return "array";
    case CTYPE_PTR:
        return "pointer";
    case CTYPE_FUNC:
        return "func";

    default:
        return "unknown";
    }
}

static ffi_type *ctype_ft(struct ctype *ct)
{
    switch (ct->type) {
    case CTYPE_ARRAY:
        return &ct->array->ft;
    case CTYPE_RECORD:
        return &ct->rc->ft;
    case CTYPE_PTR:
    case CTYPE_FUNC:
        return &ffi_type_pointer;
    default:
        return ct->ft;
    }
}

static inline size_t ctype_sizeof(struct ctype *ct)
{
    return ctype_ft(ct)->size;
}

static inline int cdata_type(struct cdata *cd)
{
    return cd->ct->type;
}

static inline void *cdata_ptr(struct cdata *cd)
{
    return cd->ptr ? cd->ptr : cd + 1;
}

static void *cdata_ptr_ptr(struct cdata *cd)
{
    int type = cdata_type(cd);

    if (type != CTYPE_PTR && type != CTYPE_FUNC)
        return NULL;

    return *(void **)cdata_ptr(cd);
}

static inline bool ctype_ptr_to(struct ctype *ct, int type)
{
    return ct->type != CTYPE_PTR ? false : ct->ptr->type == type;
}

/* Answers whether a pointer to `from` converts to a pointer to `to` without a cast, which may add a const to what it points at but never drops one. */
static bool ctype_ptr_convertible(const struct ctype *to, const struct ctype *from)
{
    struct ctype unqualified = *to;

    /* A pointer to void takes and gives any pointer, as it does in C, whatever either one qualifies. */
    if (to->type == CTYPE_VOID || from->type == CTYPE_VOID)
        return true;

    if (from->is_const && !to->is_const)
        return false;

    unqualified.is_const = from->is_const;

    return ctype_equal(&unqualified, from);
}

static bool ctype_is_int(struct ctype *ct)
{
    return ct->type < CTYPE_FLOAT;
}

static bool ctype_is_num(struct ctype *ct)
{
    return ct->type < CTYPE_VOID;
}

/* An integer narrower than a register is returned by libffi widened to a whole ffi_arg, so its storage holds one. */
static bool ctype_is_widened(struct ctype *ct)
{
    return ctype_is_int(ct) && ctype_sizeof(ct) < sizeof(ffi_arg);
}

/* Answers where the value of a widened return lies inside its ffi_arg, which is its last bytes on a big-endian machine. */
static void *widened_value(struct ctype *ct, void *ret)
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    if (ctype_is_widened(ct))
        return (char *)ret + sizeof(ffi_arg) - ctype_sizeof(ct);
#else
    (void)ct;
#endif
    return ret;
}

/* Stores a narrow integer a callback answers as the whole ffi_arg libffi reads it from, extended by its sign. */
static void widen_return(struct ctype *ct, void *ret, const void *value)
{
    switch (ct->ft->type) {
    case FFI_TYPE_SINT8:
        *(ffi_sarg *)ret = *(const int8_t *)value;
        break;
    case FFI_TYPE_UINT8:
        *(ffi_arg *)ret = *(const uint8_t *)value;
        break;
    case FFI_TYPE_SINT16:
        *(ffi_sarg *)ret = *(const int16_t *)value;
        break;
    case FFI_TYPE_UINT16:
        *(ffi_arg *)ret = *(const uint16_t *)value;
        break;
    case FFI_TYPE_SINT32:
        *(ffi_sarg *)ret = *(const int32_t *)value;
        break;
    case FFI_TYPE_UINT32:
        *(ffi_arg *)ret = *(const uint32_t *)value;
        break;
    default:
        memcpy(ret, value, ctype_sizeof(ct));
        break;
    }
}

static void cdata_ptr_set(struct cdata *cd, void *ptr)
{
    int type = cdata_type(cd);

    if (type != CTYPE_PTR && type != CTYPE_FUNC)
        return;

    *(void **)cdata_ptr(cd) = ptr;
}

static struct ctype *ctype_new(lua_State *L, bool keep)
{
    struct ctype *ct = (struct ctype *)lua_newuserdata(L, sizeof(struct ctype));

    ct->type = CTYPE_VOID;
    ct->ft = &ffi_type_void;
    ct->is_const = false;

    luaL_getmetatable(L, CTYPE_MT);
    lua_setmetatable(L, -2);

    lua_rawgetp(L, LUA_REGISTRYINDEX, &ctype_registry);
    lua_pushvalue(L, -2);
    lua_rawsetp(L, -2, ct);

    if (keep)
        lua_pop(L, 1);
    else
        lua_pop(L, 2);

    return ct;
}

static bool cfunc_equal(const struct cfunc *f1, const struct cfunc *f2)
{
    int i;

    if (f1->va != f2->va || f1->narg != f2->narg)
        return false;

    if (!ctype_equal(f1->rtype, f2->rtype))
        return false;

    for (i = 0; i < f1->narg; i++)
        if (!ctype_equal(f1->args[i], f2->args[i]))
            return false;

    return true;
}

static bool ctype_equal(const struct ctype *ct1, const struct ctype *ct2)
{
    if (ct1->type != ct2->type)
        return false;

    if (ct1->is_const != ct2->is_const)
        return false;

    switch (ct1->type) {
    case CTYPE_RECORD:
        return ct1->rc == ct2->rc;
    case CTYPE_ARRAY:
        if (ct1->array->size != ct2->array->size)
            return false;
        return ctype_equal(ct1->array->ct, ct2->array->ct);
    case CTYPE_PTR:
        return ctype_equal(ct1->ptr, ct2->ptr);
    case CTYPE_FUNC:
        return cfunc_equal(ct1->func, ct2->func);
    default:
        break;
    }

    return true;
}

static struct ctype *ctype_lookup(lua_State *L, struct ctype *match, bool keep)
{
    struct ctype *ct;

    lua_rawgetp(L, LUA_REGISTRYINDEX, &ctype_registry);

    lua_pushnil(L);

    while (lua_next(L, -2) != 0) {
        ct = (struct ctype *)lua_touserdata(L, -1);
        if (ctype_equal(match, ct)) {
            if (keep) {
                lua_replace(L, -3);
                lua_pop(L, 1);
            } else {
                lua_pop(L, 3);
            }
            return ct;
        }
        lua_pop(L, 1);
    }

    lua_pop(L, 1);

    ct = ctype_new(L, keep);
    *ct = *match;

    return ct;
}

static struct carray *carray_lookup(lua_State *L, size_t size, struct ctype *ct)
{
    struct carray *a;

    lua_rawgetp(L, LUA_REGISTRYINDEX, &carray_registry);

    lua_pushnil(L);

    while (lua_next(L, -2) != 0) {
        a = (struct carray *)lua_touserdata(L, -1);
        if (a->size == size && ctype_equal(a->ct, ct)) {
            lua_pop(L, 3);
            return a;
        }
        lua_pop(L, 1);
    }

    a = (struct carray *)lua_newuserdata(L, sizeof(struct carray));
    if (!a)
        luaL_error(L, "no mem");

    memset(a, 0, sizeof(struct carray));

    lua_rawsetp(L, -2, a);
    lua_pop(L, 1);

    if (size) {
        a->ft.type = FFI_TYPE_STRUCT;
        a->ft.alignment = ctype_ft(ct)->alignment;
        a->ft.size = ctype_sizeof(ct) * size;
    }

    a->size = size;
    a->ct = ctype_lookup(L, ct, false);

    return a;
}

static const char *cstruct_lookup_name(lua_State *L, struct crecord *st)
{
    lua_rawgetp(L, LUA_REGISTRYINDEX, &crecord_registry);

    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        if (lua_topointer(L, -1) == st) {
            lua_pop(L, 1);
            lua_remove(L, -2);
            return lua_tostring(L, -1);
        }
        lua_pop(L, 1);
    }
    return NULL;
}

static void ctype_tostring(lua_State *L, struct ctype *ct, luaL_Buffer *b, bool *first_ptr);

/* Writes a function type the way C spells it, with `declarator` such as `(*)` between its return type and its parameters. */
static void cfunc_tostring(lua_State *L, struct cfunc *func, const char *declarator, luaL_Buffer *b)
{
    bool first_ptr = true;
    int i;

    ctype_tostring(L, func->rtype, b, &first_ptr);
    luaL_addchar(b, ' ');
    if (declarator)
        luaL_addstring(b, declarator);
    luaL_addchar(b, '(');

    for (i = 0; i < func->narg; i++) {
        first_ptr = true;
        if (i > 0)
            luaL_addstring(b, ", ");
        ctype_tostring(L, func->args[i], b, &first_ptr);
    }

    if (func->va)
        luaL_addstring(b, func->narg > 0 ? ", ..." : "...");

    luaL_addchar(b, ')');
}

static void ctype_tostring(lua_State *L, struct ctype *ct, luaL_Buffer *b, bool *first_ptr)
{
    char buf[128];

    if (ct->type != CTYPE_PTR && ct->is_const)
        luaL_addstring(b, "const ");

    switch (ct->type) {
    case CTYPE_PTR:
        if (ct->ptr->type == CTYPE_FUNC) {
            cfunc_tostring(L, ct->ptr->func, ct->is_const ? "(*const)" : "(*)", b);
            break;
        }

        ctype_tostring(L, ct->ptr, b, first_ptr);
        if (*first_ptr) {
            luaL_addchar(b, ' ');
            *first_ptr = false;
        }
        luaL_addchar(b, '*');
        if (ct->is_const)
            luaL_addstring(b, " const");
        break;
    case CTYPE_ARRAY:
        ctype_tostring(L, ct->array->ct, b, first_ptr);
        snprintf(buf, sizeof(buf), "%zd", ct->array->size);
        luaL_addchar(b, '[');
        luaL_addstring(b, buf);
        luaL_addchar(b, ']');
        break;
    case CTYPE_FUNC:
        cfunc_tostring(L, ct->func, NULL, b);
        break;
    default:
        luaL_addstring(b, ctype_name(ct));
        if (ct->type == CTYPE_RECORD && !ct->rc->anonymous) {
            luaL_addchar(b, ' ');
            luaL_addstring(b, cstruct_lookup_name(L, ct->rc));
            lua_pop(L, 1);
        }
        break;
    }
}

static struct cdata *cdata_new(lua_State *L, struct ctype *ct, void *ptr)
{
    struct cdata *cd = (struct cdata *)lua_newuserdata(L, sizeof(struct cdata) + (ptr ? 0 : ctype_sizeof(ct)));

    cd->gc_ref = LUA_REFNIL;
    cd->ptr = ptr;
    cd->ct = ct;
    cd->cb = NULL;

    luaL_getmetatable(L, CDATA_MT);
    lua_setmetatable(L, -2);

    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, cd);

    if (!ptr)
        memset(cdata_ptr(cd), 0, ctype_sizeof(ct));

    return cd;
}

static int __cdata_tostring(lua_State *L, struct cdata *cd)
{
    void *ptr = cdata_type(cd) == CTYPE_PTR ? cdata_ptr_ptr(cd) : cdata_ptr(cd);
    bool first_ptr = true;
    luaL_Buffer b;
    char buf[128];

    luaL_buffinit(L, &b);
    luaL_addstring(&b, "cdata<");
    ctype_tostring(L, cd->ct, &b, &first_ptr);
    snprintf(buf, sizeof(buf), ">: %p", ptr);
    luaL_addstring(&b, buf);
    luaL_pushresult(&b);

    return 1;
}

static int cdata_tostring(lua_State *L)
{
    struct cdata *cd = (struct cdata *)luaL_checkudata(L, 1, CDATA_MT);
    struct ctype *ct = cd->ct;

    if (ct->type == CTYPE_RECORD && ct->rc->mflags & METATYPE_FLAG_TOSTRING) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ct->rc->mt_ref);
        lua_getfield(L, -1, "__tostring");
        lua_pushvalue(L, 1);
        lua_call(L, 1, 1);
        return 1;
    }

    return __cdata_tostring(L, cd);
}

static int __ctype_tostring(lua_State *L, struct ctype *ct)
{
    bool first_ptr = true;
    luaL_Buffer b;

    luaL_buffinit(L, &b);
    ctype_tostring(L, ct, &b, &first_ptr);
    luaL_pushresult(&b);

    return 1;
}

#define PUSH_INTEGER(L, type, ptr) \
    do { \
        type v; \
        memcpy(&v, ptr, sizeof(v)); \
        lua_pushinteger(L, v); \
    } while (0)

#define PUSH_NUMBER(L, type, ptr) \
    do { \
        type v; \
        memcpy(&v, ptr, sizeof(v)); \
        lua_pushnumber(L, v); \
    } while (0)

static int cdata_to_lua(lua_State *L, struct ctype *ct, void *ptr)
{
    switch (ct->type) {
    case CTYPE_RECORD:
    case CTYPE_ARRAY:
    case CTYPE_PTR:
        cdata_new(L, ct, ptr);
        return 1;
    }

    switch (ct->ft->type) {
    case FFI_TYPE_SINT8:
        PUSH_INTEGER(L, int8_t, ptr);
        break;
    case FFI_TYPE_UINT8:
        PUSH_INTEGER(L, uint8_t, ptr);
        break;
    case FFI_TYPE_SINT16:
        PUSH_INTEGER(L, int16_t, ptr);
        break;
    case FFI_TYPE_UINT16:
        PUSH_INTEGER(L, uint16_t, ptr);
        break;
    case FFI_TYPE_SINT32:
        PUSH_INTEGER(L, int32_t, ptr);
        break;
    case FFI_TYPE_UINT32:
        PUSH_INTEGER(L, uint32_t, ptr);
        break;
    case FFI_TYPE_SINT64:
        PUSH_INTEGER(L, int64_t, ptr);
        break;
    case FFI_TYPE_UINT64:
        PUSH_INTEGER(L, uint64_t, ptr);
        break;
    case FFI_TYPE_FLOAT:
        PUSH_NUMBER(L, float, ptr);
        break;
    case FFI_TYPE_DOUBLE:
        PUSH_NUMBER(L, double, ptr);
        break;
    default:
        return 0;
    }

    return 1;
}

static int cdata_from_lua(lua_State *L, struct ctype *ct, void *ptr, int idx, bool cast);

static lua_Integer from_lua_num_int(lua_State *L, int idx)
{
    if (lua_isinteger(L, idx))
        return lua_tointeger(L, idx);
    else if (lua_isboolean(L, idx))
        return lua_toboolean(L, idx);
    else
        return luaL_checknumber(L, idx);
}

static lua_Number from_lua_num_num(lua_State *L, int idx)
{
    if (lua_isboolean(L, idx))
        return lua_toboolean(L, idx);
    else
        return luaL_checknumber(L, idx);
}

static void ft_from_lua_num(lua_State *L, ffi_type *ft, void *ptr, int idx)
{
    switch (ft->type) {
    case FFI_TYPE_SINT8:
        *(int8_t *)ptr = from_lua_num_int(L, idx);
        break;
    case FFI_TYPE_UINT8:
        *(uint8_t *)ptr = from_lua_num_int(L, idx);
        break;
    case FFI_TYPE_SINT16:
        *(int16_t *)ptr = from_lua_num_int(L, idx);
        break;
    case FFI_TYPE_UINT16:
        *(uint16_t *)ptr = from_lua_num_int(L, idx);
        break;
    case FFI_TYPE_SINT32:
        *(int32_t *)ptr = from_lua_num_int(L, idx);
        break;
    case FFI_TYPE_UINT32:
        *(uint32_t *)ptr = from_lua_num_int(L, idx);
        break;
    case FFI_TYPE_SINT64:
        *(int64_t *)ptr = from_lua_num_int(L, idx);
        break;
    case FFI_TYPE_UINT64:
        *(uint64_t *)ptr = from_lua_num_int(L, idx);
        break;
    case FFI_TYPE_FLOAT:
        *(float *)ptr = from_lua_num_num(L, idx);
        break;
    case FFI_TYPE_DOUBLE:
        *(double *)ptr = from_lua_num_num(L, idx);
        break;
    }
}

static bool cdata_from_lua_num(lua_State *L, struct ctype *ct, void *ptr, int idx, bool cast)
{
    if (ct->type == CTYPE_PTR) {
        if (!cast)
            return false;
        *(void **)ptr = (void *)(intptr_t)lua_tointeger(L, idx);
        return true;
    }

    if (!ctype_is_num(ct))
        return false;

    ft_from_lua_num(L, ct->ft, ptr, idx);

    if (ct->type == CTYPE_BOOL)
        *(int8_t *)ptr = !!*(int8_t *)ptr;

    return true;
}

static bool cdata_from_lua_cdata_ptr(lua_State *L, struct ctype *ct, void *ptr,
                struct ctype *from_ct, void *from_ptr, bool cast)
{
    if (ct->type == CTYPE_PTR && (cast || ctype_ptr_convertible(ct->ptr, from_ct))) {
        *(void **)ptr = from_ptr;
        return true;
    }

    if (cast && ctype_is_int(ct)) {
        lua_pushinteger(L, (intptr_t)from_ptr);
        cdata_from_lua_num(L, ct, ptr, -1, true);
        lua_pop(L, 1);
        return true;
    }

    return false;
}

static bool cdata_from_lua_cdata(lua_State *L, struct ctype *ct, void *ptr, int idx, bool cast)
{
    struct cdata *cd = (struct cdata *)lua_touserdata(L, idx);

    switch (cdata_type(cd)) {
    case CTYPE_ARRAY:
        return cdata_from_lua_cdata_ptr(L, ct, ptr, cd->ct->array->ct, cdata_ptr(cd), cast);
    case CTYPE_PTR:
        return cdata_from_lua_cdata_ptr(L, ct, ptr, cd->ct->ptr, cdata_ptr_ptr(cd), cast);
    case CTYPE_FUNC:
        if (ct->type == CTYPE_PTR && ct->ptr->type == CTYPE_FUNC
                && (cast || ctype_equal(ct->ptr, cd->ct))) {
            *(void **)ptr = cdata_ptr_ptr(cd);
            return true;
        }
        break;
    case CTYPE_RECORD:
        if (ct->type == CTYPE_PTR && (cast || ctype_ptr_convertible(ct->ptr, cd->ct))) {
            *(void **)ptr = cdata_ptr(cd);
            return true;
        }

        if (ctype_equal(cd->ct, ct)) {
            memcpy(ptr, cdata_ptr(cd), ctype_sizeof(ct));
            return true;
        }
        break;
    default:
        if (ctype_is_num(cd->ct)) {
            cdata_to_lua(L, cd->ct, cdata_ptr(cd));
            cdata_from_lua_num(L, ct, ptr, -1, cast);
            lua_pop(L, 1);
            return true;
        }
        break;
    }

    return false;
}

static bool cdata_from_lua_cb_ret(lua_State *L, struct ctype *ct, void *ptr, int idx)
{
    struct cdata *cd;

    switch (lua_type(L, idx)) {
    case LUA_TNIL:
        if (ct->type == CTYPE_PTR) {
            *(void **)ptr = NULL;
            return true;
        }
        break;
    case LUA_TNUMBER:
    case LUA_TBOOLEAN:
        if (ctype_is_num(ct)) {
            ft_from_lua_num(L, ct->ft, ptr, idx);
            if (ct->type == CTYPE_BOOL)
                *(int8_t *)ptr = !!*(int8_t *)ptr;
            return true;
        }
        break;
    case LUA_TSTRING:
        if ((ctype_ptr_to(ct, CTYPE_CHAR) || ctype_ptr_to(ct, CTYPE_VOID))
            && ct->type == CTYPE_PTR && ct->ptr->is_const) {
            *(const char **)ptr = (const char *)lua_tostring(L, idx);
            return true;
        }
        break;
    case LUA_TLIGHTUSERDATA:
        if (ct->type == CTYPE_PTR) {
            *(void **)ptr = lua_touserdata(L, idx);
            return true;
        }
        break;
    case LUA_TUSERDATA:
        cd = (struct cdata *)luaL_testudata(L, idx, CDATA_MT);
        if (!cd)
            break;

        switch (cdata_type(cd)) {
        case CTYPE_ARRAY:
            if (ct->type == CTYPE_PTR && ctype_ptr_convertible(ct->ptr, cd->ct->array->ct)) {
                *(void **)ptr = cdata_ptr(cd);
                return true;
            }
            break;
        case CTYPE_PTR:
            if (ct->type == CTYPE_PTR && ctype_ptr_convertible(ct->ptr, cd->ct->ptr)) {
                *(void **)ptr = cdata_ptr_ptr(cd);
                return true;
            }
            break;
        case CTYPE_FUNC:
            if (ct->type == CTYPE_PTR && ct->ptr->type == CTYPE_FUNC
                    && ctype_equal(ct->ptr, cd->ct)) {
                *(void **)ptr = cdata_ptr_ptr(cd);
                return true;
            }
            break;
        case CTYPE_RECORD:
            if (ct->type == CTYPE_PTR && ctype_ptr_convertible(ct->ptr, cd->ct)) {
                *(void **)ptr = cdata_ptr(cd);
                return true;
            }

            if (ctype_equal(cd->ct, ct)) {
                memcpy(ptr, cdata_ptr(cd), ctype_sizeof(ct));
                return true;
            }
            break;
        default:
            if (ctype_is_num(cd->ct) && ctype_is_num(ct)) {
                cdata_to_lua(L, cd->ct, cdata_ptr(cd));
                ft_from_lua_num(L, ct->ft, ptr, -1);
                if (ct->type == CTYPE_BOOL)
                    *(int8_t *)ptr = !!*(int8_t *)ptr;
                lua_pop(L, 1);
                return true;
            }
            break;
        }
        break;
    default:
        break;
    }

    return false;
}

static struct cstate *cstate_get(lua_State *L)
{
    struct cstate *state;

    lua_rawgetp(L, LUA_REGISTRYINDEX, &cstate_registry);
    state = (struct cstate *)lua_touserdata(L, -1);
    lua_pop(L, 1);

    return state;
}

struct ccallback_call {
    struct ccallback *cb;
    void *ret;
    void **args;
};

static int ccallback_return_error(lua_State *L, struct ccallback *cb)
{
    struct cdata *cd = (struct cdata *)luaL_testudata(L, -1, CDATA_MT);

    if (cd)
        __ctype_tostring(L, cd->ct);
    else
        lua_pushstring(L, luaL_typename(L, -1));

    __ctype_tostring(L, cb->func->rtype);

    return luaL_error(L, "The callback \"%s\" returned \"%s\", which does not convert to \"%s\".",
            cb->name, lua_tostring(L, -2), lua_tostring(L, -1));
}

/* Runs the Lua function of a callback and stores its answer where libffi reads the result, and raises when the answer does not convert. */
static int ccallback_run(lua_State *L)
{
    struct ccallback_call *call = (struct ccallback_call *)lua_touserdata(L, 1);
    struct ccallback *cb = call->cb;
    struct cfunc *func = cb->func;
    struct ctype *rtype = func->rtype;
    ffi_arg narrow = 0;
    int i;

    luaL_checkstack(L, func->narg + 1, "The callback has more arguments than the stack holds.");
    lua_rawgeti(L, LUA_REGISTRYINDEX, cb->fn_ref);

    for (i = 0; i < func->narg; i++)
        cdata_to_lua(L, func->args[i], call->args[i]);

    lua_call(L, func->narg, rtype->type == CTYPE_VOID ? 0 : 1);

    if (rtype->type == CTYPE_VOID)
        return 0;

    if (!ctype_is_widened(rtype)) {
        if (!cdata_from_lua_cb_ret(L, rtype, call->ret, -1))
            return ccallback_return_error(L, cb);
        return 0;
    }

    if (!cdata_from_lua_cb_ret(L, rtype, &narrow, -1))
        return ccallback_return_error(L, cb);

    widen_return(rtype, call->ret, &narrow);

    return 0;
}

static int ccallback_report(lua_State *L)
{
    struct cstate *state = (struct cstate *)lua_touserdata(L, 1);

    state->host->fail(L);

    return 0;
}

/* Keeps the failure at the top of the stack for the ffi call running on the state to raise once it returns, or hands it to the failure handler of the application when no call runs. */
static void ccallback_fail(lua_State *L, struct cstate *state)
{
    if (state->ncall > 0) {
        if (state->failure_ref == LUA_NOREF)
            state->failure_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        else
            lua_pop(L, 1);
        return;
    }

    lua_pushcfunction(L, ccallback_report);
    lua_insert(L, -2);
    lua_pushlightuserdata(L, state);
    lua_insert(L, -2);

    /* Nothing may unwind through the native caller, so a handler that cannot even be reached leaves the failure behind. */
    if (lua_pcall(L, 2, 0, 0) != LUA_OK)
        lua_pop(L, 1);
}

/* Raises the error a callback raised while the call ran, as it was raised, and a task that captures it receives the traceback and the frames of the callback. */
static void ccallback_raise_failure(lua_State *L, int failure_ref)
{
    if (failure_ref == LUA_NOREF)
        return;

    lua_rawgeti(L, LUA_REGISTRYINDEX, failure_ref);
    luaL_unref(L, LUA_REGISTRYINDEX, failure_ref);
    cstate_get(L)->host->raise(L);
}

/* Reports a callback called from a thread other than the one that runs Lua to that thread, without touching the Lua state. */
static void ccallback_foreign(struct ccallback *cb)
{
    static const char format[] = "The callback \"%s\" was called from another thread, not the one that runs Lua, so it returned zero without running.";
    size_t len = strlen(cb->name) + sizeof(format);
    char *message = (char *)malloc(len);

    if (!message)
        return;

    snprintf(message, len, format, cb->name);
    cb->state->host->post_failure(cb->state->host, message);
    free(message);
}

static void ccallback_invoke(ffi_cif *cif, void *ret, void **args, void *userdata)
{
    struct ccallback *cb = (struct ccallback *)userdata;
    struct cstate *state = cb->state;
    struct ctype *rtype = cb->func->rtype;
    struct ccallback_call call = { cb, ret, args };
    lua_State *L = state->ncall > 0 ? state->calling : state->main;
    int top;

    /* A callback that fails answers zero, so the result is cleared before anything can fail. */
    if (rtype->type != CTYPE_VOID)
        memset(ret, 0, ctype_is_widened(rtype) ? sizeof(ffi_arg) : ctype_sizeof(rtype));

    if (!state->host->on_lua_thread(state->host)) {
        ccallback_foreign(cb);
        return;
    }

    /* The callback runs on the coroutine of the ffi call that runs, so its frames lead to that call, and outside any call on the main thread, which lives as long as the state. */
    /* A stack that cannot grow leaves the result zero. */
    if (!lua_checkstack(L, 3))
        return;

    top = lua_gettop(L);
    /* The failure keeps its traceback and frames in the shape the failure handler of the application takes. */
    lua_pushcfunction(L, state->host->capture);
    lua_pushcfunction(L, ccallback_run);
    lua_pushlightuserdata(L, &call);

    if (lua_pcall(L, 1, 0, top + 1) != LUA_OK)
        ccallback_fail(L, state);

    lua_settop(L, top);
}

static void ccallback_release(lua_State *L, struct ccallback *cb)
{
    if (!cb)
        return;

    if (cb->fn_ref != LUA_REFNIL)
        luaL_unref(L, LUA_REGISTRYINDEX, cb->fn_ref);

    if (cb->closure)
        ffi_closure_free(cb->closure);

    free(cb->name);
    free(cb);
}

static struct ccallback *ccallback_new(lua_State *L, struct ctype *ct, int idx)
{
    struct cfunc *func = ct->ptr->func;
    struct ccallback *cb;
    int status = 0;
    int i;

    __ctype_tostring(L, ct);

    if (func->va)
        luaL_error(L, "The callback \"%s\" is variadic, which a callback cannot be.", lua_tostring(L, -1));

    cb = (struct ccallback *)calloc(1, sizeof(struct ccallback));
    if (!cb)
        luaL_error(L, "no mem");

    cb->state = cstate_get(L);
    cb->func = func;
    cb->fn_ref = LUA_REFNIL;
    cb->name = strdup(lua_tostring(L, -1));
    lua_pop(L, 1);

    if (!cb->name)
        goto err;

    lua_pushvalue(L, idx);
    cb->fn_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    for (i = 0; i < func->narg; i++)
        cb->args[i] = ctype_ft(func->args[i]);

    status = ffi_prep_cif(&cb->cif, FFI_DEFAULT_ABI, func->narg,
            ctype_ft(func->rtype), cb->args);
    if (status)
        goto err;

    cb->closure = (ffi_closure *)ffi_closure_alloc(sizeof(ffi_closure), &cb->code);
    if (!cb->closure)
        goto err;

    status = ffi_prep_closure_loc(cb->closure, &cb->cif, ccallback_invoke, cb, cb->code);
    if (status)
        goto err;

    return cb;

err:
    ccallback_release(L, cb);
    luaL_error(L, "The callback could not be prepared, since libffi answered %d.", status);
    return NULL;
}

/* Reads the unit of the declared type of a bitfield as the unsigned integer it holds, whatever the byte order. */
static uint64_t cbitfield_read(const void *unit, size_t size)
{
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;

    switch (size) {
    case 1:
        memcpy(&u8, unit, size);
        return u8;
    case 2:
        memcpy(&u16, unit, size);
        return u16;
    case 4:
        memcpy(&u32, unit, size);
        return u32;
    default:
        memcpy(&u64, unit, size);
        return u64;
    }
}

static void cbitfield_write(void *unit, size_t size, uint64_t value)
{
    uint8_t u8 = (uint8_t)value;
    uint16_t u16 = (uint16_t)value;
    uint32_t u32 = (uint32_t)value;

    switch (size) {
    case 1:
        memcpy(unit, &u8, size);
        break;
    case 2:
        memcpy(unit, &u16, size);
        break;
    case 4:
        memcpy(unit, &u32, size);
        break;
    default:
        memcpy(unit, &value, size);
        break;
    }
}

/* Answers where the bits of a bitfield start inside its unit counted from the least significant bit, since a big-endian machine allocates them from the most significant one. */
static unsigned cbitfield_shift(struct crecord_field *field)
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return ctype_sizeof(field->ct) * 8 - field->bit_offset - field->bit_width;
#else
    return field->bit_offset;
#endif
}

static uint64_t cbitfield_mask(struct crecord_field *field)
{
    return field->bit_width == 64 ? UINT64_MAX : ((uint64_t)1 << field->bit_width) - 1;
}

/* Pushes the value of a bitfield, extending its sign when its declared type is signed. */
static void cbitfield_push(lua_State *L, struct crecord_field *field, const void *unit)
{
    uint64_t mask = cbitfield_mask(field);
    uint64_t bits = (cbitfield_read(unit, ctype_sizeof(field->ct)) >> cbitfield_shift(field)) & mask;
    unsigned short type = field->ct->ft->type;
    bool is_signed = field->ct->type != CTYPE_BOOL && (type == FFI_TYPE_SINT8 || type == FFI_TYPE_SINT16
            || type == FFI_TYPE_SINT32 || type == FFI_TYPE_SINT64);

    if (is_signed && (bits >> (field->bit_width - 1)) & 1)
        bits |= ~mask;

    lua_pushinteger(L, (lua_Integer)bits);
}

/* Writes a number into a bitfield, keeping only the bits its width holds and leaving the bits around it alone. */
static void cbitfield_set(lua_State *L, struct crecord_field *field, void *unit, int idx)
{
    struct cdata *cd = (struct cdata *)luaL_testudata(L, idx, CDATA_MT);
    size_t size = ctype_sizeof(field->ct);
    uint64_t mask = cbitfield_mask(field);
    unsigned shift = cbitfield_shift(field);
    lua_Integer value;
    uint64_t bits;

    if (cd && ctype_is_num(cd->ct)) {
        cdata_to_lua(L, cd->ct, cdata_ptr(cd));
        value = from_lua_num_int(L, -1);
        lua_pop(L, 1);
    } else if (!cd && (lua_type(L, idx) == LUA_TNUMBER || lua_type(L, idx) == LUA_TBOOLEAN)) {
        value = from_lua_num_int(L, idx);
    } else {
        __ctype_tostring(L, field->ct);
        luaL_error(L, "The value \"%s\" does not convert to the bitfield \"%s\" of type \"%s\".", luaL_typename(L, idx), field->name, lua_tostring(L, -1));
        return;
    }

    bits = cbitfield_read(unit, size);
    bits = (bits & ~(mask << shift)) | (((uint64_t)value & mask) << shift);
    cbitfield_write(unit, size, bits);
}

static bool cdata_from_lua_table(lua_State *L, struct ctype *ct, void *ptr, int idx, bool cast)
{
    int i = 0;

    if (ct->type == CTYPE_ARRAY) {
        lua_pushnil(L);

        while (lua_next(L, idx)) {
            if (lua_isinteger(L, -2)) {
                i = lua_tointeger(L, -2) - 1;
                if (i < ct->array->size)
                    cdata_from_lua(L, ct->array->ct, VARN_PTR_OFFSET(ptr, ctype_sizeof(ct->array->ct) * i),
                            lua_absindex(L, -1), cast);
            }
            lua_pop(L, 1);
        }

        return true;
    } else if (ct->type == CTYPE_RECORD) {
        while (i < ct->rc->nfield) {
            struct crecord_field *field = ct->rc->fields[i++];

            lua_rawgeti(L, idx, i);
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                lua_getfield(L, idx, field->name);
            }

            if (!lua_isnil(L, -1) && field->bit_width)
                cbitfield_set(L, field, VARN_PTR_OFFSET(ptr, field->offset), lua_absindex(L, -1));
            else if (!lua_isnil(L, -1))
                cdata_from_lua(L, field->ct, VARN_PTR_OFFSET(ptr, field->offset), lua_absindex(L, -1), cast);
            lua_pop(L, 1);
        }
        return true;
    }

    return false;
}

static int cdata_from_lua(lua_State *L, struct ctype *ct, void *ptr, int idx, bool cast)
{
    switch (ct->type) {
    case CTYPE_FUNC:
    case CTYPE_VOID:
        luaL_error(L, "invalid C type");
        break;
    case CTYPE_ARRAY:
    case CTYPE_RECORD:
        if (cast)
            luaL_error(L, "invalid C type");
        break;
    default:
        break;
    }

    switch (lua_type(L, idx)) {
    case LUA_TNIL:
        if (ct->type == CTYPE_PTR) {
            *(void **)ptr = NULL;
            return 0;
        }
        break;
    case LUA_TNUMBER:
    case LUA_TBOOLEAN:
        if (cdata_from_lua_num(L, ct, ptr, idx, cast))
            return 0;
        break;
    case LUA_TSTRING:
        if (cast || ((ctype_ptr_to(ct, CTYPE_CHAR) || ctype_ptr_to(ct, CTYPE_VOID))
            && ct->ptr->is_const)) {
            *(const char **)ptr = (const char *)lua_tostring(L, idx);
            return 0;
        }

        if (ct->type == CTYPE_ARRAY && ct->array->ct->type == CTYPE_CHAR) {
            size_t len;
            const char *str = lua_tolstring(L, idx, &len);
            /* clamp to the array capacity so an oversized string cannot overflow the cdata block */
            size_t cap = ctype_sizeof(ct);
            size_t n = len < cap ? len : cap;
            memcpy(ptr, str, n);
            if (n < cap)
                ((char *)ptr)[n] = '\0';
            return 0;
        }
        break;
    case LUA_TUSERDATA:
        if (luaL_testudata(L, idx, CDATA_MT)) {
            if (cdata_from_lua_cdata(L, ct, ptr, idx, cast))
                return 0;
        } else if (ct->type == CTYPE_PTR) {
            void *ud = lua_touserdata(L, idx);

            if (luaL_testudata(L, idx, LUA_FILEHANDLE)) {
                *(void **)ptr = *(void **)ud;
                return 0;
            }

            if (cast || ctype_ptr_to(ct, CTYPE_VOID)) {
                *(void **)ptr = ud;
                return 0;
            }
        }
        break;
    case LUA_TLIGHTUSERDATA:
        if (ct->type == CTYPE_PTR) {
            *(void **)ptr = lua_touserdata(L, idx);
            return 0;
        }
        break;
    case LUA_TTABLE:
        if (cdata_from_lua_table(L, ct, ptr, idx, cast))
            return 0;
        break;
    case LUA_TFUNCTION:
        if (ctype_ptr_to(ct, CTYPE_FUNC)) {
            __ctype_tostring(L, ct);
            return luaL_error(L, "A Lua function becomes the function pointer \"%s\" only through \"ffi.cast\", which keeps the callback alive.", lua_tostring(L, -1));
        }
        break;
    default:
        break;
    }

    if (luaL_testudata(L, idx, CDATA_MT)) {
        struct cdata *cd = (struct cdata *)lua_touserdata(L, idx);
        __ctype_tostring(L, cd->ct);
        __ctype_tostring(L, ct);
        return luaL_error(L, "The C type \"%s\" does not convert to \"%s\".", lua_tostring(L, -2), lua_tostring(L, -1));
    }

    __ctype_tostring(L, ct);
    return luaL_error(L, "The Lua type \"%s\" does not convert to \"%s\".", luaL_typename(L, idx), lua_tostring(L, -1));
}

static int cdata_index_ptr(lua_State *L, struct cdata *cd, struct ctype *ct, bool to)
{
    void *ptr = cdata_type(cd) == CTYPE_PTR ? cdata_ptr_ptr(cd) : cdata_ptr(cd);
    int idx;

    if (ct->type == CTYPE_VOID) {
        __ctype_tostring(L, cd->ct);
        return luaL_error(L, "ctype '%s' cannot be indexed", lua_tostring(L, -1));
    }

    if (!lua_isinteger(L, 2)) {
        __ctype_tostring(L, cd->ct);
        return luaL_error(L, "ctype '%s' cannot be indexed with %s", lua_tostring(L, -1), luaL_typename(L, 2));
    }

    idx = lua_tointeger(L, 2);

    if (to) {
        lua_rawgetp(L, LUA_REGISTRYINDEX, cd);
        lua_rawgeti(L, -1, idx);
        if (!lua_isnil(L, -1)) {
            lua_remove(L, -2);
            return 1;
        }
        lua_pop(L, 2);

        cdata_to_lua(L, ct, VARN_PTR_OFFSET(ptr, ctype_sizeof(ct) * idx));

        if (luaL_testudata(L, -1, CDATA_MT)) {
            lua_rawgetp(L, LUA_REGISTRYINDEX, cd);
            lua_pushvalue(L, -2);
            lua_rawseti(L, -2, idx);
            lua_pop(L, 1);
        }
        return 1;
    } else {
        return cdata_from_lua(L, ct, VARN_PTR_OFFSET(ptr, ctype_sizeof(ct) * idx), 3, false);
    }
}

static struct crecord_field *cdata_crecord_find_field(
        struct crecord_field **fields, int nfield, const char *name, size_t *offset)
{
    int i;

    for (i = 0; i < nfield; i++) {
        struct crecord_field *field = fields[i];

        if (field->name[0]) {
            if (!strcmp(field->name, name)) {
                *offset += field->offset;
                return field;
            }
        } else {
            field = cdata_crecord_find_field(field->ct->rc->fields, field->ct->rc->nfield, name, offset);
            if (field) {
                *offset += fields[i]->offset;
                return field;
            }
        }
    }

    return NULL;
}

static int cdata_index_crecord(lua_State *L, struct cdata *cd, struct ctype *ct, bool to)
{
    void *ptr = cdata_type(cd) == CTYPE_PTR ? cdata_ptr_ptr(cd) : cdata_ptr(cd);
    struct crecord *rc = ct->rc;
    struct crecord_field *field;
    size_t offset = 0;
    const char *name;

    if (lua_type(L, 2) != LUA_TSTRING)
        return luaL_error(L, "struct must be indexed with string");

    name = lua_tostring(L, 2);

    if (to) {
        lua_rawgetp(L, LUA_REGISTRYINDEX, cd);
        lua_getfield(L, -1, name);
        if (!lua_isnil(L, -1)) {
            lua_remove(L, -2);
            return 1;
        }
        lua_pop(L, 2);
    }

    field = cdata_crecord_find_field(rc->fields, rc->nfield, name, &offset);
    if (!field) {
        if (to) {
            if (rc->mflags & METATYPE_FLAG_INDEX) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, rc->mt_ref);
                lua_getfield(L, -1, "__index");

                if (lua_isfunction(L, -1))
                    return 1;

                if (lua_istable(L, -1)) {
                    lua_getfield(L, -1, name);
                    if (!lua_isnil(L, -1))
                        return 1;
                }
            }
        }

        __ctype_tostring(L, ct);
        return luaL_error(L, "ctype '%s' has no member named '%s'", lua_tostring(L, -1), name);
    }

    if (field->bit_width && to) {
        cbitfield_push(L, field, VARN_PTR_OFFSET(ptr, offset));
        return 1;
    }

    if (field->bit_width) {
        cbitfield_set(L, field, VARN_PTR_OFFSET(ptr, offset), 3);
        return 0;
    }

    if (to) {
        cdata_to_lua(L, field->ct, VARN_PTR_OFFSET(ptr, offset));
        if (luaL_testudata(L, -1, CDATA_MT)) {
            lua_rawgetp(L, LUA_REGISTRYINDEX, cd);
            lua_pushvalue(L, -2);
            lua_setfield(L, -2, name);
            lua_pop(L, 1);
        }
        return 1;
    } else {
        return cdata_from_lua(L, field->ct, VARN_PTR_OFFSET(ptr, offset), 3, false);
    }
}

static int cdata_index_common(lua_State *L, bool to)
{
    struct cdata *cd = (struct cdata *)luaL_checkudata(L, 1, CDATA_MT);
    struct ctype *ct = cd->ct;

    if (!to && ct->is_const)
        return luaL_error(L, "assignment of read-only variable");

    switch (ct->type) {
    case CTYPE_RECORD:
        return cdata_index_crecord(L, cd, ct, to);
    case CTYPE_PTR:
        if (ctype_ptr_to(ct, CTYPE_RECORD) && lua_type(L, 2) == LUA_TSTRING)
            return cdata_index_crecord(L, cd, ct->ptr, to);
        return cdata_index_ptr(L, cd, ct->ptr, to);
    case CTYPE_ARRAY:
        return cdata_index_ptr(L, cd, ct->array->ct, to);
    default:
        __ctype_tostring(L, cd->ct);
        return luaL_error(L, "ctype '%s' cannot be indexed", lua_tostring(L, -1));
    }
}

static int cdata_index(lua_State *L)
{
    return cdata_index_common(L, true);
}

static int cdata_newindex(lua_State *L)
{
    return cdata_index_common(L, false);
}

static int cdata_eq(lua_State *L)
{
    struct cdata *cd = (struct cdata *)luaL_checkudata(L, 1, CDATA_MT);
    int type = cdata_type(cd);
    struct cdata *a;
    bool eq = false;

    switch (type) {
    case CTYPE_RECORD:
    case CTYPE_ARRAY:
    case CTYPE_FUNC:
        break;
    case CTYPE_PTR:
        if (lua_isnil(L, 2)) {
            eq = cdata_ptr_ptr(cd) == NULL;
            break;
        }

        a = (struct cdata *)luaL_testudata(L, 2, CDATA_MT);
        if (a && cdata_type(a) == CTYPE_PTR)
            eq = cdata_ptr_ptr(cd) == cdata_ptr_ptr(a);

        break;
    default:
        cdata_to_lua(L, cd->ct, cdata_ptr(cd));
        eq = lua_equal(L, 2, -1);
        lua_pop(L, 1);
    }

    lua_pushboolean(L, eq);
    return 1;
}

static ffi_type *lua_to_vararg(lua_State *L, int idx)
{
    struct cdata *cd;

    switch (lua_type(L, idx)) {
    case LUA_TBOOLEAN:
        return &ffi_type_sint; /* cannot be less than the size of int, due to limited in libffi */
    case LUA_TNUMBER:
        if (lua_isinteger(L, idx))
            return ffi_type_of(sizeof(lua_Integer), false);
        return &ffi_type_double;
    case LUA_TNIL:
    case LUA_TSTRING:
    case LUA_TLIGHTUSERDATA:
        return &ffi_type_pointer;
    case LUA_TUSERDATA:
        cd = (struct cdata *)luaL_testudata(L, idx, CDATA_MT);
        if (!cd || cdata_type(cd) == CTYPE_RECORD || cdata_type(cd) == CTYPE_ARRAY)
            return &ffi_type_pointer;
        return ctype_ft(cd->ct);
    default:
        return NULL;
    }
}

/* Calls a C function while the state counts it, so a callback it runs on its coroutine leaves its failure for this call to raise once it returns. */
static void cdata_ffi_call(lua_State *L, ffi_cif *cif, void *sym, void *rvalue, void **values)
{
    struct cstate *state = cstate_get(L);
    lua_State *outer_calling = state->calling;
    int outer_failure = state->failure_ref;
    int failure_ref;

    state->calling = L;
    state->failure_ref = LUA_NOREF;
    state->ncall++;
    ffi_call(cif, FFI_FN(sym), rvalue, values);
    state->ncall--;

    /* A call made from inside a callback hands the state back to the call that ran that callback. */
    failure_ref = state->failure_ref;
    state->calling = outer_calling;
    state->failure_ref = outer_failure;

    ccallback_raise_failure(L, failure_ref);
}

static int cdata_call(lua_State *L)
{
    struct cdata *cd = (struct cdata *)luaL_checkudata(L, 1, CDATA_MT);
    ffi_type *args[MAX_FUNC_ARGS] = {};
    void *values[MAX_FUNC_ARGS] = {};
    struct ctype *ct = cd->ct;
    int i, status, narg;
    struct cfunc *func;
    struct ctype *rtype;
    ffi_cif cif;
    void *sym;

    /* A function and a pointer to a function both hold the address they call. */
    if (ct->type == CTYPE_FUNC) {
        func = ct->func;
    } else if (ctype_ptr_to(ct, CTYPE_FUNC)) {
        func = ct->ptr->func;
    } else {
        __ctype_tostring(L, ct);
        return luaL_error(L, "The C type \"%s\" is not callable.", lua_tostring(L, -1));
    }

    sym = cdata_ptr_ptr(cd);
    if (!sym) {
        __ctype_tostring(L, ct);
        return luaL_error(L, "The function pointer \"%s\" is null.", lua_tostring(L, -1));
    }

    rtype = func->rtype;

    narg = lua_gettop(L) - 1;

    if (func->va) {
        if (narg < func->narg)
            return luaL_error(L, "wrong number of arguments for function call");
    } else if (narg != func->narg) {
        return luaL_error(L, "wrong number of arguments for function call");
    }

    if (narg > MAX_FUNC_ARGS)
        return luaL_error(L, "too many arguments for function call");

    for (i = 0; i < func->narg; i++) {
        args[i] = ctype_ft(func->args[i]);
        values[i] = alloca(args[i]->size);
        cdata_from_lua(L, func->args[i], values[i], i + 2, false);
    }

    if (func->va) {
        for (i = func->narg; i < narg; i++) {
            args[i] = lua_to_vararg(L, i + 2);
            if (!args[i])
                return luaL_error(L, "unsupported type '%s'", luaL_typename(L, i + 2));
            values[i] = alloca(args[i]->size);
        }

        for (i = func->narg; i < narg; i++) {
            switch (lua_type(L, i + 2)) {
            case LUA_TBOOLEAN:
            case LUA_TNUMBER:
                ft_from_lua_num(L, args[i], values[i], i + 2);
                break;
            case LUA_TNIL:
                *(void **)values[i] = NULL;
                break;
            case LUA_TSTRING:
                *(void **)values[i] = (void *)luaL_checkstring(L, i + 2);
                break;
            case LUA_TLIGHTUSERDATA:
                *(void **)values[i] = (void *)lua_topointer(L, i + 2);
                break;
            case LUA_TUSERDATA:
                cd = (struct cdata *)luaL_testudata(L, i + 2, CDATA_MT);
                if (!cd)
                    *(void **)values[i] = lua_touserdata(L, i + 2);
                else if (cdata_type(cd) == CTYPE_RECORD || cdata_type(cd) == CTYPE_ARRAY)
                    *(void **)values[i] = cdata_ptr(cd);
                else if (cdata_type(cd) == CTYPE_FUNC || cdata_type(cd) == CTYPE_PTR)
                    *(void **)values[i] = cdata_ptr_ptr(cd);
                else {
                    cdata_to_lua(L, cd->ct, cdata_ptr(cd));
                    ft_from_lua_num(L, args[i], values[i], -1);
                    lua_pop(L, 1);
                }
                break;
            }
        }
    }

    if (func->va)
        status = ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, func->narg, narg, ctype_ft(rtype), args);
    else
        status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, func->narg, ctype_ft(rtype), args);
    if (status)
        return luaL_error(L, "ffi_prep_cif fail: %d", status);

    if (rtype->type == CTYPE_RECORD || rtype->type == CTYPE_PTR) {
        if (rtype->type == CTYPE_PTR) {
            void *rvalue;
            cdata_ffi_call(L, &cif, sym, &rvalue, values);
            cdata_ptr_set(cdata_new(L, rtype, NULL), rvalue);
        } else {
            cd = cdata_new(L, rtype, NULL);
            cdata_ffi_call(L, &cif, sym, cdata_ptr(cd), values);
        }

        return 1;
    }

    if (rtype->type <= CTYPE_RECORD) {
        void *rvalue = NULL;

        /* A narrow integer comes back widened to a whole ffi_arg, so its storage holds one and the value is read where it lies in it. */
        if (rtype->type != CTYPE_VOID)
            rvalue = alloca(ctype_is_widened(rtype) ? sizeof(ffi_arg) : ctype_sizeof(rtype));

        cdata_ffi_call(L, &cif, sym, rvalue, values);

        return cdata_to_lua(L, rtype, rvalue == NULL ? NULL : widened_value(rtype, rvalue));
    }

    return luaL_error(L, "unsupported return type '%s'", ctype_name(rtype));
}

static int cdata_len(lua_State *L)
{
    struct cdata *cd = (struct cdata *)luaL_checkudata(L, 1, CDATA_MT);

    if (cd->ct->type != CTYPE_ARRAY) {
        __ctype_tostring(L, cd->ct);
        return luaL_error(L, "attempt to get length of non-array cdata<%s>", lua_tostring(L, -1));
    }

    lua_pushinteger(L, cd->ct->array->size);

    return 1;
}

static int cdata_gc(lua_State *L)
{
    struct cdata *cd = (struct cdata *)luaL_checkudata(L, 1, CDATA_MT);
    int gc_ref = cd->gc_ref;

    if (gc_ref != LUA_REFNIL) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, gc_ref);
        lua_pushvalue(L, 1);
        if (lua_pcall(L, 1, 0, 0))
            lua_pop(L, 1);
        luaL_unref(L, LUA_REGISTRYINDEX, gc_ref);
    }

    if (cd->cb) {
        ccallback_release(L, cd->cb);
        cd->cb = NULL;
    }

    lua_pushnil(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, cd);

    return 0;
}

static const luaL_Reg cdata_methods[] = {
    {"__tostring", cdata_tostring},
    {"__index", cdata_index},
    {"__newindex", cdata_newindex},
    {"__eq", cdata_eq},
    {"__call", cdata_call},
    {"__len", cdata_len},
    {"__gc", cdata_gc},
    {NULL, NULL}
};

static int lua_ctype_tostring(lua_State *L)
{
    struct ctype *ct = (struct ctype *)luaL_checkudata(L, 1, CTYPE_MT);

    lua_pushliteral(L, "ctype<");
    __ctype_tostring(L, ct);
    lua_pushliteral(L, ">");
    lua_concat(L, 3);

    return 1;
}

static const luaL_Reg ctype_methods[] = {
    {"__tostring", lua_ctype_tostring},
    {NULL, NULL}
};

/* Finds a symbol of a library, where the default namespace first answers what the host added with "Runtime::addSymbol" under the registry key `varn.ffi.symbols`, since a statically linked symbol has no export for the dynamic linker to find. */
static void *clib_symbol(lua_State *L, struct clib *lib, const char *name)
{
    void *sym = NULL;

    if (lib->h == RTLD_DEFAULT) {
        int top = lua_gettop(L);

        if (lua_getfield(L, LUA_REGISTRYINDEX, "varn.ffi.symbols") == LUA_TTABLE
                && lua_getfield(L, -1, name) == LUA_TLIGHTUSERDATA)
            sym = lua_touserdata(L, -1);

        lua_settop(L, top);
    }

    return sym ? sym : dlsym(lib->h, name);
}

static int clib_index(lua_State *L)
{
    struct clib *lib = (struct clib *)luaL_checkudata(L, 1, CLIB_MT);
    const char *name = luaL_checkstring(L, 2);
    struct ctype match = { .type = CTYPE_FUNC };
    struct ctype *ct;
    void *sym;

    lua_rawgetp(L, LUA_REGISTRYINDEX, lib);
    lua_getfield(L, -1, name);
    if (!lua_isnil(L, -1))
        goto done;
    lua_pop(L, 1);

    /* The constants of an enum answer through every namespace, the way C sees them. */
    lua_rawgetp(L, LUA_REGISTRYINDEX, &cconst_registry);
    if (lua_getfield(L, -1, name) == LUA_TNUMBER) {
        lua_remove(L, -2);
        goto done;
    }
    lua_pop(L, 2);

    lua_rawgetp(L, LUA_REGISTRYINDEX, &cfunc_registry);
    lua_getfield(L, -1, name);

    if (lua_isnil(L, -1))
        return luaL_error(L, "The function \"%s\" is not declared.", name);

    match.func = (struct cfunc *)lua_topointer(L, -1);
    lua_pop(L, 2);

    ct = ctype_lookup(L, &match, false);

    sym = clib_symbol(L, lib, name);
    if (!sym)
        return luaL_error(L, "The function \"%s\" is declared, but no library defines it.", name);

    cdata_ptr_set(cdata_new(L, ct, NULL), sym);
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, name);

done:
    lua_remove(L, -2);
    return 1;
}

static int clib_tostring(lua_State *L)
{
    struct clib *lib = (struct clib *)luaL_checkudata(L, 1, CLIB_MT);
    if (lib->h == RTLD_DEFAULT)
        lua_pushliteral(L, "library: default");
    else
        lua_pushfstring(L, "library: %p", lib->h);
    return 1;
}

static int clib_gc(lua_State *L)
{
    struct clib *lib = (struct clib *)luaL_checkudata(L, 1, CLIB_MT);
    void *h = lib->h;

    if (h != RTLD_DEFAULT)
        dlclose(h);

    lua_pushnil(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, lib);

    return 0;
}

static const luaL_Reg clib_methods[] = {
    {"__index", clib_index},
    {"__tostring", clib_tostring},
    {"__gc", clib_gc},
    {NULL, NULL}
};

static int cparse_expected_error(lua_State *L, int tok, const char *s)
{
    if (tok)
        return luaL_error(L, "%d:'%s' expected before '%s'", yyget_lineno(), s, yyget_text());
    else
        return luaL_error(L, "%d:identifier expected", yyget_lineno());
}

static void ctype_to_ptr(lua_State *L, struct ctype *ct)
{
    struct ctype *ptr = ctype_lookup(L, ct, false);
    ct->type = CTYPE_PTR;
    ct->is_const = false;
    ct->ptr = ptr;
}

extern const char *lex_err;

static inline int cparse_check_tok(lua_State *L, int tok)
{
    if (!tok && lex_err)
        return luaL_error(L, "%d:%s", yyget_lineno(), lex_err);
    return tok;
}

static int cparse_pointer(lua_State *L, int tok, struct ctype *ct)
{
    while (cparse_check_tok(L, tok) == '*') {
        ctype_to_ptr(L, ct);
        tok = yylex();
    }

    if (cparse_check_tok(L, tok) == TOK_CONST) {
        ct->is_const = true;
        tok = yylex();
    }

    return tok;
}

/* Reads the integer the lexer just matched, written in decimal, octal or hexadecimal the way C writes it. */
static lua_Integer cparse_integer_text(lua_State *L)
{
    const char *text = yyget_text();
    unsigned long long value;
    char *end;

    errno = 0;
    value = strtoull(text, &end, 0);

    if (errno || *end || value > LUA_MAXINTEGER)
        luaL_error(L, "The integer \"%s\" on line %d is not one C can read.", text, yyget_lineno());

    return (lua_Integer)value;
}

static int cparse_array(lua_State *L, int tok, bool *flexible, int *size)
{
    *size = -1;

    if (cparse_check_tok(L, tok) != '[') {
        *flexible = false;
        return tok;
    }

    tok = yylex();

    if (!*flexible && cparse_check_tok(L, tok) != TOK_INTEGER)
        return luaL_error(L, "%d:flexible array not supported at here", yyget_lineno());

    *flexible = false;

    if (cparse_check_tok(L, tok) == TOK_INTEGER || cparse_check_tok(L, tok) == '?') {
        if (cparse_check_tok(L, tok) == TOK_INTEGER) {
            lua_Integer value = cparse_integer_text(L);

            if (value > INT_MAX)
                return luaL_error(L, "The array of %s elements on line %d is too large.", yyget_text(), yyget_lineno());
            *size = (int)value;
        } else {
            *flexible = true;
        }
        tok = yylex();
    } else {
        *flexible = true;
    }

    if (cparse_check_tok(L, tok) != ']')
        return cparse_expected_error(L, tok, "]");

    return yylex();
}

static int cparse_packed_attribute(lua_State *L, int tok, bool *is_packed)
{
    while (cparse_check_tok(L, tok) == TOK_NAME && !strcmp(yyget_text(), "__attribute__")) {
        int depth = 2;

        tok = yylex();
        if (cparse_check_tok(L, tok) != '(')
            return cparse_expected_error(L, tok, "(");

        tok = yylex();
        if (cparse_check_tok(L, tok) != '(')
            return cparse_expected_error(L, tok, "(");

        tok = yylex();

        while (depth > 0) {
            if (!cparse_check_tok(L, tok))
                return luaL_error(L, "%d:unterminated __attribute__", yyget_lineno());

            if (tok == TOK_NAME && depth == 2
                    && (!strcmp(yyget_text(), "packed") || !strcmp(yyget_text(), "__packed__"))) {
                *is_packed = true;
            }

            if (tok == '(')
                depth++;
            else if (tok == ')')
                depth--;

            tok = yylex();
        }
    }

    return tok;
}

static int cparse_basetype(lua_State *L, int tok, struct ctype *ct);

static void init_ft_struct(lua_State *L, ffi_type *ft, ffi_type **elements, size_t *offsets)
{
    int status;

    ft->type = FFI_TYPE_STRUCT;
    ft->elements = elements;

    status = ffi_get_struct_offsets(FFI_DEFAULT_ABI, ft, offsets);
    if (status)
        luaL_error(L, "ffi_get_struct_offsets fail: %d", status);
}

static void cparse_new_array(lua_State *L, size_t array_size, struct ctype *ct)
{
    struct carray *a = carray_lookup(L, array_size, ct);

    ct->type = CTYPE_ARRAY;
    ct->is_const = false;
    ct->array = a;
}

static void check_void_forbidden(lua_State *L, struct ctype *ct, int tok)
{
    if (ct->type != CTYPE_VOID)
        return;

    if (tok)
        luaL_error(L, "%d:void type in forbidden context near '%s'",
                yyget_lineno(), yyget_text());
    else
        luaL_error(L, "%d:void type in forbidden context", yyget_lineno());
}

static int cparse_record(lua_State *L, struct ctype *ct, bool is_union);

static int cparse_function_arg(lua_State *L, int tok, struct ctype *ct, char **name);

/* Allocates a named field of a record, refusing a name an earlier field of the record already took. */
static struct crecord_field *cparse_new_field(lua_State *L, struct crecord_field **fields, int nfield,
        const char *name, size_t len)
{
    struct crecord_field *field;
    int i;

    for (i = 0; i < nfield; i++)
        if (strlen(fields[i]->name) == len && !memcmp(fields[i]->name, name, len))
            luaL_error(L, "The record declares the member \"%s\" twice on line %d.", fields[i]->name, yyget_lineno());

    field = (struct crecord_field *)cmem_alloc(L, sizeof(struct crecord_field) + len + 1);
    memcpy(field->name, name, len);

    return field;
}

/* Reads the width of a bitfield, and refuses a type that is not an integer and a width its type does not hold. */
static int cparse_bitfield(lua_State *L, struct crecord_field *field, struct ctype *ct)
{
    int tok = yylex();
    lua_Integer width;

    if (cparse_check_tok(L, tok) != TOK_INTEGER)
        return cparse_expected_error(L, tok, "width");

    width = cparse_integer_text(L);

    if (!ctype_is_int(ct)) {
        __ctype_tostring(L, ct);
        return luaL_error(L, "The bitfield \"%s\" on line %d needs an integer type, not \"%s\".", field->name, yyget_lineno(), lua_tostring(L, -1));
    }

    if (width < 1)
        return luaL_error(L, "The bitfield \"%s\" on line %d needs a width of at least one bit.", field->name, yyget_lineno());

    if ((size_t)width > ctype_sizeof(ct) * 8) {
        __ctype_tostring(L, ct);
        return luaL_error(L, "The bitfield \"%s\" on line %d is %I bits wide, wider than its type \"%s\".", field->name, yyget_lineno(), (LUAI_UACINT)width, lua_tostring(L, -1));
    }

    field->bit_width = (uint8_t)width;

    return yylex();
}

static int cparse_record_field(lua_State *L, struct crecord_field **fields)
{
    int nfield = 0;
    int tok;

    while (true) {
        struct crecord_field *field;
        struct ctype bt = {}, ct;
        bool flexible = false;
        int array_size;

        tok = yylex();

        if (cparse_check_tok(L, tok) == '}')
            return nfield;

        if (cparse_check_tok(L, tok) == TOK_STRUCT || cparse_check_tok(L, tok) == TOK_UNION) {
            tok = cparse_record(L, &bt, cparse_check_tok(L, tok) == TOK_UNION);
            if (tok == ';') {
                field = (struct crecord_field *)cmem_alloc(L, sizeof(struct crecord_field) + 1);
                ct = bt;
                goto add;
            }
        } else {
            tok = cparse_basetype(L, tok, &bt);
        }

again:
        ct = bt;

        tok = cparse_pointer(L, tok, &ct);

        /* A field declared as `int (*name)(int)` is a pointer to a function, named inside its declarator. */
        if (cparse_check_tok(L, tok) == '(') {
            char *fname = NULL;

            tok = cparse_function_arg(L, tok, &ct, &fname);
            if (!fname)
                return cparse_expected_error(L, tok, "identifier");

            field = cparse_new_field(L, fields, nfield, fname, strlen(fname));
            cmem_release(L, fname);
            goto add;
        }

        check_void_forbidden(L, &ct, tok);

        if (cparse_check_tok(L, tok) != TOK_NAME)
            return cparse_expected_error(L, tok, "identifier");

        field = cparse_new_field(L, fields, nfield, yyget_text(), yyget_leng());

        tok = yylex();

        if (cparse_check_tok(L, tok) == ':') {
            tok = cparse_bitfield(L, field, &ct);
            goto add;
        }

        tok = cparse_array(L, tok, &flexible, &array_size);

        if (array_size >= 0)
            cparse_new_array(L, array_size, &ct);

add:
        if (nfield == MAX_RECORD_FIELDS)
            return luaL_error(L, "The record on line %d has more than %d fields.", yyget_lineno(), MAX_RECORD_FIELDS);

        field->ct = ctype_lookup(L, &ct, false);
        fields[nfield++] = field;

        if (cparse_check_tok(L, tok) == ',') {
            tok = yylex();
            goto again;
        }

        if (cparse_check_tok(L, tok) != ';')
            return cparse_expected_error(L, tok, ";");
    }
}

static inline bool ctype_is_zero_array(struct ctype *ct)
{
    return ct->type == CTYPE_ARRAY && ct->array->size == 0;
}

static void cparse_record_packed_layout(struct crecord *rc)
{
    size_t size = 0;
    int i;

    rc->ft.type = FFI_TYPE_STRUCT;
    rc->ft.alignment = 1;

    if (rc->is_union) {
        for (i = 0; i < rc->nfield; i++) {
            size_t field_size = ctype_sizeof(rc->fields[i]->ct);

            rc->fields[i]->offset = 0;

            if (field_size > size)
                size = field_size;
        }

        rc->ft.size = size;
        return;
    }

    for (i = 0; i < rc->nfield; i++) {
        rc->fields[i]->offset = size;

        if (!ctype_is_zero_array(rc->fields[i]->ct))
            size += ctype_sizeof(rc->fields[i]->ct);
    }

    rc->ft.size = size;
}

/* Describes an array to libffi as a struct of its elements, the only way libffi knows an array held inside a record, in the type and the slots the record keeps for it. */
static ffi_type *carray_describe(struct carray *a, ffi_type *ft, ffi_type **slots)
{
    size_t i;

    *ft = a->ft;
    ft->elements = slots;

    for (i = 0; i < a->size; i++)
        slots[i] = ctype_ft(a->ct);
    slots[a->size] = NULL;

    return ft;
}

/* Sizes a union after its largest member and aligns it after its strictest one, since libffi only sees the largest. */
static void crecord_union_layout(struct crecord *rc, ffi_type **fts)
{
    unsigned short alignment = 1;
    size_t size = 0;
    int i;

    for (i = 0; i < rc->nfield; i++) {
        if (!fts[i])
            continue;

        if (fts[i]->size > size)
            size = fts[i]->size;

        if (fts[i]->alignment > alignment)
            alignment = fts[i]->alignment;
    }

    rc->ft.alignment = alignment;
    rc->ft.size = (size + alignment - 1) / alignment * alignment;
}

enum {
    CBYTE_PADDING,
    CBYTE_FLOAT,
    CBYTE_INTEGER
};

static void crecord_mark_field(struct crecord_field *field, size_t base, uint8_t *kinds, size_t limit);

/* Marks the bytes a value of a type holds at `offset` as floating point or integer data, where integer data wins a byte two members share. */
static void ctype_mark_bytes(struct ctype *ct, size_t offset, uint8_t *kinds, size_t limit)
{
    uint8_t kind = ct->type == CTYPE_FLOAT || ct->type == CTYPE_DOUBLE ? CBYTE_FLOAT : CBYTE_INTEGER;
    size_t i;

    switch (ct->type) {
    case CTYPE_ARRAY:
        for (i = 0; i < ct->array->size; i++)
            ctype_mark_bytes(ct->array->ct, offset + i * ctype_sizeof(ct->array->ct), kinds, limit);
        return;
    case CTYPE_RECORD:
        for (i = 0; i < ct->rc->nfield; i++)
            crecord_mark_field(ct->rc->fields[i], offset, kinds, limit);
        return;
    default:
        for (i = offset; i < offset + ctype_sizeof(ct) && i < limit; i++)
            kinds[i] = kinds[i] > kind ? kinds[i] : kind;
        return;
    }
}

static void crecord_mark_field(struct crecord_field *field, size_t base, uint8_t *kinds, size_t limit)
{
    size_t i;

    if (!field->bit_width) {
        ctype_mark_bytes(field->ct, base + field->offset, kinds, limit);
        return;
    }

    for (i = field->bit_offset / 8; i <= (size_t)(field->bit_offset + field->bit_width - 1) / 8; i++) {
        if (base + field->offset + i < limit)
            kinds[base + field->offset + i] = CBYTE_INTEGER;
    }
}

/* Describes a struct holding bitfields to libffi, which knows no bitfield, by the class of each eightbyte in chunks of the alignment of the struct, since only those classes decide how the struct passes. */
static void crecord_bitfield_describe(struct crecord *rc, ffi_type **elements)
{
    uint8_t kinds[MAX_BITFIELD_ELEMENTS] = { 0 };
    size_t size = rc->ft.size;
    size_t alignment = rc->ft.alignment;
    size_t chunk, i;

    /* A struct larger than two eightbytes passes in memory or by reference on every platform, whatever it holds. */
    if (size > MAX_BITFIELD_ELEMENTS) {
        elements[0] = ffi_type_of(alignment, false);
        elements[1] = NULL;
        return;
    }

    for (i = 0; i < rc->nfield; i++)
        crecord_mark_field(rc->fields[i], 0, kinds, size);

    for (chunk = 0; chunk < size / alignment; chunk++) {
        size_t word = chunk * alignment / 8 * 8;
        bool integer = alignment < 4;

        for (i = word; i < word + 8 && i < size; i++)
            integer = integer || kinds[i] == CBYTE_INTEGER;

        if (integer)
            elements[chunk] = ffi_type_of(alignment, false);
        else
            elements[chunk] = alignment == 8 ? &ffi_type_double : &ffi_type_float;
    }

    elements[chunk] = NULL;
}

static size_t align_up(size_t value, size_t alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}

/* Lays out a struct holding bitfields the way the C compiler of the platform does, where Windows opens a new unit whenever the declared type changes size and System V packs a bitfield into the first unit of its type that holds it whole. */
static void crecord_bitfield_layout(struct crecord *rc, ffi_type **elements)
{
    size_t bit = 0, alignment = 1;
    int i;
#if defined(_WIN32)
    size_t unit = 0, unit_size = 0, unit_used = 0;
#endif

    for (i = 0; i < rc->nfield; i++) {
        struct crecord_field *field = rc->fields[i];
        size_t size = ctype_sizeof(field->ct);
        size_t align = ctype_ft(field->ct)->alignment;

        if (align > alignment)
            alignment = align;

        if (!field->bit_width) {
#if defined(_WIN32)
            if (unit_size)
                bit = (unit + unit_size) * 8;
            unit_size = 0;
#endif
            field->offset = align_up((bit + 7) / 8, align);
            bit = (field->offset + size) * 8;
            continue;
        }

#if defined(_WIN32)
        if (unit_size != size || unit_used + field->bit_width > size * 8) {
            if (unit_size)
                bit = (unit + unit_size) * 8;
            unit = align_up((bit + 7) / 8, align);
            unit_size = size;
            unit_used = 0;
        }

        field->offset = unit;
        field->bit_offset = (uint8_t)unit_used;
        unit_used += field->bit_width;
        bit = unit * 8 + unit_used;
#else
        if (bit / (size * 8) != (bit + field->bit_width - 1) / (size * 8))
            bit = align_up(bit, size * 8);

        field->offset = bit / (size * 8) * size;
        field->bit_offset = (uint8_t)(bit - field->offset * 8);
        bit += field->bit_width;
#endif
    }

#if defined(_WIN32)
    if (unit_size)
        bit = (unit + unit_size) * 8;
#endif

    rc->ft.type = FFI_TYPE_STRUCT;
    rc->ft.size = align_up((bit + 7) / 8, alignment);
    rc->ft.alignment = (unsigned short)alignment;
    rc->ft.elements = elements;

    crecord_bitfield_describe(rc, elements);
}

static int cparse_record(lua_State *L, struct ctype *ct, bool is_union)
{
    bool named = false;
    bool packed = false;
    int tok = yylex();

    ct->type = CTYPE_RECORD;

    tok = cparse_packed_attribute(L, tok, &packed);

    if (cparse_check_tok(L, tok) == TOK_NAME) {
        named = true;
        lua_pushstring(L, yyget_text());
        tok = yylex();
    }

    tok = cparse_packed_attribute(L, tok, &packed);

    if (cparse_check_tok(L, tok) == '{') {
        struct crecord_field *fields[MAX_RECORD_FIELDS];
        size_t offsets[MAX_RECORD_FIELDS];
        ffi_type *fts[MAX_RECORD_FIELDS];
        ffi_type **elements, **slots;
        ffi_type *arrays;
        size_t nfield = 0, narray = 0, nslot = 0;
        int i, j, nelement, nzero = 0, next_tok;
        bool bitfields = false;

        if (named) {
            lua_rawgetp(L, LUA_REGISTRYINDEX, &crecord_registry);
            lua_pushvalue(L, -2);
            lua_gettable(L, -2);

            if (!lua_isnil(L, -1))
                return luaL_error(L, "%d:redefinition of symbol '%s'", yyget_lineno(), lua_tostring(L, -3));
            lua_pop(L, 1);
        }

        nfield = cparse_record_field(L, fields);
        next_tok = cparse_packed_attribute(L, yylex(), &packed);

        /* An array field is described to libffi as a struct of its elements, which takes a type and a slot per element beside the record. */
        for (i = 0; i < nfield; i++) {
            if (fields[i]->bit_width)
                bitfields = true;

            if (fields[i]->ct->type != CTYPE_ARRAY)
                continue;

            if (ctype_is_zero_array(fields[i]->ct)) {
                nzero++;
                continue;
            }

            narray++;
            nslot += fields[i]->ct->array->size + 1;
        }

        if (bitfields && packed)
            return luaL_error(L, "The packed record on line %d holds bitfields, which only a record that is not packed lays out.", yyget_lineno());

        if (is_union)
            nelement = 2;
        else if (bitfields)
            nelement = MAX_BITFIELD_ELEMENTS + 1;
        else
            nelement = (int)nfield + 1 - nzero;

        ct->rc = (struct crecord *)cmem_alloc(L, sizeof(struct crecord)
                        + sizeof(struct crecord_field *) * nfield
                        + sizeof(ffi_type *) * nelement
                        + sizeof(ffi_type) * narray
                        + sizeof(ffi_type *) * nslot);

        ct->rc->mt_ref = LUA_REFNIL;

        memcpy(ct->rc->fields, fields, sizeof(struct crecord_field *) * nfield);

        if (named) {
            lua_pushvalue(L, -2);
            lua_pushlightuserdata(L, ct->rc);
            lua_settable(L, -3);
            lua_pop(L, 2);
        } else {
            ct->rc->anonymous = true;
        }

        ct->rc->is_union = is_union;
        ct->rc->nfield = nfield;
        ct->rc->packed = packed;

        elements = (ffi_type **)&ct->rc->fields[nfield];
        arrays = (ffi_type *)&elements[nelement];
        slots = (ffi_type **)&arrays[narray];

        for (i = 0; i < nfield; i++) {
            struct ctype *fct = fields[i]->ct;

            if (ctype_is_zero_array(fct)) {
                fts[i] = NULL;
            } else if (fct->type == CTYPE_ARRAY) {
                fts[i] = carray_describe(fct->array, arrays++, slots);
                slots += fct->array->size + 1;
            } else {
                fts[i] = ctype_ft(fct);
            }
        }

        if (is_union) {
            for (i = 0; i < nfield; i++) {
                if (fts[i] && (!elements[0] || fts[i]->size > elements[0]->size))
                    elements[0] = fts[i];
            }

            if (!elements[0])
                nelement--;
        } else if (!bitfields) {
            for (i = 0, j = 0; i < nfield; i++) {
                if (fts[i])
                    elements[j++] = fts[i];
            }
        }

        ct->rc->ft.type = FFI_TYPE_STRUCT;
        ct->rc->ft.elements = elements;

        if (packed) {
            cparse_record_packed_layout(ct->rc);
        } else if (bitfields && !is_union) {
            crecord_bitfield_layout(ct->rc, elements);
        } else {
            if (nelement > 1)
                init_ft_struct(L, &ct->rc->ft, elements, offsets);

            if (is_union && nelement > 1)
                crecord_union_layout(ct->rc, fts);

            if (!is_union) {
                for (i = 0, j = 0; i < nfield; i++) {
                    if (ctype_is_zero_array(fields[i]->ct)) {
                        if (i > 0)
                            ct->rc->fields[i]->offset = fields[i - 1]->offset + ctype_sizeof(fields[i - 1]->ct);
                    } else {
                        ct->rc->fields[i]->offset = offsets[j++];
                    }
                }
            }
        }

        return next_tok;
    } else {
        if (!named)
            return cparse_expected_error(L, tok, "identifier");

        lua_rawgetp(L, LUA_REGISTRYINDEX, &crecord_registry);
        lua_pushvalue(L, -2);
        lua_gettable(L, -2);

        if (lua_isnil(L, -1))
            return luaL_error(L, "%d:undeclared of symbol '%s", yyget_lineno(), lua_tostring(L, -3));

        ct->rc = (struct crecord *)lua_topointer(L, -1);
        lua_pop(L, 3);
    }

    return tok;
}

/* Gives an enum the integer type C gives it, an int, or an unsigned int when a constant is above the largest int. */
static void cenum_type(struct ctype *ct, int type)
{
    ct->type = (uint8_t)type;
    ct->ft = type == CTYPE_UINT ? &ffi_type_uint : &ffi_type_sint;
}

/* Reads the value of an enum constant, a C integer with an optional minus sign. */
static int cparse_enum_value(lua_State *L, int tok, lua_Integer *value)
{
    bool negative = cparse_check_tok(L, tok) == '-';

    if (negative)
        tok = yylex();

    if (cparse_check_tok(L, tok) != TOK_INTEGER)
        return cparse_expected_error(L, tok, "integer");

    *value = cparse_integer_text(L);
    if (negative)
        *value = -*value;

    return yylex();
}

/* Parses an enum, declaring its constants when it has a body and answering it as the integer type it is. */
static int cparse_enum(lua_State *L, struct ctype *ct)
{
    lua_Integer value = -1, low = 0, high = 0;
    bool named = false;
    int tok = yylex();
    int type;

    if (cparse_check_tok(L, tok) == TOK_NAME) {
        named = true;
        lua_pushstring(L, yyget_text());
        tok = yylex();
    }

    lua_rawgetp(L, LUA_REGISTRYINDEX, &cenum_registry);

    if (cparse_check_tok(L, tok) != '{') {
        if (!named)
            return cparse_expected_error(L, tok, "identifier");

        lua_pushvalue(L, -2);
        if (lua_rawget(L, -2) != LUA_TNUMBER)
            return luaL_error(L, "The enum \"%s\" on line %d is not declared.", lua_tostring(L, -3), yyget_lineno());

        cenum_type(ct, (int)lua_tointeger(L, -1));
        lua_pop(L, 3);

        return tok;
    }

    if (named) {
        lua_pushvalue(L, -2);
        if (lua_rawget(L, -2) != LUA_TNIL)
            return luaL_error(L, "The enum \"%s\" on line %d is declared twice.", lua_tostring(L, -3), yyget_lineno());
        lua_pop(L, 1);
    }

    /* The constants are gathered apart and declared only once the whole enum parsed, so a refused enum declares none. */
    lua_rawgetp(L, LUA_REGISTRYINDEX, &cconst_registry);
    lua_newtable(L);

    while ((tok = yylex()) != '}') {
        if (cparse_check_tok(L, tok) != TOK_NAME)
            return cparse_expected_error(L, tok, "identifier");

        lua_pushstring(L, yyget_text());

        lua_pushvalue(L, -1);
        if (lua_rawget(L, -4) != LUA_TNIL)
            return luaL_error(L, "The constant \"%s\" on line %d is declared twice.", lua_tostring(L, -2), yyget_lineno());
        lua_pop(L, 1);

        lua_pushvalue(L, -1);
        if (lua_rawget(L, -3) != LUA_TNIL)
            return luaL_error(L, "The constant \"%s\" on line %d is declared twice.", lua_tostring(L, -2), yyget_lineno());
        lua_pop(L, 1);

        tok = yylex();

        if (cparse_check_tok(L, tok) == '=')
            tok = cparse_enum_value(L, yylex(), &value);
        else
            value++;

        if (value < INT_MIN || value > UINT_MAX)
            return luaL_error(L, "The constant \"%s\" on line %d holds a value that neither an \"int\" nor an \"unsigned int\" holds.", lua_tostring(L, -1), yyget_lineno());

        low = value < low ? value : low;
        high = value > high ? value : high;

        lua_pushinteger(L, value);
        lua_rawset(L, -3);

        if (cparse_check_tok(L, tok) == '}')
            break;

        if (cparse_check_tok(L, tok) != ',')
            return cparse_expected_error(L, tok, ",");
    }

    if (low < 0 && high > INT_MAX && named)
        return luaL_error(L, "The enum \"%s\" on line %d holds values that neither an \"int\" nor an \"unsigned int\" holds whole.", lua_tostring(L, -4), yyget_lineno());

    if (low < 0 && high > INT_MAX)
        return luaL_error(L, "The enum on line %d holds values that neither an \"int\" nor an \"unsigned int\" holds whole.", yyget_lineno());

    lua_pushnil(L);
    while (lua_next(L, -2)) {
        lua_pushvalue(L, -2);
        lua_insert(L, -2);
        lua_rawset(L, -5);
    }
    lua_pop(L, 2);

    type = high > INT_MAX ? CTYPE_UINT : CTYPE_INT;

    if (named) {
        lua_pushvalue(L, -2);
        lua_pushinteger(L, type);
        lua_rawset(L, -3);
    }

    lua_pop(L, named ? 2 : 1);

    cenum_type(ct, type);

    return yylex();
}

/* Gives a signed type its own kind and an unsigned one the kind after it, so the two are never taken for each other. */
static int cparse_squals(int type, int squals, struct ctype *ct, ffi_type *s, ffi_type *u)
{
    ct->type = (uint8_t)(squals == TOK_SIGNED ? type : type + 1);
    ct->ft = squals == TOK_SIGNED ? s : u;
    return yylex();
}

static int cparse_basetype(lua_State *L, int tok, struct ctype *ct)
{
    ct->is_const = false;

    if (cparse_check_tok(L, tok) == TOK_CONST) {
        ct->is_const = true;
        tok = yylex();
    }

    if (cparse_check_tok(L, tok) == TOK_SIGNED || cparse_check_tok(L, tok) == TOK_UNSIGNED) {
        int squals = tok;

        tok = yylex();

        switch (tok) {
        case TOK_CHAR:
            tok = cparse_squals(CTYPE_CHAR, squals, ct, &ffi_type_schar, &ffi_type_uchar);
            break;
        case TOK_SHORT:
            tok = cparse_squals(CTYPE_SHORT, squals, ct, &ffi_type_sshort, &ffi_type_ushort);
            break;
        case TOK_INT:
            tok = cparse_squals(CTYPE_INT, squals, ct, &ffi_type_sint, &ffi_type_uint);
            break;
        case TOK_LONG:
            tok = cparse_squals(CTYPE_LONG, squals, ct, &ffi_type_slong, &ffi_type_ulong);
            break;
        default:
            ct->type = squals == TOK_SIGNED ? CTYPE_INT : CTYPE_UINT;
            ct->ft = squals == TOK_SIGNED ? &ffi_type_sint : &ffi_type_uint;
            break;
        }
    } else if (cparse_check_tok(L, tok) == TOK_STRUCT || cparse_check_tok(L, tok) == TOK_UNION) {
        tok = cparse_record(L, ct, cparse_check_tok(L, tok) == TOK_UNION);
    } else if (cparse_check_tok(L, tok) == TOK_ENUM) {
        tok = cparse_enum(L, ct);
    } else {
#define INIT_TYPE(t1, t2) \
            ct->type = t1; \
            ct->ft = &t2; \
            break

#define INIT_TYPE_T(t1, t2, s) \
            ct->type = t1; \
            ct->ft = ffi_type_of(sizeof(t2), s); \
            break

        switch (tok) {
        case TOK_VOID:
            INIT_TYPE(CTYPE_VOID, ffi_type_void);
        case TOK_BOOL:
            INIT_TYPE(CTYPE_BOOL, ffi_type_sint8);
        case TOK_CHAR:
            INIT_TYPE(CTYPE_CHAR, ffi_type_schar);
        case TOK_SHORT:
            INIT_TYPE(CTYPE_SHORT, ffi_type_sshort);
        case TOK_INT:
            INIT_TYPE(CTYPE_INT, ffi_type_sint);
        case TOK_LONG:
            INIT_TYPE(CTYPE_LONG, ffi_type_slong);
        case TOK_FLOAT:
            INIT_TYPE(CTYPE_FLOAT, ffi_type_float);
        case TOK_DOUBLE:
            INIT_TYPE(CTYPE_DOUBLE, ffi_type_double);
        case TOK_INT8_T:
            INIT_TYPE_T(CTYPE_INT8_T, int8_t, true);
        case TOK_INT16_T:
            INIT_TYPE_T(CTYPE_INT16_T, int16_t, true);
        case TOK_INT32_T:
            INIT_TYPE_T(CTYPE_INT32_T, int32_t, true);
        case TOK_INT64_T:
            INIT_TYPE_T(CTYPE_INT64_T, int64_t, true);
        case TOK_UINT8_T:
            INIT_TYPE_T(CTYPE_UINT8_T, uint8_t, false);
        case TOK_UINT16_T:
            INIT_TYPE_T(CTYPE_UINT16_T, uint16_t, false);
        case TOK_UINT32_T:
            INIT_TYPE_T(CTYPE_UINT32_T, uint32_t, false);
        case TOK_UINT64_T:
            INIT_TYPE_T(CTYPE_UINT64_T, uint64_t, false);
        case TOK_OFF_T:
            INIT_TYPE_T(CTYPE_OFF_T, off_t, true);
        case TOK_INO_T:
            INIT_TYPE_T(CTYPE_INO_T, ino_t, false);
        case TOK_DEV_T:
            INIT_TYPE_T(CTYPE_DEV_T, dev_t, false);
        case TOK_GID_T:
            INIT_TYPE_T(CTYPE_GID_T, gid_t, false);
        case TOK_MODE_T:
            INIT_TYPE_T(CTYPE_MODE_T, mode_t, false);
        case TOK_NLINK_T:
            INIT_TYPE_T(CTYPE_NLINK_T, nlink_t, false);
        case TOK_UID_T:
            INIT_TYPE_T(CTYPE_UID_T, uid_t, false);
        case TOK_PID_T:
            INIT_TYPE_T(CTYPE_PID_T, pid_t, true);
        case TOK_SIZE_T:
            INIT_TYPE_T(CTYPE_SIZE_T, size_t, false);
        case TOK_SSIZE_T:
            INIT_TYPE_T(CTYPE_SSIZE_T, ssize_t, true);
        case TOK_USECONDS_T:
            INIT_TYPE_T(CTYPE_USECONDS_T, useconds_t, false);
        case TOK_SUSECONDS_T:
            INIT_TYPE_T(CTYPE_SUSECONDS_T, suseconds_t, true);
        case TOK_BLKSIZE_T:
            INIT_TYPE_T(CTYPE_BLKSIZE_T, blksize_t, true);
        case TOK_BLKCNT_T:
            INIT_TYPE_T(CTYPE_BLKCNT_T, blkcnt_t, true);
        case TOK_TIME_T:
            INIT_TYPE_T(CTYPE_TIME_T, time_t, true);
        case TOK_INTPTR_T:
            INIT_TYPE_T(CTYPE_INTPTR_T, intptr_t, true);
        case TOK_UINTPTR_T:
            INIT_TYPE_T(CTYPE_UINTPTR_T, uintptr_t, false);
        case TOK_NAME:
            lua_rawgetp(L, LUA_REGISTRYINDEX, &ctdef_registry);
            lua_getfield(L, -1, yyget_text());
            if (!lua_isnil(L, -1)) {
                *ct = *(struct ctype *)lua_touserdata(L, -1);
                lua_pop(L, 2);
                break;
            }
        default:
            return luaL_error(L, "%d:unknown type name '%s'", yyget_lineno(), yyget_text());
        }
        tok = yylex();
#undef INIT_TYPE
#undef INIT_TYPE_T
    }

    if (cparse_check_tok(L, tok) == TOK_INT) {
        switch (ct->type) {
        case CTYPE_LONG:
        case CTYPE_ULONG:
            tok = yylex();
            break;
        }
    } else if (cparse_check_tok(L, tok) == TOK_LONG) {
        switch (ct->type) {
        case CTYPE_INT:
            ct->type = CTYPE_LONG;
            ct->ft = &ffi_type_slong;
            tok = yylex();
            break;
        case CTYPE_UINT:
            ct->type = CTYPE_ULONG;
            ct->ft = &ffi_type_ulong;
            tok = yylex();
            break;
        case CTYPE_LONG:
            ct->type = CTYPE_LONGLONG;
            ct->ft = &ffi_type_sint64;
            tok = yylex();
            break;
        case CTYPE_ULONG:
            ct->type = CTYPE_ULONGLONG;
            ct->ft = &ffi_type_uint64;
            tok = yylex();
            break;
        }
    }

    if (cparse_check_tok(L, tok) == TOK_CONST) {
        ct->is_const = true;
        tok = yylex();
    }

    return tok;
}

/* Builds a function type, sharing the one an earlier declaration or cast built when they are equal, so repeating a cast keeps no new memory. */
static void cparse_build_func_type(lua_State *L, struct ctype *rtype,
        struct ctype *args, int narg, bool va, struct ctype *out)
{
    struct cfunc *func;
    int i;

    /* The candidate stays on the stack while its parts are looked up, so the collector keeps it until it is shared or dropped. */
    func = (struct cfunc *)lua_newuserdata(L, sizeof(struct cfunc) + sizeof(struct ctype *) * narg);
    memset(func, 0, sizeof(struct cfunc) + sizeof(struct ctype *) * narg);

    func->narg = narg;
    func->va = va;

    for (i = 0; i < narg; i++)
        func->args[i] = ctype_lookup(L, &args[i], false);

    func->rtype = ctype_lookup(L, rtype, false);

    out->type = CTYPE_FUNC;
    out->is_const = false;

    lua_rawgetp(L, LUA_REGISTRYINDEX, &cfunctype_registry);
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        struct cfunc *known = (struct cfunc *)lua_touserdata(L, -1);

        if (cfunc_equal(known, func)) {
            out->func = known;
            lua_pop(L, 4);
            return;
        }
        lua_pop(L, 1);
    }

    lua_pushvalue(L, -2);
    lua_rawsetp(L, -2, func);
    lua_pop(L, 2);

    out->func = func;
}

static int cparse_function_args(lua_State *L, int tok, struct ctype *args,
        int *narg, bool *va);

static int cparse_function_arg(lua_State *L, int tok, struct ctype *ct, char **name)
{
    bool flexible = true;
    int array_size;

    if (name)
        *name = NULL;

    if (cparse_check_tok(L, tok) == '(') {
        struct ctype fargs[MAX_FUNC_ARGS] = {};
        struct ctype fct;
        int fnarg = 0;
        int ptr_depth = 0;
        bool ptr_const = false;
        bool fva = false;

        tok = yylex();

        while (cparse_check_tok(L, tok) == '*') {
            ptr_depth++;
            tok = yylex();
        }

        if (ptr_depth == 0)
            return cparse_expected_error(L, tok, "*");

        if (cparse_check_tok(L, tok) == TOK_CONST) {
            ptr_const = true;
            tok = yylex();
        }

        if (cparse_check_tok(L, tok) == TOK_NAME) {
            if (name) {
                *name = cmem_strdup(L, yyget_text());
            }
            tok = yylex();
        }

        if (cparse_check_tok(L, tok) != ')')
            return cparse_expected_error(L, tok, ")");

        tok = yylex();
        if (cparse_check_tok(L, tok) != '(')
            return cparse_expected_error(L, tok, "(");

        tok = cparse_function_args(L, tok, fargs, &fnarg, &fva);

        cparse_build_func_type(L, ct, fargs, fnarg, fva, &fct);
        *ct = fct;

        while (ptr_depth-- > 0)
            ctype_to_ptr(L, ct);

        if (ptr_const && ct->type == CTYPE_PTR)
            ct->is_const = true;

        tok = yylex();

        return tok;
    }

    tok = cparse_pointer(L, tok, ct);

    if (cparse_check_tok(L, tok) == TOK_NAME) {
        if (name) {
            *name = cmem_strdup(L, yyget_text());
        }
        tok = yylex();
    }

    tok = cparse_array(L, tok, &flexible, &array_size);

    if (flexible || array_size >= 0)
        ctype_to_ptr(L, ct);

    return tok;
}

static int cparse_function_args(lua_State *L, int tok, struct ctype *args,
        int *narg, bool *va)
{
    *narg = 0;
    *va = false;

    while (true) {
        tok = yylex();
        if (cparse_check_tok(L, tok) == ')')
            break;

        if (*narg >= MAX_FUNC_ARGS)
            return luaL_error(L, "%d:too many arguments", yyget_lineno());

        if (cparse_check_tok(L, tok) == TOK_STRUCT || cparse_check_tok(L, tok) == TOK_UNION) {
            tok = cparse_record(L, &args[*narg], cparse_check_tok(L, tok) == TOK_UNION);
        } else if (cparse_check_tok(L, tok) == TOK_VAL) {
            tok = yylex();
            if (cparse_check_tok(L, tok) != ')')
                return cparse_expected_error(L, tok, ")");
            *va = true;
            break;
        } else {
            tok = cparse_basetype(L, tok, &args[*narg]);
        }

        tok = cparse_function_arg(L, tok, &args[*narg], NULL);

        if (cparse_check_tok(L, tok) == ')') {
            if (args[*narg].type == CTYPE_VOID && *narg == 0)
                break;

            check_void_forbidden(L, &args[*narg], tok);
            (*narg)++;
            break;
        }

        check_void_forbidden(L, &args[*narg], tok);
        (*narg)++;

        if (cparse_check_tok(L, tok) != ',')
            return cparse_expected_error(L, tok, ",");
    }

    return tok;
}

static int cparse_function(lua_State *L, int tok, struct ctype *rtype)
{
    struct ctype args[MAX_FUNC_ARGS] = {};
    struct ctype fct;
    int narg = 0;
    bool va = false;

    tok = cparse_pointer(L, tok, rtype);

    if (cparse_check_tok(L, tok) != TOK_NAME)
        return cparse_expected_error(L, tok, "identifier");

    lua_pushstring(L, yyget_text());

    lua_rawgetp(L, LUA_REGISTRYINDEX, &cfunc_registry);
    lua_pushvalue(L, -2);
    lua_gettable(L, -2);

    if (!lua_isnil(L, -1))
        return luaL_error(L, "%d:redefinition of function '%s'", yyget_lineno(), lua_tostring(L, -3));

    lua_pop(L, 1);

    tok = yylex();

    if (cparse_check_tok(L, tok) != '(')
        return cparse_expected_error(L, tok, "(");

    tok = cparse_function_args(L, tok, args, &narg, &va);

    tok = yylex();
    if (cparse_check_tok(L, tok) != ';')
        return cparse_expected_error(L, tok, ";");

    cparse_build_func_type(L, rtype, args, narg, va, &fct);

    lua_pushvalue(L, -2);
    lua_pushlightuserdata(L, fct.func);
    lua_settable(L, -3);
    lua_pop(L, 2);

    return 0;
}

/* Answers the line of the Lua code that called the binding, which numbers the declarations it reads, or the first line when no Lua code called it. */
static int caller_line(lua_State *L)
{
    lua_Debug ar;

    if (!lua_getstack(L, 1, &ar) || !lua_getinfo(L, "l", &ar) || ar.currentline < 1)
        return 1;

    return ar.currentline;
}

static int lua_ffi_cdef(lua_State *L)
{
    size_t len;
    const char *str = luaL_checklstring(L, 1, &len);
    int tok;

    /* A declaration that failed to parse raised before it released the scanner, so its buffer goes before the next one starts. */
    yylex_destroy();
    yy_scan_bytes(str, len);
    yyset_lineno(caller_line(L));

    while ((tok = yylex())) {
        bool tdef = false;
        struct ctype ct;

        if (cparse_check_tok(L, tok) == ';')
            continue;

        if (cparse_check_tok(L, tok) == TOK_TYPEDEF) {
            tdef = true;
            tok = yylex();
        }

        tok = cparse_basetype(L, tok, &ct);

        if (tdef) {
            char *name = NULL;

            if (cparse_check_tok(L, tok) == '(') {
                tok = cparse_function_arg(L, tok, &ct, &name);
            } else {
                tok = cparse_pointer(L, tok, &ct);

                if (cparse_check_tok(L, tok) != TOK_NAME)
                    return cparse_expected_error(L, tok, "identifier");

                name = cmem_strdup(L, yyget_text());
                tok = yylex();
            }

            if (!name)
                return cparse_expected_error(L, tok, "identifier");

            lua_rawgetp(L, LUA_REGISTRYINDEX, &ctdef_registry);
            lua_getfield(L, -1, name);

            if (!lua_isnil(L, -1)) {
                return luaL_error(L, "%d:redefinition of symbol '%s'", yyget_lineno(), name);
            }

            lua_pop(L, 1);
            ctype_lookup(L, &ct, true);
            lua_setfield(L, -2, name);
            lua_pop(L, 1);

            cmem_release(L, name);

            if (cparse_check_tok(L, tok) != ';')
                return cparse_expected_error(L, tok, ";");

            continue;
        }

        if (cparse_check_tok(L, tok) == ';')
            continue;

        cparse_function(L, tok, &ct);
    }

    yylex_destroy();

    return 0;
}

static int load_lib(lua_State *L, const char *path, bool global)
{
    struct clib *lib;
    void *h;

    if (path) {
        h = dlopen(path, RTLD_LAZY | (global ? RTLD_GLOBAL : RTLD_LOCAL));
        if (!h) {
            const char *err = dlerror();
            return luaL_error(L, "The library \"%s\" could not be loaded: %s", path, err ? err : "The system gave no reason.");
        }
    } else {
        h = RTLD_DEFAULT;
    }

    lib = (struct clib *)lua_newuserdata(L, sizeof(struct clib));
    lib->h = h;

    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, lib);

    luaL_getmetatable(L, CLIB_MT);
    lua_setmetatable(L, -2);

    if (global) {
        lua_rawgetp(L, LUA_REGISTRYINDEX, &clib_registry);
        lua_pushvalue(L, -2);
        lua_rawsetp(L, -2, lib);
        lua_pop(L, 1);
    }

    return 1;
}

static int lua_ffi_load(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    bool global = lua_toboolean(L, 2);
    return load_lib(L, path, global);
}

static struct ctype *lua_check_ct(lua_State *L, bool *va, bool keep)
{
    struct cdata *cd;
    struct ctype *ct;

    if (lua_type(L, 1) == LUA_TSTRING) {
        size_t len;
        const char *str = luaL_checklstring(L, 1, &len);
        bool flexible = false;
        struct ctype match;
        int array_size;
        int tok;

        /* A type that failed to parse raised before it released the scanner, so its buffer goes before the next one starts. */
        yylex_destroy();
        yy_scan_bytes(str, len);

        yyset_lineno(caller_line(L));

        if (va)
            flexible = *va;

        tok = cparse_basetype(L, yylex(), &match);

        if (cparse_check_tok(L, tok) == '(') {
            tok = cparse_function_arg(L, tok, &match, NULL);
            flexible = false;
        } else {
            tok = cparse_pointer(L, tok, &match);
            tok = cparse_array(L, tok, &flexible, &array_size);

            if (flexible || array_size >= 0) {
                if (flexible) {
                    array_size = luaL_checkinteger(L, 2);
                    luaL_argcheck(L, 2, array_size > 0, "array size must great than 0");
                }

                cparse_new_array(L, array_size, &match);
            }
        }

        if (tok)
            luaL_error(L, "%d:unexpected '%s'", yyget_lineno(), yyget_text());

        if (va)
            *va = flexible;

        yylex_destroy();

        return ctype_lookup(L, &match, keep);
    }

    if (va)
        *va = false;

    ct = (struct ctype *)luaL_testudata(L, 1, CTYPE_MT);
    if (ct)
        return ct;

    cd = (struct cdata *)luaL_testudata(L, 1, CDATA_MT);
    if (cd) {
        if (keep) {
            lua_rawgetp(L, LUA_REGISTRYINDEX, &ctype_registry);
            lua_pushlightuserdata(L, cd->ct);
            lua_gettable(L, -2);
            lua_remove(L, -2);
        }
        return cd->ct;
    }

    lua_type_error(L, 1, "C type");

    return NULL;
}

static int lua_ffi_new(lua_State *L)
{
    bool va = true;
    struct ctype *ct = lua_check_ct(L, &va, false);

    if (ct->type == CTYPE_FUNC || ct->type == CTYPE_VOID)
        return luaL_error(L, "invalid C type");

    struct cdata *cd = cdata_new(L, ct, NULL);
    int idx = va ? 3 : 2;
    int ninit;

    ninit = lua_gettop(L) - idx;

    if (ninit == 1) {
        cdata_from_lua(L, cd->ct, cdata_ptr(cd), idx, false);
    } else if (ninit != 0) {
        __ctype_tostring(L, ct);
        return luaL_error(L, "too many initializers for '%s'", lua_tostring(L, -1));
    }

    return 1;
}

static int lua_ffi_cast(lua_State *L)
{
    struct ctype *ct = lua_check_ct(L, NULL, false);
    struct cdata *cd = cdata_new(L, ct, NULL);

    /* A Lua function becomes a callback, while an address of any kind is only reinterpreted. */
    if (ctype_ptr_to(ct, CTYPE_FUNC) && lua_type(L, 2) == LUA_TFUNCTION) {
        cd->cb = ccallback_new(L, ct, 2);
        cdata_ptr_set(cd, cd->cb->code);
    } else {
        cdata_from_lua(L, ct, cdata_ptr(cd), 2, true);
    }

    return 1;
}

static int lua_ffi_metatype(lua_State *L)
{
    struct ctype *ct = (struct ctype *)luaL_checkudata(L, 1, CTYPE_MT);
    struct crecord *rc = ct->rc;
    uint8_t mflags = 0;

    luaL_argcheck(L, ct->type == CTYPE_RECORD, 1, "invalid C type");

    luaL_checktype(L, 2, LUA_TTABLE);

#define FIELD_CHECK(name, flag) { \
            lua_getfield(L, 2, "__" name); \
            if (!lua_isnil(L, -1)) { \
                mflags |= METATYPE_FLAG_##flag; \
            } \
            lua_pop(L, 1); \
        }

    FIELD_CHECK("index", INDEX);
    FIELD_CHECK("tostring", TOSTRING);

#undef FIELD_CHECK

    rc->mflags = mflags;

    if (rc->mt_ref != LUA_REFNIL)
        luaL_unref(L, LUA_REGISTRYINDEX, rc->mt_ref);

    lua_pushvalue(L, 2);
    rc->mt_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    lua_settop(L, 1);

    return 1;
}

static int lua_ffi_typeof(lua_State *L)
{
    lua_check_ct(L, NULL, true);
    return 1;
}

static int lua_ffi_addressof(lua_State *L)
{
    struct cdata *cd = (struct cdata *)luaL_checkudata(L, 1, CDATA_MT);
    struct ctype match = {
        .type = CTYPE_PTR,
        .ptr = cd->ct
    };
    struct ctype *ct = ctype_lookup(L, &match, false);
    cdata_ptr_set(cdata_new(L, ct, NULL), cdata_ptr(cd));
    return 1;
}

static int lua_ffi_gc(lua_State *L)
{
    struct cdata *cd = (struct cdata *)luaL_checkudata(L, 1, CDATA_MT);

    if (lua_isnil(L, 2)) {
        if (cd->gc_ref != LUA_REFNIL) {
            luaL_unref(L, LUA_REGISTRYINDEX, cd->gc_ref);
            cd->gc_ref = LUA_REFNIL;
        }
    } else {
        lua_pushvalue(L, 2);
        cd->gc_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    lua_settop(L, 1);
    return 1;
}

static int lua_ffi_sizeof(lua_State *L)
{
    struct ctype *ct = lua_check_ct(L, NULL, false);
    lua_pushinteger(L, ctype_sizeof(ct));
    return 1;
}

static int lua_ffi_offsetof(lua_State *L)
{
    struct ctype *ct = lua_check_ct(L, NULL, false);
    char const *name = luaL_checkstring(L, 2);
    struct crecord_field **fields;
    int i;

    if (ct->type != CTYPE_RECORD)
        return 0;

    fields = ct->rc->fields;

    /* A bitfield answers the byte of its unit, the bit it starts at inside the unit and its width. */
    for (i = 0; i < ct->rc->nfield; i++) {
        if (strcmp(fields[i]->name, name))
            continue;

        lua_pushinteger(L, fields[i]->offset);
        if (!fields[i]->bit_width)
            return 1;

        lua_pushinteger(L, fields[i]->bit_offset);
        lua_pushinteger(L, fields[i]->bit_width);
        return 3;
    }

    return 0;
}

static int lua_ffi_istype(lua_State *L)
{
    struct ctype *ct = lua_check_ct(L, NULL, false);
    struct cdata *cd = (struct cdata *)luaL_checkudata(L, 2, CDATA_MT);
    lua_pushboolean(L, ct == cd->ct);
    return 1;
}

static int lua_ffi_tonumber(lua_State *L)
{
    struct cdata *cd = (struct cdata *)luaL_checkudata(L, 1, CDATA_MT);
    struct ctype *ct = cd->ct;

    if (ct->type < CTYPE_VOID)
        return cdata_to_lua(L, ct, cdata_ptr(cd));
    lua_pushnil(L);
    return 1;
}

static int lua_ffi_string(lua_State *L)
{
    struct cdata *cd = (struct cdata *)luaL_checkudata(L, 1, CDATA_MT);
    struct carray *array = NULL;
    struct ctype *ct = cd->ct;
    const char *ptr = (const char *)(ct->type == CTYPE_PTR ? cdata_ptr_ptr(cd) : cdata_ptr(cd));
    size_t len;

    if (lua_gettop(L) > 1) {
        len = luaL_checkinteger(L, 2);

        switch (ct->type) {
        case CTYPE_PTR:
        case CTYPE_ARRAY:
        case CTYPE_RECORD:
            lua_pushlstring(L, ptr, len);
            return 1;
        default:
            goto converr;
        }
    }

    switch (ct->type) {
    case CTYPE_PTR:
        ct = ct->ptr;
        break;
    case CTYPE_ARRAY:
        array = ct->array;
        ct = array->ct;
        break;
    default:
        goto converr;
    }

    if (ct->type != CTYPE_CHAR)
        goto converr;

    if (array && array->size) {
        char *p = (char *)memchr(ptr, '\0', array->ft.size);
        len = p ? p - ptr : array->ft.size;
        lua_pushlstring(L, ptr, len);
    } else {
        lua_pushstring(L, ptr);
    }

    return 1;

converr:
    __cdata_tostring(L, cd);
    lua_pushfstring(L, "cannot convert '%s' to 'string'", lua_tostring(L, -1));
    luaL_argcheck(L, false, 1, lua_tostring(L, -1));
    return 0;
}

static int lua_ffi_copy(lua_State *L)
{
    struct cdata *cd = (struct cdata *)luaL_checkudata(L, 1, CDATA_MT);
    void *dst = cdata_ptr(cd);
    const void *src;
    size_t len;

    if (lua_gettop(L) < 3) {
        src = luaL_checklstring(L, 2, &len);
        memcpy(dst, src, len);
        ((char *)dst)[len++] = '\0';
    } else {
        lua_Integer requested = luaL_checkinteger(L, 3);
        if (requested < 0)
            return luaL_error(L, "copy length must not be negative");
        len = (size_t)requested;

        if (lua_type(L, 2) == LUA_TSTRING)
            src = lua_tostring(L, 2);
        else
            src = cdata_ptr((struct cdata *)luaL_checkudata(L, 2, CDATA_MT));

        memcpy(dst, src, len);
    }

    lua_pushinteger(L, len);

    return 1;
}

static int lua_ffi_fill(lua_State *L)
{
    struct cdata *cd = (struct cdata *)luaL_checkudata(L, 1, CDATA_MT);
    lua_Integer requested = luaL_checkinteger(L, 2);
    int c = luaL_optinteger(L, 3, 0);

    if (requested < 0)
        return luaL_error(L, "fill length must not be negative");

    memset(cdata_ptr(cd), c, (size_t)requested);

    return 0;
}

static int lua_ffi_errno(lua_State *L)
{
    int cur = errno;

    if (lua_gettop(L) > 0)
        errno = luaL_checkinteger(L, 1);

    lua_pushinteger(L, cur);
    return 1;
}

static const luaL_Reg methods[] = {
    {"cdef", lua_ffi_cdef},
    {"load", lua_ffi_load},

    {"new", lua_ffi_new},
    {"cast", lua_ffi_cast},
    {"metatype", lua_ffi_metatype},
    {"typeof", lua_ffi_typeof},
    {"addressof", lua_ffi_addressof},
    {"gc", lua_ffi_gc},

    {"sizeof", lua_ffi_sizeof},
    {"offsetof", lua_ffi_offsetof},
    {"istype", lua_ffi_istype},

    {"tonumber", lua_ffi_tonumber},
    {"string", lua_ffi_string},
    {"copy", lua_ffi_copy},
    {"fill", lua_ffi_fill},
    {"errno", lua_ffi_errno},

    {NULL, NULL}
};

static void createmetatable(lua_State *L, const char *name, const struct luaL_Reg regs[])
{
    luaL_newmetatable(L, name);
    luaL_setfuncs(L, regs, 0);
    lua_pop(L, 1);
}

static void create_nullptr(lua_State *L)
{
    struct ctype match = {
        .type = CTYPE_VOID,
        .ft = &ffi_type_void
    };
    struct ctype *ct;

    ctype_to_ptr(L, &match);

    ct = ctype_lookup(L, &match, false);

    cdata_ptr_set(cdata_new(L, ct, NULL), NULL);
}

int varn_ffi_open(lua_State *L, const struct varn_ffi_host *host)
{
    struct cstate *state = (struct cstate *)lua_newuserdata(L, sizeof(struct cstate));

    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
    state->main = lua_tothread(L, -1);
    state->calling = NULL;
    lua_pop(L, 1);

    state->host = host;
    state->ncall = 0;
    state->failure_ref = LUA_NOREF;
    lua_rawsetp(L, LUA_REGISTRYINDEX, &cstate_registry);

    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &crecord_registry);

    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &carray_registry);

    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &cfunc_registry);

    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &ctype_registry);

    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &ctdef_registry);

    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &clib_registry);

    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &cenum_registry);

    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &cconst_registry);

    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &cmem_registry);

    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &cfunctype_registry);

    createmetatable(L, CDATA_MT, cdata_methods);
    createmetatable(L, CTYPE_MT, ctype_methods);
    createmetatable(L, CLIB_MT, clib_methods);

    luaL_newlib(L, methods);

    lua_pushstring(L, LUA_FFI_VERSION_STRING);
    lua_setfield(L, -2, "VERSION");

    create_nullptr(L);
    lua_setfield(L, -2, "nullptr");

    load_lib(L, NULL, true);
    lua_setfield(L, -2, "C");

    return 1;
}
