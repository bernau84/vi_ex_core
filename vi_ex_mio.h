#ifndef VIEX_MIO
#define VIEX_MIO

#include "vi_ex_io.h"

/*! global pointers to buffers shared by all nodes
*/
circbuffer<u8> *g_rdbuf;
circbuffer<u8> *g_wrbuf;

/*!
    \class vi_ex_mio
    \brief viex node with shared rx/tx buffer (save memory for multi node bus solution)
*/
class vi_ex_mio : public vi_ex_io
{

protected:
    virtual int read(u8 *d, u32 size){

        if(g_rdbuf) rdBuf->write_mark = g_rdbuf->write_mark;
        return 0; //false return; parser normaly work on buffer directly
    }

    virtual int write(u8 *d, u32 size){

        if(g_wrbuf) wrBuf->write_mark = g_wrbuf->write_mark;
        return 0; //false return; parser normaly work on buffer directly
    }

public:	
	
    vi_ex_iom() : t_vi_io(0) {  //no allocation buffers in ancestor

        if(g_rdbuf) rdBuf = new circbuffer<u8>(*g_rdbuf);  //uses copy constructor
        if(g_wrbuf) wrBuf = new circbuffer<u8>(*g_wrbuf);
    }

    virtual ~vi_ex_iom(){;}

};


#endif //VIEX_MIO
