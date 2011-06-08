#include <linux/serial_core.h>
#include <linux/io.h>
#include <linux/gpio.h>

#if defined(CONFIG_CPU_SUBTYPE_SH7705) || \
    defined(CONFIG_CPU_SUBTYPE_SH7706) || \
    defined(CONFIG_CPU_SUBTYPE_SH7707) || \
    defined(CONFIG_CPU_SUBTYPE_SH7708) || \
    defined(CONFIG_CPU_SUBTYPE_SH7709)
# define SCPCR  0xA4000116 /* 16 bit SCI and SCIF */
# define SCPDR  0xA4000136 /* 8  bit SCI and SCIF */
#elif defined(CONFIG_CPU_SUBTYPE_SH7720) || \
      defined(CONFIG_CPU_SUBTYPE_SH7721) || \
      defined(CONFIG_ARCH_SH73A0) || \
      defined(CONFIG_ARCH_SH7367) || \
      defined(CONFIG_ARCH_SH7377) || \
      defined(CONFIG_ARCH_SH7372)
# define PORT_PTCR	   0xA405011EUL
# define PORT_PVCR	   0xA4050122UL
#elif defined(CONFIG_CPU_SUBTYPE_SH7750)  || \
      defined(CONFIG_CPU_SUBTYPE_SH7750R) || \
      defined(CONFIG_CPU_SUBTYPE_SH7750S) || \
      defined(CONFIG_CPU_SUBTYPE_SH7091)  || \
      defined(CONFIG_CPU_SUBTYPE_SH7751)  || \
      defined(CONFIG_CPU_SUBTYPE_SH7751R) || \
      defined(CONFIG_CPU_SUBTYPE_SH4_202)
# define SCSPTR2 0xFFE80020 /* 16 bit SCIF */
#elif defined(CONFIG_CPU_SUBTYPE_SH7760)
# define SCSPTR0 0xfe600024 /* 16 bit SCIF */
# define SCSPTR2 0xfe620024 /* 16 bit SCIF */
#elif defined(CONFIG_CPU_SUBTYPE_SH7710) || defined(CONFIG_CPU_SUBTYPE_SH7712)
# define SCSPTR0 0xA4400000	  /* 16 bit SCIF */
# define PACR 0xa4050100
# define PBCR 0xa4050102
#elif defined(CONFIG_CPU_SUBTYPE_SH7343)
# define SCSPTR0 0xffe00010	/* 16 bit SCIF */
#elif defined(CONFIG_CPU_SUBTYPE_SH7722)
# define PWDR			0xA4050166
# define PSCR			0xA405011E
#elif defined(CONFIG_CPU_SUBTYPE_SH7366)
# define SCPDR0			0xA405013E      /* 16 bit SCIF0 PSDR */
# define SCSPTR0		SCPDR0
#elif defined(CONFIG_CPU_SUBTYPE_SH7723)
# define SCSPTR0                0xa4050160
#elif defined(CONFIG_CPU_SUBTYPE_SH7757)
# define SCSPTR0 0xfe4b0020
#elif defined(CONFIG_CPU_SUBTYPE_SH7763) || \
      defined(CONFIG_CPU_SUBTYPE_SH7780)
# define SCSPTR0 0xffe00024 /* 16 bit SCIF */
#elif defined(CONFIG_CPU_SUBTYPE_SH7770)
# define SCSPTR0 0xff923020 /* 16 bit SCIF */
#elif defined(CONFIG_CPU_SUBTYPE_SH7785) || \
      defined(CONFIG_CPU_SUBTYPE_SH7786)
# define SCSPTR0	0xffea0024	/* 16 bit SCIF */
#elif defined(CONFIG_CPU_SUBTYPE_SH7201) || \
      defined(CONFIG_CPU_SUBTYPE_SH7203) || \
      defined(CONFIG_CPU_SUBTYPE_SH7206) || \
      defined(CONFIG_CPU_SUBTYPE_SH7263)
# define SCSPTR0 0xfffe8020 /* 16 bit SCIF */
#elif defined(CONFIG_CPU_SUBTYPE_SH7619)
# define SCSPTR0 0xf8400020 /* 16 bit SCIF */
#elif defined(CONFIG_CPU_SUBTYPE_SHX3)
# define SCSPTR0 0xffc30020		/* 16 bit SCIF */
#else
# error CPU subtype not defined
#endif

#if defined(CONFIG_CPU_SUBTYPE_SH7705) || \
    defined(CONFIG_CPU_SUBTYPE_SH7720) || \
    defined(CONFIG_CPU_SUBTYPE_SH7721) || \
    defined(CONFIG_ARCH_SH73A0) || \
    defined(CONFIG_ARCH_SH7367) || \
    defined(CONFIG_ARCH_SH7377) || \
    defined(CONFIG_ARCH_SH7372)
# define SCIF_RFDC_MASK 0x007f
# define SCIF_TXROOM_MAX 64
#elif defined(CONFIG_CPU_SUBTYPE_SH7763)
# define SCIF_RFDC_MASK 0x007f
# define SCIF_TXROOM_MAX 64
/* SH7763 SCIF2 support */
# define SCIF2_RFDC_MASK 0x001f
# define SCIF2_TXROOM_MAX 16
#else
# define SCIF_RFDC_MASK 0x001f
# define SCIF_TXROOM_MAX 16
#endif

#define SCxSR_TEND(port)	(((port)->type == PORT_SCI) ? SCI_TEND   : SCIF_TEND)
#define SCxSR_RDxF(port)	(((port)->type == PORT_SCI) ? SCI_RDRF   : SCIF_RDF)
#define SCxSR_TDxE(port)	(((port)->type == PORT_SCI) ? SCI_TDRE   : SCIF_TDFE)
#define SCxSR_FER(port)		(((port)->type == PORT_SCI) ? SCI_FER    : SCIF_FER)
#define SCxSR_PER(port)		(((port)->type == PORT_SCI) ? SCI_PER    : SCIF_PER)
#define SCxSR_BRK(port)		(((port)->type == PORT_SCI) ? 0x00       : SCIF_BRK)

#define SCxSR_ERRORS(port)	(to_sci_port(port)->cfg->error_mask)

#if defined(CONFIG_CPU_SUBTYPE_SH7705) || \
    defined(CONFIG_CPU_SUBTYPE_SH7720) || \
    defined(CONFIG_CPU_SUBTYPE_SH7721) || \
    defined(CONFIG_ARCH_SH73A0) || \
    defined(CONFIG_ARCH_SH7367) || \
    defined(CONFIG_ARCH_SH7377) || \
    defined(CONFIG_ARCH_SH7372)
# define SCxSR_RDxF_CLEAR(port)	 (sci_in(port, SCxSR) & 0xfffc)
# define SCxSR_ERROR_CLEAR(port) (sci_in(port, SCxSR) & 0xfd73)
# define SCxSR_TDxE_CLEAR(port)	 (sci_in(port, SCxSR) & 0xffdf)
# define SCxSR_BREAK_CLEAR(port) (sci_in(port, SCxSR) & 0xffe3)
#else
# define SCxSR_RDxF_CLEAR(port)	 (((port)->type == PORT_SCI) ? 0xbc : 0x00fc)
# define SCxSR_ERROR_CLEAR(port) (((port)->type == PORT_SCI) ? 0xc4 : 0x0073)
# define SCxSR_TDxE_CLEAR(port)  (((port)->type == PORT_SCI) ? 0x78 : 0x00df)
# define SCxSR_BREAK_CLEAR(port) (((port)->type == PORT_SCI) ? 0xc4 : 0x00e3)
#endif

/* SCFCR */
#define SCFCR_RFRST 0x0002
#define SCFCR_TFRST 0x0004
#define SCFCR_MCE   0x0008

#define SCI_MAJOR		204
#define SCI_MINOR_START		8

#define SCI_IN(size, offset)		\
	ioread##size(port->membase + (offset))

#define SCI_OUT(size, offset, value)	\
	iowrite##size(value, port->membase + (offset))

#define CPU_SCIx_FNS(name, sci_offset, sci_size, scif_offset, scif_size)\
  static inline unsigned int sci_##name##_in(struct uart_port *port)	\
  {									\
    if (port->type == PORT_SCIF || port->type == PORT_SCIFB) {		\
      return SCI_IN(scif_size, scif_offset);				\
    } else {	/* PORT_SCI or PORT_SCIFA */				\
      return SCI_IN(sci_size, sci_offset);				\
    }									\
  }									\
  static inline void sci_##name##_out(struct uart_port *port, unsigned int value) \
  {									\
    if (port->type == PORT_SCIF || port->type == PORT_SCIFB) {		\
      SCI_OUT(scif_size, scif_offset, value);				\
    } else {	/* PORT_SCI or PORT_SCIFA */				\
      SCI_OUT(sci_size, sci_offset, value);				\
    }									\
  }

#define CPU_SCIF_FNS(name, scif_offset, scif_size)			\
  static inline unsigned int sci_##name##_in(struct uart_port *port)	\
  {									\
    return SCI_IN(scif_size, scif_offset);				\
  }									\
  static inline void sci_##name##_out(struct uart_port *port, unsigned int value) \
  {									\
    SCI_OUT(scif_size, scif_offset, value);				\
  }

#if defined(CONFIG_CPU_SH3) || \
    defined(CONFIG_ARCH_SH73A0) || \
    defined(CONFIG_ARCH_SH7367) || \
    defined(CONFIG_ARCH_SH7377) || \
    defined(CONFIG_ARCH_SH7372)
#if defined(CONFIG_CPU_SUBTYPE_SH7710) || defined(CONFIG_CPU_SUBTYPE_SH7712)
#define SCIx_FNS(name, sh3_sci_offset, sh3_sci_size, sh4_sci_offset, sh4_sci_size, \
		 sh3_scif_offset, sh3_scif_size, sh4_scif_offset, sh4_scif_size) \
  CPU_SCIx_FNS(name, sh4_sci_offset, sh4_sci_size, sh4_scif_offset, sh4_scif_size)
#define SCIF_FNS(name, sh3_scif_offset, sh3_scif_size, sh4_scif_offset, sh4_scif_size) \
	  CPU_SCIF_FNS(name, sh4_scif_offset, sh4_scif_size)
#elif defined(CONFIG_CPU_SUBTYPE_SH7705) || \
      defined(CONFIG_CPU_SUBTYPE_SH7720) || \
      defined(CONFIG_CPU_SUBTYPE_SH7721) || \
      defined(CONFIG_ARCH_SH7367)
#define SCIF_FNS(name, scif_offset, scif_size) \
  CPU_SCIF_FNS(name, scif_offset, scif_size)
#elif defined(CONFIG_ARCH_SH7377) || \
      defined(CONFIG_ARCH_SH7372) || \
      defined(CONFIG_ARCH_SH73A0)
#define SCIx_FNS(name, sh4_scifa_offset, sh4_scifa_size, sh4_scifb_offset, sh4_scifb_size) \
  CPU_SCIx_FNS(name, sh4_scifa_offset, sh4_scifa_size, sh4_scifb_offset, sh4_scifb_size)
#define SCIF_FNS(name, scif_offset, scif_size) \
  CPU_SCIF_FNS(name, scif_offset, scif_size)
#else
#define SCIx_FNS(name, sh3_sci_offset, sh3_sci_size, sh4_sci_offset, sh4_sci_size, \
		 sh3_scif_offset, sh3_scif_size, sh4_scif_offset, sh4_scif_size) \
  CPU_SCIx_FNS(name, sh3_sci_offset, sh3_sci_size, sh3_scif_offset, sh3_scif_size)
#define SCIF_FNS(name, sh3_scif_offset, sh3_scif_size, sh4_scif_offset, sh4_scif_size) \
  CPU_SCIF_FNS(name, sh3_scif_offset, sh3_scif_size)
#endif
#elif defined(CONFIG_CPU_SUBTYPE_SH7723) ||\
      defined(CONFIG_CPU_SUBTYPE_SH7724)
        #define SCIx_FNS(name, sh4_scifa_offset, sh4_scifa_size, sh4_scif_offset, sh4_scif_size) \
                CPU_SCIx_FNS(name, sh4_scifa_offset, sh4_scifa_size, sh4_scif_offset, sh4_scif_size)
        #define SCIF_FNS(name, sh4_scif_offset, sh4_scif_size) \
                CPU_SCIF_FNS(name, sh4_scif_offset, sh4_scif_size)
#else
#define SCIx_FNS(name, sh3_sci_offset, sh3_sci_size, sh4_sci_offset, sh4_sci_size, \
		 sh3_scif_offset, sh3_scif_size, sh4_scif_offset, sh4_scif_size) \
  CPU_SCIx_FNS(name, sh4_sci_offset, sh4_sci_size, sh4_scif_offset, sh4_scif_size)
#define SCIF_FNS(name, sh3_scif_offset, sh3_scif_size, sh4_scif_offset, sh4_scif_size) \
  CPU_SCIF_FNS(name, sh4_scif_offset, sh4_scif_size)
#endif

#if defined(CONFIG_CPU_SUBTYPE_SH7705) || \
    defined(CONFIG_CPU_SUBTYPE_SH7720) || \
    defined(CONFIG_CPU_SUBTYPE_SH7721) || \
    defined(CONFIG_ARCH_SH7367)

SCIF_FNS(SCSMR,  0x00, 16)
SCIF_FNS(SCBRR,  0x04,  8)
SCIF_FNS(SCSCR,  0x08, 16)
SCIF_FNS(SCxSR,  0x14, 16)
SCIF_FNS(SCFCR,  0x18, 16)
SCIF_FNS(SCFDR,  0x1c, 16)
SCIF_FNS(SCxTDR, 0x20,  8)
SCIF_FNS(SCxRDR, 0x24,  8)
SCIF_FNS(SCLSR,  0x00,  0)
#elif defined(CONFIG_ARCH_SH7377) || \
      defined(CONFIG_ARCH_SH7372) || \
      defined(CONFIG_ARCH_SH73A0)
SCIF_FNS(SCSMR,  0x00, 16)
SCIF_FNS(SCBRR,  0x04,  8)
SCIF_FNS(SCSCR,  0x08, 16)
SCIF_FNS(SCTDSR, 0x0c, 16)
SCIF_FNS(SCFER,  0x10, 16)
SCIF_FNS(SCxSR,  0x14, 16)
SCIF_FNS(SCFCR,  0x18, 16)
SCIF_FNS(SCFDR,  0x1c, 16)
SCIF_FNS(SCTFDR, 0x38, 16)
SCIF_FNS(SCRFDR, 0x3c, 16)
SCIx_FNS(SCxTDR, 0x20,  8, 0x40,  8)
SCIx_FNS(SCxRDR, 0x24,  8, 0x60,  8)
SCIF_FNS(SCLSR,  0x00,  0)
#elif defined(CONFIG_CPU_SUBTYPE_SH7723) ||\
      defined(CONFIG_CPU_SUBTYPE_SH7724)
SCIx_FNS(SCSMR,  0x00, 16, 0x00, 16)
SCIx_FNS(SCBRR,  0x04,  8, 0x04,  8)
SCIx_FNS(SCSCR,  0x08, 16, 0x08, 16)
SCIx_FNS(SCxTDR, 0x20,  8, 0x0c,  8)
SCIx_FNS(SCxSR,  0x14, 16, 0x10, 16)
SCIx_FNS(SCxRDR, 0x24,  8, 0x14,  8)
SCIx_FNS(SCSPTR, 0,     0,    0,  0)
SCIF_FNS(SCFCR,  0x18, 16)
SCIF_FNS(SCFDR,  0x1c, 16)
SCIF_FNS(SCLSR,  0x24, 16)
#else
/*      reg      SCI/SH3   SCI/SH4  SCIF/SH3   SCIF/SH4  */
/*      name     off  sz   off  sz   off  sz   off  sz   */
SCIx_FNS(SCSMR,  0x00,  8, 0x00,  8, 0x00,  8, 0x00, 16)
SCIx_FNS(SCBRR,  0x02,  8, 0x04,  8, 0x02,  8, 0x04,  8)
SCIx_FNS(SCSCR,  0x04,  8, 0x08,  8, 0x04,  8, 0x08, 16)
SCIx_FNS(SCxTDR, 0x06,  8, 0x0c,  8, 0x06,  8, 0x0C,  8)
SCIx_FNS(SCxSR,  0x08,  8, 0x10,  8, 0x08, 16, 0x10, 16)
SCIx_FNS(SCxRDR, 0x0a,  8, 0x14,  8, 0x0A,  8, 0x14,  8)
SCIF_FNS(SCFCR,                      0x0c,  8, 0x18, 16)
#if defined(CONFIG_CPU_SUBTYPE_SH7760) || \
    defined(CONFIG_CPU_SUBTYPE_SH7780) || \
    defined(CONFIG_CPU_SUBTYPE_SH7785) || \
    defined(CONFIG_CPU_SUBTYPE_SH7786)
SCIF_FNS(SCFDR,			     0x0e, 16, 0x1C, 16)
SCIF_FNS(SCTFDR,		     0x0e, 16, 0x1C, 16)
SCIF_FNS(SCRFDR,		     0x0e, 16, 0x20, 16)
SCIF_FNS(SCSPTR,			0,  0, 0x24, 16)
SCIF_FNS(SCLSR,				0,  0, 0x28, 16)
#elif defined(CONFIG_CPU_SUBTYPE_SH7763)
SCIF_FNS(SCFDR,				0,  0, 0x1C, 16)
SCIF_FNS(SCTFDR,		     0x0e, 16, 0x1C, 16)
SCIF_FNS(SCRFDR,		     0x0e, 16, 0x20, 16)
SCIF_FNS(SCSPTR,			0,  0, 0x24, 16)
SCIF_FNS(SCLSR,				0,  0, 0x28, 16)
#else
SCIF_FNS(SCFDR,                      0x0e, 16, 0x1C, 16)
#if defined(CONFIG_CPU_SUBTYPE_SH7722)
SCIF_FNS(SCSPTR,                        0,  0, 0, 0)
#else
SCIF_FNS(SCSPTR,                        0,  0, 0x20, 16)
#endif
SCIF_FNS(SCLSR,                         0,  0, 0x24, 16)
#endif
#endif
#define sci_in(port, reg) sci_##reg##_in(port)
#define sci_out(port, reg, value) sci_##reg##_out(port, value)
