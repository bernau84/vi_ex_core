
#include "vi_ex_cell.h"
#include <ctime>

//add tick to trace
void vi_ex_cell::debug(const char *msg){ 

    if(p_trace){

        char info[128];
        clock_t tick = clock();
        snprintf(info, sizeof(info), "%g %s: %s", ((float)tick)/CLOCKS_PER_SEC, name, msg);
        p_trace->write(info, strlen(info));
    }
}  

//hook for intern packet types (registration, echo, capabilities)
void vi_ex_cell::callback(vi_ex_io::t_vi_io_r event){

    if(event == vi_ex_io::VI_IO_OK){

        u8 sdg[VI_HLEN()]; rdBuf->get(0, sdg, VI_HLEN());   //copy of rx buf
        t_vi_exch_dgram hdg; vi_dg_deserialize(&hdg, sdg, VI_HLEN());  //fill dg head

        vi_ex_io::callback(event); //nothing for now

        t_vi_exch_dgram *dg = vi_ex_io::preparerx(NULL, hdg.type, hdg.size); //read all packet
        if(vi_ex_io::VI_IO_OK != vi_ex_io::receive(dg, 0)){

            delete[] dg;
            return;
        }

        callback_rx_new(dg);  //user notification

        switch(dg->type){

            case VI_I_CAP: { //cache remote capabilities

                t_vi_param_stream tc(dg->d, dg->size);
                t_vi_param tp;

                while((tp.type = tc.isvalid((int *)&tp.length)) != VI_TYPE_UNKNOWN){  //through all param

                    /*! \todo just only append new cap; do not support update of previously added item
                     *    cap param. stream backup maybe better? update to std::map<t_vi_param, std::vector> or
                     *    std::list<t_vi_param>
                     */
                    if(0){}
#define VI_ST_ITEM(def, ctype, idn, sz)\
                    else if(tp.type == def){\
                      u8 tv[sz/8 * tp.length];\
                      if(tc.readnext<ctype>(&tp.name, (ctype *)tv, tp.length, &tp.def_range) < 0) break;\
                      if(cap.append<ctype>(&tp.name, (ctype *)tv, tp.length, tp.def_range) < 0) break;\
                    }

#include "vi_ex_def_settings_types.inc"
#undef VI_ST_ITEM
                }

                break;
            }
            case VI_ECHO_REP:  //add new neigbour to list
            case VI_REG:{    //registration, save remote marker, originator must ensure unique marker

                if((VI_NAME_SZ < dg->size) || (0 == dg->size)) //bad format
                    break;

                if(VI_ECHO_REP == dg->type){ //VI_ECHO_REP - save new remote node

                    std::string rem_nm((char *)dg->d, dg->size);
                    std::vector<u8> rem_id(&dg->marker[0], &dg->marker[VI_MARKER_SZ]); //inicializace iteratory bgn a end
                    neighbours[rem_nm] = rem_id;
                } else if((VI_REG == dg->type) && (0 == memcmp(name, dg->d, strlen(name)))){ //registration for us?

                    destination((t_vi_io_id *)dg->marker);  //from now i do talk only with registrating node
                }
                break;
            }
            
            case VI_BULK: //to do - forward to trace buffer
                break;

            case VI_ECHO_REQ:{  //reply with our VI_ECHO_REP

                u8 d[sizeof(t_vi_exch_dgram) + VI_NAME_SZ];
                t_vi_exch_dgram *dg_rep = preparetx(d, VI_ECHO_REP, strlen(name));
                memcpy(dg_rep->d, name, strlen(name));
                vi_ex_io::submit(dg_rep);  //reply immediately (not acked)
                break;
            }

            case VI_ANY:
            default:  //silently discarded
                break;
        }

        delete[] dg;
    }
}


bool vi_ex_cell::pair(std::string &remote){

    u8 act_mark[VI_MARKER_SZ];
    vi_set_broadcast(&act_mark); //begin with broadcastem

    u8 pair_sta = 0;
    for(int timeout = 10000; timeout > 0; timeout -= 10){ //wait 10s if reply

        switch(pair_sta){

            case 0: {

                //echo to test if such node exists
                t_vi_exch_dgram echo;
                preparetx((u8 *)&echo, VI_ECHO_REQ);  //
                memcpy(echo.marker, act_mark, sizeof(mark)); //broadcast in first, unicast later, according to act_mark
                vi_ex_io::submit(&echo);

                if(remote.empty())
                    return true; //for first (broadcast) echo

                pair_sta = 1;
                break;
            }

            case 1: {

                if(neighbours.find(remote) == neighbours.end())
                    break;  //wait for reply

                if(false == vi_is_broadcast((p_vi_io_id)neighbours[remote].data())){

                    if(0 == memcmp(neighbours[remote].data(), vi_ex_io::mark, sizeof(mark)))
                        return true;  //node is already registered with us!

                    return false;  //node is already registered but not with us!
                }

                if(true == vi_is_broadcast((p_vi_io_id)mark)){ //we have no marker yet
                    //random marker generator
                    u8 eq = 1;
                    while(eq){

                        char smark[VI_NAME_SZ];
                        snprintf(smark, VI_NAME_SZ, "%04d", rand() % 9999);
                        memcpy(mark, smark, sizeof(mark)); //4 znakove cislo

                        eq = 0; //kontrola unikatnosti
                        for(std::map<std::string, std::vector<u8> >::iterator it = neighbours.begin();
                            it != neighbours.end(); it ++)
                                if(0 == memcmp(it->second.data(), vi_ex_io::mark, sizeof(mark))) eq = 1;
                    }
                }

                u8 d[VI_HLEN() + VI_NAME_SZ]; //reg packet
                t_vi_exch_dgram *dg = preparetx(d, VI_REG, remote.length());
                memcpy(dg->d, remote.c_str(), dg->size);  //add name of remote
                vi_ex_io::submit(dg);
                pair_sta = 2; //== default
                break;
            }

            case 52:  //05s after step 1 new unicast echo + clear record from cache
                neighbours.erase(remote);
                memcpy(act_mark, mark, sizeof(mark));
                pair_sta = 0;
            break;

            default:  //wait state
                pair_sta += 1;
            break;
        }

        parser();
        wait10ms();
    }

    return false;
}

