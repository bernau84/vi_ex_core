
#include <vi_ex_cell.h>


void vi_test_wait10ms(void));  //prototyp

class vi_ex_fileio : public class vi_ex_cell
{
private:	
	ifstream *ist;
	ofstream *ost;

protected:	
    
    virtual void wait10ms(void){

    	vi_test_wait10ms();
    }

	virtual int read(u8 *d, u32 size){
		
		if(ist){ 

			int n = ist->readsome((char *)d, size);
			if(n > 0) return n;
		}

		return 0;
    }

    virtual int write(u8 *d, u32 size){

		if(ost){ 

			int n = ost->write((char *)d, size);
			if(n > 0) return n;
		}

		return 0;
    }

    vi_ex_fileio(ifstream *_is, ofstream *_os, t_vi_io_mn _name = NULL, fstream *_term = NULL) :
    	ist(_is), ost(_os), vi_ex_cell(_name, _term){

    }

    virtual ~vi_ex_fileio();
};


vi_ex_fileio *nod1 = NULL;
vi_ex_fileio *nod2 = NULL;

void vi_test_wait10ms(void){


	if(nod1) nod1->parser(); for(int i=0; i<100000; i++);
	if(nod2) nod2->parser(); for(int i=0; i<100000; i++);
}

int main(){

	//jeden soubor je pro jeden nod otevren pro zapis pro druhy pro cteni
	//simulace multiplexni sbernice (+ diky souborum mame archiv syrovych dat komunikace)
	std::ifstream p1in("iotest_wr.bin", std::ifstream::binary);
	std::ofstream p1out("iotest_rw.bin", std::ofstream::binary);

	std::ifstream p2in("iotest_rw.bin", std::ifstream::binary);
	std::ofstream p2out("iotest_wr.bin", std::ofstream::binary);

	//human interface
	std::fstream term("hiterminal.txt", ios_base::in | ios_base::out);

	//p2p komunikace
	nod1 = (vi_ex_fileio *) new vi_ex_fileio(&p1in, &p1out, (t_vi_io_mn *)"ND01", &term);
	nod2 = (vi_ex_fileio *) new vi_ex_fileio(&p2in, &p2out, (t_vi_io_mn *)"ND02");

	std::map<std::string, std::vector<u8>> isum = nod1.neighbours();
	nod1.pair((t_vi_io_mn *)"ND02");

	term << "ECHO"; nod1->refreshcmdln();  //povel z prikazove radky
	     
	std::string icapab = nod1->command("CAPAB");  //primy povel
}