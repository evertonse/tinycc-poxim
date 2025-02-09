#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

// Macros to set and bits
#define bf_set_bit(bitfield, pos) ((bitfield) |= (1 << (pos)))
#define bf_clear_bit(bitfield, pos) ((bitfield) &= ~(1 << (pos)))
#define bf_toggle_bit(bitfield, pos) ((bitfield) ^= (1 << (pos)))
#define bf_read_bit(bitfield, pos) (((bitfield) >> (pos)) & 0x01)
#define branch(b) snprintf(assembly_text, count_of(assembly_text), "%-6s %d        # 0x%x",b, (i32)inst.I, (inst.I<<2)+(i32)(PC + 4));

// Force inline and no-inline macros
#if defined(__GNUC__) || defined(__GNUG__)
#define force_inline inline __attribute__((always_inline))
#define force_noinline __attribute__((noinline))
#elif defined(_MSC_VER)
#if _MSC_VER < 1300
#define force_inline
#else
#define force_inline __forceinline
#define force_noinline __declspec(noinline)
#endif
#endif
#define internal static
#define UNUSED 0x00
#define MEM_SIZE 1024
// I saw this typing in rust and casey's stream, i really like it
typedef enum { false = 0, true = 1 } bool;

typedef float f32;
typedef double f64;

typedef __uint128_t u128;
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef uint8_t b8;
typedef uint8_t byte;

typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;
typedef size_t usize;
typedef ptrdiff_t isize;

const u32 swap_endianness32(u32 num) {
  return ((num >> 24) & 0xFF) | ((num >> 8) & 0xFF00) |
         ((num << 8) & 0xFF0000) | ((num << 24) & 0xFF000000);
}

int read_bytes(void *buffer, size_t buffer_size, FILE *input_file) {
  size_t index = 0;
  int byte;
  uint8_t *data_buffer = (uint8_t *)buffer;

  if (input_file == NULL) {
    fprintf(stdout, "Error input_file is nullz\n" );
    exit(69);
  }

  while ((byte = fgetc(input_file)) != EOF) {
    if (index < buffer_size) {
      data_buffer[index] = (uint8_t)(byte & 0xFF);
      index++;
    } else {
      fprintf(stdout, "Error buffer too smal pepehand: max_buffer_size: %zu\n",
              buffer_size);
      exit(69);
    }
  }

  fclose(input_file);
  return 0;
}

u8 bit_at(u32 number, int nth_bit) {
  u32 bit;
  bit = (number >> nth_bit) & 1U;

  return (u8)bit;
}

void set_bit(u32 *number, u8 nth_bit, u8 choice) {
  if (choice) {
    *number |= (1UL << (nth_bit));
  } else {
    *number = *number & ~(1ul << (nth_bit));
  }
}

void fill_bits(u32 *number, u8 start, u8 end, u8 bit_choice) {
  for (i8 i = start; i >= end; i--) {
    set_bit(number, i, bit_choice);
  }
  return;
}

u32 bits_at(u32 number, u32 start, u32 end) {
  u32 bits = (number >> end);
  if (!(start > 31)) {
    fill_bits(&bits, 31, start - end + 1, 0);
  }
  return bits;
}

u32 extend_bit_at(u32 num, u8 bit_place) {
  u8 bit = bit_at(num, bit_place);
  fill_bits(&num, 31, bit_place, bit);
  return num;
}

char *str_upper(char *str) {
  char *start_of_str = str;
  while (*str) {
    *str = toupper(*str);
    str++;
  }
  return start_of_str;
}

// Why would a signed sizeof be more useful?
#ifndef size_of
#define size_of(x) (isize)(sizeof(x))
#endif

#ifndef count_of
#define count_of(x)                                                            \
  ((size_of(x) / size_of(0 [x])) / ((isize)(!(size_of(x) % size_of(0 [x])))))
#endif

#ifndef offset_of
#define offset_of(Type, element) ((isize) & (((Type *)0)->element))
#endif

#define timeit(msg, code)                                                      \
  do {                                                                         \
    clock_t start_time = clock();                                              \
    code;                                                                      \
    clock_t end_time = clock();                                                \
    printf(                                                                    \
        "\n[timeit] %s %lu us\n", msg,                                         \
        (unsigned long)(1000000 * (end_time - start_time) / CLOCKS_PER_SEC));  \
  } while (0)

internal void reg2str(char register_name[5], u8 id) {
  u8 len = 5;
  if (id < 26) {
    snprintf(register_name, len, "r%d", id);
  } else if (id == 26) {
    snprintf(register_name, len, "cr");
  } else if (id == 27) {
    snprintf(register_name, len, "ipc");
  } else if (id == 28) {
    snprintf(register_name, len, "ir");
  } else if (id == 29) {
    snprintf(register_name, len, "pc");
  } else if (id == 30) {
    snprintf(register_name, len, "sp");
  } else if (id == 31) {
    snprintf(register_name, len, "sr");
  }
}

typedef enum {
  //:> Format Type 'U'
  mov = 0b000000,
  movs = 0b000001,
  add = 0b000010,
  sub = 0b000011,

  // ==================================
  // Becaful Operation with the same code
  mul = 0b000100,  // FREE bits op.L: 0b000
  muls = 0b000100, // FREE bits op.L: 0b010
  div_ = 0b000100, // FREE bits op.L: 0b100
  divs = 0b000100, // FREE bits op.L: 0b110
  sll = 0b000100,  // FREE bits op.L: 0b001
  sla = 0b000100,  // FREE bits op.L: 0b011
  srl = 0b000100,  // FREE bits op.L: 0b101
  sra = 0b000100,  // FREE bits op.L: 0b111
                   //  ==================================
  cmp = 0B000101,
      and = 0B000110,
  or = 0B000111,
  not = 0b001000,
  xor = 0b001001,
  /*push pop*/
  push = 0b001010,
  pop = 0b001011,

  //:> Format Type 'F'
  addi = 0b010010,
  subi = 0b010011,
  muli = 0b010100,
  divi = 0b010101,
  modi = 0b010110,
  cmpi = 0b010111,
  /*Memory*/
  l8 = 0b011000,
  l16 = 0b011001,
  l32 = 0b011010,
  s8 = 0b011011,
  s16 = 0b011100,
  s32 = 0b011101,
  /*call return*/
  call = 0b011110,
  ret = 0b011111,

  //:> Format Type 'S'
  bae = 0b101010,
  bat = 0b101011,
  bbe = 0b101100,
  bbt = 0b101101,
  beq = 0b101110,
  bge = 0b101111,
  bgt = 0b110000,
  biv = 0b110001,
  ble = 0b110010,
  blt = 0b110011,
  bne = 0b110100,
  bni = 0b110101,
  bnz = 0b110110,
  bun = 0b110111,
  bzd = 0b111000,
  int_ = 0b111111,
  calli = 0b111001,

  reti = 0b100000,

  cbr = 0b100001, // equals last bit (0th bit) == 0
  sbr = 0b100001  // equals last bit (0th bit) == 1

} PoximOperation;

// GLOBAL u32 registers[32] = {};
//
// // R0 must always be zero (0);
// u32& R0 = registers[0];
//
// // SR => Status Register
// u32& SR = registers[31];
//
// // SP => Stack Pointer
// u32& SP = registers[30];
//
// // PC => Aponta para as instru∩┐╜∩┐╜es < controla fluxo
// u32& PC = registers[29];
// // Pseudo 'PC' to print correctly
// u32  PPC = 0x0;
// u32  SSP = 0x0;
//
// // IR => Intruction Register < Armazena a intru∩┐╜∩┐╜o a ser carregada
// u32& IR = registers[28];
//
// u32& IPC = registers[27];
//
// u32& CR = registers[26];

typedef struct {
  PoximOperation O;
  u32 Z, X, Y, L, I;
  u32 partial;
  char format;
} PoximInstruction;

u32 PC = 0;
void create_operands(PoximInstruction *this, u32 inst) {
  switch (this->format) {
  case 'U': {
    this->Z = (i32)((inst & 0b00000011111000000000000000000000) >> 21);
    this->X = (i32)((inst & 0b00000000000111110000000000000000) >> 16);
    this->Y = (i32)((inst & 0b00000000000000001111100000000000) >> 11);
    this->L = (i32)((inst & 0b00000000000000000000011111111111) >> 0);
    this->I = UNUSED;
    break;
  }
  case 'F': {
    this->Z = (i32)((inst & 0b00000011111000000000000000000000) >> 21);
    this->X = (i32)((inst & 0b00000000000111110000000000000000) >> 16);
    this->Y = UNUSED;
    this->L = UNUSED;
    this->I = (i32)((inst & 0b00000000000000001111111111111111) >> 0);
    break;
  }
  case 'S': {
    this->Z = UNUSED;
    this->X = UNUSED;
    this->Y = UNUSED;
    this->L = UNUSED;
    this->I = (i32)((inst & 0b00000011111111111111111111111111) >> 0);
    break;
  }
  default: {

    puts("[[WARNING]] Invalid Expression Format");
    break;
  }
  }
}

internal PoximInstruction parse_instruction(u32 inst) {
  PoximInstruction result = {0};
  result.O = ((inst & 0b11111100000000000000000000000000) >> 26);
  // If the first bit is 1.
  if ((inst >> 31) == 0b1) {
    result.format = 'S';
    // if the first is a 0, and the second bit 1
  } else if ((inst >> 30) == 0b1) {
    result.format = 'F';
  } else {
    result.format = 'U';
  }

  // Catching Outliers
  if (result.O == 0b100001) {
    result.format = 'F';
  }

  create_operands(&result, inst);

  return result;
}

internal void sprint_instruction(char src[512], PoximInstruction inst) {
  char rl[8], rz[8], rx[8], ry[8];
  char assembly_text[128];
	char* bop;

  switch (inst.O) {
  case mov: {
    // mov r1,1193046           	R1=0x00123456
    u32 mask = 0b00000000000111111111111111111111ul;
    u32 partial = mask & ((inst.X << 16) | (inst.Y << 11) | (inst.L << 0));
    reg2str(rz, inst.Z);
    if (rz[1] == '0' && partial == 0) {
      snprintf(assembly_text, count_of(assembly_text), "%-6s","nop");
    } else {
      snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %u","mov ", rz, partial);
    }
    break;
  }
  case movs: {
    // movs r2,-1048576         	R2=0xFFF00000
    u32 mask = 0b00000000000111111111111111111111ull;
    u32 partial = mask & ((inst.X << 16) | (inst.Y << 11) | (inst.L << 0));
    fill_bits(&partial, 31, 20, bit_at(inst.X, 4));
    reg2str(rz, inst.Z);
    snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %d","movs ", rz,
             partial);
    break;
  }
  case add: {
    reg2str(rz, inst.Z);
    reg2str(rx, inst.X);
    if (inst.Y == 0) {
      snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s","mov ", rz, rx);
    } else {
      reg2str(ry, inst.Y);
      snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s, %s","add ", rz, rx,
               ry);
    }
    break;
  }
  case sub: {
    // sub r4, r2, r3             	R4 = R2 - R3 = 0x7FFDCBAA, SR =
    // 0x00000008
    reg2str(rz, inst.Z);
    reg2str(rx, inst.X);
    if (inst.Y == 0) {
      snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s","mov ", rz, rx);
    } else {
      reg2str(ry, inst.Y);
      snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s, %s","sub ", rz, rx,
               ry);
    }
    break;
  }
  case       /*:>the first 3 bits of inst.L are:*/
      mul |  // mul	 = op.L: 0b000
      muls | // muls = op.L: 0b010
      div_ | // div	 = op.L: 0b100
      divs | // divs = op.L: 0b110
      sll |  // sll	 = op.L: 0b001
      sla |  // sla	 = op.L: 0b011
      srl |  // srl	 = op.L: 0b101
      sra:   // sra  = op.L: 0b111
  {

    switch (bits_at(inst.L, 10, 8)) {
    // mul
    case 0b000: {
      // mul r0,r5,r4,r3
      // R0:R5=R4*R3=0x0000000023F4F31C,SR=0x00000008
      u32 L_4_0 = (u32)bits_at(inst.L, 4, 0);
      reg2str(rl, L_4_0);
      reg2str(rz, inst.Z);
      reg2str(rx, inst.X);
      reg2str(ry, inst.Y);
      snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s, %s, %s","mul ", rl,
               rz, rx, ry);
      break;
    }
    case 0b010: // muls
    {
      // muls r0,r7,r6,r5
      // R0:R7=R6*R5=0x0000000000000000,SR=0x00000040
      u32 L_4_0 = (u32)bits_at(inst.L, 4, 0);
      reg2str(rl, L_4_0);
      reg2str(rz, inst.Z);
      reg2str(rx, inst.X);
      reg2str(ry, inst.Y);
      snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s, %s, %s","muls ",
               rl, rz, rx, ry);
      break;
    }
    case 0b100: /*div */
    {
      // div r0,r9,r8,r7
      // R0=R8%R7=0x00000000,R9=R8/R7=0x00000000,SR=0x00000060
      char rz[5], rx[5], ry[5], rl[5];
      u32 r_index_L = bits_at(inst.L, 4, 0);
      reg2str(rz, inst.Z);
      reg2str(rx, inst.X);
      reg2str(ry, inst.Y);
      reg2str(rl, r_index_L);

      snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s, %s, %s","div ", rl,
               rz, rx, ry);
      break;
    }
    case 0b110: /*divs*/
    {
      // divs r10,r11,r9,r8
      // R10=R9%R8=0x00000000,R11=R9/R8=0x00000000,SR=0x00000060
      u32 r_index_L = bits_at(inst.L, 4, 0);
      reg2str(rz, inst.Z);
      reg2str(rx, inst.X);
      reg2str(ry, inst.Y);
      reg2str(rl, r_index_L);

      snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s, %s, %s","divs ",
               rl, rz, rx, ry);
      break;
    }
    case 0b001: /*sll */
    {
      // sll r6,r5,r5,0
      // R6:R5=R6:R5<<1=0x0000000047E9E638,SR=0x00000008
      u32 L_4_0 = (u32)bits_at(inst.L, 4, 0);
      reg2str(rz, inst.Z);
      reg2str(rx, inst.X);
      reg2str(ry, inst.Y);

      snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s, %s, %d","sll ", rz,
               rx, ry, L_4_0);
      break;
    }
    case 0b011: /*sla */
    {
      // sla r0,r2,r2,10
      // R0:R2=R0:R2<<11=0x0000000080000000,SR=0x00000001
      u32 L_4_0 = (u32)bits_at(inst.L, 4, 0);
      reg2str(rz, inst.Z);
      reg2str(rx, inst.X);
      reg2str(ry, inst.Y);

      snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s, %s, %u","sla ", rz,
               rx, ry, L_4_0);

      break;
    }
    case 0b101: /*srl */
    {
      // srl r10,r9,r9,2
      // R10:R9=R10:R9>>3=0x0000000000000000,SR=0x00000060
      u32 L_4_0 = (u32)bits_at(inst.L, 4, 0);
      reg2str(rz, inst.Z);
      reg2str(rx, inst.X);
      reg2str(ry, inst.Y);

      snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s, %s, %d","srl ", rz,
               rx, ry, L_4_0);
      break;
    }
    case 0b111: /*sra */
    {
      // sra r12,r10,r10,3
      // R12:R10=R12:R10>>4=0x0000000000000000,SR=0x0000
      u32 L_4_0 = (u32)bits_at(inst.L, 4, 0);
      reg2str(rz, inst.Z);
      reg2str(rx, inst.X);
      reg2str(ry, inst.Y);

      snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s, %s, %d    # 0x%x","sra ", rz,
               rx, ry,L_4_0, L_4_0+1);
      break;
    }
    }
    break;
  }

  case cmp: {
    // cmp ir,pc                	SR=0x00000020
    reg2str(rx, inst.X);
    reg2str(ry, inst.Y);
    snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s","cmp ", rx, ry);
    break;
  }

  case and: {
    // and r13,r1,r5            	R13=R1&R5=0x00002410,SR=0x00000020
    reg2str(rz, inst.Z);
    reg2str(rx, inst.X);
    reg2str(ry, inst.Y);

    snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s, %s","and ", rz, rx,
             ry);
    break;
  }
  case or: {
    // or r14,r2,r6             	R14=R2|R6=0x80000000,SR=0x00000030
    reg2str(rz, inst.Z);
    reg2str(rx, inst.X);
    reg2str(ry, inst.Y);
    snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s, %s","or ", rz, rx,
             ry);
    break;
  }
  case not: {
    // not r15,r7               	R15=~R7=0xFFFFFFFF,SR=0x00000030
    reg2str(rz, inst.Z);
    reg2str(rx, inst.X);
    snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s","not ", rz, rx);
    break;
  }
  case xor: {
    // xor r16, r16, r8           	R16 = R16 ^ R8 = 0x00000000, SR =
    // 0x00000060
    reg2str(rz, inst.Z);
    reg2str(rx, inst.X);
    reg2str(ry, inst.Y);
    snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s, %s","xor ", rz, rx,
             ry);
    break;
  }
  //:> Format Type 'F'
  case addi: {
    // addi r1,r1,32771                R1 = R1 + 0x00008003 = 0xFFFF8003,SR
    inst.I = extend_bit_at(inst.I, 15);
    reg2str(rz, inst.Z);
    reg2str(rx, inst.X);

    snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s, %d","addi ", rz, rx,
             (i32)inst.I);
    break;
  }

  case subi: {
    // subi r18, r18, -1          	R18 = R18 - 0xFFFFFFFF = 0x00000001, SR
    // = 0x00000020
    inst.I = extend_bit_at(inst.I, 15);
    reg2str(rz, inst.Z);
    reg2str(rx, inst.X);
    snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s, %d","subi ", rz, rx,
             (i32)inst.I);
    break;
  }
  case muli: {
    // muli r19, r17, 2           	R19 = R17 * 0x00000002 = 0x00000002, SR
    // = 0x00000020
    inst.I = extend_bit_at(inst.I, 15);
    reg2str(rz, inst.Z);
    reg2str(rx, inst.X);
    snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s, %d","muli ", rz, rx,
             (i32)inst.I);
    break;
  }
  case divi: {
    // divi r20, r19, 2           	R20 = R19 / 0x00000002 = 0x00000001, SR
    // = 0x00000000
    inst.I = extend_bit_at(inst.I, 15);
    reg2str(rz, inst.Z);
    reg2str(rx, inst.X);
    snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s, %d","divi ", rz, rx,
             (i32)inst.I);
    break;
  }
  case modi: {
    // modi r20, r19, 2           	R20 = R19 mod  0x00000002 = 0x00000001,
    // SR = 0x00000000
    inst.I = extend_bit_at(inst.I, 15);
    reg2str(rz, inst.Z);
    reg2str(rx, inst.X);
    snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %s, %d","modi ", rz, rx,
             (i32)inst.I);
    break;
  }

  case cmpi: {
    // cmpi r20, r19, 2           	CMPI = R19 - 0x00000001 = 0x00000001, SR
    // = 0x00000000
    inst.I = extend_bit_at(inst.I, 15);
    reg2str(rx, inst.X);
    snprintf(assembly_text, count_of(assembly_text), "%-6s %s, %d","cmpi ", rx,
             (i32)inst.I);
    break;
  }

  case l8: {
    inst.I = extend_bit_at(inst.I, 15);
		bop = (i32)inst.I < 0 ? "" : "+";
    reg2str(rz, inst.Z);
    reg2str(rx, inst.X);
    snprintf(assembly_text, count_of(assembly_text), "%-6s %s, [%s%s%d]","l8 ", rz,
             rx, bop, inst.I);
    break;
  }

  case l16: {
    inst.I = extend_bit_at(inst.I, 15);
		bop = (i32)inst.I < 0 ? "" : "+";
    reg2str(rz, inst.Z);
    reg2str(rx, inst.X);
    snprintf(assembly_text, count_of(assembly_text), "%-6s %s, [%s%s%d]<<1","l16 ",
             rz, rx, bop, inst.I);
    break;
  }
  case l32: {
    inst.I = extend_bit_at(inst.I, 15);
		bop = (i32)inst.I < 0 ? "" : "+";
    reg2str(rz, inst.Z);
    reg2str(rx, inst.X);
    snprintf(assembly_text, count_of(assembly_text), "%-6s %s, [%s%s%d]<<2","l32 ",
             rz, rx, bop, inst.I);
    break;
  }
  case s8: {
    inst.I = extend_bit_at(inst.I, 15);
		bop = (i32)inst.I < 0 ? "" : "+";
    reg2str(rz, inst.Z);
    reg2str(rx, inst.X);
    snprintf(assembly_text, count_of(assembly_text), "%-6s [%s%s%d], %s","s8 ", rx,
             bop, inst.I, rz);
    break;
  }
  case s16: {
    inst.I = extend_bit_at(inst.I, 15);
		bop = (i32)inst.I < 0 ? "" : "+";
    reg2str(rz, inst.Z);
    reg2str(rx, inst.X);
    snprintf(assembly_text, count_of(assembly_text), "%-6s [%s%s%d]<<1, %s","s16 ", rx,
             bop, inst.I, rz);
    break;
  }
  case s32: {
    inst.I = extend_bit_at(inst.I, 15);
		bop = (i32)inst.I < 0 ? "" : "+";
    reg2str(rz, inst.Z);
    reg2str(rx, inst.X);
    snprintf(assembly_text, count_of(assembly_text), "%-6s [%s%s%d]<<2, %s","s32 ", rx,
             bop, inst.I, rz);
    break;
  }
  case bae: {
    inst.I = extend_bit_at(inst.I, 25);
    branch("bae");
    break;
  }
  case bat: {

    inst.I = extend_bit_at(inst.I, 25);
    branch("bat");
    break;
  }
  case bbe: {
    inst.I = extend_bit_at(inst.I, 25);
    branch("bbe");
    break;
  }
  case bbt: {
    inst.I = extend_bit_at(inst.I, 25);
    branch("bbt");
    break;
  }
  case beq: {
    inst.I = extend_bit_at(inst.I, 25);
    branch("beq");
    break;
  }
  case bge: {

    inst.I = extend_bit_at(inst.I, 25);
    branch("bge");
    break;
  }
  case bgt: {

    inst.I = extend_bit_at(inst.I, 25);
    branch("bgt");
    break;
  }
  case biv: {

    inst.I = extend_bit_at(inst.I, 25);
    branch("biv");
    break;
  }
  case ble: {
    inst.I = extend_bit_at(inst.I, 25);
    branch("ble");
    break;
  }
  case blt: {
    inst.I = extend_bit_at(inst.I, 25);
    branch("blt");
    break;
  }
  case bne: {
    inst.I = extend_bit_at(inst.I, 25);
    branch("bne");
    break;
  }
  case bni: {
    inst.I = extend_bit_at(inst.I, 25);
    branch("bni");
    break;
  }
  case bnz: {
    inst.I = extend_bit_at(inst.I, 25);
    branch("bnz");
    break;
  }
  case bun: {
    inst.I = extend_bit_at(inst.I, 25);
    branch("bun");
    // snprintf(assembly_text, count_of(assembly_text), "%-6s %d    # 0x%x","bun", (i32)inst.I, (inst.I<<2)+(i32)(PC + 4));
    break;
  }
  case bzd: {
    inst.I = extend_bit_at(inst.I, 25);
    branch("bzd");
    break;
  }
  case int_: {
    // 0x000000D4:	int 0 CR=0x00000000,PC=0x00000000
    snprintf(assembly_text, count_of(assembly_text), "%-6s %d", "int", inst.I);
    break;
  }
  //::>> FLOW CONTROL:
  case call: {
    inst.I = extend_bit_at(inst.I, 15);
    reg2str(rx, inst.X);
    // call[r0 + 16]             	PC=0x00000040,MEM[0x00007FFC]=0x000002C4
    snprintf(assembly_text, count_of(assembly_text), "%-6s [%s+%d]", "call", rx,
             (i32)(inst.I));
    break;
  }
  case calli: {
    // call[r0 + 16]             	PC=0x00000040,MEM[0x00007FFC]=0x000002C4
    inst.I = extend_bit_at(inst.I, 25);
    snprintf(assembly_text, count_of(assembly_text), "%-6s %d<<2        # 0x%x", "call", inst.I, (inst.I<<2)+(i32)(PC + 4));
    break;
  }
  case ret: {
    // ret                      	PC=MEM[0x00007FFC]=0x000002C4
    snprintf(assembly_text, count_of(assembly_text), "%-6s", "ret");
    break;
  }
  case push: {
    u32 X = inst.X;
    u32 Y = inst.Y;
    u32 Z = inst.Z;

    u32 L = inst.L;
    u32 W = bits_at(L, 4, 0);
    u32 V = bits_at(L, 10, 6);
    u32 registers_to_push[] = {V, W, X, Y, Z};

    char reg[5];
    int chars_written =
        snprintf(assembly_text, count_of(assembly_text), "%-6s ","push");
    for (u32 i = 0; i < count_of(registers_to_push); i++) {
      u32 index = registers_to_push[i];
      reg2str(reg, index);
      if (index == 0) {
        break;
      }

      chars_written +=
          snprintf(assembly_text + chars_written,
                   count_of(assembly_text) - chars_written, "%s", reg);
    }

    break;
  }
  case pop: {
    u32 X = inst.X;
    u32 Y = inst.Y;
    u32 Z = inst.Z;

    u32 L = inst.L;
    u32 W = bits_at(L, 4, 0);
    u32 V = bits_at(L, 10, 6);
    u32 registers_to_push[] = {V, W, X, Y, Z};

    char reg[5];
    int chars_written = snprintf(assembly_text, count_of(assembly_text), "%-6s ","pop");
    for (u32 i = 0; i < count_of(registers_to_push); i++) {
      u32 index = registers_to_push[i];
      reg2str(reg, index);
      if (index == 0) {
        break;
      }

      chars_written +=
          snprintf(assembly_text + chars_written,
                   count_of(assembly_text) - chars_written, "%s", reg);
    }

    break;
  }
  // cbr
  case sbr: {
    // 0x00000034:	sbr sr[1]                	SR=0x00000002
    char instruction[5];
    if (bit_at(inst.I, 0) == 0) {
      sprintf(instruction, "cbr");
    } else {
      sprintf(instruction, "sbr");
    }

    reg2str(rz, inst.Z);
    snprintf(assembly_text, count_of(assembly_text), "%-6s %s[%u]", instruction,
             rz, inst.X);
    break;
  }
  case reti: {
    // reti
    // IPC=MEM[0x????????]=0x????????,CR=MEM[0x????????]=0x????????,PC=MEM[0x????????]=0x????????
    snprintf(assembly_text, count_of(assembly_text), "%-6s", "reti");
    break;
  }

  default: {
    sprintf(assembly_text, "unkown");
    break;
  }
  }
  snprintf(src, count_of(assembly_text), "%-6s", assembly_text);
}

typedef struct {
  union {
    u32 instructions[32 * MEM_SIZE / 4];
    u8 RAM8[32 * MEM_SIZE];
    u16 RAM16[32 * MEM_SIZE / 2];
    u32 RAM32[32 * MEM_SIZE / 4];
  };
} PoximMemory;

PoximMemory mem;
FILE *input, *output;

// '--bin' flag reads as binary
bool as_bin = false;
const char *as_bin_flag = "--bin";

int main(int argc, char *argv[]) {
#if POXIM_DEBUG
  for (int i = 0; i < argc; i++) {
    printf("arg[%d] %s \n", i, argv[i]);
  }
#endif
  if (argc > 1 && strcmp(argv[1], as_bin_flag) == 0) {
    as_bin = true;
    // Shift the arguments to skip the '--bin' flag
    for (int i = 1; i < argc - 1; i++) {
      argv[i] = argv[i + 1];
    }
    argv[argc - 1] = NULL;
    argc--;
  }

  input = fopen(argv[1], as_bin ? "rb" : "r");
  if (input == NULL) {
    printf("usage: poxim-dump [%s] <file.input> [file.output]\n", as_bin_flag);
    return 24;
  }
  {
    uint32_t dword = 0, n = 0;
    if (!as_bin) {
      while (fscanf(input, "0x%8X\n", &dword) == 1) {
        mem.RAM32[n++] = dword;
#if POXIM_DEBUG
        printf("mem.RAM32[n++] = %8x; dword = %8x\n", mem.RAM32[n - 1], dword);
#endif
      }
    } else {
      read_bytes(mem.RAM8, count_of(mem.RAM8), input);
    }

    if (output == NULL) {
      output = stdout;
    } else {
      output = fopen(argv[2], "w");
    }

    {
      char text[512];
      u32 previous = 0;
      PoximInstruction inst;

      for (size_t i = 0; i < count_of(mem.RAM32); i++) {
        PC = i*4;
				previous = i == 0 ? 0 : mem.RAM32[i-1];
        if (mem.RAM32[i] == 0) {
					if(previous != 0)  {
						fprintf(output, "  ...\n");
					}
          continue;
				}
        if (as_bin) {
          mem.RAM32[i] = swap_endianness32(mem.RAM32[i]);
        }
        inst = parse_instruction(mem.RAM32[i]);
        sprint_instruction(text, inst);
        fprintf(output,
                "%4x:    "
                "%02x %02x %02x %02x       "
                "%s\n",
                (u32)(i * 4), 
								mem.RAM8[i * 4 + 3], mem.RAM8[i * 4 + 2],
                mem.RAM8[i * 4 + 1], mem.RAM8[i * 4 + 0],
								text
				);
      }
    }
    fflush(output);

    return 0;
  }
}
