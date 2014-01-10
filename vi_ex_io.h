#ifndef VI_EX_IO_H
#define VI_EX_IO_H

#include "vi_ex_def.h"
#include "vi_ex_par.h"
#include "include/circbuffer.h"

/*!
    some viex settings
 */
#define VI_QUEUE_N        10             /*! projected size of rx cache */
#define VI_QUEUE_INVALID_REC  ((u32)-1)  /*! empty record in queue */

#define VI_IO_DGRAM_SZ    1400    /*! maximal io viex datagram size */
#define VI_IO_I_BUF_SZ    ((u32)(VI_QUEUE_N * VI_IO_DGRAM_SZ)) /*! default receiving buffer size (for X buffers) */
#define VI_IO_O_BUF_SZ    ((u32)(1          * VI_IO_DGRAM_SZ)) /*! default retransmith buffer size (for 1 buffer) */
#define VI_IO_RESEND_T    (1000)        /*! resend timeout */
#define VI_IO_WAITMS_T    (3*VI_IO_RESEND_T) /*! overal timeout for recieve or acknowledge */
/*!
    \def VI_LINK_ACK
    \brief conditional compilation - turn on acknowledging

    packet is acked while is recived by application layer
 */
#define VI_LINK_ACK


/*!
    \class vi_ex_io
    \brief parser and dispatcher of viex packets

    supperting resend and acknowledging
    platform independend abstract class
 */
class vi_ex_io
{
public:

    /*!
        \enum vi_ex_io::t_vi_io_r
        \brief vi_ex_io return vale
     */
    enum t_vi_io_r {

        VI_IO_OK = 0,     /*!< general ok; yes we have paket */
        VI_IO_WAITING,    /*!< packet on the way */
        VI_IO_TIMEOUT,    /*!< packet receive / ack. timeout */
        VI_IO_SYNTAX,     /*!< corrupted paket (length, crc, unknown, etc..) */
        VI_IO_FAIL        /*!< general error; no paket */
    };

    enum t_vi_io_f {

        VI_INITED = (1 << 0),       /*!< instanced */
        VI_PAIRED = (1 << 1),       /*!< uniqu marger negotiated */
        VI_READWAIT = (1 << 2),     /*!< async receive postponed until previous reading ends */
        VI_OVERFLOW = (1 << 3)      /*!< inprocessed dgram overwritten */
    };


private:
    static u32 cref;  /*!< reference counter */

    u32 sess_id;  /*!< incremental packet id */
    u32 node_id;  /*!< number of node (for debug) */
    u32 flags;    /*!< indicators t_vi_io_f bitfield*/

    u8 *imem;   /*!< internal memory for receiving */
    u8 *omem;   /*!< internal memory for retransmit */

    u32 queuemem[VI_QUEUE_N]; /*!< internal memory for rx packet indexes */

    int       validate(u32 pos, u32 n = (u32)-1, t_vi_exch_dgram *dg = NULL); /*!< test validity of packet inside rx buffer */
    t_vi_io_r parser();  /*!< single pass parser */
    t_vi_io_r resend();  /*!< resend last packet if resend time elapsed an no ack received */

protected:
    t_vi_io_id mark;  /*!< see t_vi_exch_dgram::mark */

    circbuffer<u8> *rdBuf;  /*!< incomming circular buffer */
    circbuffer<u8> *wrBuf;  /*!< outgoing circular buffer */
    circbuffer<u32> rxQue;  /*!< queue of incomming packet */

    virtual int read(u8 *d, u32 size) = 0;   /*!< pure virtual read from interface */
    virtual int write(u8 *d, u32 size) = 0;  /*!< pure virtual write to interface */

    virtual void wait10ms(void){ for(int t=10000; t; t--); } /*!< default waitstate */
    virtual void debug(const char *msg){ fprintf(stderr, "%s", msg); }  /*!< default debug interface */
    //virtual void callback(t_vi_io_r ){;}     /*!< callback function; call with every packet rx */

public:
    /*!<
        \brief test is some new packet is waiting in receive queue
        in asynchronous mode it should be call regularly
        @return pasition; VI_QUEUE_INVALID_REC if not pending
    */
    u32 ispending(t_vi_exch_dgram *d = NULL){

        //active check rx queue; in asynchronous mode it should be call regularly
        parser();

        if(d == NULL){

            t_vi_exch_dgram idg;
            preparerx(&idg);  //any packet, any size
            d = &idg;
        }

        rxQue.overflow = 0;  //trough all buffer
        rxQue.read_mark = rxQue.write_mark;   //from oldest records

        for(int i = rxQue.read_mark; rxQue.rdAvail() > 0; i = rxQue.read_mark){

            u32 pos = VI_QUEUE_INVALID_REC; rxQue.read(&pos, 1);  //read index + shord rd pointer
            if(pos == VI_QUEUE_INVALID_REC)   //valid position?
              continue;

            t_vi_exch_dgram dg; preparerx(&dg);
            if(0 == validate(pos, rdBuf->size, &dg)){  //packet still ready? (we dont know size, so MAX is assumed)

                if(d->sess_id)  //we want only this concrete packet
                  if(d->sess_id != dg.sess_id)
                    continue;

                if((d->type == VI_ANY) || (d->type == dg.type)){  //ten na ktery cekame

                  memcpy(d, &dg, sizeof(t_vi_exch_dgram));
                  return i;
                }
            }
        }

        return VI_QUEUE_INVALID_REC;
    }

    /*!<
        \brief set new marker (== connect to new nod)

        for bus usage
    */
    void destination(p_vi_io_id p2pid){

        memcpy(mark, p2pid, sizeof(mark));
        flags |= VI_PAIRED;
    }

    /*!<
        \brief prepare rx stucture

        @param d[in] - poiter to already allocated memory (if NULL packet will be allocated)
        @param t[in] - see t_vi_exch_type
        @param size[in] - size of payload to be allocated
        @param _sess_id[in] - automated usualy
        @return packet memory or NULL if allocation failed
    */
    t_vi_exch_dgram *preparetx(void *d, t_vi_exch_type t, u32 size = 0, u32 _sess_id = 0){
                
        if(NULL == d) //!!!if d == NULL than heap is alocated, app has to free it afterwards
            if(NULL == (d = new u8[sizeof(t_vi_exch_dgram) + size])){

              debug("(!!!)E bad_alloc caught: preparetx()\n");
              return NULL;
          }

        t_vi_exch_dgram *dg = (t_vi_exch_dgram *)d;
        memcpy(dg->marker, mark, VI_MARKER_SZ);
        dg->type = t;
        dg->sess_id = (_sess_id) ? _sess_id : /*(t >= VI_I) ? sess_id :*/ ++sess_id;
        dg->crc = 0;
        dg->size = size;
        return dg;
    }       

    /*!<
        \brief prepare rx stucture

        @param d[in] - poiter to already allocated memory (if NULL packet will be allocated)
        @param t[in] - see t_vi_exch_type
        @param size[in] - size of payload to be allocated
    */
    t_vi_exch_dgram *preparerx(void *d, t_vi_exch_type t = VI_ANY, u32 size = 0){

        if(NULL == d) //!!!if d == NULL than heap is alocated, app has to free it afterwards by delete[]
            if(NULL == (d = new u8[sizeof(t_vi_exch_dgram) + size])){

                debug("(!!!)E bad_alloc caught: preparerx()\n");
                return NULL;
            }

        t_vi_exch_dgram *dg = (t_vi_exch_dgram *)d;
        memcpy(dg->marker, mark, VI_MARKER_SZ);
        dg->type = t;
        dg->sess_id = 0;
        dg->crc = 0;
        dg->size = size;
        return dg;
    }

    /*!<
        \brief transmit packet

        ...and wait for acknowledge until timeout elapse
        d must be contructed by preparetx() method
    */
    t_vi_io_r submit(t_vi_exch_dgram *d, int timeout = VI_IO_WAITMS_T);

    /*!<
        \brief wait for d->type new packet

        ...until timeout elapse
        d must be contructed by preparerx() method
        method will not read more thad d->size bytes of payload
        @see ispending()
    */
    t_vi_io_r receive(t_vi_exch_dgram *d, int timeout = VI_IO_WAITMS_T);

    vi_ex_io(int iosize = VI_IO_I_BUF_SZ); /*!< default name is created from reference counter */
    virtual ~vi_ex_io();
};


#endif // VI_EX_IO_H
