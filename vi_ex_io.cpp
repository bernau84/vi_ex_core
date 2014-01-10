#include "vi_ex_io.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

u32 vi_ex_io::cref;  //static has to be declared extra

#define VI_DMSG(...) \
{ \
    char arg[256]; snprintf(arg, sizeof(arg), __VA_ARGS__); \
    char msg[256]; snprintf(msg, sizeof(msg), "\tref %d: %s\n\r", node_id, arg); \
    debug(msg); \
} \

vi_ex_io::vi_ex_io(int iosize):
  rxQue(queuemem, VI_QUEUE_N){

    sess_id = 0;
    node_id = cref++;
    flags = VI_INITED;

    memset(mark, 0, sizeof(mark)); // == receive everything by default
    for(int i=0; i<VI_QUEUE_N; i++)
      queuemem[i] = VI_QUEUE_INVALID_REC; // == emptiing

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

int vi_ex_io::validate(u32 abs_pos, u32 n, t_vi_exch_dgram *dg)
{
    t_vi_exch_dgram idg;
    if(dg == NULL) dg = &idg;

    preparerx(dg);
    s32 offs = abs_pos - rdBuf->read_mark;
    rdBuf->get(offs, (u8 *)dg->marker, VI_MARKER_SZ); //works with shift not abs position in first param

    if((0 == memcmp(dg->marker, mark, VI_MARKER_SZ) || //unicast
        (vi_is_broadcast(&(dg->marker))) ||  //broadcast
        (vi_is_broadcast((p_vi_io_id)mark)))){   //no filter

        //have something?
        u8 sdg[VI_HLEN()+64]; //head and crc controled payload
        rdBuf->get(offs, sdg, VI_HLEN()); //copy head from rx queue (no read in fact)
        vi_dg_deserialize(dg, sdg, VI_HLEN());  //fill dg head

        u32 rn = VI_LEN(dg);
        if(rn >= rdBuf->size) //size sanity check
            return 1; //some nonsense; escape byte

        if(n >= rn){  //is it whole?

            //read again begin of payload if any
            if(dg->size){

                rn = (rn < (int)sizeof(sdg)) ? rn : sizeof(sdg); //limit n
                rdBuf->get(offs, sdg, rn);
            }

            //crc compatible? - test directly on streamed data
            u32 icrc = vi_ex_crc32(
                            sdg + VI_MARKER_SZ + VI_CRC_SZ, //shft above crc covered data
                            rn - VI_MARKER_SZ - VI_CRC_SZ,
                            VI_CRC_INI);

            if(dg->crc != icrc){

                return VI_LEN(dg);  //error; throw whole datagram
            }

            return 0; //ready; we have packet
        }

        return -1; //wait; incomplete packet
    }

    return 1;  //empty; no valid marker
}


vi_ex_io::t_vi_io_r vi_ex_io::parser()
{
    u8 rd[1024];
    int pn = 0, en = 1, rn = sizeof(rd);
    t_vi_exch_dgram dg;

    while(rn == sizeof(rd))
        if((rn = read(rd, sizeof(rd)))) //piece by piece
            rdBuf->write(rd, rn);  //write to rx circular buffer; shifts write pointer

    u32 n = rdBuf->rdAvail();  //all byte in rx
    if(!n) return VI_IO_FAIL;

    //VI_DMSG("Nod%d Rx avail %dB\n", id, n);

    while((en > 0) && (n >= VI_HLEN())){      //look for whole packet

        if((en = validate(rdBuf->read_mark, n, &dg)) > 0){

            n -= en;
            pn += en;
            rdBuf->read(0, en); //no header, shift
        }
    }

    if(pn) VI_DMSG("(!) rx trashed, %dB", pn);  //info about junk data

    if(en == 0){

        if(VI_QUEUE_INVALID_REC != queuemem[rxQue.write_mark])
            flags |= VI_OVERFLOW;  //packet lost

        rxQue.write(&rdBuf->read_mark, 1); //remember position
        rdBuf->read(0, VI_LEN(&dg));  //shift rd pntr to another pottential dgam
        VI_DMSG("rx 0x%x / no%d / %dB", dg.type, dg.sess_id, dg.size);

#ifdef VI_LINK_ACK
        if((dg.type >= VI_I) && (0 == (dg.type & VI_ACK))){  //packed demand acknowledge

            t_vi_exch_dgram ack;  //performed immediately
            preparetx(&ack, VI_ACK, 0, dg.sess_id);
            submit(&ack, 0);
        }
#endif // VI_LINK_ACK

        return VI_IO_OK;
    }

    if(en == -1)
      return VI_IO_WAITING;
    else
      return VI_IO_SYNTAX;
}

vi_ex_io::t_vi_io_r vi_ex_io::resend()
{
#ifndef VI_LINK_ACK
    return VI_IO_OK;
#endif // VI_LINK_ACK

    int n = wrBuf->rdAvail();

    u8 sdg[VI_HLEN()];
    t_vi_exch_dgram dg; preparerx(&dg);
    rdBuf->get(0, sdg, VI_HLEN());   //copy head from rx queue (no read in fact)
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

    if((0 != memcmp(d->marker, mark, VI_MARKER_SZ)) &&
            (false == vi_is_broadcast((p_vi_io_id)d->marker))){  //bradcast or unicast

        VI_DMSG("(!) rejected 0x%x / no%d", d->type, d->sess_id);
        return VI_IO_FAIL;  //unsuported format
    }

    d->crc = vi_ex_crc32(d); //fill crc
    u32 rn = VI_LEN(d);
    u8 *b = new u8[rn];
    vi_dg_serialize(d, b, rn);
    write((u8 *)b, rn);       //send imediately

    VI_DMSG("tx 0x%x / no%d / %dB", d->type, d->sess_id, d->size);

#ifndef VI_LINK_ACK
   timeout = 0;      //bypass
#endif // VI_LINK_ACK

    if((timeout != 0) && ((d->type & ~VI_ACK) >= VI_I)){ //support for resend

        wrBuf->read_mark = wrBuf->write_mark; //multiple packet resend is not supported
        if(wrBuf->size >= rn)
            wrBuf->write((u8 *)b, rn); //fill resend buffer
        else
            VI_DMSG("(!) too long for resend 0x%x / no%d", d->type, d->sess_id);  //no resend

        if(b) delete[] b;
        t_vi_exch_dgram ack;
        preparerx(&ack, VI_ACK);
        ack.sess_id = d->sess_id;  //we expection ony this id!
        return receive(&ack, timeout);
    }

    if(b) delete[] b;
    return VI_IO_OK;
}

vi_ex_io::t_vi_io_r vi_ex_io::receive(t_vi_exch_dgram *d, int timeout){

    if((0 != memcmp(d->marker, mark, VI_MARKER_SZ)) &&
            (false == vi_is_broadcast((p_vi_io_id)d->marker))){  //sanity check

        VI_DMSG("(!) rx dgram not prepared");
        return VI_IO_FAIL;
    }

    if(flags & VI_READWAIT)
        return VI_IO_WAITING;

    flags |= VI_READWAIT;

    u32 i = VI_QUEUE_INVALID_REC;
    while(timeout >= 0){  //wait for whole packet

        t_vi_exch_dgram dg;
        memcpy(&dg, d, sizeof(t_vi_exch_dgram)); //copy reguested parameters of packet (instead of preparerx)

        //refresh rx and parse new data
        if((i = ispending(&dg)) != VI_QUEUE_INVALID_REC){   //valid possition

          u32 sz = VI_LEN(&dg); //rx packet overall lenght
          u8 *b = new u8[sz];
          rdBuf->get(queuemem[i] - rdBuf->read_mark, b, sz);  //copy datagram
          queuemem[i] = VI_QUEUE_INVALID_REC;  //record freed again

          int bsz = d->size;  //backup of free space in struct
          if(false == vi_dg_deserialize(d, b, sz)){

              VI_DMSG("(!) rx / no%d dgram size < pending", dg.sess_id);
              d->size = bsz;  //size correction if payload isnt complete
          }

          delete[] b;
          flags &= ~VI_READWAIT;
          return VI_IO_OK;
        }

        wait10ms();
        timeout -= 10;  //delay
    }

    flags &= ~VI_READWAIT;
    VI_DMSG("(!) rx timeout");
    return VI_IO_TIMEOUT; //OK

}

