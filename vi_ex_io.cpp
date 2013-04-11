#include "vi_ex_io.h"
#include <stdio.h>
#include <cstring>
#include <cstdlib>

u32 vi_ex_io::cref;  //staticka promenna musi mit deklaraci zvlast

#define VI_DMSG(...) \
{ \
    char arg[256]; snprintf(arg, sizeof(arg), __VA_ARGS__); \
    char msg[256]; snprintf(msg, sizeof(msg), "%s: %s\n", (char *)name, arg); \
    debug(msg); \
} \

vi_ex_io::vi_ex_io(t_vi_io_mn _name, int iosize){

    reading = 0;
    sess_id = 0;
    cref++;

    //vyrobime alespon nejakou identifikaci
    if(_name == NULL) snprintf(name, sizeof(name), "%04d", cref);  //defaultni nazev je poradove cislo v systemu
        else memcpy(name, _name, sizeof(name)); //jinak berem co nam nuti

    memset(mark, 0, sizeof(mark)); // == defultne prijimame vse

    omem = imem = NULL;
    rdBuf = wrBuf = NULL;

    if(0 == iosize) return; //buffery se dodefinuji jinde

    if(NULL != (imem = (u8 *)calloc(iosize, 1)))
        rdBuf = new circbuffer<u8>(imem, iosize);
    else
        VI_DMSG("(!) input aloc. failed");

    if(NULL != (omem = (u8 *)calloc(iosize, 1)))
        wrBuf = new circbuffer<u8>(omem, iosize);
    else
        VI_DMSG("(!) output aloc. failed");
}

vi_ex_io::~vi_ex_io(){

    if(imem) free(imem); imem = 0;
    if(omem) free(omem); omem = 0;

    if(rdBuf) delete rdBuf; rdBuf = 0;
    if(wrBuf) delete wrBuf; wrBuf = 0;

    VI_DMSG("destroyed");

    //cref--;
}

u32 vi_ex_io::crc(t_vi_exch_dgram *dg)
{
    u32 n = dg->size + ((u8 *)&(dg->crc) - (u8 *)dg); //pocet dat za CRC polickem
    if(n > 64) n = 64;  //pocitame jen ze zacatku
    return vi_ex_crc32((u8 *)&(dg->crc), n, VI_CRC_INI);
}

vi_ex_io::t_vi_io_r vi_ex_io::parser(u32 offs)
{
    t_vi_exch_dgram dg;

    int rn; //refresh rx fronty, fce read ma byt neblokujici, vyctem vse co je
    u8 rd[2048];  //jen po kouskach aby sme zas nemuseli alokovat kvanta
    while((rn = read(rd, sizeof(rd))) > 0){

        //VI_DMSG("Nod%d Rx imed %dB\n", id, rn);
        rdBuf->write(rd, rn);  //zapisem to k nam do buferu
    }

    u32 n = rdBuf->rdAvail();  //kolik je tam celkem
    //VI_DMSG("Nod%d Rx avail %dB\n", id, n);

    if(!n) return VI_IO_FAIL; //zadny paket tam neni

    while((n - offs) >= VI_MARKER_SZ){      //pokud tam neco je tak v tom hledame marker

        rdBuf->get(offs, (u8 *)dg.marker, VI_MARKER_SZ);

        if((0 == memcmp(dg.marker, mark, VI_MARKER_SZ) || //unicast
            (vi_is_broadcast(&dg.marker)) ||  //broadcast
            (vi_is_broadcast((t_vi_io_id)mark)))){   //no filter

            //mame binarmi marker
            rdBuf->get(offs, (u8 *)&dg, VI_HLEN());   //vyctem hlavicku

            if(dg.size >= rdBuf->size)  //muze se nam vubec vejit?
                goto PARSER_TRASH_IT; //ne - zahodime vse co ceka

            if(n >= VI_LEN(&dg)){  //mame tam data z celeho paketu

                if(dg.crc == crc(&dg)){ //to do opravdova kontrola crc

                    n = VI_LEN(&dg);  //zahodime jen ten datagram
                    goto PARSER_TRASH_IT;
                }

                VI_DMSG("rx 0x%x / no%d / %dB", dg.type, dg.sess_id, dg.size);

                if(false == islocked()) //probiha cteni
                    callback(VI_IO_OK); //nejspise na zaklade predesleho callbacku

                return VI_IO_OK;  //mame paket
            }

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
    return VI_IO_SYNTAX;
}

vi_ex_io::t_vi_io_r vi_ex_io::resend()
{
#ifndef VI_LINK_ACK
    return VI_IO_OK;
#endif // VI_LINK_ACK

    int n = wrBuf->rdAvail();

    t_vi_exch_dgram dg;
    wrBuf->get(0, (u8 *)&dg, VI_HLEN());  //jen hlavicka

    int rn = VI_LEN(&dg);
    if((n < rn) || (0 != memcmp(dg.marker, mark, VI_MARKER_SZ))) { //nic naseho k preposlani

        VI_DMSG("(!) resend length/marker error 0x%x / no%d / %dB", dg.type, dg.sess_id, dg.size);
        return VI_IO_OK; //neco spatne; koncime, jako by se stalo
    }

    u8 *buf = new u8[rn];
    wrBuf->get(0, buf, rn); //znovu vyctem vse
    write(buf, rn);  //odeslem
    delete[] buf;

    VI_DMSG("resend 0x%x / no%d / %dB", dg.type, dg.sess_id, dg.size);
    return VI_IO_OK;
}

vi_ex_io::t_vi_io_r vi_ex_io::submit(t_vi_exch_dgram *d, int timeout){

    if(!d) return VI_IO_FAIL;

    d->crc = crc(d); //dopocitame

    if((0 == memcmp(d->marker, mark, VI_MARKER_SZ)) ||
            (vi_is_broadcast((t_vi_io_id)d->marker))){  //je to paket od nas? (vcertne broadcastu)

        write((u8 *)d, VI_LEN(d));       //odeslem
        VI_DMSG("tx 0x%x / no%d / %dB", d->type, d->sess_id, d->size);
    } else {

        VI_DMSG("(!) rejected 0x%x / no%d", d->type, d->sess_id);
        return VI_IO_FAIL;  //jinak
    }

#ifdef VI_LINK_ACK
    if((timeout) && ((d->type & ~VI_ACK) < VI_I)) //chceme potrvzeni?
#endif // VI_LINK_ACK        
        return VI_IO_OK;

    timeout = 10 * (timeout / 10); //resend to vyzaduje v nasobcich 10ms    

    wrBuf->read_mark = wrBuf->write_mark; //nepodporujem odesilani vicero paketu
    if(wrBuf->size >= VI_LEN(d)){

        wrBuf->write((u8 *)d, VI_LEN(d)); //tim zajistime resend, jinak holt bez nej - jen cekame na dovysilani
    } else {

        VI_DMSG("(!) too long for resend 0x%x / no%d", d->type, d->sess_id);
    }

    //zamek hlavne zabranuje tomu aby v ramci cekani volala parser i appl
    bool lo = trylock(); //pokud uz bylo zamknuto nechavame byt (mohou se zanorovat)
    t_vi_exch_dgram dg;
    u32 offs = 0;

    while(timeout > 0){  //jde o potrvzovana data, cekame na ACK

        if(VI_IO_OK == parser(offs)){

            rdBuf->get(0, (u8 *)&dg, VI_HLEN());   //nakopirujem head
            if((VI_ACK & dg.type) && (d->sess_id == dg.sess_id)){   //dostali jsme potvrzeni na nas paket?

                VI_DMSG("rx ack / no%d", dg.sess_id);
                return VI_IO_OK;
            } else {

                offs += VI_LEN(&dg); //tendle to nebyl; budem hledat za nim
            }
        }

        wait10ms();  //cekame a odmerujem
        if((((timeout -= 10) % VI_IO_RESEND_T) == 0) && (timeout > 0)) //co X vterin
            resend();  //zajistuje preposilani paketu
    }

    VI_DMSG("(!) rx ack timeout / no%d", dg.sess_id);

    if(lo) unlock();
    return VI_IO_TIMEOUT; //Ok pokud prislo ACK nebo pokud nebylo treba
}

vi_ex_io::t_vi_io_r vi_ex_io::receive(t_vi_exch_dgram *d, int timeout){

    if((0 != memcmp(d->marker, mark, VI_MARKER_SZ)) &&
            (false == vi_is_broadcast((t_vi_io_id)d->marker))){  //sanity check

        VI_DMSG("(!) rx dgram not prepared");
        return VI_IO_FAIL;  //jinak
    }

    lock(); //nechcem aby kdokoli jiny take cekal, v jednovlakene app riziko deadlocku pokud
    t_vi_exch_dgram dg;
    memset(&dg, 0, sizeof(dg));
    u32 offs = 0;

    while(timeout > 0){  //cekame na prijem celeho paketu (pokud tam neni)

        if(VI_IO_OK == parser(offs)){

            rdBuf->get(offs, (u8 *)&dg, VI_HLEN());   //vyctem hlavicku

            if((d->type == VI_ANY) || (d->type == dg.type)){  //ten na ktery cekame

#ifdef VI_LINK_ACK
                if((dg.type &= ~VI_ACK) >= VI_I){  //paket s potrvzenim?

                    t_vi_exch_dgram ack;
                    preparetx(&ack, VI_ACK, 0, dg.sess_id);
                    write((u8 *)&ack, VI_LEN(&ack));  //odeslem hned potvrzovaci paket
                    VI_DMSG("tx ack / no%d", dg.sess_id); //!!s id puvodniho paketu
                }
#endif // VI_LINK_ACK

                u32 sz = VI_LEN(&dg); //celkova delka paketu
                if(VI_LEN(d) < sz){  //rx paket ma ukazovat na co je alokovany

                    VI_DMSG("(!) rx / no%d dgram size < pending", dg.sess_id);
                    sz = VI_LEN(d); //vyctem jen co se da; zbytek se pak v parseru zahodi
                }

                if(0 == offs){
                    
                    rdBuf->read((u8 *)d, sz);   //vyctem a posunem kruh buffer
                } else {
                    
                    rdBuf->get(offs, (u8 *)d, sz);  //abychmo nepreskakovali tak vyctem bez posunu rd
                    dg.type = VI_ANY; //ale  paket zneplatnime aby nebyl v app zprac. 2x
                    rdBuf->set(offs, (u8 *)&dg, VI_HLEN());   //zapisem zneplatnenou hlavicku
                }

                d->size = sz - VI_HLEN();  //upravime skutecnou velikost jestli bylo kraceno
                unlock();
                return VI_IO_OK;
            }

            offs += VI_LEN(&dg); //tendle to nebyl; budem hledat za nim
        }

        wait10ms();
        timeout -= 10;  //pozdrzeni parseru
    }

    VI_DMSG("(!) rx timeout");

    unlock();
    return VI_IO_TIMEOUT; //Ok pokud prislo ACK nebo pokud nebylo treba
}

