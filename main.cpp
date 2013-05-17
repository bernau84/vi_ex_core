#include "vi_ex_cell.h"
#include <fstream>
#include <ctime>
#include <iostream>
#include <fstream>

using namespace std;

void vi_test_wait10ms(void);  //prototyp

class vi_ex_fileio : public vi_ex_cell
{
private:
    std::ifstream *ist;
    std::ofstream *ost;

    friend void vi_test_wait10ms(void);

protected:

    virtual void wait10ms(void){

        vi_test_wait10ms();
    }

    virtual void callback(vi_ex_io::t_vi_io_r event){

        vi_ex_cell::callback(event);  //zpracuje vnitrni kontrolni pakety protokolu
        if(vi_ex_io::VI_IO_OK == event){

            char rcv[VIEX_HID_SP];  //pakety aby byly potvrzeny tak se musi vycist
            if(vi_ex_io::VI_IO_OK == receive(rcv, VIEX_HID_SP)){  //rx a prevvod do cloveciny

                char info[128];
                snprintf(info, sizeof(info), "%s: rx cmd \"%s\"\r\n", name, rcv);  //vytisknem
                debug(info);  //vypis do debugovaho okna
            }
        }
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

            ost->write((char *)d, size);
            if(ost->rdstate() & std::ifstream::failbit) return 0;
            ost->flush();
            return size;
        }

        return 0;
    }

public:
    vi_ex_fileio(std::ifstream *_is, std::ofstream *_os,    //kominikacni soubory
                 p_vi_io_mn _name = NULL,   //nazem prvku
                 std::fstream *_term = NULL) :  //terminalovy cloveci vstup/vystup
        vi_ex_cell(_name, _term), ist(_is), ost(_os){;}

    virtual ~vi_ex_fileio(){;}
};




vi_ex_fileio *nod1 = NULL;
vi_ex_fileio *nod2 = NULL;

void vi_test_wait10ms(void){

    clock_t goal;

    if(nod1) nod1->parser();
    goal = 5 + clock();
    while (goal > clock());

    if(nod2) nod2->parser();
    goal = 5 + clock();
    while (goal > clock());
}

/*!
  \todo vyzkouset jak funguje settings
  \todo settings ve spojeni s textovym i/o
  \todo settings vyuzitelny misto INI
*/

int main(int argc, char *argv[])
{
    //jeden soubor je pro jeden nod otevren pro zapis pro druhy pro cteni
    //simulace multiplexni sbernice (+ diky souborum mame archiv syrovych dat komunikace)
    std::ifstream p1in("iotest_wr.bin", std::ifstream::binary);
    std::ofstream p1out("iotest_rw.bin", std::ofstream::binary);

    std::ifstream p2in("iotest_rw.bin", std::ifstream::binary);
    std::ofstream p2out("iotest_wr.bin", std::ofstream::binary);

    //human interface
    std::fstream term("hiterminal.txt", std::fstream::in | std::fstream::out | std::fstream::trunc);

    //p2p komunikace
    nod1 = (vi_ex_fileio *) new vi_ex_fileio(&p1in, &p1out, (p_vi_io_mn)"ND01", &term);
    nod2 = (vi_ex_fileio *) new vi_ex_fileio(&p2in, &p2out, (p_vi_io_mn)"ND02");

    std::string remote("ND02");
    nod1->pair(remote);

   if(term.is_open()){

        term << "ECHO"; nod1->refreshcmdln();  //povel z prikazove radky
        term << "\r\n"; nod1->refreshcmdln();  //povel z prikazove radky, ted
        term.flush();
   }
//    std::string cmd("CAPAB");
//    std::string icapab = nod1->command(cmd);  //primy povel
//    std::cout << icapab;

    return 0;
}
