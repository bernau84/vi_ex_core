#ifndef VI_EX_CELL_H
#define VI_EX_CELL_H

#include <vi_ex_hid.h>
#include <vi_ex_par.h>

#include <sstream>
#include <string>
#include <map>
#include <list>
#include <array>

//prijmnejsi sprava capabilities
//a zakladnich fukci spolecnych vsem io
class vi_ex_cell : public vi_ex_hid
{
private:
    t_vi_param *p_cap;  //zaloha binarniho paketu s capab
    std::ostream *tt_o;  //terminal kam padaji tracy

    //low-level hodnota parametru vyctena jako pole
    template <typedef T> std::list<T> param(std::string name, t_vi_param_flags f){

        std::list<T> v;
        if(!p_cap) return v;

        t_vi_param_stream reader(p_cap);
        if(reader.setpos(name, f)){

            int n; _tp = reader.isvalid(&n); //vime ze je v poradku - jen chceme velikost
            T *tv = new T[n];
            n = reader.readnext<T>(name, &tv, n);
            for(int i=0; i<n; i++) v.insert(tv[i]);
            delete tv;
            if(tp) tp = _tp;
        }
        return v;
    }

    //low-level hodnota parametru vyctena jako pole
    template <typedef T> T param(std::string name, t_vi_param_flags f){

        std::list<T> v = param(name, f);
        return (v.size()) ? v[0] : T();
    }

protected:
    //potrebuji udelat hook na capab. a discovery paket
    virtual void callback(vi_ex_io::t_vi_io_r event){

        if(event == vi_ex_io::VI_IO_OK){

            t_vi_exch_dgram dg;
            rdBuf->get(0, (u8 *)&dg, sizeof(dg.h));   //vykopirujem header (bez posunu rx pntr!)
            if(dg.h.type == VI_I_CAP){

               if(p_cap) delete; p_cap = NULL;
               p_cap = (t_vi_param *) new u8[dg.h.size];
               rdBuf->get(sizeof(t_vi_exch_dgram), (u8 *)p_cap, dg.h.size);
            } else if(dg.h.type == VI_DISCOVERY){

                //to do - vygenerujem si marker na toho s kym chceme mluvit
            }
        }

        vi_ex_io::callback();
    }

public:

    template <typedef T> std::list<T> def(std::string name){ //defaultni hodnota parametru pokud ocekavame pole

        return param(name, VI_TYPE_P_DEF);
    }

    //vraci 3 hodnoty v poradi nazvu fce pokud je polozka tohoto typu jinak 0
    template <typedef T> std::array<T, 3> minmaxstep(std::string name){

        std::array<T, 3> v;
        v[0] = param(name, VI_TYPE_P_MIN);
        v[1] = param(name, VI_TYPE_P_MAX);
        v[2] = param(name, VI_TYPE_P_RANGE2) - v[0];
        return v;
    }

    //vraci seznam hodnot menu pokud je polozka vyctova
    template <typedef T> std::set<T> menulist(std::string name){

        t_vi_param_flags f = VI_TYPE_P_MIN;
        std::list<T> tv(1), v;

        while((tv = param<T>(name, f++)).size())  //pokud stale neco vycitame
            v.insert(*(tv.begin())); //berem prvni; vic by jich stejne byt nemelo

        return v;
    }

    //vraci seznam stringu parametru
    std::set<std::string> stringlist(std::string name){

        t_vi_param_flags f = VI_TYPE_P_MIN;
        std::list<std::string> v;
        std::list<char> tv(1);
        std::string s;

        while((tv = param<char>(name, f++)).size()){//pokud stale neco vycitame

            for(std::list<char>::iterator c = tv.begin; c != tv.end; c++) s.append(1, c);
            v.insert(s);
        }

        return v;
    }

    //zmena hodnoty parametru
    template <typedef T> std::list<T> set(std::string name, std::list<T> &v){

        u8 *vals = t_vi_param
        t_vi_param_stream writer(p_cap);
        if(reader.setpos(name, f)){

            int n; _tp = reader.isvalid(&n); //vime ze je v poradku - jen chceme velikost
            T *tv = new T[n];
            n = reader.readnext<T>(name, &tv, n);
            for(int i=0; i<n; i++) v.insert(tv[i]);
            delete tv;
            if(tp) tp = _tp;
        }
        return v;

        t_vi_exch_dgram *confs = preparetx(NULL, VI_I_SETUP_SET, u32 size = 0, u32 sess_id = 0)
        if(vi_ex_hid::submit(name) == vi_ex_io::VI_IO_OK)  //prelozime a poslem binarne
            if(receive(rcv) == vi_ex_io::VI_IO_OK) //pockame si na odpoved a tu hned prelozime do cloveciny
                return std::string(rcv); //vratime
    }

    //cteni konfigurace
    template <typedef T> std::list<T> get(std::string name){

        vi_ex_io::submit();
    }

    std::string command(std::string ord){

        char rcv[VIEX_HID_SP];
        if(vi_ex_hid::submit(ord) == vi_ex_io::VI_IO_OK)  //prelozime a poslem binarne
            if(receive(rcv) == vi_ex_io::VI_IO_OK) //pockame si na odpoved a tu hned prelozime do cloveciny
                return std::string(rcv); //vratime

        return std::string();  //jinka nic
    }

    //pokud je io typu hid pak muzem vypisovat tracy na terminal
    vi_ex_cell(std::ostream *_tt_io):tt_io(_tt_io){ p_cap = NULL; }
    virtual ~vi_ex_cell();
};

#endif // VI_EX_CELL_H
