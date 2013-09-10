#include "vi_ex_io.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

u32 vi_ex_io::cref;  //static has to be declared extra

#define VI_DMSG(...) \
{ \
    char arg[256]; snprintf(arg, sizeof(arg), __VA_ARGS__); \
    char msg[256]; snprintf(msg, sizeof(msg), "%s: %s\n", (char *)name, arg); \
    debug(msg); \
} \

vi_ex_io::vi_ex_io(p_vi_io_mn _name, int iosize){

    reading = 0;
    sess_id = 0;
    cref++;

    //default identification with ref. counter
    if(_name == NULL) snprintf(name, sizeof(name), "%04d", cref);
        else memcpy(name, _name, sizeof(name));

    memset(mark, 0, sizeof(mark)); // == receive everything by default

    omem = imem = NULL;
    rdBuf = wrBuf = NULL;

    if(0 == iosize) return;

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

vi_ex_io::t_vi_io_r vi_ex_io::parser(u32 offs)
{
    int rn;
    u8 rd[1024];

    do {

        if((rn = read(rd, sizeof(rd)))) //piece by piece
            rdBuf->write(rd, rn);  //write to rx circular buffer; shifts write pointer
    } while(rn == sizeof(rd));

    u32 n = rdBuf->rdAvail();  //all byte in rx
    //VI_DMSG("Nod%d Rx avail %dB\n", id, n);

    if(!n) return VI_IO_FAIL;

    t_vi_exch_dgram dg;
    while((n - offs) >= VI_MARKER_SZ){      //look for marker

        rdBuf->get(offs, (u8 *)dg.marker, VI_MARKER_SZ);

        if((0 == memcmp(dg.marker, mark, VI_MARKER_SZ) || //unicast
            (vi_is_broadcast(&dg.marker)) ||  //broadcast
            (vi_is_broadcast((p_vi_io_id)mark)))){   //no filter

            //have something?
            u8 sdg[VI_HLEN()+64]; //head and crc controled payload
            rdBuf->get(0, sdg, VI_HLEN()); //copy head from rx queue (no read in fact)
            vi_dg_deserialize(&dg, sdg, VI_HLEN());  //fill dg head

            rn = VI_LEN(&dg);
            if(rn >= rdBuf->size) //size sanity check
                goto PARSER_TRASH_IT; //some nonsense

            if(n >= rn){  //is it whole?

                //read again begin of payload if any
                if(dg.size){

                    rn = (rn < sizeof(sdg)) ? rn : sizeof(sdg); //limit n
                    rdBuf->get(0, sdg, rn);
                }

                //crc compatible? - test directly on streamed data
                u32 icrc = vi_ex_crc32(
                                sdg + VI_MARKER_SZ + VI_CRC_SZ, //shft above crc covered data
                                rn - VI_MARKER_SZ - VI_CRC_SZ,
                                VI_CRC_INI);

                if(dg.crc != icrc){

                    n = VI_LEN(&dg);  //throw whole datagram
                    goto PARSER_TRASH_IT;
                }

                VI_DMSG("rx 0x%x / no%d / %dB", dg.type, dg.sess_id, dg.size);

                if(false == islocked()) //rx queue is reading elsewhere
                    callback(VI_IO_OK); //call back will be redundat

                return VI_IO_OK;  //we have packet
            }

            return VI_IO_WAITING;
        }

        n -= 1;
        rdBuf->read(0, 1); //no header, shift 1byte for another match test
        //VI_DMSG("(!) rx purged, %dB\n", 1);
    }


PARSER_TRASH_IT:
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

    u8 sdg[VI_HLEN()];
    rdBuf->get(0, sdg, VI_HLEN());   //copy head from rx queue (no read in fact)

    t_vi_exch_dgram dg;
    vi_dg_deserialize(&dg, sdg, VI_HLEN());  //fill dg head

    int rn = VI_LEN(&dg);
    if((n < rn) || (0 != memcmp(dg.marker, mark, VI_MARKER_SZ))) { //is it our packet?

        VI_DMSG("(!) resend length/marker error 0x%x / no%d / %dB", dg.type, dg.sess_id, dg.size);
        return VI_IO_OK; //end, cached datagram is wrong
    }

    u8 *b = new u8[rn];
    wrBuf->get(0, b, rn); //readback
    write(b, rn);  //tx
    delete[] b;

    VI_DMSG("resend 0x%x / no%d / %dB", dg.type, dg.sess_id, dg.size);
    return VI_IO_OK;
}

vi_ex_io::t_vi_io_r vi_ex_io::submit(t_vi_exch_dgram *d, int timeout){

    if(!d) return VI_IO_FAIL;

    d->crc = vi_ex_crc32(d); //fill crc
    u32 rn = VI_LEN(d);
    u8 *b = NULL;

    if((0 == memcmp(d->marker, mark, VI_MARKER_SZ)) ||
            (vi_is_broadcast((p_vi_io_id)d->marker))){  //bradcast or unicast

        b = new u8[rn];
        vi_dg_serialize(d, b, rn);
        write((u8 *)b, rn);       //send imediately

        VI_DMSG("tx 0x%x / no%d / %dB", d->type, d->sess_id, d->size);
    } else {

        VI_DMSG("(!) rejected 0x%x / no%d", d->type, d->sess_id);
        return VI_IO_FAIL;  //unsuported format
    }

#ifdef VI_LINK_ACK
    if((timeout) && ((d->type & ~VI_ACK) < VI_I)) //chceme potrvzeni?
#endif // VI_LINK_ACK        
        return VI_IO_OK;

    timeout = 10 * (timeout / 10); //resend tmo in 10ms dot product

    wrBuf->read_mark = wrBuf->write_mark; //multiple packet resend is not supported
    if(wrBuf->size >= rn){

        wrBuf->write((u8 *)b, rn); //resend
    } else {

        VI_DMSG("(!) too long for resend 0x%x / no%d", d->type, d->sess_id);  //no resend
    }

    if(b) delete[] b;

    //lock prevents read packet from app side
    bool lo = trylock();
    t_vi_exch_dgram dg;
    u8 sdg[VI_HLEN()];
    u32 offs = 0;

    while(timeout > 0){  //acknowledged data; wait timeout ms (in mutitask slightly longer probably)

        if(VI_IO_OK == parser(offs)){

            rdBuf->get(0, sdg, VI_HLEN());   //head copy
            vi_dg_deserialize(&dg, sdg, VI_HLEN());  //fill dg head

            if((VI_ACK & dg.type) && (d->sess_id == dg.sess_id)){   //ack to last packet?

                VI_DMSG("rx ack / no%d", dg.sess_id);
                return VI_IO_OK;
            } else {

                offs += VI_LEN(&dg); //it is not our ack; we shift behind and
            }
        }

        wait10ms();  //waiting
        if((((timeout -= 10) % VI_IO_RESEND_T) == 0) && (timeout > 0)) //every RESEND_T multiple
            resend();  //zajistuje preposilani paketu
    }

    VI_DMSG("(!) rx ack timeout / no%d", dg.sess_id);

    if(lo) unlock();
    return VI_IO_TIMEOUT; //OK
}

vi_ex_io::t_vi_io_r vi_ex_io::receive(t_vi_exch_dgram *d, int timeout){

    if((0 != memcmp(d->marker, mark, VI_MARKER_SZ)) &&
            (false == vi_is_broadcast((p_vi_io_id)d->marker))){  //sanity check

        VI_DMSG("(!) rx dgram not prepared");
        return VI_IO_FAIL;
    }

    lock(); //prevent multiple waiting and data stealing
    t_vi_exch_dgram dg;
    u8 sdg[VI_HLEN()];
    u32 offs = 0;

    while(timeout > 0){  //wait for whole packet

        if(VI_IO_OK == parser(offs)){

            rdBuf->get(offs, sdg, VI_HLEN());   //header only
            vi_dg_deserialize(&dg, sdg, VI_HLEN());  //fill dg head

            if((d->type == VI_ANY) || (d->type == dg.type)){  //ten na ktery cekame

#ifdef VI_LINK_ACK
                if((dg.type >= VI_I) && (0 == (dg.type & VI_ACK))){  //paket s potrvzenim (ale ne potrvzovaci paket)

                    t_vi_exch_dgram ack;
                    preparetx((u8 *)&ack, VI_ACK, 0, dg.sess_id);
                    submit(&ack, 0);
                }
#endif // VI_LINK_ACK

                u32 sz = VI_LEN(&dg); //rx packet overall lenght
                u8 *b = new u8[sz];

                if(0 == offs){
                    
                    rdBuf->read(b, sz);   //read (and shift rd pointer)
                } else {
                    
                    rdBuf->get(offs, b, sz);  //need (to skip some data); copy datagram
                    rdBuf->set(offs, (u8 *)"\x0\x0\x0\x0", 4);   //corrup. dgram head to prevent its repeated read
                }

                int bsz = d->size;  //backup of free space in struct
                if(false == vi_dg_deserialize(d, b, sz)){

                    VI_DMSG("(!) rx / no%d dgram size < pending", dg.sess_id);
                    d->size = bsz;  //size correction if payload isnt complete
                }

                delete[] b;
                unlock();
                return VI_IO_OK;
            }

            offs += VI_LEN(&dg); //this wast our packet; keep waiting for another
        }

        wait10ms();
        timeout -= 10;  //delay
    }

    VI_DMSG("(!) rx timeout");

    unlock();
    return VI_IO_TIMEOUT; //OK
}

