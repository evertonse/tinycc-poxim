#ifdef TARGET_DEFS_ONLY

#define EM_TCC_TARGET EM_386

/* relocation type for 32 bit data relocation */
#define R_DATA_32 R_386_32
#define R_DATA_PTR R_386_32
#define R_JMP_SLOT R_386_JMP_SLOT
#define R_GLOB_DAT R_386_GLOB_DAT
#define R_COPY R_386_COPY
#define R_RELATIVE R_386_RELATIVE

#define R_NUM R_386_NUM

#define ELF_START_ADDR 0x08048000
#define ELF_PAGE_SIZE 0x1000

#define PCRELATIVE_DLLPLT 0
#define RELOCATE_DLLPLT 1

#else /* !TARGET_DEFS_ONLY */
#include "poxim.h"
#include "tcc.h"

#ifdef NEED_RELOC_TYPE
/* Returns 1 for a code relocation, 0 for a data relocation. For unknown
   relocations, returns -1. */
int code_reloc(int reloc_type) {
  switch (reloc_type) {
  case R_386_RELATIVE:
  case R_386_16:
  case R_386_32:
  case R_386_GOTPC:
  case R_386_GOTOFF:
  case R_386_GOT32:
  case R_386_GOT32X:
  case R_386_GLOB_DAT:
  case R_386_COPY:
  case R_386_TLS_GD:
  case R_386_TLS_LDM:
  case R_386_TLS_LDO_32:
  case R_386_TLS_LE:
    return 0;

  case R_386_PC16:
  case R_386_PC32:
  case R_386_PLT32:
  case R_386_JMP_SLOT:
    return 1;
  }
  return -1;
}

/* Returns an enumerator to describe whether and when the relocation needs a
   GOT and/or PLT entry to be created. See tcc.h for a description of the
   different values. */
int gotplt_entry_type(int reloc_type) {
  switch (reloc_type) {
  case R_386_RELATIVE:
  case R_386_16:
  case R_386_GLOB_DAT:
  case R_386_JMP_SLOT:
  case R_386_COPY:
    return NO_GOTPLT_ENTRY;

  case R_386_32:
    /* This relocations shouldn't normally need GOT or PLT
       slots if it weren't for simplicity in the code generator.
       See our caller for comments.  */
    return AUTO_GOTPLT_ENTRY;

  case R_386_PC16:
  case R_386_PC32:
    return AUTO_GOTPLT_ENTRY;

  case R_386_GOTPC:
  case R_386_GOTOFF:
    return BUILD_GOT_ONLY;

  case R_386_GOT32:
  case R_386_GOT32X:
  case R_386_PLT32:
  case R_386_TLS_GD:
  case R_386_TLS_LDM:
  case R_386_TLS_LDO_32:
  case R_386_TLS_LE:
    return ALWAYS_GOTPLT_ENTRY;
  }
  return -1;
}

#ifdef NEED_BUILD_GOT
ST_FUNC unsigned create_plt_entry(TCCState *s1, unsigned got_offset,
                                  struct sym_attr *attr) {
  Section *plt = s1->plt;
  uint8_t *p;
  int modrm;
  unsigned plt_offset, relofs;
  // assert(0 && "poxim plt entry not implemented");

  /* on i386 if we build a DLL, we add a %ebx offset */
  if (s1->output_type & TCC_OUTPUT_DYN)
    modrm = 0xa3;
  else
    modrm = 0x25;

  /* empty PLT: create PLT0 entry that pushes the library identifier
     (GOT + PTR_SIZE) and jumps to ld.so resolution routine
     (GOT + 2 * PTR_SIZE) */
  if (plt->data_offset == 0) {
    p = section_ptr_add(plt, 16);
    p[0] = 0xff; /* pushl got + PTR_SIZE */
    p[1] = modrm + 0x10;
    write32le(p + 2, PTR_SIZE);
    p[6] = 0xff; /* jmp *(got + PTR_SIZE * 2) */
    p[7] = modrm;
    write32le(p + 8, PTR_SIZE * 2);
  }
  plt_offset = plt->data_offset;

  /* The PLT slot refers to the relocation entry it needs via offset.
     The reloc entry is created below, so its offset is the current
     data_offset */
  relofs = s1->plt->reloc ? s1->plt->reloc->data_offset : 0;

  /* Jump to GOT entry where ld.so initially put the address of ip + 4 */
  p = section_ptr_add(plt, 16);
  p[0] = 0xff; /* jmp *(got + x) */
  p[1] = modrm;
  write32le(p + 2, got_offset);
  p[6] = 0x68; /* push $xxx */
  write32le(p + 7, relofs - sizeof(ElfW_Rel));
  p[11] = 0xe9; /* jmp plt_start */
  write32le(p + 12, -(plt->data_offset));
  return plt_offset;
}

/* relocate the PLT: compute addresses and offsets in the PLT now that final
   address for PLT and GOT are known (see fill_program_header) */
ST_FUNC void relocate_plt(TCCState *s1) {
  uint8_t *p, *p_end;

  if (!s1->plt)
    return;

  p = s1->plt->data;
  p_end = p + s1->plt->data_offset;

  if (!(s1->output_type & TCC_OUTPUT_DYN) && p < p_end) {
    add32le(p + 2, s1->got->sh_addr);
    add32le(p + 8, s1->got->sh_addr);
    p += 16;
    while (p < p_end) {
      add32le(p + 2, s1->got->sh_addr);
      p += 16;
    }
  }

  if (s1->plt->reloc) {
    ElfW_Rel *rel;
    int x = s1->plt->sh_addr + 16 + 6;
    p = s1->got->data;
    for_each_elem(s1->plt->reloc, 0, rel, ElfW_Rel) {
      write32le(p + rel->r_offset, x);
      x += 16;
    }
  }
}
#endif
#endif

/*
  NOTE(EVERTON): Took me year to find where relocation occurs  goddanm
  mostly because the concept of relocation inst so well understood by me
  see : https://en.wikipedia.org/wiki/Relocation_%28computing%29
  NOTE: the code section is the s1->sections[1] , got it ?  don't forget this
*/
void relocate(TCCState *s1, ElfW_Rel *rel, int type, unsigned char *ptr,
              addr_t addr, addr_t val) {
  int sym_index, esym_index;

  sym_index = ELFW(R_SYM)(rel->r_info);

  assert(type == R_386_32 || type == R_386_PC32);

  switch (type) {
  case R_386_32:
    if (s1->output_type & TCC_OUTPUT_DYN) {
      esym_index = get_sym_attr(s1, sym_index, 0)->dyn_index;
      qrel->r_offset = rel->r_offset;
      if (esym_index) {
        qrel->r_info = ELFW(R_INFO)(esym_index, R_386_32);
        qrel++;
        return;
      } else {
        qrel->r_info = ELFW(R_INFO)(0, R_386_RELATIVE);
        qrel++;
      }
    }
    {

      u32 *inst_ptr = (u32 *)(ptr - 4);
      u32 *inst_ptr2 = (u32 *)(ptr);
      // XXX: Val is always fater ELF_START_ADDR
      u32 abs_val = val - ELF_START_ADDR;
      u32 inst = swap_endianness32(*inst_ptr);
      i32 opcode = bits_at(inst, 31, 26);
      i32 rest = 0;
      b8 handled = 0;

      switch (opcode) {
      case opcode_l32: {
        i32 rz = bits_at(inst, 25, 21);
        abs_val >>= 2;
        rest = rz << 21 | rt << 16;
        break;
      }
      case opcode_s32: {
        i32 rz = bits_at(inst, 25, 21);
        abs_val >>= 2;
        rest = rz << 21 | rt << 16;
        break;
      }
      case opcode_call: {
        abs_val >>= 2;
        rest = rt << 16;
        break;
      }
      case opcode_mov:
      case opcode_movs: {
        i32 rz = bits_at(inst, 25, 21);
        handled = 1;
        abs_val = (abs_val >> 2) & 0x1fffff;
        rest = (rz & 0b11111) << 21 | rt << 16;
        *inst_ptr = swap_endianness32(opcode_mov << 26 
																			| (rz & 0b11111) << 21 
																			| (abs_val & 0xFFFFFFFF));
        // *(inst_ptr2) = swap_endianness32(opcode_add << 26  | rest);

        break;
      }
      default:
        assert(0 && "Linking unhandled case for poxim opcode relocation");
      }

      if (!handled) {

        *inst_ptr = swap_endianness32(0x00 << 26 | (rt & 0b11111) << 21 |
                                      (abs_val & 0xFFFFFFFF));
        *(inst_ptr2) = swap_endianness32(opcode << 26 | rest);
      }
    }
    return;
    if (s1->output_type == TCC_OUTPUT_DLL) {
    case R_386_PC32: /* PC relative 32 bit */ /* DLL relocation */
      esym_index = get_sym_attr(s1, sym_index, 0)->dyn_index;
      if (esym_index) {
        qrel->r_offset = rel->r_offset;
        qrel->r_info = ELFW(R_INFO)(esym_index, R_386_PC32);
        qrel++;
        return;
      }
    }

    {
      // XXX: You  need to understand this, 'val'-'addr' gives you how
      // many bytes
      //  from 'ptr' is the address of the function that you need to call
      u32 *inst_ptr = (u32 *)(ptr - 1);
      u32 inst = swap_endianness32(*inst_ptr);
      i32 a = extend_bit_at(inst & 0x03FFFFFF, 25);
      i32 b = extend_bit_at((val - addr) & 0x03FFFFFF, 25);
      i32 offset = ((((b) >> 2) & 0x03FFFFFF));
      *inst_ptr = swap_endianness32(bits_at(inst, 31, 26) << 26 | offset);
      // add32le(ptr, val - addr);
    }
    return;

  case R_386_PLT32:
    add32le(ptr, val - addr);
    return;
  case R_386_GLOB_DAT:
  case R_386_JMP_SLOT:
    write32le(ptr, val);
    return;
  case R_386_GOTPC:
    add32le(ptr, s1->got->sh_addr - addr);
    return;
  case R_386_GOTOFF:
    add32le(ptr, val - s1->got->sh_addr);
    return;
  case R_386_GOT32:
  case R_386_GOT32X:
    /* we load the got offset */
    add32le(ptr, get_sym_attr(s1, sym_index, 0)->got_offset);
    return;
  case R_386_16:
    if (s1->output_format != TCC_OUTPUT_FORMAT_BINARY) {
    output_file:
      tcc_error_noabort("can only produce 16-bit binary files");
    }
    write16le(ptr, read16le(ptr) + val);
    return;
  case R_386_PC16:
    if (s1->output_format != TCC_OUTPUT_FORMAT_BINARY)
      goto output_file;
    write16le(ptr, read16le(ptr) + val - addr);
    return;
  case R_386_RELATIVE:
#ifdef TCC_TARGET_PE
    add32le(ptr, val - s1->pe_imagebase);
#endif
    /* do nothing */
    return;
  case R_386_COPY:
    /* This relocation must copy initialized data from the library
    to the program .bss segment. Currently made like for ARM
    (to remove noise of default case). Is this true?
    */
    return;
  case R_386_TLS_GD: {
    static const unsigned char expect[] = {/* lea 0(,%ebx,1),%eax */
                                           0x8d, 0x04, 0x1d, 0x00, 0x00, 0x00,
                                           0x00,
                                           /* call __tls_get_addr@PLT */
                                           0xe8, 0xfc, 0xff, 0xff, 0xff};
    static const unsigned char replace[] = {/* mov %gs:0,%eax */
                                            0x65, 0xa1, 0x00, 0x00, 0x00, 0x00,
                                            /* sub 0,%eax */
                                            0x81, 0xe8, 0x00, 0x00, 0x00, 0x00};

    if (memcmp(ptr - 3, expect, sizeof(expect)) == 0) {
      ElfW(Sym) * sym;
      Section *sec;
      int32_t x;

      memcpy(ptr - 3, replace, sizeof(replace));
      rel[1].r_info = ELFW(R_INFO)(0, R_386_NONE);
      sym = &((ElfW(Sym) *)symtab_section->data)[sym_index];
      sec = s1->sections[sym->st_shndx];
      x = sym->st_value - sec->sh_addr - sec->data_offset;
      add32le(ptr + 5, -x);
    } else
      tcc_error_noabort("unexpected R_386_TLS_GD pattern");
  }
    return;
  case R_386_TLS_LDM: {
    static const unsigned char expect[] = {/* lea 0(%ebx),%eax */
                                           0x8d, 0x83, 0x00, 0x00, 0x00, 0x00,
                                           /* call __tls_get_addr@PLT */
                                           0xe8, 0xfc, 0xff, 0xff, 0xff};
    static const unsigned char replace[] = {/* mov %gs:0,%eax */
                                            0x65, 0xa1, 0x00, 0x00, 0x00, 0x00,
                                            /* nop */
                                            0x90,
                                            /* lea 0(%esi,%eiz,1),%esi */
                                            0x8d, 0x74, 0x26, 0x00};

    if (memcmp(ptr - 2, expect, sizeof(expect)) == 0) {
      memcpy(ptr - 2, replace, sizeof(replace));
      rel[1].r_info = ELFW(R_INFO)(0, R_386_NONE);
    } else
      tcc_error_noabort("unexpected R_386_TLS_LDM pattern");
  }
    return;
  case R_386_TLS_LDO_32:
  case R_386_TLS_LE: {
    ElfW(Sym) * sym;
    Section *sec;
    int32_t x;

    sym = &((ElfW(Sym) *)symtab_section->data)[sym_index];
    sec = s1->sections[sym->st_shndx];
    x = val - sec->sh_addr - sec->data_offset;
    add32le(ptr, x);
  }
    return;
  case R_386_NONE:
    return;
  default:
    fprintf(stderr, "FIXME: handle reloc type %d at %x [%p] to %x\n", type,
            (unsigned)addr, ptr, (unsigned)val);
    return;
  }
}

#endif /* !TARGET_DEFS_ONLY */
