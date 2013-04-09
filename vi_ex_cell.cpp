
#include <vi_ex_cell.h>


//tracy doplnene o timestamp (strojovy cas v sec)
void vi_ex_cell::debug(const char *msg){ 

	if(p_trace){ 

	    char s_tick[32]; clock_t tick = clock();
	    snprintf(s_tick, sizeof(s_tick), "%g ", ((float)t)/CLOCKS_PER_SEC);
	    *p_trace << s_tick;
		*p_trace << msg;
	}
}  

//potrebuji udelat hook na capab. a discovery paket
void vi_ex_cell::callback(vi_ex_io::t_vi_io_r event){

    if(event == vi_ex_io::VI_IO_OK){

        vi_ex_io::callback(); //zatim nic nedela

        t_vi_exch_dgram dg;
        rdBuf->get(0, (u8 *)&dg, sizeof(dg.h));   //vykopirujem header (bez posunu rx pntr!)
        switch(dg.h.type){

            case VI_I_CAP:  //moznosti druhe strany; zalohujeme si u sebe

                if(p_cap) delete[] p_cap; p_cap = NULL;

                try { //to do - takhle osetrit vsechny potencialne velke alokace

                	p_cap = (t_vi_param *) new u8[dg.h.size];
            	} catch(std::bad_alloc& ba){

            		char inf[128]; snprintf(inf, sizeof(inf), "(!!!)E bad_alloc caught: %s\n", ba.what());
					debug(inf);
					assert(1); //while(1);
            	}

                rdBuf->get(VI_HLEN(), (u8 *)p_cap, dg.h.size);
                break;

            case VI_ECHO_REP:  //pridame do neigbours seznamu
            case VI_REG:{    //registrace, pouzijem marker toho kdo chceme s nami mluvit 
                        //!!!ten kdo to poslal musi zarucit je jde o unikatni marker
                char lname[VI_NAME_SZ], lmark[VI_MARKER_SZ];                    
                if(size(lname) != dg.h.size) //vadny format
                    break;
                    
                rdBuf->get(VI_HLEN(), (u8 *)lname, sizeof(lname)); //sedi delka i po vycteni
                memcpy(lmark, dg.h.mark, sizeof(lmark));
                if(VI_ECHO_REP == dg.h.type) //VI_ECHO_REP zalozime si jen do seznamu okolnich prvku
                    neighbours[std::string(lname)] = std::vector<u8>(&lmark[0], &lmark[VI_MARKER_SZ]);  //inicializace iteratory bgn a end
                else if((VI_REG == dg.h.type) && (0 == memcmp(vi_ex_io::name, lname, sizeof(lname)))) //jde o reistraci a je pro nas?
                    destination(lmark);  //a od ted se s nikym jinym nebavim; app vrstva by nasledne mela poslat VI_CAP
                break;
            }
            
            case VI_BULK://to do - vsechny prichozi pakety BULK formatovat do tracovaciho int.
                break;

            case VI_ECHO_REQ:{  //odpovime nasim VI_ECHO_REP

                u8 d[VI_HLEN() + VI_NAME_SZ];
                preparetx((t_vi_exch_dgram *)d, VI_ECHO_REP, VI_NAME_SZ);  //paket si pripravime
                memcpy(&d[VI_HLEN()], name, VI_NAME_SZ);
                submit((t_vi_exch_dgram *)d);  //a pryc s tim
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


bool vi_ex_cell::pair(std::string remote){  

	u8 act_mark[VI_MARKER_SZ]; 
	vi_set_broadcast(&act_mark); //zaciname s broadcastem

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

	            if(0 == memcmp(neighbours[remote].data(), vi_ex_io::mark, sizeof(mark)))
	                return true;  //zarizeni uz je sparovane (registrovane) s nami!                    

	            if(false == vi_is_broadcast((t_vi_io_id)neighbours[remote].data()))
	                return false;  //zarizeni uz je sparovane (registrovane) a my to nejsme!

	            if(true == vi_is_broadcast(mark)){ //ani my nemame jeste unikatni marker
	                //nahodny vyber noveho markeru
	                u8 eq = 1;
	                while(eq){

	                    char smark[VI_NAME_SZ]; snprintf(smark, VI_NAME_SZ, "%04d", rand() % 9999); //vygenerujem nahodny
	                    memcpy(mark, smark, sizeof(mark)); //4 znakove cislo

	                    eq = 0; //kontrola unikatnosti
	                    for(std::map<std::string, std::vector<u8>>::iterator it = mymap.begin();
	                        it != mymap.end(); it ++) if(0 == memcmp(it->second.data(), vi_ex_io::mark, sizoef(mark)) eq = 1; 
	                }
	            }    

	            u8 d[VI_HLEN() + VI_NAME_SZ]; //odeslem registracni paket
	            preparetx((t_vi_exch_dgram *)d, VI_REG, VI_NAME_SZ);  //paket si pripravime
	            memcpy(&d[VI_HLEN()], remote.c_str(), VI_NAME_SZ);
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

