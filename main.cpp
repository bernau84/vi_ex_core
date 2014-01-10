#include "vi_ex_ter.h"

#include <fstream>
#include <ctime>
#include <iostream>
#include <fstream>
#include <map>
#include <vector>

using namespace std;

class vi_ex_fileio;

void vi_test_wait10ms(vi_ex_fileio *rs);  //prototyp

class vi_ex_fileio : public vi_ex_ter
{
private:
    std::ifstream *ist;
    std::ofstream *ost;

    friend void vi_test_wait10ms(void);

protected:

    virtual void wait10ms(void){

        vi_test_wait10ms(this);
    }

   virtual void callback_rx_new(t_vi_exch_dgram *rx){

      switch(rx->type){

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

    virtual void callback_read_par_request(const t_vi_param_descr &id, t_vi_param_content &cont){

      cont = setup[id];
    }

    virtual void callback_write_par_request(const t_vi_param_descr &id, t_vi_param_content &cont){

      setup[id] = cont;
    }


public:

    std::map<t_vi_param_descr, t_vi_param_content> setup;

    vi_ex_fileio(std::ifstream *_is, std::ofstream *_os,    //io files simulating physical layer
                 p_vi_io_mn _name = NULL,   //name
                 std::istream *_term_i = NULL) :  //human terminal io
        vi_ex_ter(_name, _term_i), ist(_is), ost(_os){;}

    virtual ~vi_ex_fileio(){;}

    void share(){

      int n;
      for(std::map<t_vi_param_descr, t_vi_param_content>::iterator i = setup.begin();
          i != setup.end();
          i++){

          switch(i->second.type){

              /*! \todo - hrozne slozity; kdyz ale jedna vrstva pocita s polema a druha s
               * vektorem; trochu chyba bylo nahrazovat cap slozitou mapou
               * ale u retezeneho seznamu to take zustat nemohlo (neni fce pro repalace)
               *
               * realne se to ale bude delat jinak - nejspis rucne parametr po parametru
               **/
              case VI_TYPE_INTEGER: {
                  int t[i->second.length];
                  int n = i->second.readrange(0, i->second.length-1, t);
                  std::vector<int> v(&t[0], &t[n]);
                  p_register(i->first, v);
              } break;
              case VI_TYPE_INTEGER64: {
                  long long int t[i->second.length];
                  int n = i->second.readrange(0, i->second.length-1, t);
                  std::vector<long long int> v(&t[0], &t[n]);
                  p_register(i->first, v);
              } break;
              case VI_TYPE_FLOAT: {
                  double t[i->second.length];
                  int n = i->second.readrange(0, i->second.length-1, t);
                  std::vector<double> v(&t[0], &t[n]);
                  p_register(i->first, v);
              } break;
              case VI_TYPE_CHAR: {
                  char t[i->second.length];
                  int n = i->second.readrange(0, i->second.length-1, t);
                  std::vector<char> v(&t[0], &t[n]);
                  p_register(i->first, v);
              } break;
              case VI_TYPE_UNKNOWN:
              case VI_TYPE_BYTE: {
                  unsigned char t[i->second.length];
                  int n = i->second.readrange(0, i->second.length-1, t);
                  std::vector<unsigned char> v(&t[0], &t[n]);
                  p_register(i->first, v);
              } break;
           }
        }
     }
};

vi_ex_fileio *nod1 = NULL;
vi_ex_fileio *nod2 = NULL;

void vi_test_wait10ms(vi_ex_fileio *rs){

    if((nod2) && (rs == nod1))
       nod2->process();

    if((nod1) && (rs == nod2))
       nod1->process();

    clock_t goal;
    goal = 10 + clock();
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

    u8 *bt = NULL;
    for(u8 i=0; i<100; i++){

        if(bt == NULL) bt = (u8 *)new double[i];
        memset(bt, i, sizeof(double)*i);
        if(bt) delete[] bt;
        bt = NULL;
      }

    //onefile is opened as read second as write for 1. node
    //opposite manner for 2. node
    //we have full binary log of communication in log
    std::ifstream p1in(".\\iotest_wr.bin", std::ifstream::binary);
    std::ofstream p1out(".\\iotest_rw.bin", std::ofstream::binary);

    std::ifstream p2in(".\\iotest_rw.bin", std::ifstream::binary);
    std::ofstream p2out(".\\iotest_wr.bin", std::ofstream::binary);

    //human interface
    std::ofstream term_o(".\\hiterminal.txt", std::fstream::binary | std::fstream::trunc);
    std::ifstream term_i(".\\hiterminal.txt", std::fstream::binary);
    if(!term_i.is_open() || !term_o.is_open())  //terminal io test
        return 0;

    //p2p communication
    nod1 = (vi_ex_fileio *) new vi_ex_fileio(&p1in, &p1out, (p_vi_io_mn)"ND01", &term_i);
    nod2 = (vi_ex_fileio *) new vi_ex_fileio(&p2in, &p2out, (p_vi_io_mn)"ND02");

    //------------------------connection test----------------------------
    nod1->pair("ND02");

    //------------------------terminal io test---------------------------
    term_o << "ECHO\r\n";
    term_o << "CAP result/def(int)=70000\r\n";
    term_o.flush();
    nod1->refreshcmdln();

//    term_o.flush();
//    term_o << "ECHO\r\n";
//    term_o << "CAP result/def(int)=70000\r\n";
//    term_o.flush();
//    nod1->refreshcmdln();  //not worked! bytes are flushed but not synced with input stream

    //------------------------capabilities exchange io test--------------
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
    nod1->setup[t_vi_param_descr("result", VI_TYPE_P_DEF)].append((int)70000);
    nod1->setup[t_vi_param_descr("result", VI_TYPE_P_MIN)].append((int)-100000);
    nod1->setup[t_vi_param_descr("result", VI_TYPE_P_MAX)].append((int)111111);
    nod1->setup[t_vi_param_descr("fish", VI_TYPE_P_DEF)].append((char *)"--select--", 11);
    nod1->setup[t_vi_param_descr("fish", (t_vi_param_flags)1)].append((char *)"trout", 6);
    nod1->setup[t_vi_param_descr("fish", (t_vi_param_flags)2)].append((char *)"eal", 4);
    nod1->setup[t_vi_param_descr("fish", (t_vi_param_flags)3)].append((char *)"haddock", 8);
    nod1->setup[t_vi_param_descr("fish", (t_vi_param_flags)4)].append((char *)"salmon", 7);
    nod1->setup[t_vi_param_descr("fish", (t_vi_param_flags)5)].append((char *)"catfish", 8);
    nod1->share();

//  //syntax name/'def|menu|min|max'('byte|int|char|float|bool|long')=1,2,3,4,5,6,......
//    std::list<string> c_init_nod2;
//    c_init_nod2.push_back("CAP probability/def(float)=0.0\n");
//    c_init_nod2.push_back("CAP probability/min(float)=0.0\n");
//    c_init_nod2.push_back("CAP probability/max(float)=0.999999\n");
//    c_init_nod2.push_back("CAP logmenu/def(byte)=255\n");
//    c_init_nod2.push_back("CAP logmenu/menu1(byte)=0\n");
//    c_init_nod2.push_back("CAP logmenu/menu2(byte)=25\n");
//    c_init_nod2.push_back("CAP logmenu/menu3(byte)=50\n");
//    c_init_nod2.push_back("CAP logmenu/menu4(byte)=100\n");
//    c_init_nod2.push_back("CAP logmenu/menu5(byte)=200\n");
//    c_init_nod2.push_back("CAP primes/def(byte)=1,3,5,7,11,13,17,19,23,29,31,37,41\n");
//    for(std::list<string>::iterator cs = c_init_nod2.begin(); cs != c_init_nod2.end(); cs++){
//        /*! \todo - test return values */
//        nod2->submit(*cs);
//      }
    nod2->setup[t_vi_param_descr("probability", VI_TYPE_P_DEF)].append((double)0.0);
    nod2->setup[t_vi_param_descr("probability", VI_TYPE_P_MIN)].append((double)0.1);
    nod2->setup[t_vi_param_descr("probability", VI_TYPE_P_MAX)].append((double)0.99999999);
    nod2->setup[t_vi_param_descr("logmenu", VI_TYPE_P_DEF)].append((unsigned char)255);
    nod2->setup[t_vi_param_descr("logmenu", (t_vi_param_flags)1)].append((unsigned char)0);
    nod2->setup[t_vi_param_descr("logmenu", (t_vi_param_flags)2)].append((unsigned char)25);
    nod2->setup[t_vi_param_descr("logmenu", (t_vi_param_flags)3)].append((unsigned char)50);
    nod2->setup[t_vi_param_descr("logmenu", (t_vi_param_flags)4)].append((unsigned char)100);
    nod2->setup[t_vi_param_descr("logmenu", (t_vi_param_flags)5)].append((unsigned char)200);

    const unsigned char primes[] = {1,3,5,7,11,13,17,19,23,29,31,37,41};
    nod2->setup[t_vi_param_descr("primes", VI_TYPE_P_DEF)].append((unsigned char *)primes, sizeof(primes));
    nod2->share();

    //------------------------parametr io test------------------------
    nod2->command("GETP result");
    nod2->command("GETP result/min");
    nod2->command("GETP result/max");
    nod2->command("GETP result/def");
    nod2->command("GETP result(string)");
    nod2->command("SETP result(double)=-50000");
    nod2->command("SETP result(int)=-50000");
    nod2->command("SETP result=-500000");
    nod2->command("SETP fish(string)=salmon");
    nod2->command("SETP fish(string)=carp");
    nod2->command("GETP fish");

    nod1->command("SETP probability(float)=0.5");
    nod1->command("SETP probability(float)=-0.5");
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
