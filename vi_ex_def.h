
#ifndef VI_EX_DEF_H
#define VI_EX_DEF_H
//datova vrstva

#include "bitmap.h"

typedef enum {

    VI_ANY = 0,

    //ridici povely
    VI_DISCOVERY = 0x10, //vyzva a rekce na pritomnost zarizeni

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

    {"DISC",      VI_DISCOVERY},
    {"BULK",      VI_BULK},
    {"ECHO",      VI_I},
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
    
    struct { //mela by byt anonymni strukturou, gcc to ale nepodporuje
        u8  marker[VI_MARKER_SZ];  //znacka zacatku hlavicky - unikatni pro kazdy P2P kanal

        u16 type;           //jinak t_vi_exch_type s kterym ale gcc zachazi jako s 32b 
            //to do rozmyslet pouziti bit struct
        u32 sess_id;     //kontrola konzistence paketu
            //zatim jen pro debug; mozno vyuzit pro dlouhe BULK data ktere muzem fragmentovat na mesi
            //a na urovni app. slozit (idelane prave pro obrazky kdy uz z hlavicky vime kolik cekat)
        u32 crc;            //to do - zatim se nepocita
            //otazka z ceho vseho se ma pociata (z celeho paketu (obrazku) asi neee)
        u32 size;           //velikost nasledujicich dat
    } __attribute__ ((packed)) h;           //to do - pokud budu umet zjistit velikost anonymni hlavicky, 
                                            //pak je pomenovana astruktura zbytecna
    union {  //anonymni union
        char              d[0];       //obecna data (bulk)
        t_vi_param   setup[0];   //nastaveni
        BITMAPINFO        bmp;        //obrazek
        u32               yuyv[0];    //raw
    };
} __attribute__ ((packed)) t_vi_exch_dgram;



#define VI_HLEN() (sizeof(t_vi_exch_dgram::h))  //velikost hlavicky pouze
#define VI_DLEN() (sizeof(t_vi_exch_dgram))  //velikost struktury
#define VI_LEN(P) ((P)->h.size + sizeof((P)->h))  //celkova velikost paketu


//pozn. - aby nam to fungovalo i jako sbernicovy protokol
//treba na seriove lince tak musime umet adresovat zarizeni
//nejlepsi je pretizit fci pro zapis do spolecneho bufferu tak aby 
//paket obalila jeste spec. sbernicovym markerem == adresou daneho nodu
//pre-parser bude pracovat v pretizene fci read ktera ale nebude delat nic jineho nez
//vycitat z bufferu podle pozice aktualich rx a tx indexu jednotlivych nodu
//bude potreba jakysi multi-kruhovy buffer (ktery uz jsem nekde videl)

//ze 2 unikatnich cisel dela jine unikatni u ktereho nesejde na poradi a bude take unikatni takze xor
//prave pro tu podporu P2P na sbernici; ID prijemce a vysilace musim znat dopredu (coz je jasne)
#define VI_BUS_UNIQUE_MARKER_GEN(u32 SRCID, u32 DESID) (SRCID ^ DESID)

#endif // VI_EX_DEF_H



























	
