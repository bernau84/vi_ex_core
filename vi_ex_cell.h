#ifndef VI_EX_CELL_H
#define VI_EX_CELL_H

#include <vi_ex_hid.h>
#include <vi_ex_par.h>

#include <sstream>
#include <string>
#include <map>
#include <list>
#include <array>
#include <algorithm>

//prijmnejsi sprava capabilities
//a zakladnich fukci spolecnych vsem io
//podpora high level get, set configu
//human readable text terminal
class vi_ex_cell : public vi_ex_hid
{
private:
    t_vi_param *p_cap;  //zaloha binarniho paketu s capab
    std::iostream *p_hi;  //terminal kde ctem prikazy a kam je davame
    std::ostream *p_trace;  //tracovaci vystup
    std::string cmdln;  //cache prikazove radky
    std::map<std::string, t_vi_io_id> neighbours; 

    //low-level hodnota parametru vyctena jako pole
    template <typedef T> std::vector<T> param(t_vi_param_mn name, t_vi_param_flags f){

        std::vector<T> v;
        if(!p_cap) return v;

        t_vi_param_stream reader(p_cap);
        if(reader.setpos(name, f)){

            int n; _tp = reader.isvalid(&n); //vime ze je v poradku - jen chceme velikost
            T *tv = new T[n];
            n = reader.readnext<T>(name, &tv, n);
            for(int i=0; i<n; i++) v.push_back(tv[i]);
            delete tv;
            if(tp) tp = _tp;
        }
        return v;
    }

    //low-level hodnota parametru vyctena jako pole
    template <typedef T> T param(t_vi_param_mn name, t_vi_param_flags f){

        std::vector<T> v = param(name, f);
        return (v.size()) ? v[0] : T();
    }

    //zmena hodnoty parametru a vycrteni (nebo jen vycteni pokud nic neposilame)
    //zalezi na action - 0 - cteni (request pouze), 1 - cteni (s cekanim na odpoved), 2 - zapis (requst pouze), 3 - zapis a cteni
    template <typedef T> std::vector<T> setup(t_vi_param_mn name, std::vector<T> &v, u8 action){

        std::vector<T> confr;
        std::vector<T> d = def(name); //vyctem defaultni prametr kvuli velikosti        
        int N = (v.size() > d.size()) ? v.size() : d.size();  //berem vetsi
        t_vi_exch_type t = (action > 1) ? VI_I_SETUP_GET : VI_I_SETUP_SET;

        T confv[N]; for(int i = 0; i != v.size(); i++) confv[i] = v[i];  //z vekotru c-pole
        t_vi_exch_dgram *confs = preparetx(NULL, t, VIEX_PARAM_LEN(T, v.size()));  //paket si pripravime
        t_vi_param_stream writer(confs->setup, confs->h.size); //priparavime si zapisovac
        writer.append<T>(name, confv, v.size());  //a do paketu vrazime tam to pole

        if((submit(confs) == vi_ex_io::VI_IO_OK) && (0 == (action & 0x1))){       
            //cekame na odpoved s nastavenim
            //pripravime si ten samy paket, neprijmem ovsem vic nez jsme poslali!!!
            preparerx(confs, t, VIEX_PARAM_LEN(T, v.size()));
            if(receive(confs) != vi_ex_io::VI_IO_OK){

                t_vi_param_stream reader(confs->setup, confs->h.size); //priparavime si vycitac parametru
                N = writer.readnext<T>(name, confv, N);  //mame neco?
                for(int i=0; i<N; i++) confr.push_back(conv[i]);  //preklopime
            }
        }
        
        delete confs;
        return confr; //vratime
    }    

protected:

    //tracy doplnene o timestamp (strojovy cas v sec)
	virtual void debug(const char *msg){ 
	
        clock_t tick = clock();
        char stick[32]; snprintf(stick, sizeof(stick), "%g ", ((float)t)/CLOCKS_PER_SEC);
        *p_trace << stick;
		*p_trace << msg;
	}  

    //potrebuji udelat hook na capab. a discovery paket
    virtual void callback(vi_ex_io::t_vi_io_r event){

        if(event == vi_ex_io::VI_IO_OK){

            vi_ex_io::callback(); //zatim nic nedela

            t_vi_exch_dgram dg;
            rdBuf->get(0, (u8 *)&dg, sizeof(dg.h));   //vykopirujem header (bez posunu rx pntr!)
            switch(dg.h.type){

                case VI_I_CAP:  //moznosti druhe strany; zalohujeme si u sebe

                    if(p_cap) delete; p_cap = NULL;
                    p_cap = (t_vi_param *) new u8[dg.h.size];
                    rdBuf->get(VI_HLEN(), (u8 *)p_cap, dg.h.size);
                    break;

                case VI_ECHO_REP:  //pridame do neigbours seznamu
                case VI_REG:    //registrace, pouzijem marker toho kdo chceme s nami mluvit 
                            //!!!ten kdo to poslal musi zarucit je jde o unikatni marker
                    t_vi_io_id lname;
                    t_vi_io_id lmark;                    
                    if(size(lname) != dg.h.size) //vadny format
                        break;
                        
                    rdBuf->get(VI_HLEN()), (u8 *)lname, sizeof(lname)); //sedi delka i po vycteni
                    memcpy(lmark, dg.h.mark, sizeof(lmark));
                    if(VI_ECHO_REP == dg.h.type) //VI_ECHO_REP zalozime si jen do seznamu okolnich prvku
                        neighbours[lname] = lmark;  
                    else if((VI_REG == dg.h.type) && (0 == memcmp(vi_ex_io::name, lname, sizeof(lname))) //jde o reistraci a je pro nas?
                        destination(lmark);  //a od ted se s nikym jinym nebavim; app vrstva by nasledne mela poslat VI_CAP
                    break;

                case VI_BULK://to do - vsechny prichozi pakety BULK formatovat do tracovaciho int.
                    break;

                case VI_ECHO_REQ:{  //odpovime nasim VI_ECHO_REP

                    u8 d[VI_HLEN() + sizeof(t_vi_io_nm)];
                    preparetx((t_vi_exch_dgram *)d, VI_ECHO_REP, sizeof(t_vi_io_nm));  //paket si pripravime
                    memcpy(erep->d, name, sizeof(t_vi_io_nm));
                    submit(erep);  //a pryc s tim
                    break;
                }

                case VI_ANY:{
                    //potichu zahodime
                    rdBuf->read(NULL, VI_LEN(P));
                    break;
                }
            }
        }
    }

public:

    template <typedef T> std::vector<T> def(t_vi_param_mn name){ //defaultni hodnota parametru pokud ocekavame pole

        return param(name, VI_TYPE_P_DEF);
    }

    //vraci 3 hodnoty v poradi nazvu fce pokud je polozka tohoto typu jinak 0
    template <typedef T> std::array<T, 3> minmaxstep(t_vi_param_mn name){

        std::array<T, 3> v;
        v[0] = param(name, VI_TYPE_P_MIN);
        v[1] = param(name, VI_TYPE_P_MAX);
        v[2] = param(name, VI_TYPE_P_RANGE2) - v[0];
        return v;
    }

    //vraci seznam hodnot menu pokud je polozka vyctova
    template <typedef T> std::set<T> menudef(t_vi_param_mn name){

        t_vi_param_flags f = VI_TYPE_P_MIN;
        std::list<T> tv(1), v;

        while((tv = param<T>(name, f++)).size())  //pokud stale neco vycitame
            v.insert(*(tv.begin())); //berem prvni; vic by jich stejne byt nemelo

        return v;
    }

    //vraci seznam stringu parametru
    std::set<std::string> stringdef(t_vi_param_mn name){

        t_vi_param_flags f = VI_TYPE_P_MIN;
        std::list<std::string> v;
        std::list<char> tv(1);
        std::string s;

        while((tv = param<char>(name, f++)).size()){//pokud stale neco vycitame

            for(std::list<char>::iterator c = tv.begin; c != tv.end(); c++) s.append(1, c);
            v.insert(s);
        }

        return v;
    }

    //zmena hodnoty parametru a vycteni (nebo jen vycteni pokud nic neposilame)
    template <typedef T> std::vector<T> set(t_vi_param_mn name, std::vector<T> &v){

        return setup(name, v, 3);
    }

    //cteni konfigurace
    template <typedef T> std::vector<T> get(t_vi_param_mn name){

        std::vector<T> nullv; 
        return setup(name, v, 1);
    }

    std::string command(std::string ord){

        char rcv[VIEX_HID_SP];
        if(vi_ex_hid::submit(ord) == vi_ex_io::VI_IO_OK)  //prelozime a poslem binarne
            if(receive(rcv) == vi_ex_io::VI_IO_OK) //pockame si na odpoved a tu hned prelozime do cloveciny
                return std::string(rcv); //vratime

        return std::string();  //jinka nic
    }

    //koukne jestli je zadan nejaky textovy povel (odradkovany)
    //pokud ano predhodiho 
    void refreshcmdln(void){

        char rcv[VIEX_HID_SP];
        if(p_hi) if(p_hi->readsome(rcv, VIEX_HID_SP)) cmdln << rcv;  //sup do cache pokud tam neco ceka
        if(cmdln.find('\n') != std::string::npos){    //je tam uz cely komand?

            std::string cmd;
            std::getline(cmdln, cmd);  //vyctem prikaz
            cmdln = cmdln.substring(cmd.length()+1); //cache zkratime

            *p_hi << "<< \"" << cmd << "\"";
            *p_hi << ">> \"" << command(cmd) << "\""; //vykoname a vratime vysledek
        }
    }    

    //pokud je io typu hid pak muzem vypisovat tracy na terminal
    vi_ex_cell(std::iostream *_p_hi = 0, std::ostream *_p_trace = &std::cout):
		p_hi(_p_hi), p_trace(_p_trace){ 
	
		p_cap = NULL;
	}

    std::map<std::string, t_vi_io_id> neighbours(void){

        return neighbours;
    }

    //spojeni s druhym koncem p2p; stavovy automat na 10s max
    //fce je blokujici; ceka na ozvani se vzdaleneho nodu na broadcast echo
    //pak se registruje, pak se ceka na reakci na prime echo
    bool pair(std::string remote){  

        t_vi_io_id br_mark; memset(br_mark, 0, sizeof(mark)); //broadcast marker
        t_vi_io_id act_mark; memcpy(act_mark, br_mark, sizeof(mark)); //zaciname s broadcastem

        u8 pair_sta = 0;
        for(int timeout = 10000; timeout > 0; timeout -= 10){ //cekame 10s jestli se ozve
            
            switch(pair_sta){

                case 0: {

                    //udelame echo a zjistime jestli takove zarizeni posloucha a ceka
                    t_vi_exch_dgram echo;
                    preparetx(&echo, VI_ECHO_REQ);  //paket si pripravime
                    memcpy(echo.h.marker, act_mark, sizeof(mark)); //udelame z toho broadcast nebo unicast podle act_mark
                    submit(echo);

                    if(remote.empty()) 
                        return true; //jen broadcastove echo

                    pair_sta = 1;                    
                    break;
                }

                case 1: {

                    if(neighbours.find(remote) == neighbours.end())
                        break;  //cekame az se ozve

                    if(0 == memcmp(neighbours[remote], mark, sizeof(mark)))
                        return true;  //zarizeni uz je sparovane (registrovane) s nami!                    

                    if(0 != memcmp(neighbours[remote], br_mark, sizeof(mark)))
                        return false;  //zarizeni uz je sparovane (registrovane) a my to nejsme!

                    if(0 == memcmp(br_mark, mark, sizeof(mark))){ //ani my nemame jeste unikatni marker
                        //nahodny vyber noveho markeru
                        u8 eq = 1;
                        while(equal){

                            char smark[sizeof(mark)+1]; snprintf(smark, sizeof(smark), "%04d", rand() % 9999); //vygenerujem nahodny
                            memcpy(mark, smark, sizeof(mark)); //4 znakove cislo

                            eq = 0; //kontrola unikatnosti
                            for(std::map<char,int>::iterator it = mymap.begin();
                                it != mymap.end(); it ++) if(0 == memcmp(it->second, mark, sizoef(mark)) eq = 1; 
                        }
                    }    

                    u8 d[VI_HLEN() + sizeof(t_vi_io_nm)]; //odeslem registracni paket
                    preparetx((t_vi_exch_dgram *)d, VI_REG, sizeof(t_vi_io_nm));  //paket si pripravime
                    memcpy(&d[VI_HLEN()], remote.c_str(), sizeof(t_vi_io_nm));
                    submit((t_vi_exch_dgram *)d);  //a pryc s tim                
                    pair_sta = 2; //== default
                    break;
                }

                case 52:  //05s po kroku 1 zkusime zas echo, ale uz unicast + vymazem zaznam z cache
                    neighbours.erase(remote);
                    memcpy(act_mark, mark, sizeof(mark));
                    pair_sta = 0;

                default:  //jen cekani v podtstate
                    pair_sta += 1;


            }

            parse();
            wait10ms();            
        }

        return false;       
    }

    virtual ~vi_ex_cell(){

        if(p_cap) delete; p_cap = NULL;
    }
};

#endif // VI_EX_CELL_H
