#ifndef VI_EX_IO_H
#define VI_EX_IO_H

#include "vi_ex_def.h"
#include "vi_ex_par.h"
#include "include/circbuffer.h"

/*!
    some viex settings
 */
#define VI_IO_I_BUF_SZ    ((u32)(2000)) /*! default receiving buffer size */
#define VI_IO_O_BUF_SZ    ((u32)(2000)) /*! default retransmith buffer size */
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

private:
    static u32 cref;  /*!< reference counter */

    bool reading; /*!< rx queue reading lock */
    u32 sess_id;  /*!< incremental packet id */
    u32 node_id;  /*!< number of node (for debug) */

    u8 *imem;   /*!< internal memory for receiving */
    u8 *omem;   /*!< internal memory for retransmit */

protected:
    t_vi_io_id mark;  /*!< see t_vi_exch_dgram::mark */

    t_vi_io_r parser(u32 offs = 0);  /*!< single pass parser */
    t_vi_io_r resend();  /*!< resend last packet if resend time elapsed an no ack received */

    circbuffer<u8> *rdBuf;  /*!< incomming circular buffer */
    circbuffer<u8> *wrBuf;  /*!< outgoing circular buffer */

    virtual int read(u8 *d, u32 size) = 0;   /*!< pure virtual read from interface */
    virtual int write(u8 *d, u32 size) = 0;  /*!< pure virtual write to interface */

    virtual void wait10ms(void){ for(int t=10000; t; t--); } /*!< default waitstate */
    virtual void debug(const char *msg){ fprintf(stderr, "%s", msg); }  /*!< default debug interface */
    virtual void callback(t_vi_io_r event){;}     /*!< callback function; call with every packet rx */
    
    virtual void lock(){ while(reading)/* wait10ms()*/; reading = true; }
    virtual bool trylock(){ if(reading) return false; reading = true; return true; }
    virtual void unlock(){ reading = false; }
    virtual bool islocked(){ if(reading) return true; else return false; }

public:
    /*!<
        \brief test is some new packet is waiting in receive queue
        @return packet size
    */
    u32 ispending(void){

        //active check rx queue; in asynchronous mode it should be call regularly
        if(VI_IO_OK != parser())
            return 0; 

        u8 sdg[VI_HLEN()];
        rdBuf->get(0, sdg, VI_HLEN());   //copy head from rx queue (no read in fact)

        t_vi_exch_dgram dg;
        vi_dg_deserialize(&dg, sdg, VI_HLEN());
        return dg.size;
    }

    /*!<
        \brief set new marker (== connect to new nod)

        for bus usage
    */
    void destination(p_vi_io_id p2pid){

        memcpy(mark, p2pid, sizeof(mark));
    }

    /*!<
        \brief prepare rx stucture

        @param d[in] - poiter to already allocated memory (if NULL packet will be allocated)
        @param t[in] - see t_vi_exch_type
        @param size[in] - size of payload to be allocated
        @param _sess_id[in] - automated usualy
        @return packet memory or NULL if allocation failed
    */
    t_vi_exch_dgram *preparetx(u8 *d, t_vi_exch_type t, u32 size = 0, u32 _sess_id = 0){
                
        if(NULL == d) //!!!if d == NULL than heap is alocated, app has to free it afterwards
            if(NULL == (d = new u8[sizeof(t_vi_exch_dgram) + size]))
                return NULL;

        t_vi_exch_dgram *dg = (t_vi_exch_dgram *)d;
        memcpy(dg->marker, mark, VI_MARKER_SZ);
        dg->type = t;
        dg->sess_id = (_sess_id) ? _sess_id : (t >= VI_I) ? sess_id : ++sess_id;
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
    t_vi_exch_dgram *preparerx(u8 *d, t_vi_exch_type t =  VI_ANY, u32 size = 0){

        if(NULL == d) //!!!if d == NULL than heap is alocated, app has to free it afterwards
            if(NULL == (d = new u8[sizeof(t_vi_exch_dgram) + size]))
                return NULL;

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
