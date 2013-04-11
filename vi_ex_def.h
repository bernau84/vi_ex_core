
#ifndef VI_EX_DEF_H
#define VI_EX_DEF_H
//datova vrstva

#include <stdint.h>
#include <cstring>

#define u8	unsigned char
#define u16	unsigned short
#define u32	unsigned int
#define s8	signed char
#define s16	signed short
#define s32	signed int
#define u64	unsigned long int
#define s64	signed long int

#define VI_MARKER_SZ    4
typedef u8 (*t_vi_io_id)[VI_MARKER_SZ];

typedef enum {

    VI_ANY = 0,

    //ridici povely
    VI_ECHO_REQ = 0x10, //vyzva na pritomnost zarizeni; muze se poslat i jako broadcast ala discovery pakety
    VI_ECHO_REP, //odpoved na echo
    VI_REG,  //registracni paket; dosud nespojeny nod (ktery posloucha vse) to inicializuje s vysilajicim

    //nepotrvzovana data
    VI_BULK = 0x20,  	//nestrukturovana/nepotvrzovana data - jina

    //potvrzovana data
    VI_I = 0x80,      //zacatek potvrzovanych dat
    VI_I_CAP,         //zadost o capabilities
    VI_I_SETUP_SET,   //zapis nastaveni a vycteni nastaveni
    VI_I_SETUP_GET,   //zadost o vycteni nastaveni a vlastni vycteni nastaveni

//    VI_I_STREAM_ON,   //zapnuti streamingu
//    VI_I_STREAM_OFF,  //vypnuti
//    VI_I_SNAP,        //zadost o obrazek, vycteni obrazku
//    VI_I_FLASH,       //nastaveni rezimu prisvetleni
//    VI_I_SHUTTER,     //nastaveni rezimu uzaverky (clony)

    //potvrzeni - flag
    VI_ACK = 0x8000
} t_vi_exch_type;

const struct t_vi_exch_cmd{

    const char       *cmd;
    t_vi_exch_type    type;
} vi_exch_cmd[] = {  //definicce textovych povelu + vazba na binarni

    {"ECHO",      VI_ECHO_REQ},
    {"BULK",      VI_BULK},
    {"CAPAB",     VI_I_CAP},
    {"SET",       VI_I_SETUP_SET},  //priklad: >>SET Contrast 30
    {"GET",       VI_I_SETUP_GET},  //priklad: >>GET Brightness
//    {"FLASH",     VI_I_FLASH},
//    {"SHUTTER",   VI_I_SHUTTER},
//    {"VIDEOOFF",  VI_I_STREAM_OFF},
//    {"VIDEOON",   VI_I_STREAM_ON},
//    {"SNAP",      VI_I_SNAP},       //priklad: <<SNAP + binarni paket vi_BULK
    {"UNKNOWN", (t_vi_exch_type)0}  //end mark
};


typedef struct {    //genericky paket

    u8  marker[VI_MARKER_SZ];  //znacka zacatku hlavicky - unikatni pro kazdy P2P kanal
    u32 crc;            //hlavicka a nasledujich max 64B
    u32 sess_id;        //kontrola konzistence paketu; slouzi k potrvzovani
    u16 type;           //jinak t_vi_exch_type s kterym ale gcc zachazi jako s 32b
    u32 size;           //velikost nasledujicich dat
    u8  d[0];           //obecna data
} __attribute__ ((packed)) t_vi_exch_dgram;


#define VI_HLEN() (sizeof(t_vi_exch_dgram))  //velikost hlavicky pouze
#define VI_LEN(P) ((P)->size + sizeof(t_vi_exch_dgram))  //celkova velikost paketu


static const u32 vi_ex_crc32_table[] =
{
  0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,  0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
  0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,  0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
  0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,  0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
  0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011,  0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
  0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,  0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
  0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81,  0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
  0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49,  0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
  0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,  0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
  0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae,  0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
  0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,  0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
  0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,  0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
  0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066,  0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
  0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,  0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
  0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,  0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
  0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,  0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
  0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686,  0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
  0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,  0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
  0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,  0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
  0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47,  0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
  0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,  0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
  0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7,  0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
  0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f,  0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
  0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,  0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
  0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f,  0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
  0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,  0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
  0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,  0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
  0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30,  0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
  0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,  0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
  0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,  0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
  0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,  0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
  0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0,  0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
  0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,  0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

inline u32 vi_ex_crc32 (const u8 *d, int n, u32 icrc){

    u32 crc = icrc;
    while(n--) crc = (crc << 8) ^ vi_ex_crc32_table[((crc >> 24) ^ *d++) & 255];
    return crc;
}

static const u8 vi_marker_broadcast[VI_MARKER_SZ] = {0, 0, 0, 0};

inline bool vi_is_broadcast(t_vi_io_id m){
  
  return (0 == memcmp(m, vi_marker_broadcast, VI_MARKER_SZ)) ? 1 : 0;
}  

inline void vi_set_broadcast(t_vi_io_id m){

  memcpy(m, vi_marker_broadcast, VI_MARKER_SZ);
}

//inline bool vi_is_broadcast(t_vi_exch_dgram::h *h){
  
//  return (0 == memcmp(h->marker, vi_marker_broadcast, VI_MARKER_SZ)) ? 1 : 0;
//}

//inline void vi_set_broadcast(t_vi_exch_dgram::h *h){

//  memcpy(h->marker, vi_marker_broadcast, VI_MARKER_SZ);
//}


#endif // VI_EX_DEF_H



























	
