#ifndef VI_EX_IO_H
#define VI_EX_IO_H

#include "vi_ex_def.h"
#include "vi_ex_par.h"
#include "include\circbuffer.h"

//defaultni parametry komunice
#define VI_IO_I_BUF_SZ    ((u32)(5000000*3 + 2000000)) //alespon na jeden obrazek
#define VI_IO_O_BUF_SZ    ((u32)(5000000*3 + 2000000))
#define VI_IO_RESEND_T    (1000)    //podle rychlosti site
#define VI_IO_WAITMS_T    (3*VI_IO_WAITMS_T)

#define VI_LINK_ACK   //resi spolehlivost uz na linkove vrstve
//pokud ano pak je kazdy paket potrzovan hned po prijeti do bufferu prazdnym ACk paketem
//potrvzovani na aplikacni vrstve je zalezitost pridani ACK flagu do patricnych podpovedi

#define VI_MARKER_BROADCAST 0x0
#define VI_MARKER_SZ    4
#define VI_CRC_INI      0x41154115  //pocatecni hodnota vypoctu CRC

/*
 * parser a dispatcher datove a textove komunikace
 * resi jak stranu serveru (odesilace pozadavku) i klienta (vykonava pozadavky)
 * resi preposilani paketu a cekani na potvrzeni (pokud jde o potrvzovane povely)
 * jde o ciste abstraktni tridu tvorici obecneho predka naslednym implementacim
 * zavislym na konkretnim prenosovem mediu (typ. tcp)
 * uzce navazane na vi_ex_def kde je popsana struktura paketu a rozsah podporovanych povelu
 */

class vi_ex_io
{
public:
    enum t_vi_io_r {

        VI_IO_OK = 0,     //mame paket
        VI_IO_WAITING,    //cekame na prijem paketu
        VI_IO_TIMEOUT,    //timeout cekani na paket nebo potrvzeni prijmu
        VI_IO_SYNTAX,     //vadny paket (nesedela delka, crc, neznamy typ, apod..)
        VI_IO_FAIL        //nic neceka
    };

private:
    static u32 vi_ex_io_n;  //jen pomocny citac "referenci"
    
    volatile bool reading;  //zamyka vycitani fronty (mutex by byl lepsi, ale to az ve wrapperu)
    volatile u32 sess_id_tx;  //inkremenralni session id - zvedame s kazdym paketem
    volatile u32 sess_id_rx;  //id ktere ocekavame nejdrive; normalne stejne s tx jenom pokud cekame na vic oaketu na raz rtak zaostava

    char mark[VI_MARKER_SZ+1];  //tvori zaroven marker pro pakety unikatni pro P2P spojeni
    
    u8 *imem;  //vnitrni buffery pro prijem a resend
    u8 *omem;

protected:
    t_vi_io_r parser(u32 offs = 0);  //jednopruchodovy, kontroluje vstupni buffer a parsuje pozadavky
    t_vi_io_r resend();  //zajistujeme preposlani potvrzovanych paketu

    circbuffer<u8> *rdBuf;  //jen deklarace sablon, inicializace v konstruktoru
    circbuffer<u8> *wrBuf;

    virtual void wait10ms(void){ for(int t=10000; t; t--); } //cekani - zavisle na platforme
    virtual int read(u8 *d, u32 size) = 0;   //cteni syrovych dat, neblokujici - pokud neni co cist koncime ihned
    virtual int write(u8 *d, u32 size) = 0;  //zapis syrovych dat
    virtual void debug(const char *msg){ fprintf(stderr, "%s", msg); }  //tracy
    
    virtual void callback(t_vi_io_r event){;}     //s prijmem paketu nebo nejakou chybou obecne
    
    virtual void lock(){ while(reading) wait10ms(); reading = true; }  //zamykani cteni aby se nam nepretahovali readery
    virtual bool trylock(){ if(reading) return false; reading = true; return true; }
    virtual void unlock(){ reading = false; }

public:
    //ceka neco na prijmu
    u32 ispending(void){  //pokud tam ceka nejaky validni paket tak vraci jeho velikost

        if(!trylock()) //nesmime pokud uz to ceka jinde
            return 0;

        //fce aktivne vynuti kontrolu rx fronty; tj, nejakou dobu trva a mela by byt volana pravidlene
        if(VI_IO_OK != parser())
            return 0; 

        t_vi_exch_dgram dg;
        rdBuf->get(0, (u8 *)&dg, sizeof(dg.h));   //vykopirujem header (bez posunu rx pntr!)
        return dg.h.size;

        unlock();
    } 

    //kam to chcem posilat - ma smysl jen u sbernice
    //id musime obou stranama nastavit stejne a unikatni
    //muzeme si pomoci napriklad makrem VI_BUS_UNIQUE_MARKER_GEN
    void destination(char *id){

        snprintf(mark, sizeof(mark), "%s", id);
    }

    //prednastavi binarni paket
    t_vi_exch_dgram *preparetx(t_vi_exch_dgram *d, t_vi_exch_type t, u32 size = 0, u32 sess_id = 0){
                
        if(NULL == d) //!!!pokud je d == NULL tak ho alokuje, app si ho pak musi uvolnit
            if(NULL == (d = (t_vi_exch_dgram *) new u8[VI_HLEN() + size]))
                return NULL;

        memcpy(d->h.marker, mark, VI_MARKER_SZ);
        d->h.type = t;
        d->h.sess_id = (sess_id) ? sess_id : (t >= VI_I) ? sess_id_tx : ++sess_id_tx;  //tx inc jen pokud je paket potvrzovany
        d->h.crc = VI_CRC_INI;
        d->h.size = size;
        return d;
    }       

    //prednastavi binarni paket
    t_vi_exch_dgram *preparerx(t_vi_exch_dgram *d, t_vi_exch_type t =  VI_ANY, u32 size = 0){

        if(NULL == d) //!!!pokud je d == NULL tak ho alokuje, app si ho pak musi uvolnit
            if(NULL == (d = (t_vi_exch_dgram *) new u8[VI_HLEN() + size]))
                return NULL;

        memcpy(d->h.marker, mark, VI_MARKER_SZ);
        d->h.type = t;
        d->h.sess_id = 0;
        d->h.crc = VI_CRC_INI;
        d->h.size = size;        
        return d;
    }

    t_vi_io_r submit(t_vi_exch_dgram *d, int timeout = VI_IO_WAITMS_T);  //posle pozadavek ceka na odpoved
        //a preposila pokud jde o potvrzovaci paket do timeoutu v ms

    t_vi_io_r receive(t_vi_exch_dgram *d, int timeout = VI_IO_WAITMS_T); //vycte odpoved pokud je pripravena
        //jinak ceka po dobu timeoutu; d musi byt pripraveny fci preparerx, pokud je type = 0 pak vycte 1. ceajici; nevycte vic nez na kolik je buffer alokovany!
        //zbytek se zahazuje; vetsinou ale vime jak dlouhy buffer ceka diky fci ispending nebo holt musime buffer nadimenzovat
        //na nejvetsi mozny paket na sbernici

    vi_ex_io(char *_id = 0);
    virtual ~vi_ex_io();
};

#endif // VI_EX_IO_H
