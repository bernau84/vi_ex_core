#include "vi_ex_ter.h"

#include <fstream>
#include <ctime>
#include <iostream>
#include <fstream>
#include <map>
#include <vector>

using namespace std;

void vi_test_wait10ms(void);  //prototyp

class vi_ex_fileio : public vi_ex_ter
{
private:
    std::ifstream *ist;
    std::ofstream *ost;

    friend void vi_test_wait10ms(void);
    std::map<std::string, std::vector<u8> > setup;

protected:

    virtual void wait10ms(void){

        vi_test_wait10ms();
    }

    virtual void callback_rx_new(t_vi_exch_dgram *rx){

      int tn;
      string tnm;

      switch(rx->type){

        case VI_I_SET_PAR:

          tnm = string(((t_vi_param *)&rx->d[0])->name);
          setup[tnm] = std::vector<u8>(&rx->d[0], &rx->d[rx->size]); //update
        break;
        case VI_I_GET_PAR:{

          tnm = string(((t_vi_param *)&rx->d[0])->name);
          tn = setup[tnm].size();
          u8 td[tn + VI_HLEN()];
          t_vi_exch_dgram *tx = preparetx(td, VI_I_RET_PAR, tn);
          for(int i=0; i<tn; i++) tx->d[i] = setup[tnm][i];
          vi_ex_io::submit(tx);  //return
        }
        break;

        default:
        break;
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
    vi_ex_fileio(std::ifstream *_is, std::ofstream *_os,    //io files simulating physical layer
                 p_vi_io_mn _name = NULL,   //name
                 std::istream *_term_i = NULL) :  //human terminal io
        vi_ex_ter(_name, _term_i), ist(_is), ost(_os){;}

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

//    std::map<t_vi_param_descr, t_vi_param_content> cap;
//    t_vi_param_descr k("propX", (t_vi_param_flags)0);
//    t_vi_param_content d(VI_TYPE_INTEGER, 0);
//    for(int i=0; i<100; i++){

//        k.def_range = (t_vi_param_flags)i;
//        k.name[4] = 'a' + i;
//        d.length = i;
//        d.v.push_back(i);
//        cap[k] = d;
//      }

    //onefile is opened as read second as write for 1. node
    //opposite manner for 2. node
    //we have full binary log of communication in log
    std::ifstream p1in("iotest_wr.bin", std::ifstream::binary);
    std::ofstream p1out("iotest_rw.bin", std::ofstream::binary);

    std::ifstream p2in("iotest_rw.bin", std::ifstream::binary);
    std::ofstream p2out("iotest_wr.bin", std::ofstream::binary);

    //human interface
    std::ofstream term_o("hiterminal.txt", std::fstream::binary | std::fstream::trunc);
    std::ifstream term_i("hiterminal.txt", std::fstream::binary);
    if(!term_i.is_open() || !term_o.is_open())  //terminal io test
        return 0;

    //p2p communication
    nod1 = (vi_ex_fileio *) new vi_ex_fileio(&p1in, &p1out, (p_vi_io_mn)"ND01", &term_i);
    nod2 = (vi_ex_fileio *) new vi_ex_fileio(&p2in, &p2out, (p_vi_io_mn)"ND02");

    nod1->pair("ND02");

    //capabilities exchange io test
    //syntax name/'def|menu|min|max'('byte|int|char|float|bool|long')=1,2,3,4,5,6,......
    std::list<string> c_init_nod1;
    std::list<string> c_init_nod2;

//    //terminal io test
//    term_o << "ECHO\r\n";
//    term_o << "CAP result/def(int)=70000\r\n";
//    term_o << "CAP result/min(int)=-100000\r\n";
//    term_o << "CAP result/max(int)=100000\r\n";

//    term_o << "CAP fish/def(string)=--select--\n";
//    term_o << "CAP fish/menu1(string)=trout\n";
//    term_o << "CAP fish/menu2(string)=eal\n";
//    term_o << "CAP fish/menu3(string)=haddock\n";
//    term_o << "CAP fish/menu4(string)=salmon\n";
//    term_o << "CAP fish/menu5(string)=catfish\n";

//    term_o.flush();
//    nod1->refreshcmdln();

    c_init_nod2.push_back("CAP probability/def(float)=0.0\n");
    c_init_nod2.push_back("CAP probability/min(float)=0.0\n");
    c_init_nod2.push_back("CAP probability/max(float)=0.999999\n");

    c_init_nod2.push_back("CAP logmenu/def(byte)=255\n");
    c_init_nod2.push_back("CAP logmenu/menu1(byte)=0\n");
    c_init_nod2.push_back("CAP logmenu/menu2(byte)=25\n");
    c_init_nod2.push_back("CAP logmenu/menu3(byte)=50\n");
    c_init_nod2.push_back("CAP logmenu/menu4(byte)=100\n");
    c_init_nod2.push_back("CAP logmenu/menu5(byte)=200\n");

    c_init_nod2.push_back("CAP primes/def(byte)=1,3,5,7,11,13,17,19,23,29,31,37,41\n");

    for(std::list<string>::iterator cs = c_init_nod1.begin(); cs != c_init_nod1.end(); cs++){
        /*! \todo - test return values */
        nod1->command(*cs);
      }

    for(std::list<string>::iterator cs = c_init_nod2.begin(); cs != c_init_nod2.end(); cs++){
        /*! \todo - test return values */
        nod2->command(*cs);
      }

    //parametr io test
    nod2->command("GETP result");
    nod2->command("GETP result/min");
    nod2->command("GETP result/max");
    nod2->command("GETP result/def");
    nod2->command("GETP result(string)");
    nod2->command("SETP result(double)=-50000");
    nod2->command("SETP result(int)=-50000");
    nod2->command("SETP result=-500000");
    nod2->command("SETP fish=salmon");
    nod2->command("SETP fish=carp");
    nod2->command("GETP fish");

    nod1->command("SETP probability=0.5");
    nod1->command("SETP probability=-0.5");
    nod1->command("SETP logmenu=50");
    nod1->command("SETP logmenu=1000");
    nod1->command("SETP primes=1001,1003,1009");
    nod1->command("SETP unknown=1.1");
    nod1->command("SETP free(string)=one-dot-two");
    nod1->command("GETP free(string)");

    {
    std::vector<double> range;
    range = nod1->min<double>("probability");
    range = nod1->max<double>("probability");
    range = nod1->step<double>("probability");
    }

    {
    std::vector<u8> range;
    range = nod1->menudef<u8>("logmenu");
    }

    {
    std::vector<int> range;
    range = nod2->min<int>("result");
    range = nod2->max<int>("result");
    range = nod2->step<int>("result");
    range = nod2->def<int>("result");
    }

    {
    std::vector<string> range;
    range = nod2->stringdef("fish");
    }

    p1in.close(); p1out.close();
    p2in.close(); p2out.close();
    term_o.close(); term_i.close();

   return 0;
}
