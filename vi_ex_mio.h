#ifndef VIEX_MIO
#define VIEX_MIO

#include "vi_ex_io.h"

//globalni pointery na globalni bufery ktere mohou sdilet vsechny jednotky
circbuffer<u8> *g_rdbuf;
circbuffer<u8> *g_wrbuf;

//multi io se zdileni prijimaciho buferu
//pokud potrebujem zmensit pametove naroky tak vsechny prijimace mohou pracovat nad jednim prostorem
//stejne se da zaridit i u vysilacu; u obou musime pretizit read a write tak aby sdileli zapisovaci pointery! 
class vi_ex_mio : public class vi_ex_io
{

protected:
	virtual int read(u8 *d, u32 size){

		if(g_rdbuf) rdBuf->write_mark = g_rdbuf->write_mark;
		return 0; //vracime jako ze jsme nic nezapsali, parser ale pracuje nad kruh. bufferem
	}

	virtual int write(u8 *d, u32 size){

		if(g_wrbuf) wrBuf->write_mark = g_wrbuf->write_mark;
		return 0; //vracime jako ze jsme nic nezapsali, parser ale pracuje nad kruh. bufferem
	}

public:	
	
	vi_ex_iom(t_vi_io_mn _name, circbuffer<u8> *_rdbuf, circbuffer<u8> *_wrbuf){ //univerzalni rezim defaultne

	    reading = 0;
	    sess_id = 0;
	    cref++;

	    if(_name == NULL) snprintf(name, sizeof(name), "%04d", cref);  //defaultni nazev je poradove cislo v systemu
	        else strcpy(name, _name); //jinak berem co nam nuti

	    //vyrobime alespon nejakou identifikaci
	    memset(mark, _nam sizeof(mark)); // == prijimame vse

	    if(_rdbuf != NULL) rdBuf = new circbuffer<u8>(*_rdbuf);  //kopirovaci konstruktor; bufer bude sdileny
	        else if((imem = (u8 *)calloc(VI_IO_I_BUF_SZ, 1))) VI_DMSG("input buf alocated");
	            else rdBuf = new circbuffer<u8>(imem, VI_IO_I_BUF_SZ);

	    if(_rdbuf != NULL) wrBuf = new circbuffer<u8>(*_wrbuf);
	        else if((omem = (u8 *)calloc(VI_IO_O_BUF_SZ, 1))) VI_DMSG("output buf alocated");
	            else wrBuf = new circbuffer<u8>(omem, VI_IO_O_BUF_SZ);
	}

	virtual ~vi_ex_iom(){

		//to do - nezabranuje virtual volani destruktoru v predkovi?
	}
}



#endif //VIEX_MIO