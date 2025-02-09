/*
 *  X86 code generator for TCC
 *
 *  Copyright (c) 2001-2004 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <poxim.h>
#define POXIM_MAX_REGISTERS 32
#define PARAMETER_OFFSET (1 * 4)

#define N_INSTRUCTIONS_FOR_FUNC_PROLOG (6)
#define LOCAL_OFFSET (4)
#define FUNC_PROLOG_SIZE (4 * (N_INSTRUCTIONS_FOR_FUNC_PROLOG))
// #define FUNC_STRUCT_PARAM_AS_PTR

#if defined(TARGET_DEFS_ONLY)
/* number of available registers */
#define NB_REGS (5)
#define NB_ASM_REGS 20
#define CONFIG_TCC_ASM

/* a register can belong to several classes. The classes must be
   sorted from more general to more precise (see gv2() code which does
   assumptions on it). */
#define RC_INT 0x0001   /* generic integer register */
#define RC_FLOAT 0x0002 /* generic float register does not exist in poxim */
#define RC_1 0x0004
#define RC_2 0x0008
#define RC_3 0x0010
#define RC_4 0x0020
#define RC_F 0x0040 // WARN: all floats must be eliminated for now
#define RC_5 0x0080

/* Anotehr possible ideia for dealing with float could be just doing integer
 * things with it idk*/
#define RC_IRET RC_1 /* function return: integer register */
#define RC_IRE2 RC_2 /* function return: second integer register */
#define RC_FRET RC_F /* function return: float register */

/* return registers for function */
#define REG_IRET r1 /* single word int return register */
#define REG_IRE2 r2 /* second word return register (for long long) */
#define REG_FRET rf

/* defined if function parameters must be evaluated in reverse order */
#define INVERT_FUNC_PARAMS

/* defined if structures are passed as pointers. Otherwise structures
   are directly pushed on stack. */
/* #define FUNC_STRUCT_PARAM_AS_PTR */

/* pointer size, in bytes */
#define PTR_SIZE 4

/* long double size and alignment, in bytes */
#define LDOUBLE_SIZE 12
#define LDOUBLE_ALIGN 4
/* maximum alignment (for aligned attribute support) */
#define MAX_ALIGN 4

/* define if return values need to be extended explicitely
   at caller side (for interfacing with non-TCC compilers) */
#define PROMOTE_RET

/******************************************************/
#else /* ! TARGET_DEFS_ONLY */
/******************************************************/
#define USING_GLOBALS
#include "tcc.h"

ST_DATA const char *const target_machine_defs = "__poxim__\0"
                                                "__poxim\0";

/* define to 1/0 to [not] have EBX as 4th register */
#define USE_EBX 0

ST_DATA const int reg_classes[NB_REGS] = {
    /* r1 */ RC_INT | RC_1,
    /* r2 */ RC_INT | RC_2,
    /* r3 */ RC_INT | RC_3,
    /* r4 */ RC_INT | RC_4
    // /* rf not equilavente */ RC_FLOAT | RC_ST0,
};

static unsigned long func_sub_sp_offset;
static int func_ret_sub;
#ifdef CONFIG_TCC_BCHECK
static addr_t func_bound_offset;
static unsigned long func_bound_ind;
ST_DATA int func_bound_add_epilog;
ST_DATA int func_bound_is_naked = 0;
static void gen_bounds_prolog(void);
static void gen_bounds_epilog(void);
static void gen_be32(unsigned int c); /* gen reversed 32 bit*/
#endif

#if 0 // Little endinannes writing, reading, adding
ST_INLN uint16_t read16le(unsigned char *p) {
    return p[0] | (uint16_t)p[1] << 8;
}
ST_INLN void write16le(unsigned char *p, uint16_t x) {
    p[0] = x & 255;  p[1] = x >> 8 & 255;
}
ST_INLN uint32_t read32le(unsigned char *p) {
  return read16le(p) | (uint32_t)read16le(p + 2) << 16;
}
ST_INLN void write32le(unsigned char *p, uint32_t x) {
    write16le(p, x);  write16le(p + 2, x >> 16);
}
ST_INLN void add32le(unsigned char *p, int32_t x) {
    write32le(p, read32le(p) + x);
}
ST_INLN uint64_t read64le(unsigned char *p) {
  return read32le(p) | (uint64_t)read32le(p + 4) << 32;
}
ST_INLN void write64le(unsigned char *p, uint64_t x) {
    write32le(p, x);  write32le(p + 4, x >> 32);
}
ST_INLN void add64le(unsigned char *p, int64_t x) {
    write64le(p, read64le(p) + x);
}

#else

ST_INLN uint16_t read16le(unsigned char *p) {
  assert(0 && "havent check where read16");
  return p[0] | (uint16_t)p[1] << 8;
}
ST_INLN void write16le(unsigned char *p, uint16_t x) {
  *(uint16_t *)p = swap_endianness16(x);
}

ST_INLN uint32_t read32le(unsigned char *p) {
  uint32_t val = swap_endianness32(*(uint32_t *)p);
  // XXX: Still aint sure, we might need to offset this
  return val;
}

ST_INLN void write32le(unsigned char *p, uint32_t x) {
  *(uint32_t *)p = swap_endianness32(x);
}

// ST_INLN uint32_t read32le(unsigned char *p) {
//   return read16le(p) | (uint32_t)read16le(p + 2) << 16;
// }
//
// ST_INLN void write32le(unsigned char *p, uint32_t x) {
//     write16le(p, x);  write16le(p + 2, x >> 16);
// }

ST_INLN void add32le(unsigned char *p, int32_t x) {
  assert(0 && "havent checked add");
  write32le(p, read32le(p) + x);
}

ST_INLN uint64_t read64le(unsigned char *p) {
  assert(0 && "havent checked read64");
  return read32le(p) | (uint64_t)read32le(p + 4) << 32;
}

ST_INLN void write64le(unsigned char *p, uint64_t x) {
  assert(0 && "havent checked write64");
  write32le(p, x);
  write32le(p + 4, x >> 32);
}

ST_INLN void add64le(unsigned char *p, int64_t x) {
  assert(0 && "havent checked add64");
  write64le(p, read64le(p) + x);
}
#endif

/* DONE: don't make it faster */
ST_FUNC void g(int c) {
  int ind1;
  if (nocode_wanted)
    return;
  ind1 = ind + 1;
  if (ind1 > cur_text_section->data_allocated)
    section_realloc(cur_text_section, ind1);
  cur_text_section->data[ind] = c;
  ind = ind1;
}

ST_FUNC void o(unsigned int c) {
  while (c) {
    g(c);
    c = c >> 8;
  }
}

ST_FUNC void gen_le8(int v) { g(v); }

ST_FUNC void gen_le16(int v) {
  g(v);
  g(v >> 8);
}

ST_FUNC void gen_le32(int c) {
  g(c);
  g(c >> 8);
  g(c >> 16);
  g(c >> 24);
}

/* gen reversed 16 bit*/
static void gen_be16(unsigned int c) {
  g(c >> 8);
  g(c);
};

static void gen_be32(unsigned int c) {
  g(c >> 24);
  g(c >> 16);
  g(c >> 8);
  g(c);

}; /* gen reversed 32 bit*/

/* output a symbol and patch all calls to it */
ST_FUNC void gsym_addr(int t, int a) {
  while (t) {
    unsigned char *ptr = cur_text_section->data + t;
    int i = a - t - 4; /* offset jmp */
    u32 *inst_ptr = (u32 *)(ptr);
    u32 inst = swap_endianness32(*inst_ptr);
    u32 n = (inst & 0x3ffffff);
    // write32le(ptr, a - t - 4);
    *inst_ptr =
        swap_endianness32(((inst >> 26) << 26) | ((i & 0x3ffffff) >> 2));
    // printf("%s -> n=0x%x a=0x%x t=0x%x (int)a-t=%d\n", __func__, n, a, t, a - t);
    // /* TODO: Endiannes check for next addr offset which is 't' ok? */
    t = n;
    // t = 0;
  }
}

/* instruction of 6 bits + 26 bit of data. Return the address of the data */
static int oad(int c, int s) {
  int t;
  if (nocode_wanted)
    return s;
  // o(c);
  t = ind;
  gen_be32((c & 0b111111) << 26 | (s & 0x3ffffff));
  return t;
}

/***************************
 >> START POXIM Instructions
****************************/
static b8 first_func = 1;
ST_FUNC void poxim_compile_init(struct TCCState *s) {
  // o(0xcafebabe);
}

ST_FUNC void poxim_gen_init(struct TCCState *s) {}

static void divu(int rz, int ri, int rx, int ry) {
  u32 freebits = 0b100;
  assert(r1 <= POXIM_MAX_REGISTERS && r2 <= POXIM_MAX_REGISTERS &&
         r3 <= POXIM_MAX_REGISTERS);
  gen_be32(opcode_div_ << 26 | (rz & 0xff) << 21 | (rx & 0xff) << 16 |
           (ry & 0xff) << 11 | freebits << 8 | (0b11111 & ri));
}
static void divs(int rz, int ri, int rx, int ry) {
  u32 freebits = 0b110;
  assert(r1 <= POXIM_MAX_REGISTERS && r2 <= POXIM_MAX_REGISTERS &&
         r3 <= POXIM_MAX_REGISTERS);
  gen_be32(opcode_divs << 26 | (rz & 0xff) << 21 | (rx & 0xff) << 16 |
           (ry & 0xff) << 11 | freebits << 8 | (0b11111 & ri));
}

static void mul(int rz, int ri, int rx, int ry) {
  u32 freebits = 0b000;
  assert(r1 <= POXIM_MAX_REGISTERS && r2 <= POXIM_MAX_REGISTERS &&
         r3 <= POXIM_MAX_REGISTERS);
  gen_be32(opcode_mul << 26 | (rz & 0xff) << 21 | (rx & 0xff) << 16 |
           (ry & 0xff) << 11 | freebits << 8 | (0b11111 & ri));
}

static void muls(int rz, int ri, int rx, int ry) {
  u32 freebits = 0b010;
  assert(r1 <= POXIM_MAX_REGISTERS && r2 <= POXIM_MAX_REGISTERS &&
         r3 <= POXIM_MAX_REGISTERS);
  gen_be32(opcode_muls << 26 | (rz & 0xff) << 21 | (rx & 0xff) << 16 |
           (ry & 0xff) << 11 | freebits << 8 | (0b11111 & ri));
}

static void add(int r1, int r2, int r3) {
  assert(r1 <= POXIM_MAX_REGISTERS && r2 <= POXIM_MAX_REGISTERS &&
         r3 <= POXIM_MAX_REGISTERS);
  gen_be32(0x02 << 26 | (r1 & 0xff) << 21 | (r2 & 0xff) << 16 |
           (r3 & 0xff) << 11);
}
static void addi(int rz, int rx, int i) {
  assert(rz <= POXIM_MAX_REGISTERS && rx <= POXIM_MAX_REGISTERS && i <= 0xFFFF);
  gen_be32(0b010010 << 26 | (rz & 0xff) << 21 | (rx & 0xff) << 16 |
           (i & 0xffff));
}

static void cmp(int rx, int ry) {
  assert(rx <= POXIM_MAX_REGISTERS && ry <= POXIM_MAX_REGISTERS);
  gen_be32(0b000101 << 26 | (rx & 0xff) << 16 | (ry & 0xff) << 11);
}

static void cmpi(int rx, int i) {
  assert(rx <= POXIM_MAX_REGISTERS && i <= 0xffff);
  gen_be32(0b010111 << 26 | (rx & 0xff) << 16 | (i & 0xffff));
}


static void calli(int i) {
  assert(i <= 0x3ffffff);
  gen_be32(opcode_calli << 26 | (i & 0x3ffffff));
}

static void call(int rx, int i) {
  assert(rx <= POXIM_MAX_REGISTERS && i <= 0xffff);
  gen_be32(opcode_call << 26 | (rx & 0xff) << 16 | (i & 0xffff));
}

static void bgt(int i) {
  assert((i <= 0x3ffffff));
  gen_be32(0b110000 << 26 | (i & 0x3ffffff));
}

static void bun(int i) {
  assert((i <= 0x3ffffff));
  gen_be32(0b110111 << 26 | (i & 0x3ffffff));
}

static void blt(int i) {
  assert((i <= 0x3ffffff));
  gen_be32(0b110010 << 26 | (i & 0x3ffffff));
}

static void push_or_pop(b8 is_pop, int r1, int r2, int r3, int r4, int r5) {
  u8 inst = is_pop ? 0b001011 : 0b001010;
  assert(r1 <= POXIM_MAX_REGISTERS && r2 <= POXIM_MAX_REGISTERS &&
         r3 <= POXIM_MAX_REGISTERS && r4 <= POXIM_MAX_REGISTERS &&
         r5 <= POXIM_MAX_REGISTERS);
  gen_be32(inst << 26 | (r1 & 0xff) << 6 | (r2 & 0xff) << 0 |
           (r3 & 0xff) << 16 | (r4 & 0xff) << 11 | (r5 & 0xff) << 21);
}

static void push5(int r1, int r2, int r3, int r4, int r5) {
  push_or_pop(0, r1, r2, r3, r4, r5);
}

static void pop5(int r1, int r2, int r3, int r4, int r5) {
  push_or_pop(1, r1, r2, r3, r4, r5);
}

static void push2(int r1, int r2) { push5(r1, r2, 0, 0, 0); }

static void pop2(int r1, int r2) { pop5(r1, r2, 0, 0, 0); }

static void push(int r) { push5(r, 0, 0, 0, 0); }

static void pop(int r) { pop5(r, 0, 0, 0, 0); }

static void sub(int r1, int r2, int r3) {
  assert(r1 <= POXIM_MAX_REGISTERS && r2 <= POXIM_MAX_REGISTERS &&
         r3 <= POXIM_MAX_REGISTERS);
  gen_be32(0x03 << 26 | (r1 & 0xff) << 21 | (r2 & 0xff) << 16 |
           (r3 & 0xff) << 11);
}

static void subi(int rz, int rx, int i) {
  assert(rz <= POXIM_MAX_REGISTERS && rx <= POXIM_MAX_REGISTERS && i <= 0xFFFF);
  gen_be32(0b010011 << 26 | (rz & 0xff) << 21 | (rx & 0xff) << 16 |
           (i & 0xffff));
}

static void muli(int rz, int rx, int i) {
  assert(rz <= POXIM_MAX_REGISTERS && rx <= POXIM_MAX_REGISTERS && i <= 0xFFFF);
  gen_be32(opcode_muli << 26 | (rz & 0xff) << 21 | (rx & 0xff) << 16 |
           (i & 0xffff));
}

static void divi(int rz, int rx, int i) {
  assert(rz <= POXIM_MAX_REGISTERS && rx <= POXIM_MAX_REGISTERS && i <= 0xFFFF);
  gen_be32(opcode_divi << 26 | (rz & 0xff) << 21 | (rx & 0xff) << 16 |
           (i & 0xffff));
}

static void remi(int rz, int rx, int i) {
  assert(rz <= POXIM_MAX_REGISTERS && rx <= POXIM_MAX_REGISTERS && i <= 0xFFFF);
  gen_be32(opcode_remi << 26 | (rz & 0xff) << 21 | (rx & 0xff) << 16 |
           (i & 0xffff));
}

static void movr(int r1, int r2) { add(r1, r2, 0); };

static void mov(int r, int i) {
  if (!(r <= POXIM_MAX_REGISTERS && i <= 0x1fffff)) {
    tcc_error("int is 32 bits, but a immediate value can only be 20 bit in arch poxim");
  }
  // g((u32)i);
  // g(0x92fa);
  // g((u32)i);
  // g((u32)i);
  gen_be32(opcode_mov << 26 | (r & 0b11111) << 21 | (i & 0x1fffff));
}

static void movs(int r, int i) {
  if (!(r <= POXIM_MAX_REGISTERS && i <= 0x1fffff)) {
    tcc_error("It's possible i have made a mistake, int is actually 20 "
              "bits on a const mov, i'm sorry");
  }
  gen_be32(opcode_movs << 26 | (r & 0b11111) << 21 | (i & 0x1fffff));
}

static void and(int rz, int rx, int ry) {
  assert(rz <= POXIM_MAX_REGISTERS && rx <= POXIM_MAX_REGISTERS &&
         ry <= POXIM_MAX_REGISTERS);
  gen_be32(opcode_and << 26 | (rz & 0xff) << 21 | (rx & 0xff) << 16 |
           (ry & 0xff) << 11);
}

static void andi(int rz, int rx, int i) {
  assert(rz <= POXIM_MAX_REGISTERS && rx <= POXIM_MAX_REGISTERS);
  movs(rt, i);
  and(rz, rx, rt);
}

static void xor(int rz, int rx, int ry) {
  assert(rz <= POXIM_MAX_REGISTERS && rx <= POXIM_MAX_REGISTERS &&
         ry <= POXIM_MAX_REGISTERS);
  gen_be32(opcode_xor << 26 | (rz & 0xff) << 21 | (rx & 0xff) << 16 |
           (ry & 0xff) << 11);
}
static void xori(int rz, int rx, int i) {
  assert(rz <= POXIM_MAX_REGISTERS && rx <= POXIM_MAX_REGISTERS);
  movs(rt, i);
  xor(rz, rx, rt);
}

static void or(int rz, int rx, int ry) {
  assert(rz <= POXIM_MAX_REGISTERS && rx <= POXIM_MAX_REGISTERS &&
         ry <= POXIM_MAX_REGISTERS);
  gen_be32(opcode_or << 26 | (rz & 0xff) << 21 | (rx & 0xff) << 16 |
           (ry & 0xff) << 11);
}
static void ori(int rz, int rx, int i) {
  assert(rz <= POXIM_MAX_REGISTERS && rx <= POXIM_MAX_REGISTERS);
  movs(rt, i);
  xor(rz, rx, rt);
}

static void s32(int rz, int rx, int i) {
  u32 inst = 0b011101;
  gen_be32(inst << 26 | (rz & 0b11111) << 21 | (rx & 0b11111) << 16 |
           (i & 0xFFFF));
}

static void l32(int r1, int r2, int i) {
  u32 inst = 0b011010;
  gen_be32(inst << 26 | (r1 & 0b11111) << 21 | (r2 & 0b11111) << 16 |
           (i & 0xFFFF));
}

static void shift(u8 sub_inst, int rz, int rx, int ry, unsigned int i) {
  u32 inst = 0b00100;
  gen_be32(inst << 26 | (rz & 0b11111) << 21 | (rx & 0b11111) << 16 |
           (ry & 0b11111) << 11 | (sub_inst & 0b111) << 8 |
           ((unsigned int)(i - 1) & 0b11111));
  // | ((unsigned int)log2((f64)i) & 0b1111));
}

static void sra(int rz, int rx, int ry, int i) { shift(0b111, rz, rx, ry, i); }

// Rigt Logic
static void srl(int rz, int rx, int ry, int i) { shift(0b101, rz, rx, ry, i); }
// Left Logic
static void sll(int rz, int rx, int ry, int i) { shift(0b001, rz, rx, ry, i); }
// Left Arithmetic
static void sla(int rz, int rx, int ry, int i) { shift(0b011, rz, rx, ry, i); }
/***************************
 << END POXIM Instructions
****************************/

ST_FUNC void gen_fill_nops(int ninstructions) {
  while (ninstructions--)
    gen_le32(0x00000000);
}

/* generate jmp to a label */
#define gjmp2(instr, lbl) oad(instr, lbl)

/* output constant with relocation if 'r & VT_SYM' is true */
ST_FUNC void gen_addr32(int r, Sym *sym, int c) {
  if (r & VT_SYM)
    greloc(cur_text_section, sym, ind, R_386_32);
  gen_le32(c);
}

ST_FUNC void gen_addrpc32(int r, Sym *sym, int c) {
  if (r & VT_SYM)
    greloc(cur_text_section, sym, ind, R_386_PC32);
  gen_le32(c - 4);
}

static void gen_poxim_direct_addr(int r, Sym *sym, int c) {
  if ((r & VT_VALMASK) == VT_CONST) {
    if (r & VT_SYM) {
      greloc(cur_text_section, sym, ind, R_386_32);
    }
    // TODO: Might need to check the endiannes of this
    /*
       We reserve two 4*1 byte instructions for moving the DIRECT 32 bit
       addr then we patch the instruction on the linker such as mv reg_addr,
       addr l32 [reg_addr] / call reg_addr
    */
    gen_le32(c); // TODO: Might need to check the endiannes of this
                 // ind += 8;
  }
}

static b8 gen_poxim_addr(int r, Sym *sym, int c) {
  b8 is_const = (r & VT_VALMASK) == VT_CONST;
  if (is_const) {
    if (r & VT_SYM) {
      greloc(cur_text_section, sym, ind, R_386_PC32);
    }
    // TODO: Might need to check the endiannes of this
    /*
       We reserve two 4*1 byte instructions for moving the DIRECT 32 bit
       addr then we patch the instruction on the linker such as mv reg_addr,
       addr l32 [reg_addr] / call reg_addr
    */
    // gen_le32(c); //TODO: Might need to check the endiannes of this
  }
  return is_const;
}
/* generate a modrm reference. 'op_reg' contains the additional 3
   opcode bits */
static void gen_modrm(int op_reg, int r, Sym *sym, int c) {
  op_reg = op_reg << 3;
  if ((r & VT_VALMASK) == VT_CONST) {
    /* constant memory reference */
    o(0x05 | op_reg);
    gen_addr32(r, sym, c);
  } else if ((r & VT_VALMASK) == VT_LOCAL) {
    /* currently, we use only ebp as base */
    if (c == (char)c) {
      /* short reference */
      o(0x45 | op_reg);
      g(c);
    } else {
      oad(0x85 | op_reg, c);
    }
  } else {
    g(0x00 | op_reg | (r & VT_VALMASK));
  }
}

/*
  load 'r' from value 'sv
  Must generate the code needed to load a stack value into a register.
*/
ST_FUNC void load(int r, SValue *sv) {
  int v, t, ft, fc, fr, bt;
  u8 opcode = -1;
  u32 imm = -1, rbp = bp2;
  SValue v1;

#ifdef TCC_TARGET_PE
  SValue v2;
  sv = pe_getimport(sv, &v2);
#endif

  fr = sv->r;
  ft = sv->type.t & ~VT_DEFSIGN;
  fc = sv->c.i;

  ft &= ~(VT_VOLATILE | VT_CONSTANT);
  bt = ft & VT_BTYPE;

  v = fr & VT_VALMASK;
  if (fr & VT_LVAL) {
    if (v == VT_LLOCAL) {
      v1.type.t = VT_INT;
      v1.r = VT_LOCAL | VT_LVAL;
      v1.c.i = fc;
      v1.sym = NULL;
      fr = r;
      if (!(reg_classes[fr] & RC_INT))
        fr = get_reg(RC_INT);
      load(fr, &v1);
    }
    if (bt == VT_FLOAT) {
      tcc_error("We aint supporting float, boy");
      o(0xd9); /* flds */
      r = 0;
    } else if (bt == VT_DOUBLE) {
      tcc_error("We aint supporting double, boy");
      o(0xdd); /* fldl */
      r = 0;
    } else if (bt == VT_LDOUBLE) {
      tcc_error("We aint supporting long double, boy");
      o(0xdb); /* fldt */
      r = 5;
    } else if ((ft & VT_TYPE) == VT_BYTE || (ft & VT_TYPE) == VT_BOOL) {
      imm = (fc + LOCAL_OFFSET) >> 0 & 0xFFFF;
      opcode = opcode_l8;
      // tcc_error("We aint supporting byte for now or bool");
      // o(0xbe0f); /* movsbl */
    } else if ((ft & VT_TYPE) == (VT_BYTE | VT_UNSIGNED)) {
      imm = (fc + LOCAL_OFFSET) & 0xFFFF;
      opcode = opcode_l8;
      // o(0xb60f); /* movzbl */
    } else if ((ft & VT_TYPE) == VT_SHORT) {
      tcc_error("We aint supporting short for now or bool");
      o(0xbf0f); /* movswl */
    } else if ((ft & VT_TYPE) == (VT_SHORT | VT_UNSIGNED)) {
      tcc_error("We aint supporting (VT_SHORT | VT_UNSIGNED)");
      imm = (fc + LOCAL_OFFSET) >> 1 & 0xFFFF;
      opcode = opcode_l8;
      // o(0xb70f); /* movzwl */
    } else if ((ft & VT_TYPE) == (VT_FUNC)) {
      imm = (fc + LOCAL_OFFSET) >> 2 & 0xFFFF;
      opcode = opcode_l32;
    } else if ((ft & VT_TYPE) == (VT_PTR | VT_ARRAY)) {
      opcode = opcode_l32;
      imm = (fc + LOCAL_OFFSET) >> 2 & 0xFFFF;
    } else if ((ft & VT_TYPE) == (VT_PTR)) {
      /* l32 */
      opcode = opcode_l32;
      imm = ((fc + LOCAL_OFFSET) >> 2) & 0xFFFF;
    } else if ((ft & VT_TYPE) == (VT_INT)) {
      /* l32 */
      imm = (fc + LOCAL_OFFSET) >> 2 & 0xFFFF;
      opcode = 0b011010;
    } else if ((ft & VT_TYPE) == (VT_INT | VT_UNSIGNED)) {
      imm = (fc + LOCAL_OFFSET) >> 2 & 0xFFFF;
      opcode = opcode_l32;
    } else if ((ft & VT_TYPE) == (VT_QLONG)) {
      /* l64 */
      tcc_error("poxim does not support VT_QLONG type");
    } else if ((ft & VT_TYPE) == (VT_QLONG)) {
      tcc_error("poxim does not support VT_QLONG type");
    } else {
      tcc_error("poxim does not support 0x%x type", (ft & VT_TYPE));
    }
    // DONE:  gen_modrm(r, fr, sv->sym, fc);
    if ((fr & VT_VALMASK) == VT_LOCAL) {
      /* currently, we use only bp2 as base */
      gen_be32(opcode << 26 | (r + 1) << 21 | rbp << 16 | (imm));
    } else {
      u32 reg = ((fr & VT_VALMASK) + 1);
      if (opcode == opcode_l8) {
        u32 rtemp = rt2;
        movr(rtemp, reg);
        sll(r0, rtemp, rtemp, 2);
        gen_be32(opcode << 26 | ((r + 1) & 0b11111) << 21 | rtemp << 16);
      } else {
        gen_be32(opcode << 26 | (r + 1) << 21 | reg << 16);
      }
    }
    gen_poxim_direct_addr(fr, sv->sym, (fc));

  } else {
    if (v == VT_CONST) {
      if (((ft & VT_TYPE) & 0x00F0) == VT_UNSIGNED) {
        mov(r + 1, fc); /* mov r, fc */
      } else {
        // o(0xb8 + r);     /* mov $xx, r */
        movs(r + 1, fc); /* mov r, fc */
      }
      // in x86 gen_addr32(fr, sv->sym, fc);
      // TODO: Check is this break examples
      if ((fr & VT_VALMASK) == VT_CONST) {
        if (fr & VT_SYM) {
          greloc(cur_text_section, sv->sym, ind, R_386_32);
        }
      }
      // gen_poxim_direct_addr(fr, sv->sym, fc); /* I removed this because we do greloc instead*/ 

    } else if (v == VT_LOCAL) {
      if (fc) {
        if ((VT_LOCAL & VT_VALMASK) == VT_LOCAL) {
          /* in x86 would be lea xxx(%ebp), r */
          /* currently, we use only bp2 as base */
          addi(r + 1, rbp, (fc + LOCAL_OFFSET) >> 2);
        } else {
          assert(0 && "first time i saw this case, inspect further, "
                      "although it must be correct anyways");
          movr(r + 1, ((VT_LOCAL & VT_VALMASK) + 1));
        }
        // TODO:  check if we can do someething like this addi(r+1, bp2,
        // fc >> 2);
        //  o(0x8d);
        // TODO:@symbolcheck
        gen_poxim_direct_addr(VT_LOCAL, sv->sym, fc);
        // gen_modrm(r, VT_LOCAL, sv->sym, fc);
      } else {
        tcc_error("%s poxim-gen does not support fc == 0 ? ", __func__);
        o(0x89);
        o(0xe8 + r); /* mov %ebp, r */
      }
    } else if (v == VT_CMP) {
        switch (fc) {
        case TOK_UGE: {
          opcode = opcode_bae;
          break;
        } /* 0x93 */
        case TOK_EQ: {
          opcode = opcode_beq;
          break;
        } /* 0x94 */
        case TOK_NE: {
          opcode = opcode_bne;
          break;
        } /* 0x95 */
        case TOK_ULE: {
          opcode = opcode_bbe;
          break;
        } /* 0x96 */
        case TOK_UGT: {
          opcode = opcode_bat;
          break;
        } /* 0x97 */
        case TOK_Nset: {
          assert(0 && "jmp cond not handled TOK_Nset");
          break;
        } /* 0x98 */
        case TOK_Nclear: {
          assert(0 && "jmp cond not handled TOK_Nclear");
          break;
        } /* 0x99 */
        case TOK_LT: {
          opcode = opcode_blt;
          break;
        } /* 0x9c */
        case TOK_GE: {
          opcode = opcode_bge;
          break;
        } /* 0x9d */
        case TOK_LE: {
          opcode = opcode_ble;
          break;
        } /* 0x9e */
        case TOK_GT: {
          opcode = opcode_bgt;
          break;
        } /* 0x9f */
        default: {
          assert(0 && " Unknown token for a jump instruction");
        }
      }
      {
        int words_to_jmp = 2;
        oad(opcode, words_to_jmp);  /* jmp if cond */
        mov(r+1, 0);  /* move zero to r is false */
        bun(1);
        mov(r+1, 1);  /* move zero to 1 is true */

      }
      // o(0x0f); /* setxx %br */
      // o(fc);
      // o(0xc0 + r);
      // o(0xc0b60f + r * 0x90000); /* movzbl %al, %eax */
      //  examples in i386
      // 5a4:	0f 9f c0             	setg   al
      // 5a7:	0f b6 c0             	movzx  eax,al
    } else if (v == VT_JMP) {
      t = v & 1;
      mov(r + 1, t); /* mov r, 0 */
      bun(1);        /* bun to next instruction */
      // XXX: if gsym is generated, it'll crash for you now what reasons
      // from
      //  days 25,26,27 :)
      gsym((fc));
      mov(r + 1, t ^ 1); /* mov r, 1 */
    } else if (v == VT_JMPI) {
      t = v & 1;
      mov(r + 1, t); /* mov r, 0 */
      bun(1);        /* bun to next instruction */
      gsym((fc));
      mov(r + 1, t ^ 1); /* mov r, 1 */
    } else if (v != r) {
      movr(r + 1, v + 1); /* mov  r, v */
    }
  }
}

/* store register 'r' in lvalue 'v' */
ST_FUNC void store(int r, SValue *v) {
  int fr, bt, ft, fc;
  u32 opcode = -1;
  u32 imm = -1, rbp = bp2;

#ifdef TCC_TARGET_PE
  SValue v2;
  v = pe_getimport(v, &v2);
#endif

  ft = v->type.t;
  fc = v->c.i;
  fr = v->r & VT_VALMASK;
  ft &= ~(VT_VOLATILE | VT_CONSTANT);
  bt = ft & VT_BTYPE;
  /* XXX: incorrect if float reg to reg */
  if (bt == VT_FLOAT) {
    tcc_error("VT_FLOAT poxim not hanlded %s", __func__);
    o(0xd9); /* fsts */
    r = 2;
  } else if (bt == VT_DOUBLE) {
    tcc_error("VT_DOUBLE poxim not hanlded %s", __func__);
    o(0xdd); /* fstpl */
    r = 2;
  } else if (bt == VT_LDOUBLE) {
    tcc_error("VT_LDOUBLE poxim not hanlded %s", __func__);
    o(0xc0d9); /* fld %st(0) */
    o(0xdb);   /* fstpt */
    r = 7;
  } else {
    if (bt == VT_SHORT) {
      tcc_error("VT_SHORT poxim not hanlded %s", __func__);
      o(0x66);
    }
    if (bt == VT_BYTE || bt == VT_BOOL) {
      imm = (fc + LOCAL_OFFSET) & 0xFFFF;
      opcode = opcode_s8;
    } else if (bt == VT_INT) { // VT_INT
      imm = ((fc + LOCAL_OFFSET) >> 2) & 0xFFFF;
      opcode = opcode_s32;
    } else if (bt == VT_PTR) {
      imm = ((fc + LOCAL_OFFSET) >> 2) & 0xFFFF;
      opcode = 0b011101;
    } else {
      tcc_error("%s Why did we get here, basice type bt = %x", __func__, bt);
    }
  }
  if (fr == VT_CONST) {
    u32 reg = ((v->r & VT_VALMASK) + 1);
    // TODO:  check the endiannes of this, does it need changing?
    if ((v->r & VT_VALMASK) == VT_LOCAL) {
      if (((ft & VT_TYPE) & 0x00F0) == VT_UNSIGNED) {
        mov(r + 1, (fc));
      } else {
        movs(r + 1, (fc));
      }
    } else {
       gen_be32(opcode << 26 | ((r + 1) & 0b11111) << 21 | reg << 16);
      // movr(r + 1, ((v->r & VT_VALMASK) + 1) & 0b11111);
    }
    gen_poxim_direct_addr(v->r, v->sym, fc);

  } else if (fr == VT_LOCAL) {
    // tcc_error("VT_LOCAL poxim not hanlded %s", __func__);
    /* currently, we use only ebp as base */
    // v->r

    if ((v->r & VT_VALMASK) == VT_LOCAL) {
      gen_be32(opcode << 26 | ((r + 1) & 0b11111) << 21 | rbp << 16 | (imm));
    } else {
      u32 reg = ((v->r & VT_VALMASK) + 1);
      if (opcode == opcode_s8) {
        u32 rtemp = rt2;
        movr(rtemp, reg);
        sll(r0, rtemp, rtemp, 2);
        gen_be32(opcode << 26 | ((r + 1) & 0b11111) << 21 | rtemp << 16);
      } else {
        gen_be32(opcode << 26 | ((r + 1) & 0b11111) << 21 | reg << 16);
      }
    }
    gen_poxim_direct_addr(v->r, v->sym, fc);
    // gen_poxim_addr(v->r, v->sym, fc);

  } else if (v->r & VT_LVAL) {
    // TODO: does this break anythin? s32(r + 1, fr + 1, 0);
    if ((v->r & VT_VALMASK) == VT_LOCAL) {
      gen_be32(opcode << 26 | ((r + 1) & 0b11111) << 21 | rbp << 16 | (imm));
    } else {
      u32 reg = ((v->r & VT_VALMASK) + 1);
      if (opcode == opcode_s8) {
        u32 rtemp = rt2;
        movr(rtemp, reg);
        sll(r0, rtemp, rtemp, 2);
        gen_be32(opcode << 26 | ((r + 1) & 0b11111) << 21 | rtemp << 16);
      } else {
        gen_be32(opcode << 26 | ((r + 1) & 0b11111) << 21 | reg << 16);
      }
    }
    gen_poxim_direct_addr(v->r, v->sym, fc);
    // gen_poxim_addr(v->r, v->sym, fc);
  } else if (fr != r) {
    tcc_error("fr != r poxim not hanlded %s", __func__);
    o(0xc0 + fr + r * 8); /* mov r, fr */
  }
}

static void gadd_sp(int val) { addi(sp, sp, val); }

#if defined CONFIG_TCC_BCHECK || defined TCC_TARGET_PE
static void gen_static_call(int v) {
  Sym *sym;
  tcc_error("%s, we aitn doing that yet", __func__);

  sym = external_helper_sym(v);
  oad(0xe8, -4);
  greloc(cur_text_section, sym, ind - 4, R_386_PC32);
}
#endif

/* 'is_jmp' is '1' if it is a jump */
static void gcall_or_jmp(int is_jmp) {
  int imm = 0x000;
  if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST && (vtop->r & VT_SYM)) {
    greloc(cur_text_section, vtop->sym, ind + 1, R_386_PC32);
    /* constant and relocation case */
    /* call/jmp im */
    // imm =  (vtop->c.i) & 0x03FFFFFF;
    // imm =  (vtop->c.i - 4) & 0x03FFFFFF;

    if (is_jmp) {
      bun(imm); /* bun imm */
    } else {
      calli(imm); /* call imm */
      // printf("calli imm = %x\n", imm);
    }
    // oad(0xe8 + is_jmp, vtop->c.i - 4);
  } else {
    int r;
    /* otherwise, indirect call */
    r = gv(RC_INT);
    // o(0xff); /* call/jmp *r */
    // o(0xd0 + r + (is_jmp << 4));
    if (is_jmp) {
      // tcc_error("indirect jmp not supported in poxim, try using a function pointer");
      call(r+1, 0); /* call r */
      // printf("bun r = %x\n", r+1);
      /* cleanup the stack since we pushed the addr because of previous call */
      addi(sp, sp, 4); 
    } else {
      call(r+1, 0); /* call r */
      // printf("call r = %x\n", r+1);
    }
  }
}

static const uint8_t fastcall_regs[3] = {r1, r3, r2};
static const uint8_t fastcallw_regs[2] = {r2, r3};

/* Return the number of registers needed to return the struct, or 0 if
   returning via struct pointer. */
ST_FUNC int gfunc_sret(CType *vt, int variadic, CType *ret, int *ret_align,
                       int *regsize) {
#if defined(TCC_TARGET_PE) || TARGETOS_FreeBSD || TARGETOS_OpenBSD
  int size, align, nregs;
  *ret_align = 1; // Never have to re-align return values for x86
  *regsize = 4;
  size = type_size(vt, &align);
  if (size > 8 || (size & (size - 1)))
    return 0;
  nregs = 1;
  if (size == 8)
    ret->t = VT_INT, nregs = 2;
  else if (size == 4)
    ret->t = VT_INT;
  else if (size == 2)
    ret->t = VT_SHORT;
  else
    ret->t = VT_BYTE;
  ret->ref = NULL;
  return nregs;
#else
  *ret_align = 1; // Never have to re-align return values for x86
  return 0;
#endif
}

/* Generate function call. The function address is pushed first, then
   all the parameters in call order. This functions pops all the
   parameters and the function address. */
ST_FUNC void gfunc_call(int nb_args) {
  int size, align, r, args_size, i, func_call;
  Sym *func_sym;

#ifdef CONFIG_TCC_BCHECK
  if (tcc_state->do_bounds_check)
    gbound_args(nb_args);
#endif

  args_size = 0;
  for (i = 0; i < nb_args; i++) {
    if ((vtop->type.t & VT_BTYPE) == VT_STRUCT) {
      // tcc_error("%s, struct not supported in poxim-gen for now", __func__);
      size = type_size(&vtop->type, &align);
      /* align to stack align size */

      /* NOTE:(everton) this is always result in the same 
        size, before and after size = (size + 3) & ~3;
        because we only allow structs with members with size multiple of 4 
      */
      size = (size + 3) & ~3;
      /* allocate the necessary size on stack */
#ifdef TCC_TARGET_PE
      if (size >= 4096) {
        r = get_reg(r1);
        oad(0x68, size); // push size
        /* cannot call normal 'alloca' with bound checking */
        gen_static_call(tok_alloc_const("__alloca"));
        gadd_sp(4);
      } else
#endif
      /* NOTE: (Everton) This might all be different if we define FUNC_STRUCT_PARAM_AS_PTR, ok?*/
      {
        // oad(0xec81, size); /* sub $xxx, %esp */
        subi(sp, sp, size); /* sub sp, sp, xxx */
        /* generate structure store */
        r = get_reg(RC_INT);
        movr(rt, sp); /* mov rt sp */
        /* This is a pointer now and since every pointer in
           shifted in poxim we need to scale it  before passing to memove
        */
        srl(r0, rt, rt, 2);
        // printf("size of struct = 0x%x", size);
        addi(rt, rt, 1); 
        movr(r + 1, rt); /* mov %esp, r */
      }
      vset(&vtop->type, r | VT_LVAL, 0);
      vswap();
      vstore();
      // gen_be32(0xcafebabe);
      args_size += size;
    } else if (is_float(vtop->type.t)) {
      tcc_error("%s, floating point not supported in poxim-gen for now",
                __func__);
      gv(RC_FLOAT); /* only one float register */
      if ((vtop->type.t & VT_BTYPE) == VT_FLOAT)
        size = 4;
      else if ((vtop->type.t & VT_BTYPE) == VT_DOUBLE)
        size = 8;
      else
        size = 12;
      // oad(0xec81, size); /* sub $xxx, %esp */
      subi(sp, sp, size); /* sub sp, sp, xxx */
      if (size == 12)
        o(0x7cdb);
      else
        o(0x5cd9 + size - 4); /* fstp[s|l] 0(%esp) */
      g(0x24);
      g(0x00);
      args_size += size;
    } else {
      /* simple type (currently always same size) */
      /* XXX: implicit cast ? */
      r = gv(RC_INT);
      if ((vtop->type.t & VT_BTYPE) == VT_LLONG) {
        tcc_error(" poxim can't deal with long long data size");
        size = 8;
        o(0x50 + vtop->r2); /* push r */
      } else {
        size = 4;
      }
      /*
              we do a + 1 below because of the same reason as always,
              r0 is reserved for the zero register
              push r ;
      */
      push(r + 1);
      // o(0x50 + r);
      args_size += size;
    }
    vtop--;
  }
  save_regs(0); /* save used temporary registers */
  func_sym = vtop->type.ref;
  func_call = func_sym->f.func_call;
  /* fast call case */
  if ((func_call >= FUNC_FASTCALL1 && func_call <= FUNC_FASTCALL3) 
      || func_call == FUNC_FASTCALLW) {
    int fastcall_nb_regs;
    const uint8_t *fastcall_regs_ptr;
    tcc_error("FUNC_FASTCALL not supported in poxim-gen for now");
    if (func_call == FUNC_FASTCALLW) {
      fastcall_regs_ptr = fastcallw_regs;
      fastcall_nb_regs = 2;
    } else {
      fastcall_regs_ptr = fastcall_regs;
      fastcall_nb_regs = func_call - FUNC_FASTCALL1 + 1;
    }
    for (i = 0; i < fastcall_nb_regs; i++) {
      if (args_size <= 0)
        break;
      o(0x58 + fastcall_regs_ptr[i]); /* pop r */
      /* XXX: incorrect for struct/floats */
      args_size -= 4;
    }
  }
#if !defined(TCC_TARGET_PE) && !TARGETOS_FreeBSD || TARGETOS_OpenBSD
  else if ((vtop->type.ref->type.t & VT_BTYPE) == VT_STRUCT)
    args_size -= 4;
#endif

  gcall_or_jmp(0);

  if (args_size && func_call != FUNC_STDCALL && func_call != FUNC_FASTCALLW)
    gadd_sp(args_size);
  vtop--;
}

/* generate function prolog of type 't' */
ST_FUNC void gfunc_prolog(Sym *func_sym) {
  CType *func_type = &func_sym->type;
  int addr, align, size, func_call, fastcall_nb_regs;
  int param_index, param_addr;
  const uint8_t *fastcall_regs_ptr;
  Sym *sym;
  CType *type;
  b8 is_naked = 0;

  sym = func_type->ref;
  func_call = sym->f.func_call;
  is_naked = sym->f.func_naked;
  addr = 8;
  loc = 0;
  func_vc = 0;

  if (func_call >= FUNC_FASTCALL1 && func_call <= FUNC_FASTCALL3) {
    fastcall_nb_regs = func_call - FUNC_FASTCALL1 + 1;
    fastcall_regs_ptr = fastcall_regs;
  } else if (func_call == FUNC_FASTCALLW) {
    fastcall_nb_regs = 2;
    fastcall_regs_ptr = fastcallw_regs;
  } else {
    fastcall_nb_regs = 0;
    fastcall_regs_ptr = NULL;
  }
  param_index = 0;

  ind += FUNC_PROLOG_SIZE;
  func_sub_sp_offset = ind;
  /* if the function returns a structure, then add an
     implicit pointer parameter */
#if defined(TCC_TARGET_PE) || TARGETOS_FreeBSD || TARGETOS_OpenBSD
  size = type_size(&func_vt, &align);
  if (((func_vt.t & VT_BTYPE) == VT_STRUCT) &&
      (size > 8 || (size & (size - 1)))) {
#else
  if ((func_vt.t & VT_BTYPE) == VT_STRUCT) {
#endif
    /* XXX: fastcall case ? */
    func_vc = addr;
    addr += 4;
    param_index++;
  }
  /* define parameters */
  while ((sym = sym->next) != NULL) {
    type = &sym->type;
    size = type_size(type, &align);
    size = (size + 3) & ~3;
#ifdef FUNC_STRUCT_PARAM_AS_PTR
    /* structs are passed as pointer */
    if ((type->t & VT_BTYPE) == VT_STRUCT) {
      size = 4;
    }
#endif
    if (param_index < fastcall_nb_regs) {
      assert(0 && "param_index < fastcall_nb_regs");
      /* save FASTCALL register */
      loc -= 4;
      o(0x89); /* movl */
      gen_modrm(fastcall_regs_ptr[param_index], VT_LOCAL, NULL, loc);
      param_addr = loc;
    } else {
      param_addr = addr + PARAMETER_OFFSET;
      addr += size;
    }
    sym_push(sym->v & ~SYM_FIELD, type, VT_LOCAL | VT_LVAL, param_addr);
    param_index++;
  }
  func_ret_sub = 0;
  /* pascal type call or fastcall ? */
  if (func_call == FUNC_STDCALL || func_call == FUNC_FASTCALLW) {
    assert(0 && "(func_call == FUNC_STDCALL || func_call == FUNC_FASTCALLW)");
    func_ret_sub = addr - 8;
  }
#if !defined(TCC_TARGET_PE) && !TARGETOS_FreeBSD || TARGETOS_OpenBSD
  else if (func_vc) {
    func_ret_sub = 4;
  }
#endif

#ifdef CONFIG_TCC_BCHECK
  if (tcc_state->do_bounds_check)
    gen_bounds_prolog();
#endif
}

/* generate function epilog */
ST_FUNC void gfunc_epilog(void) {
  addr_t v, saved_ind;
#ifdef CONFIG_TCC_BCHECK
  if (tcc_state->do_bounds_check)
    gen_bounds_epilog();
#endif
  if (func_bound_is_naked) {
    return;
  }

  /* align local size to word & save local variables */
  v = (-loc + 3) & -4;

#if USE_EBX
  o(0x8b);
  gen_modrm(r3, VT_LOCAL, NULL, -(v + 4));
#endif

  {
    const u8 offset_for_func_addr = 4;
    movr(sp, bp);
    pop(bp2);
    pop(bp);
  }
  if (func_ret_sub == 0) {
    gen_le32(0x7c); /* ret */
  } else {
    tcc_error("poxim does not support 'ret n' you probably are trying to return a struct, try passing by pointer");
    pop(rret);
    addi(sp, sp, func_ret_sub - 4);
    push(rret);
    gen_le32(0x7c); /* ret */

    // o(0xc2); /* ret n */
    // g(func_ret_sub);
    // g(func_ret_sub >> 8);
  }
  saved_ind = ind;
  ind = func_sub_sp_offset - FUNC_PROLOG_SIZE;
#ifdef TCC_TARGET_PE
  if (v >= 4096) {
    oad(0xb8, v); /* mov stacksize, %eax */
    gen_static_call(
        TOK___chkstk); /* call __chkstk, (does the stackframe too) */
  } else
#endif
  {
    /* push %ebp; mov %esp, %ebp */
    push(bp);
    push(bp2);
    movr(bp, sp);
    movr(bp2, bp);
    sra(r0, bp2, bp2, 2);
    // gen_be32(0x10E03F01);
    /* sub esp, stacksize */
    subi(sp, sp, v);
#ifdef TCC_TARGET_PE
    o(0x000000); /* adjust to FUNC_PROLOG_SIZE  with nop*/
#endif
  }
  ind = saved_ind;
}

/* generate a jump to a label */
ST_FUNC int gjmp(int t) {
  u8 opcode = opcode_bun;
  t = oad(opcode, t);
  return t;
}

/* generate a jump to a fixed address */
ST_FUNC void gjmp_addr(int a) {
  // tcc_error("we don't generate jmp to to fixed address for now");
  int r;
  r = (a - ind - 4) >> 2;
  bun(r);
  // if (r == (char)r) {
  //   g(0xeb);
  //   g(r);
  // } else {
  //   oad(0xe9, a - ind - 5);
  // }
}

#if 0
/* generate a jump to a fixed address */
ST_FUNC void gjmp_cond_addr(int a, int op)
{
    int r = a - ind - 2;
    if (r == (char)r)
        g(op - 32), g(r);
    else
        g(0x0f), gjmp2(op - 16, r - 4);
}
#endif

ST_FUNC int gjmp_append(int n, int t) {
  void *p;
  /* insert vtop->c jump list in t */
  if (n) { // XXX: This aint workign
    u32 n1, n2;
    u32 *inst_ptr;
    u32 inst;

    n1 = n;
    p = cur_text_section->data + n1;
    inst_ptr = (u32*)(p);
    inst = swap_endianness32(*inst_ptr);
    n2 = (inst & 0x3ffffff);

    while (n2) {
      n1 = n2;
      p = cur_text_section->data + n1;
      inst_ptr = (u32*)(p);
      inst = swap_endianness32(*inst_ptr);
      n2 = (inst & 0x3ffffff);
      // printf("%s -> n=0x%x t=0x%x n1=0x%x n2=0x%x\n", __func__, n, t, n1, n2);
    }
    *inst_ptr =
         swap_endianness32(((inst >> 26) << 26) | ((n2 & 0x3ffffff) >> 2));
    t = n;
  }
  return t;
}

ST_FUNC int gjmp_cond(int op, int t) {

  u32 opcode = (u32)-1;

  switch (op) {

  case TOK_UGE: {
    opcode = opcode_bae;
    break;
  } /* 0x93 */
  case TOK_EQ: {
    opcode = opcode_beq;
    break;
  } /* 0x94 */
  case TOK_NE: {
    opcode = opcode_bne;
    break;
  } /* 0x95 */
  case TOK_ULE: {
    opcode = opcode_bbe;
    break;
  } /* 0x96 */
  case TOK_UGT: {
    opcode = opcode_bat;
    break;
  } /* 0x97 */
  case TOK_Nset: {
    assert(0 && "jmp cond not handled TOK_Nset");
    break;
  } /* 0x98 */
  case TOK_Nclear: {
    assert(0 && "jmp cond not handled TOK_Nclear");
    break;
  } /* 0x99 */
  case TOK_LT: {
    opcode = opcode_blt;
    break;
  } /* 0x9c */
  case TOK_GE: {
    opcode = opcode_bge;
    break;
  } /* 0x9d */
  case TOK_LE: {
    opcode = opcode_ble;
    break;
  } /* 0x9e */
  case TOK_GT: {
    opcode = opcode_bgt;
    break;
  } /* 0x9f */
  default: {
    assert(0 && " Unknown token for a jump instruction");
  }
  }
  t = oad(opcode, t);
  return t;
}

ST_FUNC void gen_opi(int op) {
  int r, fr, opcode, c;

  switch (op) {
  case '+':
  case TOK_ADDC1: /* add with carry generation */
    opcode = 0;
  gen_op8:
    if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
      /* constant case */
      vswap();
      r = gv(RC_INT);
      vswap();
      c = vtop->c.i;
      if (op == '+') {
        addi(r + 1, r + 1, c);
      } else if (op == '-') {
        subi(r + 1, r + 1, c);
      } else if (op == '^') {
        xori(r + 1, r + 1, c);
      } else if (op == '&') {
        andi(r + 1, r + 1, c);
      } else if (op == '|') {
        ori(r + 1, r + 1, c);
      } else {
        assert(0 && "op not handled");
      }
        // o(0x81);
        // oad(0xc0 | (opc << 3) | r, c);
    } else {
      gv2(RC_INT, RC_INT);
      r = vtop[-1].r;
      fr = vtop[0].r;
      // TODO: Wadde need to know what opc means in this context
      // NOTE:  Do believe opc is the '+' or '-'
      if (op == '+') {
        add(r + 1, r + 1, fr + 1);
      } else if (op == '&') {
        and(r + 1, r + 1, fr + 1);
      }else if (op == '|') {
        or(r + 1, r + 1, fr + 1);
      }else {
        sub(r + 1, r + 1, fr + 1);
      }

      // o((opc << 3) | 0x01);
      // o(0xc0 + r + fr * 8);
    }
    vtop--;
    if (op >= TOK_ULT && op <= TOK_GT) {
      vset_VT_CMP(op);
    }
    break;
  case '-':
  case TOK_SUBC1: /* sub with carry generation */
    opcode = 5;
    goto gen_op8;
  case TOK_ADDC2: /* add with carry use */
    tcc_error("%s poxim-gen not handled add with carry use", __func__);
    opcode = 2;
    goto gen_op8;
  case TOK_SUBC2: /* sub with carry use */
    tcc_error("%s poxim-gen not handled sub with carry use", __func__);
    opcode = 3;
    goto gen_op8;
  case '&':
    opcode = opcode_and;
    goto gen_op8;
  case '^':
    // tcc_error("%s poxim-gen not handled ^", __func__);
    opcode = opcode_xor;
    goto gen_op8;
  case '|':
    opcode = opcode_or;
    goto gen_op8;
  case '*':
    // tcc_error("%s poxim-gen not handled mul", __func__);
    gv2(RC_INT, RC_INT);
    r = vtop[-1].r;
    fr = vtop[0].r;
    vtop--;
    muls(r + 1, rt, r + 1, fr + 1);
    // o(0xaf0f); /* imul fr, r */
    // o(0xc0 + fr + r * 8);
    break;
  case TOK_SHL:
    opcode = 0b011;
    goto gen_shift;
  case TOK_SHR:
    opcode = 0b101;
    goto gen_shift;
  case TOK_SAR:
    opcode = 0b111;
  gen_shift:
    if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
      /* constant case */
      vswap();
      r = gv(RC_INT);
      vswap();
      c = vtop->c.i & 0b11111; /* we only has 5 bit for shifts*/
      if (c != vtop->c.i) {
        tcc_warning("Unprecise shift, beware that in poxim we only has "
                    "4 bits (0x1f) for constant shift");
      }
      /* shl/shr/sar $xxx, r */
      shift(opcode, r0, r + 1, r + 1, c);
    } else {
      tcc_error("%s poxim-gen not handled TOK_SAR not constant case", __func__);
      /* we generate the shift in ecx */
      gv2(RC_INT, r3);
      r = vtop[-1].r;
      o(0xd3); /* shl/shr/sar %cl, r */
      o(opcode | r);
    }
    vtop--;
    break;
  case '/':
  case TOK_UDIV:
  case TOK_PDIV:
  case '%':
  case TOK_UMOD:
  case TOK_UMULL: {

    u32 rmod, rdiv, rmul, rrest;
    /* first operand must be in eax */
    /* XXX: need better constraint for second operand */
    gv2(r1, r3);
    r = vtop[-1].r;
    fr = vtop[0].r;
    rdiv = r;
    rmod = r3;
    rmul = r1;
    rrest = r3;
    vtop--;
    save_reg(r3);
    /* save EAX too if used otherwise */
    save_reg_upstack(r1, 1);
    if (op == TOK_UMULL) {
      mul(rmul, rrest, r + 1, fr + 1);
      // o(0xf7); /* mul fr */
      // o(0xe0 + fr);
      vtop->r2 = r3;
      r = r1;
    } else {
      if (op == TOK_UDIV || op == TOK_UMOD) {
        // tcc_error("%s poxim-gen not handled TOK_UDIV", __func__);
        // o(0xf799); /* cltd, idiv fr, %eax */
        divu(rdiv + 1, rmod + 1, r + 1, fr + 1);
        // o(0xf7d231); /* xor %edx, %edx, div fr, %eax */
        // o(0xf0 + fr);
      } else {
        // tcc_error("op = 0x%x %s poxim-gen not handled TOK_UDIV", op,
        // __func__);
        // TODO: I'm not too sure about the state ot the vtop->r being
        // r1 or r3
        divs(rdiv + 1, rmod + 1, r + 1, fr + 1);
        // o(0xf799); /* cltd, idiv fr, %eax */
        // o(0xf8 + fr);
      }
      if (op == '%' || op == TOK_UMOD) {
        r = r3;
      } else if (op == TOK_UDIV) {
        r = r1;
      }
    }
    vtop->r = r;
    break;
  }
  case TOK_NE:
  case TOK_LE:
  case TOK_EQ:
  case TOK_LT:
  case TOK_UGT:
  case TOK_ULT:
  case TOK_GE:
  case TOK_GT: {
  gen_cmp:
    if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
      /* constant case */
      vswap();
      r = gv(RC_INT);
      vswap();
      c = vtop->c.i;
      cmpi(r + 1, c);

    } else {
      gv2(RC_INT, RC_INT);
      r = vtop[-1].r;
      fr = vtop[0].r;
      cmp(r + 1, fr + 1);
      // o((opc << 3) | 0x01);
      // o(0xc0 + r + fr * 8);
    }
    vtop--;
    /* I believe this is check if is a compare of some kinda*/
    if (op >= TOK_ULT && op <= TOK_GT) {
      vset_VT_CMP(op);
    }
    break;
  }
  default:
    tcc_error("%s poxim-gen does not support defaul op=0x%x", __func__, op);
    opcode = 7;
    goto gen_op8;
  }
}

/* generate a floating point operation 'v = t1 op t2' instruction. The
   two operands are guaranteed to have the same floating point type */
/* DONE: just not support float lol */
ST_FUNC void gen_opf(int op) {
  int a, ft, fc, swapped, r;
  tcc_error("%s, floating point not supported in poxim-gen for now", __func__);
}

/* convert integers to fp 't' type. Must handle 'int', 'unsigned int'
   and 'long long' cases. */
ST_FUNC void gen_cvt_itof(int t) {
  tcc_error("%s, floating point not supported in poxim-gen for now", __func__);
}

/* convert fp to int 't' type */
ST_FUNC void gen_cvt_ftoi(int t) {
  tcc_error("%s, floating point not supported in poxim-gen for now", __func__);
}

/* convert from one floating point type to another */
ST_FUNC void gen_cvt_ftof(int t) {
  tcc_error("%s, floating point not supported in poxim-gen for now", __func__);
  /* all we have to do on poxim is to put the something in register than call
   * memory to talk with fpu and some other shit that i don't wanna deal with
   * rn*/
  gv(RC_FLOAT);
}

/* char/short to int conversion */
ST_FUNC void gen_cvt_csti(int t) {
  int r, sz, xl;
  r = gv(RC_INT);
  sz = !(t & VT_UNSIGNED);
  xl = (t & VT_BTYPE) == VT_SHORT;
  o(0xc0b60f /* mov[sz] %a[xl], %eax */
    | (sz << 3 | xl) << 8 | (r << 3 | r) << 16);
}

/* increment tcov counter */
ST_FUNC void gen_increment_tcov(SValue *sv) {
  o(0x0583); /* addl $1, xxx */
  greloc(cur_text_section, sv->sym, ind, R_386_32);
  gen_le32(0);
  o(1);
  o(0x1583); /* addcl $0, xxx */
  greloc(cur_text_section, sv->sym, ind, R_386_32);
  gen_le32(4);
  g(0);
}

/* computed goto support */
ST_FUNC void ggoto(void) {
  gcall_or_jmp(1);
  vtop--;
}

/* bound check support functions */
#ifdef CONFIG_TCC_BCHECK

static void gen_bounds_prolog(void) {
  /* leave some room for bound checking code */
  func_bound_offset = lbounds_section->data_offset;
  func_bound_ind = ind;
  func_bound_add_epilog = 0;
  oad(0xb8, 0); /* lbound section pointer */
  oad(0xb8, 0); /* call to function */
}

static void gen_bounds_epilog(void) {
  addr_t saved_ind;
  addr_t *bounds_ptr;
  Sym *sym_data;
  int offset_modified = func_bound_offset != lbounds_section->data_offset;

  if (!offset_modified && !func_bound_add_epilog)
    return;

  /* add end of table info */
  bounds_ptr = section_ptr_add(lbounds_section, sizeof(addr_t));
  *bounds_ptr = 0;

  sym_data = get_sym_ref(&char_pointer_type, lbounds_section, func_bound_offset,
                         PTR_SIZE);

  /* generate bound local allocation */
  if (offset_modified) {
    saved_ind = ind;
    ind = func_bound_ind;
    greloc(cur_text_section, sym_data, ind + 1, R_386_32);
    ind = ind + 5;
    gen_static_call(TOK___bound_local_new);
    ind = saved_ind;
  }

  /* generate bound check local freeing */
  o(0x5250); /* save returned value, if any */
  greloc(cur_text_section, sym_data, ind + 1, R_386_32);
  oad(0xb8, 0); /* mov %eax, xxx */
  gen_static_call(TOK___bound_local_delete);
  o(0x585a); /* restore returned value, if any */
}
#endif

/* Save the stack pointer onto the stack */
ST_FUNC void gen_vla_sp_save(int addr) {
  tcc_error("gen_vla_sp_save");
  /* mov %esp, addr(%ebp)*/
  o(0x89);
  gen_modrm(sp, VT_LOCAL, NULL, addr);
}

/* Restore the SP from a location on the stack */
ST_FUNC void gen_vla_sp_restore(int addr) {
  tcc_error("gen_vla_sp_restore");
  o(0x8b);
  gen_modrm(sp, VT_LOCAL, NULL, addr);
}

/* Subtract from the stack pointer, and push the resulting value onto the stack
 */
ST_FUNC void gen_vla_alloc(CType *type, int align) {
  int use_call = 0;
  tcc_error("gen_vla_alloc");

#if defined(CONFIG_TCC_BCHECK)
  use_call = tcc_state->do_bounds_check;
#endif
#ifdef TCC_TARGET_PE /* alloca does more than just adjust %rsp on Windows */
  use_call = 1;
#endif
  if (use_call) {
    vpush_helper_func(TOK_alloca);
    vswap(); /* Move alloca ref past allocation size */
    gfunc_call(1);
  } else {
    int r;
    r = gv(RC_INT); /* allocation size */
    /* sub r,%rsp */
    o(0x2b);
    o(0xe0 | r);
    /* We align to 16 bytes rather than align */
    /* and ~15, %esp */
    o(0xf0e483);
    vpop();
  }
}

/* end of Poxim code generator */
/*************************************************************/
#endif
/*************************************************************/
