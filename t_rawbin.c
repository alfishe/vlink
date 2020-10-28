/* $VER: vlink t_rawbin.c V0.16f (26.07.20)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2020  Frank Wille
 */


#include "config.h"
#if defined(RAWBIN1) || defined(RAWBIN2) || \
    defined(SREC19) || defined(SREC28) || defined(SREC37) || \
    defined(IHEX) || defined(SHEX1) || \
    defined(AMSDOS) || defined(APPLEBIN) || defined(ATARICOM) || \
    defined(CBMPRG) || defined(COCOML) || defined(DRAGONBIN) || \
    defined(JAGSRV)
#define T_RAWBIN_C
#include "vlink.h"

#define MAXGAP 16  /* from MAXGAP bytes on, a new file will be created */
#define MAXSREC 32 /* max. number of bytes in an S1/S2/S3 record */
#define MAXIREC 32 /* max. number of bytes in an ihex record (actually 255) */

/* header types */
enum {
  HDR_NONE,
  HDR_AMSDOS,
  HDR_APPLEBIN,
  HDR_ATARICOM,
  HDR_CBMPRG,
  HDR_COCOML,
  HDR_DRAGONBIN,
  HDR_JAGSRV
};

static unsigned long rawbin_headersize(struct GlobalVars *);
static int rawbin_identify(char *,uint8_t *,unsigned long,bool);
static void rawbin_readconv(struct GlobalVars *,struct LinkFile *);
static int rawbin_targetlink(struct GlobalVars *,struct LinkedSection *,
                              struct Section *);
static void rawbin_writeobject(struct GlobalVars *,FILE *);
static void rawbin_writeshared(struct GlobalVars *,FILE *);
static void rawbin_writeexec(struct GlobalVars *,FILE *,bool,int);
#ifdef RAWBIN1
static void rawbin_writesingle(struct GlobalVars *,FILE *);
#endif
#ifdef RAWBIN2
static void rawbin_writemultiple(struct GlobalVars *,FILE *);
#endif
#ifdef AMSDOS
static unsigned long amsdos_headersize(struct GlobalVars *);
static void amsdos_write(struct GlobalVars *,FILE *);
#endif
#ifdef APPLEBIN
static unsigned long applebin_headersize(struct GlobalVars *);
static void applebin_write(struct GlobalVars *,FILE *);
#endif
#ifdef ATARICOM
static unsigned long ataricom_headersize(struct GlobalVars *);
static void ataricom_write(struct GlobalVars *,FILE *);
#endif
#ifdef CBMPRG
static unsigned long cbmprg_headersize(struct GlobalVars *);
static void cbmprg_write(struct GlobalVars *,FILE *);
static void cbmreu_write(struct GlobalVars *,FILE *);
#endif
#ifdef COCOML
static unsigned long cocoml_headersize(struct GlobalVars *);
static void cocoml_write(struct GlobalVars *,FILE *);
#endif
#ifdef DRAGONBIN
static unsigned long dragonbin_headersize(struct GlobalVars *);
static void dragonbin_write(struct GlobalVars *,FILE *);
#endif
#ifdef JAGSRV
#define TOSHDR_SIZE 28
#define JAGR_SIZE 18
static unsigned long jagsrv_headersize(struct GlobalVars *);
static void jagsrv_write(struct GlobalVars *,FILE *);
#endif
#ifdef SREC19
static void srec19_write(struct GlobalVars *,FILE *);
#endif
#ifdef SREC28
static void srec28_write(struct GlobalVars *,FILE *);
#endif
#ifdef SREC37
static void srec37_write(struct GlobalVars *,FILE *);
#endif
#ifdef IHEX
static void ihex_write(struct GlobalVars *,FILE *);
#endif
#ifdef SHEX1
static void shex1_write(struct GlobalVars *,FILE *);
#endif

static lword execaddr;
static FILE *hdrfile;
static long hdroffs;
static struct LinkedSection *hdrsect;

static const char defaultscript[] =
  "SECTIONS {\n"
  "  .text: { *(.text CODE text) *(seg*) *(.rodata*) }\n"
  "  .data: { *(.data DATA data) }\n"
  "  .bss: { *(.bss BSS bss) *(COMMON) }\n"
  "  .comment 0 : { *(.comment) }\n"
  "}\n";


#ifdef RAWBIN1
struct FFFuncs fff_rawbin1 = {
  "rawbin1",
  defaultscript,
  NULL,
  NULL,
  rawbin_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  rawbin_writesingle,
  NULL,NULL,
  0,
  0x8000,
  0,
  0,
  RTAB_UNDEF,0,
  -1, /* endianess undefined, only write */
  0,  /* addr_bits from input */
  0,  /* ptr-alignment unknown */
  FFF_SECTOUT
};
#endif

#ifdef RAWBIN2
struct FFFuncs fff_rawbin2 = {
  "rawbin2",
  defaultscript,
  NULL,
  NULL,
  rawbin_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  rawbin_writemultiple,
  NULL,NULL,
  0,
  0x8000,
  0,
  0,
  RTAB_UNDEF,0,
  -1, /* endianess undefined, only write */
  0,  /* addr_bits from input */
  0,  /* ptr-alignment unknown */
  FFF_SECTOUT
};
#endif

#ifdef AMSDOS
static const char amsdosscript[] =
  "MEMORY {\n"
  "  m: org=., len=0x10000-.\n"
  "  b1: org=0x4000, len=0x4000\n"
  "  b2: org=0x4000, len=0x4000\n"
  "  b3: org=0x4000, len=0x4000\n"
  "  b4: org=0x4000, len=0x4000\n"
  "}\n"
  "SECTIONS {\n"
  "  bin: { *(.text) *(.rodata*) *(.data*) *(.bss) *(COMMON) } > m\n"
  "  c4 : { *(.c4) } > b1 AT> m\n"
  "  c5 : { *(.c5) } > b2 AT> m\n"
  "  c6 : { *(.c6) } > b3 AT> m\n"
  "  c7 : { *(.c7) } > b4 AT> m\n"
  "}\n";

struct FFFuncs fff_amsdos = {
  "amsdos",
  amsdosscript,
  NULL,
  NULL,
  amsdos_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  amsdos_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  _LITTLE_ENDIAN_,
  16,0,
  FFF_SECTOUT
};
#endif

#ifdef APPLEBIN
struct FFFuncs fff_applebin = {
  "applebin",
  defaultscript,
  NULL,
  NULL,
  applebin_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  applebin_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  _LITTLE_ENDIAN_,
  16,0,
  FFF_SECTOUT
};
#endif

#ifdef ATARICOM
struct FFFuncs fff_ataricom = {
  "ataricom",
  defaultscript,
  NULL,
  NULL,
  ataricom_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  ataricom_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  _LITTLE_ENDIAN_,
  16,0,
  FFF_SECTOUT
};
#endif

#ifdef CBMPRG
struct FFFuncs fff_cbmprg = {
  "cbmprg",
  defaultscript,
  NULL,
  NULL,
  cbmprg_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  cbmprg_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  _LITTLE_ENDIAN_,
  16,0,
  FFF_SECTOUT
};
struct FFFuncs fff_cbmreu = {
  "cbmreu",
  defaultscript,
  NULL,
  NULL,
  cbmprg_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  cbmreu_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  _LITTLE_ENDIAN_,
  16,0,
  FFF_SECTOUT
};
#endif

#ifdef COCOML
struct FFFuncs fff_cocoml = {
  "cocoml",
  defaultscript,
  NULL,
  NULL,
  cocoml_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  cocoml_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  _BIG_ENDIAN_,
  16,0,
  FFF_SECTOUT
};
#endif

#ifdef DRAGONBIN
struct FFFuncs fff_dragonbin = {
  "dragonbin",
  defaultscript,
  NULL,
  NULL,
  dragonbin_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  dragonbin_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  _BIG_ENDIAN_,
  16,0,
  FFF_SECTOUT
};
#endif

#ifdef JAGSRV
static const char jaguarscript[] =
  "SECTIONS {\n"
  "  . = 0x4000;\n"
  "  .text: {\n"
  "    *(.text CODE text)\n"
  "  }\n"
  "  . = ALIGN(32);\n"
  "  .data: {\n"
  "    VBCC_CONSTRUCTORS\n"
  "    *(.data DATA data)\n"
  "  }\n"
  "  . = ALIGN(32);\n"
  "  .bss: {\n"
  "    *(.bss BSS bss)\n"
  "    *(COMMON)\n"
  "    _BSS_END = ALIGN(32);\n"
  "  }\n"
  "}\n";

struct FFFuncs fff_jagsrv = {
  "jagsrv",
  jaguarscript,
  NULL,
  NULL,
  jagsrv_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  jagsrv_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  _BIG_ENDIAN_,
  32,1,
  0
};
#endif

#ifdef SREC19
struct FFFuncs fff_srec19 = {
  "srec19",
  defaultscript,
  NULL,
  NULL,
  rawbin_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  srec19_write,
  NULL,NULL,
  0,
  0x8000,
  0,
  0,
  RTAB_UNDEF,0,
  -1, /* endianess undefined, only write */
  16,
  0   /* ptr-alignment unknown */
};
#endif

#ifdef SREC28
struct FFFuncs fff_srec28 = {
  "srec28",
  defaultscript,
  NULL,
  NULL,
  rawbin_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  srec28_write,
  NULL,NULL,
  0,
  0x8000,
  0,
  0,
  RTAB_UNDEF,0,
  -1, /* endianess undefined, only write */
  32, /* 24 */
  0   /* ptr-alignment unknown */
};
#endif

#ifdef SREC37
struct FFFuncs fff_srec37 = {
  "srec37",
  defaultscript,
  NULL,
  NULL,
  rawbin_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  srec37_write,
  NULL,NULL,
  0,
  0x8000,
  0,
  0,
  RTAB_UNDEF,0,
  -1, /* endianess undefined, only write */
  32,
  0   /* ptr-alignment unknown */
};
#endif

#ifdef IHEX
struct FFFuncs fff_ihex = {
  "ihex",
  defaultscript,
  NULL,
  NULL,
  rawbin_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  ihex_write,
  NULL,NULL,
  0,
  0x8000,
  0,
  0,
  RTAB_UNDEF,0,
  -1, /* endianess undefined, only write */
  0,  /* addr_bits from input */
  0   /* ptr-alignment unknown */
};
#endif

#ifdef SHEX1
struct FFFuncs fff_shex1 = {
  "oilhex",
  defaultscript,
  NULL,
  NULL,
  rawbin_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  shex1_write,
  NULL,NULL,
  0,
  0x8000,
  0,
  0,
  RTAB_UNDEF,0,
  -1, /* endianess undefined, only write */
  32,
  0   /* ptr-alignment unknown */
};
#endif


/*****************************************************************/
/*                        Read Binary                            */
/*****************************************************************/


static unsigned long rawbin_headersize(struct GlobalVars *gv)
{
  return 0;  /* no header - it's pure binary! */
}


#ifdef AMSDOS
static unsigned long amsdos_headersize(struct GlobalVars *gv)
{
  return 128;
}
#endif


#ifdef APPLEBIN
static unsigned long applebin_headersize(struct GlobalVars *gv)
{
  return 4;
}
#endif


#ifdef ATARICOM
static unsigned long ataricom_headersize(struct GlobalVars *gv)
{
  return 2;  /* followed by 4 bytes segment header */
}
#endif


#ifdef CBMPRG
static unsigned long cbmprg_headersize(struct GlobalVars *gv)
{
  return 2;
}
#endif


#ifdef COCOML
static unsigned long cocoml_headersize(struct GlobalVars *gv)
{
  return 0;
}
#endif


#ifdef DRAGONBIN
static unsigned long dragonbin_headersize(struct GlobalVars *gv)
{
  return 9;
}
#endif


#ifdef JAGSRV
static unsigned long jagsrv_headersize(struct GlobalVars *gv)
{
  return TOSHDR_SIZE+JAGR_SIZE;  /* uses TOS-executable header */
}
#endif


static int rawbin_identify(char *name,uint8_t *p,unsigned long plen,bool lib)
/* identify a binary file */
{
  return ID_UNKNOWN;  /* binaries are only allowed to be written! */
}


static void rawbin_readconv(struct GlobalVars *gv,struct LinkFile *lf)
{
  ierror("rawbin_readconv(): Can't read raw-binaries");
}


static int rawbin_targetlink(struct GlobalVars *gv,struct LinkedSection *ls,
                             struct Section *s)
/* returns 1, if target requires the combination of the two sections, */
/* returns -1, if target doesn't want to combine them, */
/* returns 0, if target doesn't care - standard linking rules are used. */
{
  ierror("rawbin_targetlink(): Impossible to link raw-binaries");
  return 0;
}



/*****************************************************************/
/*                        Write Binary                           */
/*****************************************************************/


static void savehdroffs(FILE *f,size_t bytes,struct LinkedSection *ls)
{
  /* remember location in file header and skip it */
  hdrsect = ls;
  hdrfile = f;
  hdroffs = ftell(f);
  fwritegap(f,bytes);
}


static int rewindtohdr(FILE *f)
{
  if (hdrfile == f) {
    fseek(f,hdroffs,SEEK_SET);
    return 1;
  }
  return 0;
}


#ifdef AMSDOS
static void amsdos_header(FILE *f,uint16_t loadaddr,unsigned size)
{
  uint8_t buffer[128];
  uint16_t checksum;
  int i;

  memset(buffer,0,128);                 /* initialize header with zeros */
  memset(&buffer[1],' ',11);            /*  1 > 11: spaces */
  buffer[18] = 2;                       /* 18     : filetype = 2 - binary */
  write16le(&buffer[19],size);          /* 19 > 20: data length */
  write16le(&buffer[21],loadaddr);      /* 21 > 22: load address */
  buffer[23] = 0xff;                    /* 23     : 0xFF */
  write16le(&buffer[24],size);          /* 24 > 25: logical length */
  write16le(&buffer[26],execaddr);      /* 26 > 27: entry point */
  write32le(&buffer[63],size);          /* 64 > 66: filesize (3 bytes) */
  for (checksum=0,i=0; i<67; i++)
    checksum += buffer[i];
  write16le(&buffer[67],checksum);      /* 67 > 68: checksum */
  fwritex(f,buffer,128);
}
#endif


#ifdef JAGSRV
static void jagsrv_header(FILE *f,uint32_t loadaddr,unsigned size)
{
  uint8_t header[TOSHDR_SIZE+JAGR_SIZE];

  memset(header,0,TOSHDR_SIZE+JAGR_SIZE);
  write16be(&header[0],0x601a);
  write32be(&header[2],size+JAGR_SIZE);

  memcpy(&header[TOSHDR_SIZE+0],"JAGR",4);
  write16be(&header[TOSHDR_SIZE+4],3);  /* command 3: Load & Run */
  write32be(&header[TOSHDR_SIZE+6],loadaddr);
  write32be(&header[TOSHDR_SIZE+10],size);
  write32be(&header[TOSHDR_SIZE+14],execaddr);
  fwritex(f,header,TOSHDR_SIZE+JAGR_SIZE);
}
#endif


static int rawbin_segheader(struct GlobalVars *gv,FILE *f,
                            struct LinkedSection *ls,int header)
{
  /* write a segment/section header, when needed */
#ifdef ATARICOM
  if (header == HDR_ATARICOM) {  /* Atari COM section header */
    fwrite16le(f,ls->copybase);
    fwrite16le(f,ls->copybase+ls->size-1);
    return 1;
  }
#endif
#ifdef COCOML
  if (header == HDR_COCOML) {  /* Tandy Color Computer segment header */
    fwrite8(f,0);
    fwrite16be(f,ls->size);
    fwrite16be(f,ls->copybase);
    return 1;
  }
#endif
  return 0;
}


static void rawbin_fileheader(struct GlobalVars *gv,FILE *f,
                              struct LinkedSection *ls,int header)
{
  /* write a file header, when needed */
#ifdef AMSDOS
  if (header == HDR_AMSDOS)  /* Amstrad/Schneider CPC */
    amsdos_header(f,ls->copybase,ls->size);
#endif
#ifdef APPLEBIN
  if (header == HDR_APPLEBIN) {  /* Apple DOS binary file header */
    fwrite16le(f,ls->copybase);
    savehdroffs(f,2,ls);  /* insert file length later */
  }
#endif
#ifdef ATARICOM
  if (header == HDR_ATARICOM)  /* Atari COM file header */
    fwrite16le(f,0xffff);
#endif
#ifdef CBMPRG
  if (header == HDR_CBMPRG) {  /* Commodore PET, VIC-20, 64, etc. */
    static int done;
    if (!done)
      fwrite16le(f,ls->copybase);
    done = 1;
  }
#endif
#ifdef DRAGONBIN
  if (header == HDR_DRAGONBIN) {  /* Dragon DOS file header */
    fwrite16be(f,0x5502);
    fwrite16be(f,ls->copybase);
    savehdroffs(f,2,ls);  /* insert file length later */
    fwrite16be(f,execaddr?(uint16_t)execaddr:ls->copybase);
    fwrite8(f,0xaa);
  }
#endif
#ifdef JAGSRV
  if (header == HDR_JAGSRV) {  /* Atari Jaguar, JAGSRV header for SkunkBoard */
    struct LinkedSection *finls = (struct LinkedSection *)gv->lnksec.last;
    jagsrv_header(f,ls->copybase,
                  (finls->copybase+finls->filesize)-ls->copybase);
  }
#endif
  /* followed by optional section/segment header */
  rawbin_segheader(gv,f,ls,header);
}


static void rawbin_trailer(struct GlobalVars *gv,FILE *f,
                           struct LinkedSection *ls,int header)
{
  /* write a file trailer or patch the header, when needed */
#ifdef APPLEBIN
  if (header == HDR_APPLEBIN && rewindtohdr(f))
    fwrite16le(f,ls->copybase+ls->size-hdrsect->copybase);
#endif
#ifdef DRAGONBIN
  if (header == HDR_DRAGONBIN && rewindtohdr(f))
    fwrite16be(f,ls->copybase+ls->size-hdrsect->copybase);
#endif
#ifdef COCOML
  if (header == HDR_COCOML) {
    fwrite8(f,0xff);
    fwrite16be(f,0);
    fwrite16be(f,execaddr);
  }
#endif
}


static void rawbin_writeexec(struct GlobalVars *gv,FILE *f,bool singlefile,
                             int header)
/* creates executable raw-binary files (with absolute addresses) */
{
  FILE *firstfile = f;
  bool firstsec = TRUE;
  unsigned long addr;
  struct LinkedSection *ls,*prevls;
  char *name;

  /* determine program's execution address */
  execaddr = entry_address(gv);

  /* section loop */
  while (ls = load_next_section(gv)) {
    if (ls->size==0 || !(ls->flags & SF_ALLOC) || (ls->ld_flags & LSF_NOLOAD))
      continue;  /* ignore empty or unallocated sections */

    /* resolve all relocations */
    calc_relocs(gv,ls);

    if (gv->output_sections) {
      /* make a new file for each output section */
      if (gv->trace_file)
        fprintf(gv->trace_file,"Base address section %s = 0x%08lx.\n",
                ls->name,ls->copybase);
      if (f != NULL)
        ierror("rawbin_writeexec: open file with output_sections");
      if (gv->osec_base_name != NULL) {
        /* use a common base name before the section name */
        name = alloc(strlen(gv->osec_base_name)+strlen(ls->name)+2);
        sprintf(name,"%s.%s",gv->osec_base_name,ls->name);
      }
      else
        name = (char *)ls->name;
      if (!(f = fopen(name,"wb"))) {
        error(29,name);
        break;
      }
      if (gv->osec_base_name != NULL)
        free(name);
      rawbin_fileheader(gv,f,ls,header);
    }
    else if (firstsec) {
      /* write an optional header before the first section */
      if (gv->trace_file)
        fprintf(gv->trace_file,"Base address = 0x%08lx.\n",ls->copybase);
      firstsec = FALSE;
      rawbin_fileheader(gv,f,ls,header);
    }
    else {
      /* handle gaps between this and the previous section */
      if (ls->copybase > addr) {
        if (ls->copybase-addr < MAXGAP || singlefile) {
          if (!rawbin_segheader(gv,f,ls,header))
            fwritegap(f,ls->copybase-addr);
        }
        else {  /* open a new file for this section */
          if (f != firstfile)
            fclose(f);
          name = alloc(strlen(gv->dest_name)+strlen(ls->name)+2);
          sprintf(name,"%s.%s",gv->dest_name,ls->name);
          if (!(f = fopen(name,"wb"))) {
            error(29,name);
            break;
          }
          free(name);
          rawbin_fileheader(gv,f,ls,header);
        }
      }
      else if (ls->copybase < addr)
        error(98,fff[gv->dest_format]->tname,ls->name,prevls->name);
      else  /* next section attaches exactly */
        rawbin_segheader(gv,f,ls,header);
    }

    /* write section contents */
    fwritex(f,ls->data,ls->filesize);
    if (ls->filesize < ls->size)
      fwritegap(f,ls->size - ls->filesize);

    addr = ls->copybase + ls->size;
    prevls = ls;
  }

  /* write optional trailer */
  rawbin_trailer(gv,f,prevls,header);

  if (f!=NULL && f!=firstfile)
    fclose(f);
}


static void rawbin_writeshared(struct GlobalVars *gv,FILE *f)
{
  error(30);  /* Target file format doesn't support shared objects */
}


static void rawbin_writeobject(struct GlobalVars *gv,FILE *f)
{
  error(62);  /* Target file format doesn't support relocatable objects */
}


#ifdef RAWBIN1
static void rawbin_writesingle(struct GlobalVars *gv,FILE *f)
/* creates a single raw-binary file, fill gaps between */
/* sections with zero */
{
  rawbin_writeexec(gv,f,TRUE,HDR_NONE);
}
#endif


#ifdef RAWBIN2
static void rawbin_writemultiple(struct GlobalVars *gv,FILE *f)
/* creates raw-binary which might get splitted over several */
/* files, because of different section base addresses */
{
  rawbin_writeexec(gv,f,FALSE,HDR_NONE);
}
#endif


#ifdef AMSDOS
static void amsdos_write(struct GlobalVars *gv,FILE *f)
/* creates one or more raw-binary files with an AMSDOS header, suitable */
/* for loading as an executable on Amstrad/Scheider CPC computers */
{
  rawbin_writeexec(gv,f,FALSE,HDR_AMSDOS);
}
#endif


#ifdef APPLEBIN
static void applebin_write(struct GlobalVars *gv,FILE *f)
/* creates a raw-binary file with an Apple DOS binary header, suitable */
/* for loading as an executable on Apple II computers */
{
  rawbin_writeexec(gv,f,TRUE,HDR_APPLEBIN);
}
#endif


#ifdef ATARICOM
static void ataricom_write(struct GlobalVars *gv,FILE *f)
/* creates a raw-binary file with an ATARI COM file header, suitable */
/* for loading as an executable on Atari 8-bit computers */
{
  rawbin_writeexec(gv,f,TRUE,HDR_ATARICOM);
}
#endif


#ifdef CBMPRG
static void cbmprg_write(struct GlobalVars *gv,FILE *f)
/* creates a raw-binary file with a Commodore header, suitable */
/* for loading as an executable on PET, VIC-20, 64, etc. computers */
{
  rawbin_writeexec(gv,f,TRUE,HDR_CBMPRG);
}

static void cbmreu_write(struct GlobalVars *gv,FILE *f)
/* creates a raw-binary file with a Commodore header, followed by */
/* a REU image. */
{
  rawbin_writeexec(gv,f,FALSE,HDR_CBMPRG);
}
#endif


#ifdef COCOML
static void cocoml_write(struct GlobalVars *gv,FILE *f)
/* creates a raw-binary file with CoCo segment headers and trailer, suitable */
/* for loading as a machine language file on Tandy Color Computer 1,2,3 */
{
  rawbin_writeexec(gv,f,TRUE,HDR_COCOML);
}
#endif


#ifdef DRAGONBIN
static void dragonbin_write(struct GlobalVars *gv,FILE *f)
{
/* creates a raw-binary file with a Dragon DOS file header, suitable */
/* for loading as an executable on Dragon 32 and 64 computers */
  rawbin_writeexec(gv,f,TRUE,HDR_DRAGONBIN);
}
#endif


#ifdef JAGSRV
static void jagsrv_write(struct GlobalVars *gv,FILE *f)
/* creates a raw-binary file with a JAGSRV header, suitable */
/* for loading and executing on a SkunkBoard equipped Atari Jaguar, or */
/* a VirtualJaguar emulator */
{
  rawbin_writeexec(gv,f,TRUE,HDR_JAGSRV);
}
#endif


#if defined(SREC19) || defined(SREC28) || defined(SREC37)
static void SRecOut(FILE *f,int stype,uint8_t *buf,int len)
{
  uint8_t chksum = 0xff-len-1;

  fprintf(f,"S%1d%02X",stype,(unsigned)(len+1));
  for (; len>0; len--) {
    fprintf(f,"%02X",(unsigned)*buf);
    chksum -= *buf++;
  }
  fprintf(f,"%02X\n",(unsigned)chksum);
}


static void srec_write(struct GlobalVars *gv,FILE *f,int addrsize)
{
  bool firstsec = TRUE;
  struct LinkedSection *ls;
  unsigned long len,addr;
  uint8_t *p,buf[MAXSREC+8];

  execaddr = entry_address(gv);  /* determine program's execution address */
  untrim_sections(gv);           /* output trailing 0-bytes in hex */

  /* write header */
  buf[0] = buf[1] = 0;
  strncpy((char *)&buf[2],gv->dest_name,MAXSREC+6);
  SRecOut(f,0,buf,(strlen(gv->dest_name)<(MAXSREC+6)) ?
          strlen(gv->dest_name)+2 : MAXSREC+8);

  while (ls = load_next_section(gv)) {
    if (ls->size == 0 || !(ls->flags & SF_ALLOC) || (ls->ld_flags & LSF_NOLOAD))
      continue;  /* ignore empty sections */

    if (firstsec) {
      if (gv->trace_file)
        fprintf(gv->trace_file,"Base address = 0x%08lx.\n",ls->copybase);
      firstsec = FALSE;
    }

    /* resolve all relocations and write section contents */
    calc_relocs(gv,ls);

    for (p=ls->data,addr=ls->copybase,len=ls->filesize; len>0; ) {
      int nbytes = (len>MAXSREC) ? MAXSREC : len;

      switch (addrsize) {
        case 2:
          write16be(buf,(uint16_t)addr);
          memcpy(buf+2,p,nbytes);
          SRecOut(f,1,buf,2+nbytes);
          break;
        case 3:
          buf[0] = (uint8_t)((addr>>16)&0xff);
          buf[1] = (uint8_t)((addr>>8)&0xff);
          buf[2] = (uint8_t)(addr&0xff);
          memcpy(buf+3,p,nbytes);
          SRecOut(f,2,buf,3+nbytes);
          break;
        case 4:
          write32be(buf,(uint32_t)addr);
          memcpy(buf+4,p,nbytes);
          SRecOut(f,3,buf,4+nbytes);
          break;
        default:
          ierror("srec_write(): Illegal SRec-type: %d",addrsize);
          nbytes = len;
          break;
      }
      p += nbytes;
      addr += nbytes;
      len -= nbytes;
    }
  }

  /* write trailer */
  write32be(buf,(uint32_t)execaddr);
  SRecOut(f,11-addrsize,buf+4-addrsize,addrsize);
}

#endif


#ifdef SREC19
static void srec19_write(struct GlobalVars *gv,FILE *f)
/* creates a Motorola S-Record file (S0,S1,S9), using 16-bit addresses */
{
  srec_write(gv,f,2);
}
#endif


#ifdef SREC28
static void srec28_write(struct GlobalVars *gv,FILE *f)
/* creates a Motorola S-Record file (S0,S2,S8), using 24-bit addresses */
{
  srec_write(gv,f,3);
}
#endif


#ifdef SREC37
static void srec37_write(struct GlobalVars *gv,FILE *f)
/* creates a Motorola S-Record file (S0,S3,S7), using 32-bit addresses */
{
  srec_write(gv,f,4);
}
#endif


#ifdef IHEX
static void IHexOut(FILE *f,unsigned long addr,uint8_t *buf,int len)
{
  static unsigned long hiaddr;
  uint8_t chksum;

  if (((addr&0xffff0000)>>16)!=hiaddr) {
    hiaddr = (addr&0xffff0000) >> 16;
    fprintf(f,":02000004%02X%02X%02X\n",
            (unsigned)(hiaddr>>8)&0xff,
            (unsigned)hiaddr&0xff,
            (unsigned)(-(2+4+(hiaddr>>8)+hiaddr))&0xff);
  }
  fprintf(f,":%02X%02X%02X00",
          (unsigned)len&0xff,(unsigned)(addr>>8)&0xff,(unsigned)addr&0xff);
  chksum = len + (addr>>8) + addr;
  for (; len>0; len--) {
    fprintf(f,"%02X",((unsigned)*buf)&0xff);
    chksum += *buf++;
  }
  fprintf(f,"%02X\n",(-chksum)&0xff);
}


static void ihex_write(struct GlobalVars *gv,FILE *f)
{
  bool firstsec = TRUE;
  struct LinkedSection *ls;
  unsigned long len,addr;
  uint8_t *p;

  untrim_sections(gv);  /* output trailing 0-bytes in hex */

  while (ls = load_next_section(gv)) {
    if (ls->size == 0 || !(ls->flags & SF_ALLOC) || (ls->ld_flags & LSF_NOLOAD))
      continue;  /* ignore empty sections */

    if (firstsec) {
      if (gv->trace_file)
        fprintf(gv->trace_file,"Base address = 0x%08lx.\n",ls->copybase);
      firstsec = FALSE;
    }

    /* resolve all relocations and write section contents */
    calc_relocs(gv,ls);

    for (p=ls->data,addr=ls->copybase,len=ls->filesize; len>0; ) {
      int nbytes = (len>MAXIREC) ? MAXIREC : len;

      if ((((unsigned long)addr)&0xffff)+nbytes > 0xffff)
        nbytes = 0x10000 - (((unsigned long)addr) & 0xffff);
      IHexOut(f,addr,p,nbytes);
      p += nbytes;
      addr += nbytes;
      len -= nbytes;
    }
  }

  fprintf(f,":00000001FF\n");
}
#endif

#ifdef SHEX1
static void SHex1Out(FILE *f,unsigned long addr,uint8_t *buf,int len)
{
  int wordcnt=(len+3)/4;
  int i;

  fprintf(f,"%06lX %d",addr,wordcnt);

  for(i=0;i<len;i++){
    if((i&3)==0)
      fprintf(f," ");
    fprintf(f,"%02X",buf[i]);
  }
  for(i=0;i<wordcnt*4-len;i++)
    fprintf(f,"00");
  fprintf(f,"\n");
}


static void shex1_write(struct GlobalVars *gv,FILE *f)
{
  bool firstsec = TRUE;
  struct LinkedSection *ls;
  unsigned long len,addr;
  uint8_t *p;

  untrim_sections(gv);  /* output trailing 0-bytes in hex */

  while (ls = load_next_section(gv)) {
    if (ls->size == 0 || !(ls->flags & SF_ALLOC) || (ls->ld_flags & LSF_NOLOAD))
      continue;  /* ignore empty sections */

    if (firstsec) {
      if (gv->trace_file)
        fprintf(gv->trace_file,"Base address = 0x%08lx.\n",ls->copybase);
      firstsec = FALSE;
    }

    /* resolve all relocations and write section contents */
    calc_relocs(gv,ls);

    for (p=ls->data,addr=ls->copybase,len=ls->filesize; len>0; ) {
      int nbytes = (len>32) ? 32 : len;

      SHex1Out(f,addr,p,nbytes);
      p += nbytes;
      addr += nbytes;
      len -= nbytes;
    }
  }
  fprintf(f,"000000 0\n");
}
#endif


#endif
