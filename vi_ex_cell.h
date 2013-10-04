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


#define VI_CAPS_SZ      (2048)      /*!< how much space reserved of capabilities remote */
#define VI_NAME_SZ      (255 + 1)   /*!< friendly name of node max length */


typedef char (t_vi_io_mn)[VI_NAME_SZ];
typedef t_vi_io_mn *p_vi_io_mn;

/*!
    \class vi_ex_cell
    \brief high level function for viex node; uses stl

    confortable managmenet of settings (cache of capabilities)
    basic methods common for all viex nodes
    human readable text terminal
    connecting and maping of nodes on network bus
*/

class vi_ex_cell : public vi_ex_hid
{
private:
    u8 p_cap[VI_CAPS_SZ];
    t_vi_param_stream cap; /*!< backup of binary capabilities of other side */
    t_vi_io_mn name;  /*!< unique node name on bus */

    std::iostream *p_hi;  /*!< human terminal; enter for text commands and responses */
    std::ostream *p_trace;  /*!< trace output */
    std::string cmdln;  /*!< cache of command line */
    std::map<std::string, std::vector<u8> > neighbours;  /*!< bus network nodes */

    /*!
        \brief helper method for reading value from capabilities
        template
    */
    template <typename T> std::vector<T> params(const std::string &name, t_vi_param_flags f){

        std::vector<T> v;

        t_vi_param_mn lname; memset(lname, 0, sizeof(lname));
        name.copy(lname, name.length());

        if(cap.setpos(&lname, f)){

            int n; cap.isvalid(&n); //vime ze je v poradku - jen chceme velikost
            T *tv = new T[n];
            n = cap.readnext<T>(&lname, tv, n);
            v.assign(tv, tv+n);
            delete[] tv;
        }
        return v;
    }

    /*!
        \brief helper method for reading value from capabilities
        template
    */
    template <typename T> T param(const std::string &name, t_vi_param_flags f){

        std::vector<T> v = params<T>(name, f);
        return (v.size()) ? v[0] : T();
    }

    /*!
        \brief read or write of parametr
        \param action[in] - 0 - read request
            1 - read and wait for reply
            2 - write param request
            3 - write param and wait for reply
    */
    template <typename T> std::vector<T> setup(const std::string &name, std::vector<T> &v, u8 action){

        std::vector<T> confr;
        std::vector<T> d = params<T>(name, VI_TYPE_P_DEF); //default param for size determination
        int N = (v.size() > d.size()) ? v.size() : d.size();  //choose bigger
        t_vi_exch_type t = (action > 1) ? VI_I_SETUP_GET : VI_I_SETUP_SET;

        t_vi_param_mn lname;
        name.copy(lname, sizeof(lname));

        T confv[N]; for(int i = 0; i != v.size(); i++) confv[i] = v[i];  //vector to c-array
        t_vi_exch_dgram *confs = preparetx(NULL, t, VIEX_PARAM_LEN(T, v.size()));
        t_vi_param_stream writer(confs->d, confs->size);
        writer.append<T>(&lname, confv, v.size());

        if((vi_ex_io::submit(confs) == vi_ex_io::VI_IO_OK) && (0 == (action & 0x1))){
            //waiting for reply
            preparerx(confs, t, VIEX_PARAM_LEN(T, v.size())); //we are expect the same payload size as we've send before
            if(vi_ex_io::receive(confs) != vi_ex_io::VI_IO_OK){

                t_vi_param_stream reader(confs->d, confs->size);
                N = reader.readnext<T>(&lname, confv, N);
                for(int i=0; i<N; i++) confr.push_back(confv[i]);
            }
        }
        
        delete[] confs;
        return confr;
    }    

    /*! \brief hook for internal bus packet like capabilities, registration, echo, ... */
    virtual void callback(vi_ex_io::t_vi_io_r event);

protected:

    /*! \brief traces plus timestamp */
    virtual void debug(const char *msg);

    /*! \brief high level callback (only for relevant packet, omits intrenal control packet) */
    virtual void callback_rx_new(t_vi_exch_dgram *rx){;}

public:

    /*! \brief returns default param value */
    template <typename T> std::vector<T> def(const std::string &name){ return params<T>(name, VI_TYPE_P_DEF); }

    /*! \brief returns param range - min */
    template <typename T> std::vector<T> min(const std::string &name){ return params<T>(name, VI_TYPE_P_MIN); }

    /*! \brief returns param range - max */
    template <typename T> std::vector<T> max(const std::string &name){ return params<T>(name, VI_TYPE_P_MAX); }

    /*! \brief returns param range - step */
    template <typename T> std::vector<T> step(const std::string &name){

        std::vector<T> st;
        std::vector<T> m = params<T>(name, VI_TYPE_P_MIN);
        std::vector<T> v = params<T>(name, VI_TYPE_P_VAL2);
        if(m.size() == v.size())  //steb by step
          for(int i = 0; i < v.size(); i++) st.push_back(v[i] - m[i]);

        return st;
    }

    /*! \brief returns param range - enumeration list
     *  restricted only on list of values (not arrays)
     */
    template <typename T> std::vector<T> menudef(const std::string &name){

        int f = (int)VI_TYPE_P_MIN;
        std::vector<T> tv(1), v;

        while((tv = params<T>(name, (t_vi_param_flags)f++)).size())
            v.push_back(*(tv.begin())); //pick the first only

        return v;
    }

    /*! \brief returns string param enumeration list */
    std::vector<std::string> stringdef(const std::string &name){

        int f = (int)VI_TYPE_P_MIN;
        std::vector<std::string> v;
        std::vector<char> tv;
        std::string s;

        while((tv = params<char>(name, (t_vi_param_flags)f++)).size()){

            s.clear();
            for(std::vector<char>::iterator c = tv.begin(); c != tv.end(); c++) s += *c;
            v.push_back(s);
        }

        return v;
    }

    /*! \brief change param value and read it again */
    template <typename T> std::vector<T> set(const std::string &name, std::vector<T> &v){

        return setup<T>(name, v, 3);
    }

    /*! \brief read actual param value */
    template <typename T> std::vector<T> get(const std::string &name){

        std::vector<T> v(1); //nulovy
        return setup<T>(name, v, 1);
    }

    /*! \brief text command
        \return text response
    */
    std::string command(const std::string &ord){

        char rcv[VIEX_HID_SP];
        if(vi_ex_hid::submit(ord.c_str()) == vi_ex_io::VI_IO_OK)  //translate to binary
            if(vi_ex_hid::receive(rcv, VIEX_HID_SP) == vi_ex_io::VI_IO_OK) //wait for reply & translate to text
                return std::string(rcv); //return

        return std::string();  //fail
    }

    /*! \brief command line refresh & execution
    */
    void refreshcmdln(void){

        size_t n;
        char rcv[VIEX_HID_SP];

        if(p_hi){

            p_hi->flush();
            /*! \todo readsome doesnt work (?!) */
            if(p_hi->readsome(rcv, VIEX_HID_SP)) cmdln.append(rcv);  //cache this chunk
        }

        if((n = cmdln.find('\n')) != std::string::npos){    //comman is complete?

            cmdln.copy(rcv, n); //command readout
            cmdln.erase(0, n); //flush cache

            std::string cmd(rcv);

            *p_hi << "<< \"" << cmd << "\"";
            *p_hi << ">> \"" << command(cmd) << "\""; //execute and write out response

            p_hi->flush();
            p_hi->readsome(rcv, VIEX_HID_SP); //dummy read of written
        }
    }    

    /*! \brief find all neighbours on bus
        using echo broadcast
    */
    std::map<std::string, std::vector<u8> > neighbourslist(void){

        if(neighbours.empty()){
            //make echo to find who is listening
            t_vi_exch_dgram echo; preparetx((u8 *)&echo, VI_ECHO_REQ);
            vi_set_broadcast(&echo.marker); //to all
            vi_ex_io::submit(&echo);
        }

        for(int i=0; i<50; i++) //05s
            wait10ms();

        return neighbours;
    }

    /*! \brief make p2p connection
        wait 10s for connection establishment
        act as state autonmat using broadcast and unicast echo
    */
    bool pair(std::string &remote);


    /*! \brief contructor
    */
    vi_ex_cell(p_vi_io_mn _name, std::iostream *_p_hi = NULL, std::ostream *_p_trace = &std::cout):
      vi_ex_hid(), cap(p_cap, VI_CAPS_SZ), p_hi(_p_hi), p_trace(_p_trace){
    
        memcpy(name, _name, sizeof(t_vi_io_mn));
    }

    /*! \brief destructor
    */
    virtual ~vi_ex_cell(void){

    }
};

#endif // VI_EX_CELL_H
