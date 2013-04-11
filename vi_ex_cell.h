#ifndef VI_EX_CELL_H
#define VI_EX_CELL_H

#include "vi_ex_hid.h"
#include "vi_ex_par.h"

#include <sstream>
#include <string>
#include <map>
#include <set>
#include <list>
#include <vector>
#include <algorithm>
#include <new>          // std::bad_alloc
#include <iostream>  //cout

//prijmnejsi sprava capabilities
//a zakladnich fukci spolecnych vsem io
//podpora high level get, set configu
//human readable text terminal
//pripojovani a mapovani prvku na sbernici
class vi_ex_cell : public vi_ex_hid
{
private:
    t_vi_param *p_cap;  //zaloha binarniho paketu s capab
    int sz_cap;

    std::iostream *p_hi;  //terminal kde ctem prikazy a kam je davame
    std::ostream *p_trace;  //tracovaci vystup
    std::string cmdln;  //cache prikazove radky
    std::map<std::string, std::vector<u8> > neighbours;

    //low-level hodnota parametru vyctena jako pole
    template <typename T> std::vector<T> params(std::string &name, t_vi_param_flags f){

        std::vector<T> v;
        if(!p_cap || !sz_cap) return v;

        char lname[VIEX_PARAM_NAME_SIZE];
        name.copy(lname, VIEX_PARAM_NAME_SIZE);

        t_vi_param_stream reader((u8 *)p_cap, sz_cap);
        if(reader.setpos(&lname, f)){

            int n; reader.isvalid(&n); //vime ze je v poradku - jen chceme velikost
            T *tv = new T[n];
            n = reader.readnext<T>(&lname, tv, n);
            for(int i=0; i<n; i++) v.push_back(tv[i]);
            delete[] tv;
        }
        return v;
    }

    //low-level hodnota parametru vyctena jako pole
    template <typename T> T param(std::string &name, t_vi_param_flags f){

        std::vector<T> v = params<T>(name, f);
        return (v.size()) ? v[0] : T();
    }

    //zmena hodnoty parametru a vycrteni (nebo jen vycteni pokud nic neposilame)
    //zalezi na action - 0 - cteni (request pouze), 1 - cteni (s cekanim na odpoved), 2 - zapis (requst pouze), 3 - zapis a cteni
    template <typename T> std::vector<T> setup(std::string &name, std::vector<T> &v, u8 action){

        std::vector<T> confr;
        std::vector<T> d = params<T>(name, VI_TYPE_P_DEF); //vyctem defaultni prametr kvuli velikosti
        int N = (v.size() > d.size()) ? v.size() : d.size();  //berem vetsi
        t_vi_exch_type t = (action > 1) ? VI_I_SETUP_GET : VI_I_SETUP_SET;

        char lname[VIEX_PARAM_NAME_SIZE]; 
        name.copy(lname, VIEX_PARAM_NAME_SIZE);

        T confv[N]; for(int i = 0; i != v.size(); i++) confv[i] = v[i];  //z vekotru c-pole
        t_vi_exch_dgram *confs = preparetx(NULL, t, VIEX_PARAM_LEN(T, v.size()));  //paket si pripravime
        t_vi_param_stream writer(confs->d, confs->size); //priparavime si zapisovac
        writer.append<T>(&lname, confv, v.size());  //a do paketu vrazime tam to pole

        if((vi_ex_io::submit(confs) == vi_ex_io::VI_IO_OK) && (0 == (action & 0x1))){
            //cekame na odpoved s nastavenim
            //pripravime si ten samy paket, neprijmem ovsem vic nez jsme poslali!!!
            preparerx(confs, t, VIEX_PARAM_LEN(T, v.size()));
            if(vi_ex_io::receive(confs) != vi_ex_io::VI_IO_OK){

                t_vi_param_stream reader(confs->d, confs->size); //priparavime si vycitac parametru
                N = reader.readnext<T>(&lname, confv, N);  //mame neco?
                for(int i=0; i<N; i++) confr.push_back(confv[i]);  //preklopime
            }
        }
        
        delete[] confs;  //uvolnuje pole bytu
        return confr; //vratime
    }    

protected:

    //tracy doplnene o timestamp (strojovy cas v sec)
    virtual void debug(const char *msg);

    //potrebuji udelat hook na capab., discovery paket, registraci, apod.
    virtual void callback(vi_ex_io::t_vi_io_r event);

public:

    template <typename T> std::vector<T> def(std::string &name){ //defaultni hodnota parametru pokud ocekavame pole

        return params<T>(name, VI_TYPE_P_DEF);
    }

    //vraci 3 hodnoty v poradi nazvu fce pokud je polozka tohoto typu jinak 0
    template <typename T> std::vector<T> minmaxstep(std::string &name){

        std::vector<T> v(3);
        v[0] = param<T>(name, VI_TYPE_P_MIN);
        v[1] = param<T>(name, VI_TYPE_P_MAX);
        v[2] = param<T>(name, VI_TYPE_P_RANGE2) - v[0];
        return v;
    }

    //vraci seznam hodnot menu pokud je polozka vyctova
    template <typename T> std::vector<T> menudef(std::string &name){

        int f = (int)VI_TYPE_P_MIN;
        std::vector<T> tv(1), v;

        while((tv = param<T>(name, (t_vi_param_flags)f++)).size())  //pokud stale neco vycitame
            v.push_back(*(tv.begin())); //berem prvni; vic by jich stejne byt nemelo

        return v;
    }

    //vraci seznam stringu parametru
    std::vector<std::string> stringdef(std::string &name){

        int f = (int)VI_TYPE_P_MIN;
        std::vector<std::string> v;
        std::vector<char> tv;
        std::string s;

        while((tv = params<char>(name, (t_vi_param_flags)f++)).size()){//pokud stale neco vycitame

            for(std::vector<char>::iterator c = tv.begin(); c != tv.end(); c++) s += *c;
            v.push_back(s);
        }

        return v;
    }

    //zmena hodnoty parametru a vycteni (nebo jen vycteni pokud nic neposilame)
    template <typename T> std::vector<T> set(std::string &name, std::vector<T> &v){

        return setup<T>(name, v, 3);
    }

    //cteni konfigurace
    template <typename T> std::vector<T> get(std::string &name){

        std::vector<T> v(1); //nulovy
        return setup<T>(name, v, 1);
    }

    std::string command(std::string &ord){

        char rcv[VIEX_HID_SP];
        if(vi_ex_hid::submit(ord.c_str()) == vi_ex_io::VI_IO_OK)  //prelozime a poslem binarne
            if(vi_ex_hid::receive(rcv) == vi_ex_io::VI_IO_OK) //pockame si na odpoved a tu hned prelozime do cloveciny
                return std::string(rcv); //vratime

        return std::string();  //jinka nic
    }

    //koukne jestli je zadan nejaky textovy povel (odradkovany)
    //pokud ano predhodiho 
    void refreshcmdln(void){

//        char rcv[VIEX_HID_SP];
//        if(p_hi) if(p_hi->readsome(rcv, VIEX_HID_SP)) cmdln << rcv;  //sup do cache pokud tam neco ceka
//        if(cmdln.find('\n') != std::string::npos){    //je tam uz cely komand?

//            std::string cmd;
//            std::getline(cmdln, cmd);  //vyctem prikaz
//            cmdln = cmdln.substring(cmd.length()+1); //cache zkratime

//            *p_hi << "<< \"" << cmd << "\"";
//            *p_hi << ">> \"" << command(cmd) << "\""; //vykoname a vratime vysledek
//        }
    }    

    std::map<std::string, std::vector<u8> > neighbourslist(void){

        if(neighbours.empty()){
            //udelame echo a zjistime jestli kdo uz posloucha
            t_vi_exch_dgram echo; preparetx(&echo, VI_ECHO_REQ);  //paket si pripravime
            memset(echo.marker, 0, sizeof(mark)); //udelame z toho broadcast nebo unicast podle act_mark
            vi_ex_io::submit(&echo);
        }

        for(int i=0; i<50; i++) //05s
            wait10ms();

        return neighbours;
    }

    //spojeni s druhym koncem p2p; stavovy automat na 10s max
    //fce je blokujici; ceka na ozvani se vzdaleneho nodu na broadcast echo
    //pak se registruje, pak se ceka na reakci na prime echo
    bool pair(std::string &remote);

    //pokud je io typu hid pak muzem vypisovat tracy na terminal
    vi_ex_cell(t_vi_io_mn _name, std::iostream *_p_hi = NULL, std::ostream *_p_trace = &std::cout):
        vi_ex_hid(_name), p_hi(_p_hi), p_trace(_p_trace){ 
    
        p_cap = NULL;
        sz_cap = 0;
    }

    virtual ~vi_ex_cell(void){

        if(p_cap) delete[] p_cap;
        p_cap = NULL;
    }
};

#endif // VI_EX_CELL_H
