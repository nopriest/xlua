/*
** $Id: lstate.h,v 2.82.1.1 2013/04/12 18:48:47 roberto Exp $
** Global State
** See Copyright Notice in lua.h
*/

#ifndef lstate_h
#define lstate_h

#include "lua.h"

#include "lobject.h"
#include "ltm.h"
#include "lzio.h"


/*

** Some notes about garbage-collected objects:  All objects in Lua must
** be kept somehow accessible until being freed.
**
** Lua keeps most objects linked in list g->allgc. The link uses field
** 'next' of the CommonHeader.
**
** Strings are kept in several lists headed by the array g->strt.hash.
**
** Open upvalues are not subject to independent garbage collection. They
** are collected together with their respective threads. Lua keeps a
** double-linked list with all open upvalues (g->uvhead) so that it can
** mark objects referred by them. (They are always gray, so they must
** be remarked in the atomic step. Usually their contents would be marked
** when traversing the respective threads, but the thread may already be
** dead, while the upvalue is still accessible through closures.)
**
** Objects with finalizers are kept in the list g->finobj.
**
** The list g->tobefnz links all objects being finalized.

*/

/* jmp状态在ldo.c中定义 */
struct lua_longjmp;  /* defined in ldo.c */



/* extra stack space to handle TM calls and some other extras */
/* 扩展栈数量 */
#define EXTRA_STACK   5

/* 基础栈大小,2倍的最小栈大小 */
#define BASIC_STACK_SIZE        (2*LUA_MINSTACK)


/* kinds of Garbage Collection */
/* 垃圾回收的类型 */
#define KGC_NORMAL	0
#define KGC_EMERGENCY	1	/* gc was forced by an allocation failure */
#define KGC_GEN		2	/* generational collection */

/* 字符串表 */
typedef struct stringtable {
  GCObject **hash;
	/* 元素的个数 */
  lu_int32 nuse;  /* number of elements */
  int size;
} stringtable;


/*
** information about a call
*/
/* 函数调用信息结构 */
typedef struct CallInfo {
	/* 要调用函数在栈中的索引 */
  StkId func;  /* function index in the stack */
	/* 这个函数的栈顶 */
  StkId	top;  /* top for this function */
	/* 双向链表 */
  struct CallInfo *previous, *next;  /* dynamic call link */
	/* 这个函数返回的返回值 */
  short nresults;  /* expected number of results from this function */
	/* 调用状态 */
  lu_byte callstatus;
  ptrdiff_t extra;
	/* 用于描述函数 */
  union {
		/* lua函数 */
    struct {  /* only for Lua functions */
			/* 函数的基地址 */
      StkId base;  /* base for this function */
			/* 指令队列 */
      const Instruction *savedpc;
    } l;
		/* C函数 */
    struct {  /* only for C functions */
      int ctx;  /* context info. in case of yields */
			/* 函数指针 */
      lua_CFunction k;  /* continuation in case of yields */
			/* 旧的错误处理函数指针 */
      ptrdiff_t old_errfunc;
			/* 旧的允许hook标记 */
      lu_byte old_allowhook;
			/* 状态 */
      lu_byte status;
    } c;
  } u;
} CallInfo;


/*
** Bits in CallInfo status
*/
/* 调用信息状态 */
/* 正在运行一个lua函数 */
#define CIST_LUA (1<<0)	/* call is running a Lua function */
/* 正在运行一个调试HOOK函数 */
#define CIST_HOOKED	(1<<1)	/* call is running a debug hook */
/* 重新进入上一个函数调用 */
#define CIST_REENTRY	(1<<2)	/* call is running on same invocation of
                                   luaV_execute of previous call */
/* 在挂起后重新进入 */
#define CIST_YIELDED	(1<<3)	/* call reentered after suspension */
/* 调用一个可继续的调用 */
#define CIST_YPCALL	(1<<4)	/* call is a yieldable protected call */
/* 错误状态调用 */
#define CIST_STAT	(1<<5)	/* call has an error status (pcall) */
/* 尾递归 */
#define CIST_TAIL	(1<<6)	/* call was tail called */
/* 最后的hook调用可继续 */
#define CIST_HOOKYIELD	(1<<7)	/* last hook called yielded */

/* 当前是lua函数 */
#define isLua(ci)	((ci)->callstatus & CIST_LUA)


/*
** `global state', shared by all threads of this state
*/
/* 全局状态 */
typedef struct global_State {
	/* 重新分配内存函数指针 */
  lua_Alloc frealloc;  /* function to reallocate memory */
	/* 虚拟机内存句柄 */
  void *ud;         /* auxiliary data to `frealloc' */
	/* 总给分配的内存数量 */
  lu_mem totalbytes;  /* number of bytes currently allocated - GCdebt */
	/* 已经分配的内存大小 */
  l_mem GCdebt;  /* bytes allocated not yet compensated by the collector */
  lu_mem GCmemtrav;  /* memory traversed by the GC */
  lu_mem GCestimate;  /* an estimate of the non-garbage memory in use */
	/* 哈希字符串表 */
  stringtable strt;  /* hash table for strings */
  TValue l_registry;
	/* 哈希算法随机种子 */
  unsigned int seed;  /* randomized seed for hashes */
  lu_byte currentwhite;
	/* 垃圾回收状态 */
  lu_byte gcstate;  /* state of garbage collector */
	/* 垃圾回收类型 */
  lu_byte gckind;  /* kind of GC running */
	/* 如果是true,表明垃圾回收正在运行 */
  lu_byte gcrunning;  /* true if GC is running */
  int sweepstrgc;  /* position of sweep in `strt' */
	/* 列出所有可回收对象 */
  GCObject *allgc;  /* list of all collectable objects */
	/* 列出所有可终结的可回收对象 */
  GCObject *finobj;  /* list of collectable objects with finalizers */
  GCObject **sweepgc;  /* current position of sweep in list 'allgc' */
  GCObject **sweepfin;  /* current position of sweep in list 'finobj' */
  GCObject *gray;  /* list of gray objects */
  GCObject *grayagain;  /* list of objects to be traversed atomically */
  GCObject *weak;  /* list of tables with weak values */
  GCObject *ephemeron;  /* list of ephemeron tables (weak keys) */
  GCObject *allweak;  /* list of all-weak tables */
  GCObject *tobefnz;  /* list of userdata to be GC */
  UpVal uvhead;  /* head of double-linked list of all open upvalues */
  Mbuffer buff;  /* temporary buffer for string concatenation */
  int gcpause;  /* size of pause between successive GCs */
  int gcmajorinc;  /* pause between major collections (only in gen. mode) */
  int gcstepmul;  /* GC `granularity' */
  lua_CFunction panic;  /* to be called in unprotected errors */
	/* 虚拟机主线程 */
  struct lua_State *mainthread;
  const lua_Number *version;  /* pointer to version number */
	/* 内存错误字符串描述 */
  TString *memerrmsg;  /* memory-error message */
	/* TM运算的名称 */
  TString *tmname[TM_N];  /* array with tag-method names */
	/* 每个基础类型都对应一个哈希表
	 * 这个字段就是类型哈希表
	 */
  struct Table *mt[LUA_NUMTAGS];  /* metatables for basic types */
} global_State;


/*
** `per thread' state
*/
/* 虚拟机状态实例子，针对于每条线程
 */
struct lua_State {
  CommonHeader;
	/* 状态码 */
  lu_byte status;
	/* 栈顶 */
  StkId top;  /* first free slot in the stack */
	/* 全局状态结构指针 */
  global_State *l_G;
	/* 当前调用函数的调用结构 */
  CallInfo *ci;  /* call info for current function */
	/* 最后一条指令指针 */
  const Instruction *oldpc;  /* last pc traced */
	/* 栈末尾 */
  StkId stack_last;  /* last free slot in the stack */
	/* 栈基指针 */
  StkId stack;  /* stack base */
	/* 栈大小 */
  int stacksize;
	/* 在栈中调用，无法放弃个数 */
  unsigned short nny;  /* number of non-yieldable calls in stack */
	/* 内嵌C调用数量 */
  unsigned short nCcalls;  /* number of nested C calls */
  lu_byte hookmask;
  lu_byte allowhook;
  int basehookcount;
  int hookcount;
	/* hook函数 */
  lua_Hook hook;
	/* 列出upvalues在当前栈中的列表 */
  GCObject *openupval;  /* list of open upvalues in this stack */
  GCObject *gclist;
	/* 错误恢复点 */
  struct lua_longjmp *errorJmp;  /* current error recover point */
	/* 一个栈索引，指向在栈中压入的错误处理函数的栈地址 */
  ptrdiff_t errfunc;  /* current error handling function (stack index) */
	/* 第一个调用信息结构 */
  CallInfo base_ci;  /* CallInfo for first level (C calling Lua) */
};

/* 返回虚拟机状态的全局状态结构 */
#define G(L)	(L->l_G)


/*
** Union of all collectable objects
*/
/* 可回收对象 */
union GCObject {
	/* 可回收对象公共头 */
  GCheader gch;  /* common header */
	/* 不同的可回收对象 */
  union TString ts;        /* 字符串 */
  union Udata u;           /* 用户定义 */
  union Closure cl;        /* 闭包函数 */
  struct Table h;          /* 哈希表 */
  struct Proto p;          /* lua函数 */
  struct UpVal uv;         /* upval值 */
	/* 线程状态 */
  struct lua_State th;  /* thread */
};

/* 获取可回收对象公共头 */
#define gch(o)		(&(o)->gch)

/* macros to convert a GCObject into a specific value */
/* 转换一个可回收对象到一个指定的值 */
#define rawgco2ts(o)  \
	check_exp(novariant((o)->gch.tt) == LUA_TSTRING, &((o)->ts))
#define gco2ts(o)	(&rawgco2ts(o)->tsv)
#define rawgco2u(o)	check_exp((o)->gch.tt == LUA_TUSERDATA, &((o)->u))
#define gco2u(o)	(&rawgco2u(o)->uv)
#define gco2lcl(o)	check_exp((o)->gch.tt == LUA_TLCL, &((o)->cl.l))
#define gco2ccl(o)	check_exp((o)->gch.tt == LUA_TCCL, &((o)->cl.c))
#define gco2cl(o)  \
	check_exp(novariant((o)->gch.tt) == LUA_TFUNCTION, &((o)->cl))
#define gco2t(o)	check_exp((o)->gch.tt == LUA_TTABLE, &((o)->h))
#define gco2p(o)	check_exp((o)->gch.tt == LUA_TPROTO, &((o)->p))
#define gco2uv(o)	check_exp((o)->gch.tt == LUA_TUPVAL, &((o)->uv))
#define gco2th(o)	check_exp((o)->gch.tt == LUA_TTHREAD, &((o)->th))

/* macro to convert any Lua object into a GCObject */
/* 将一个Lua对象转化成可回收对象 */
#define obj2gco(v)	(cast(GCObject *, (v)))


/* actual number of total bytes allocated */
/* 当前真实的总共分配字节数 */
#define gettotalbytes(g)	((g)->totalbytes + (g)->GCdebt)

LUAI_FUNC void luaE_setdebt (global_State *g, l_mem debt);
LUAI_FUNC void luaE_freethread (lua_State *L, lua_State *L1);
LUAI_FUNC CallInfo *luaE_extendCI (lua_State *L);
LUAI_FUNC void luaE_freeCI (lua_State *L);


#endif

