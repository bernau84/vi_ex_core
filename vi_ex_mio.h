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
	
	vi_ex_iom(t_vi_io_mn _name) : t_vi_io(_name, 0) {  //inicializace predka bez alokace bufferu

	    if(g_rdbuf) rdBuf = new circbuffer<u8>(*g_rdbuf);  //kopirovaci konstruktory se pouziji
	    if(g_wrbuf) wrBuf = new circbuffer<u8>(*g_wrbuf);	    
	}

	virtual ~vi_ex_iom(){;}   //to do - nezabranuje virtual volani destruktoru v predkovi

};


#endif //VIEX_MIO