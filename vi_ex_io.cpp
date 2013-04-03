#include "vi_ex_io.h"
#include <stdio.h>
#include <cstring>
#include <cstdlib>

#define VI_DMSG(...) \
{ \
    char t[2*VI_PARAM_NM]; snprintf(t, sizeof(t), __VA_ARGS__); \
    char msg[3*PP_PARAM_NM]; snprintf(msg, sizeof(msg), "Node %d %s\n", vi_ex_io_n, t); \
    debug(msg); \
}

vi_ex_io::vi_ex_io(void)  //univerzalni rezim defaultne
{
    reading = 0;
    sess_id = 0;
    vi_ex_io_n++;

    //vyrobime alespon nejakou identifikaci
    memset(mark, 0, sizeof(mark)); // == prijimame vse

    if((imem = (u8 *)calloc(VI_IO_I_BUF_SZ, 1))) VI_DMSG("input buf alocated");
    if((omem = (u8 *)calloc(VI_IO_O_BUF_SZ, 1))) VI_DMSG("output buf alocated");

    rdBuf = new circbuffer<u8>(imem, VI_IO_I_BUF_SZ);
    wrBuf = new circbuffer<u8>(omem, VI_IO_O_BUF_SZ);
}

vi_ex_io::~vi_ex_io(){

    if(imem) free(imem); imem = 0;
    if(omem) free(omem); omem = 0;

    if(rdBuf) delete rdBuf; rdBuf = 0;
    if(wrBuf) delete wrBuf; wrBuf = 0;

    VI_DMSG("destroyed");

    //vi_ex_io_n--;
}

vi_ex_io::t_vi_io_r vi_ex_io::parser(u32 offs)
{
    t_vi_exch_dgram dg;

    if(!trylock()) //neprovadime pokud uz se parsuje jinde
        return VI_IO_FAIL;

    int rn; //refresh rx fronty, fce read ma byt neblokujici, vyctem vse co je
    u8 rd[VI_COMMAND_NM*2048];  //jen po kouskach aby sme zas nemuseli alokovat kvanta
    while((rn = read(rd, sizeof(rd))) > 0){

        //VI_DMSG("Nod%d Rx imed %dB\n", id, rn);
        rdBuf->write(rd, rn);  //zapisem to k nam do buferu
    }

    int n = rdBuf->rdAvail();  //kolik je tam celkem
    //VI_DMSG("Nod%d Rx avail %dB\n", id, n);

    u32 m = VI_MARKER_SZ;  //kolik minimalne chcem

    if(!n){

        unlock();
        return VI_IO_FAIL; //zadny paket tam neni
    }

    while((n - offs) >= m){      //pokud tam neco je tak v tom hledame marker

        rdBuf->get(offs, (u8 *)dg.h.marker, m);
        u8 summ = 0; for(int im = 0; im<m; i++) summ |= dg.h.marker[im];
        if((0 == memcmp(dg.h.marker, vi_b_mark, m) || (VI_MARKER_BROADCAST == summ))){  //unicast nebo broadcast

            //mame binarmi marker
            rdBuf->get(offs, (u8 *)&dg, sizeof(dg.h));   //vyctem hlavicku

            if(dg.h.size >= rdBuf->size)  //muze se nam vubec vejit?
                goto PARSER_TRASH_IT; //ne - zahodime vse co ceka

            if(n >= VI_LEN(&dg)){  //mame tam data z celeho paketu

                if(dg.h.crc != vi_CRC_INI){ //to do opravdova kontrola crc

                    n = VI_LEN(&dg);  //zahodime jen ten datagram
                    goto PARSER_TRASH_IT;
                }

                VI_DMSG("rx 0x%x / no%d / %dB", dg.h.type, dg.h.sess_id, dg.h.size);
                callback(VI_IO_OK);//mame paket
                unlock();
                return VI_IO_OK;
            }

            unlock();
            return VI_IO_WAITING;
        }

        n -= 1;
        rdBuf->read(0, 1); //zadny header to nebyl, zahodime jeden byte a zkousime dal
        //VI_DMSG("(!) rx purged, %dB\n", 1);
    }


PARSER_TRASH_IT:  //zahodime cekajici data
    VI_DMSG("(!) rx purged, %dB", n);
    rdBuf->read(0, n);
    callback(VI_IO_SYNTAX);
    unlock();

    return VI_IO_SYNTAX;
}

vi_ex_io::t_vi_io_r vi_ex_io::resend()
{
#ifndef VI_LINK_ACK
    return VI_IO_OK;
#endif // VI_LINK_ACK

    u32 n = wrBuf->rdAvail();
    if(!n) return VI_IO_OK;

    u8 dg[n];  //znovu vyctem a poslem
    wrBuf->get(0, dg, n);
    write((u8 *)dg, n);
    VI_DMSG("tx resend %dB", n);
    return VI_IO_OK;
}

vi_ex_io::t_vi_io_r vi_ex_io::submit(t_vi_exch_dgram *d, int timeout){

    if(!d) return VI_IO_FAIL;

    timeout = 10 * (timeout / 10); //resend to vyzaduje v nasobcich 10ms

#ifndef VI_LINK_ACK
    d->h.type &= ~VI_ACK;  //bypass
#endif // VI_LINK_ACK

    u8 summ = 0; for(int im = 0; im<VI_MARKER_SZ; i++) summ |= d->h.marker[im];
    if((0 == memcmp(d->h.marker, mark, VI_MARKER_SZ)) || (VI_MARKER_BROADCAST == summ)){  //sanity check

        write((u8 *)d, VI_LEN(d));       //odeslem
        VI_DMSG("tx 0x%x / no%d / %dB", d->h.type, d->h.sess_id, d->h.size);
    } else {

        VI_DMSG("(!) rejected 0x%x / no%d", d->h.type, d->h.sess_id);
        return VI_IO_FAIL;  //jinak
    }

    if((timeout) && ((d->h.type & ~VI_ACK) < vi_I)) //chceme potrvzeni?
        return VI_IO_OK;

    u32 n = wrBuf->wrAvail();
    if(n >= VI_LEN(d)){

        wrBuf->write((u8 *)d, VI_LEN(d)); //tim zajistime resend, jinak holt bez nej
    } else {

        VI_DMSG("(!) too long for resend 0x%x / no%d", d->h.type, d->h.sess_id);
    }

    //zamek hlavne zabranuje tomu aby v ramci cekani volala parser i appl
    bool lo = trylock(); //pokud uz bylo zamknuto nechavame byt (mohou se zanorovat)
    t_vi_exch_dgram dg;
    u32 offs = 0;

    while(timeout > 0){  //jde o potrvzovana data, cekame na ACK

        if(VI_IO_OK == parser(offs)){

            rdBuf->get(0, (u8 *)&dg, sizeof(dg.h));   //nakopirujem head
            if((VI_ACK & dg.h.type) && (d->h.sess_id == dg.h.sess_id)){   //dostali jsme potvrzeni na nas paket?

                wrBuf->read(0, n);   //paket potrvzen
                VI_DMSG("rx ack / no%d", dg.h.sess_id);
                return VI_IO_OK;
            } else {

                offs += VI_LEN(&dg); //tendle to nebyl; budem hledat za nim
            }
        }

        wait10ms();  //cekame a odmerujem
        if((((timeout -= 10) % VI_IO_RESEND_T) == 0) && (timeout > 0)) //co X vterin
            resend();  //zajistuje preposilani paketu
    }

    VI_DMSG("(!) rx ack timeout / no%d", dg.h.sess_id);
    u32 txn = wrBuf->read(0, wrBuf->size);  //zrusime resend
    VI_DMSG("(!) tx purged %dB", txn);

    if(lo) unlock();
    return VI_IO_TIMEOUT; //Ok pokud prislo ACK nebo pokud nebylo treba
}

vi_ex_io::t_vi_io_r vi_ex_io::receive(t_vi_exch_dgram *d, int timeout){

    u8 summ = 0; for(int im = 0; im<VI_MARKER_SZ; i++) summ |= d->h.marker[im];
    if((0 != memcmp(d->h.marker, mark, VI_MARKER_SZ)) && (VI_MARKER_BROADCAST != summ)){  //sanity check

        VI_DMSG("(!) rx dgram not prepared");
        return VI_IO_FAIL;  //jinak
    }

    lock(); //nechcem aby kdokoli jiny take cekal, v jednovlakene app riziko deadlocku pokud
    t_vi_exch_dgram dg;
    memset(&dg, 0, sizeof(dg));
    u32 offs = 0;

    while(timeout > 0){  //cekame na prijem celeho paketu (pokud tam neni)

        if(VI_IO_OK == parser(offs)){

            rdBuf->get(0, (u8 *)&dg, sizeof(dg.h));   //vyctem hlavicku

            if((d->h.type == 0) || (d->h.type == dg.h.type)){  //ten na ktery cekame

#ifdef VI_LINK_ACK
                if((dg.h.type &= ~VI_ACK) >= VI_I){  //paket s potrvzenim?

                    t_vi_exch_dgram ack;
                    preparetx(&ack, VI_ACK, 0, d->h.sess_id);
                    write((u8 *)&ack, sizeof(ack.h));  //odeslem hned potvrzovaci paket
                    VI_DMSG("tx ack / no%d", d->h.sess_id); //!!s id puvodniho paketu
                }
#endif // VI_LINK_ACK

                u32 sz = VI_LEN(&dg); //celkova delka paketu
                if(VI_LEN(d) < sz){  //rx paket ma ukazovat na co je alokovany

                    VI_DMSG("(!) rx dgram size < pending");
                    sz = VI_LEN(d); //vyctem jen co se da; zbytek se pak v parseru zahodi
                }

                rdBuf->read((u8 *)d, sz);   //vyctem a posunem kruh buffer
                d->size = sz - sizeof(dg.h);  //upravime skutecnou velikost jestdi bylo kraceno
                return VI_IO_OK;
            }

            offs += VI_LEN(&dg); //tendle to nebyl; budem hledat za nim
        }

        wait10ms();
        timeout -= 10;  //pozdrzeni parseru
    }

    VI_DMSG("(!) rx timeout, rx purged");
    rdBuf->read(0, rdBuf->size);   //zrusime prijem pokud je tam neco nepodporovaneho

    unlock();
    return VI_IO_TIMEOUT; //Ok pokud prislo ACK nebo pokud nebylo treba
}

