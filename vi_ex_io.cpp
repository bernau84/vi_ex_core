#include "vi_ex_io.h"
#include <stdio.h>
#include <cstring>
#include <cstdlib>

#define VI_DMSG(...) \
{ \
    char t[2*VI_PARAM_NM]; snprintf(t, sizeof(t), __VA_ARGS__); \
    char msg[3*PP_PARAM_NM]; snprintf(msg, sizeof(msg), "Node %d %s\n", cref, t); \
    debug(msg); \
}


vi_ex_io::vi_ex_io(t_vi_io_mn _name, circbuffer<u8> *_rdbuf, circbuffer<u8> *_wrbuf)  //univerzalni rezim defaultne
{
    reading = 0;
    sess_id = 0;
    cref++;

    if(_name == NULL) snprintf(name, sizeof(name), "%04d", cref);  //defaultni nazev je poradove cislo v systemu
        else strcpy(name, _name); //jinak berem co nam nuti

    //vyrobime alespon nejakou identifikaci
    memset(mark, 0, sizeof(mark)); // == prijimame vse

    omem = imem = NULL;

    if(_rdbuf != NULL) rdBuf = new circbuffer<u8>(*_rdbuf);  //kopirovaci konstruktor; bufer bude sdileny
        else if((imem = (u8 *)calloc(VI_IO_I_BUF_SZ, 1))) VI_DMSG("input buf alocated");
            else rdBuf = new circbuffer<u8>(imem, VI_IO_I_BUF_SZ);

    if(_rdbuf != NULL) wrBuf = new circbuffer<u8>(*_wrbuf);
        else if((omem = (u8 *)calloc(VI_IO_O_BUF_SZ, 1))) VI_DMSG("output buf alocated");
            else wrBuf = new circbuffer<u8>(omem, VI_IO_O_BUF_SZ);
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
    u32 n = dg->size + (&(dg->crc) - dg);
    if(n > 64) n = 64;
    return vi_ex_crc32(&(dg->crc), n, VI_CRC_INI);
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
    u8 summ; for(int im = 0; im<m; im++) summ |= marker[im];  //mame nejaky marker; pokud musime poslouchat vsechno   

    if(!n){

        unlock();
        return VI_IO_FAIL; //zadny paket tam neni
    }

    while((n - offs) >= m){      //pokud tam neco je tak v tom hledame marker

        rdBuf->get(offs, (u8 *)dg.h.marker, m);

        if(VI_MARKER_BROADCAST != summ)  //pokud uz nemame flag VI_MARKER_BROADCAST sami (tj. nas marker je neinizializovany == 0)
            for(int im = 0; im<m; im++) summ |= dg.h.marker[im];

        if((0 == memcmp(dg.h.marker, vi_b_mark, m) || //unicast
            (VI_MARKER_BROADCAST == summ))){   //broadcast

            //mame binarmi marker
            rdBuf->get(offs, (u8 *)&dg, sizeof(dg.h));   //vyctem hlavicku

            if(dg.h.size >= rdBuf->size)  //muze se nam vubec vejit?
                goto PARSER_TRASH_IT; //ne - zahodime vse co ceka

            if(n >= VI_LEN(&dg)){  //mame tam data z celeho paketu

                if(dg.crc == crc(&dg)){ //to do opravdova kontrola crc

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

    t_vi_exch_dgram::h hd;
    wrBuf->get(0, &hd, sizeof(hd));  //jen hlavicka

    int rn = sizeof(hd)+hd.size;
    if((n < rn) || (0 != memcmp(d->h.marker, mark, VI_MARKER_SZ)) { //nic naseho k preposlani

        VI_DMSG("(!) resend length/marker error 0x%x / no%d / %dB", hd.type, hd.sess_id, hd.size);
        return VI_IO_OK; //neco spatne; koncime, jako by se stalo
    }

    u8 *buf = new u8[rn]; wrBuf->get(0, buf, rn); //znovu vyctem
    t_vi_exch_dgram *d = (t_vi_exch_dgram *)buf;
    write((u8 *)d, rn);  //odeslem
    VI_DMSG("resend 0x%x / no%d / %dB", d->h.type, d->h.sess_id, d->h.size);

    return VI_IO_OK;
}

vi_ex_io::t_vi_io_r vi_ex_io::submit(t_vi_exch_dgram *d, int timeout){

    if(!d) return VI_IO_FAIL;

    d->crc = crc(d); //dopocitame

    u8 summ = 0; for(int im = 0; im<VI_MARKER_SZ; i++) summ |= d->h.marker[im];
    if((0 == memcmp(d->h.marker, mark, VI_MARKER_SZ)) || (VI_MARKER_BROADCAST == summ)){  //sanity check

        write((u8 *)d, VI_LEN(d));       //odeslem
        VI_DMSG("tx 0x%x / no%d / %dB", d->h.type, d->h.sess_id, d->h.size);
    } else {

        VI_DMSG("(!) rejected 0x%x / no%d", d->h.type, d->h.sess_id);
        return VI_IO_FAIL;  //jinak
    }

#ifdef VI_LINK_ACK
    if((timeout) && ((d->h.type & ~VI_ACK) < VI_I)) //chceme potrvzeni?
#endif // VI_LINK_ACK        
        return VI_IO_OK;

    timeout = 10 * (timeout / 10); //resend to vyzaduje v nasobcich 10ms    

    wrBuf->read_mark = wrBuf->write_mark; //nepodporujem odesilani vicero paketu
    if(wrBuf->size >= VI_LEN(d)){

        wrBuf->write((u8 *)d, VI_LEN(d)); //tim zajistime resend, jinak holt bez nej - jen cekame na dovysilani
    } else {

        VI_DMSG("(!) too long for resend 0x%x / no%d", d->h.type, d->h.sess_id);
    }

    //zamek hlavne zabranuje tomu aby v ramci cekani volala parser i appl
    bool lo = trylock(); //pokud uz bylo zamknuto nechavame byt (mohou se zanorovat)
    t_vi_exch_dgram::h hd;
    u32 offs = 0;

    while(timeout > 0){  //jde o potrvzovana data, cekame na ACK

        if(VI_IO_OK == parser(offs)){

            rdBuf->get(0, (u8 *)&hd, sizeof(hd));   //nakopirujem head
            if((VI_ACK & hd.type) && (d->h.sess_id == hd.sess_id)){   //dostali jsme potvrzeni na nas paket?

                VI_DMSG("rx ack / no%d", hd.sess_id);
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
    t_vi_exch_dgram::h hd;
    memset(&hd, 0, sizeof(hd));
    u32 offs = 0;

    while(timeout > 0){  //cekame na prijem celeho paketu (pokud tam neni)

        if(VI_IO_OK == parser(offs)){

            rdBuf->get(offs, (u8 *)&hd, sizeof(dg.h));   //vyctem hlavicku

            if((hd.type == 0) || (d->h.type == hd.type)){  //ten na ktery cekame

#ifdef VI_LINK_ACK
                if((hd.type &= ~VI_ACK) >= VI_I){  //paket s potrvzenim?

                    t_vi_exch_dgram ack;
                    preparetx(&ack, VI_ACK, 0, hd.sess_id);
                    write((u8 *)&ack, sizeof(ack.h));  //odeslem hned potvrzovaci paket
                    VI_DMSG("tx ack / no%d", hd.sess_id); //!!s id puvodniho paketu
                }
#endif // VI_LINK_ACK

                u32 sz = sizeof(hd)+h.size; //celkova delka paketu
                if(VI_LEN(d) < sz){  //rx paket ma ukazovat na co je alokovany

                    VI_DMSG("(!) rx / no%d dgram size < pending", hd.sess_id);
                    sz = VI_LEN(d); //vyctem jen co se da; zbytek se pak v parseru zahodi
                }

                if(offs){
                    
                    rdBuf->read((u8 *)d, sz);   //vyctem a posunem kruh buffer
                } else {
                    
                    rdBuf->get(offs, (u8 *)d, sz)   //abychmo nepreskakovali tak vyctem bez posunu rd
                    hd.type = VI_ANY; //ale  paket zneplatnime aby nebyl v app zprac. 2x
                    rdBuf->set(offs, (u8 *)&hd, sizeof(dg.h));   //zapisem zneplatnenou hlavicku
                }

                d->size = sz - sizeof(dg.h);  //upravime skutecnou velikost jestli bylo kraceno
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

