#ifndef VI_EX_CELL_H
#define VI_EX_CELL_H

#include "vi_ex_hid.h"
#include "vi_ex_par.h"

#include <string>
#include <map>
#include <list>
#include <vector>
#include <algorithm>
#include <iostream>  //cout


#define VI_CAPS_SZ      (2048)      /*!< how much space reserved of capabilities remote */
#define VI_NAME_SZ      (255 + 1)   /*!< friendly name of node max length */


typedef char (t_vi_io_mn)[VI_NAME_SZ];
typedef t_vi_io_mn *p_vi_io_mn;

/*!
    \class t_vi_param_descr
    \brief unique definition of parametr and value type
*/
class t_vi_param_descr{
public:
  std::string name;           /*!< name of parametr (contrast, hue, etc.) */
  t_vi_param_flags def_range; /*!< type of parametr (vaule, default, min, max, etc.) */

  bool operator<(const t_vi_param_descr& op) const {

//    return((name < op.name) || (def_range < op.def_range)); not worked with map(); undefined result with empty items

      if(name != op.name)  //sorting with name priority
          return(name < op.name);
      else
          return(def_range < op.def_range);  //for the same name, type
  }

  const t_vi_param_descr &operator=(const t_vi_param_descr& op){

    name = op.name;
    def_range = op.def_range;
    return *this;
  }

  t_vi_param_descr(std::string _name = std::string(), t_vi_param_flags _def_range = VI_TYPE_P_VAL){

    name = _name;
    def_range = _def_range;
  }

  t_vi_param_descr(const t_vi_param_descr& op){

    name = op.name;
    def_range = op.def_range;
  }
};

/*!
    \class t_vi_param_content
    \brief unique definition of parametr and value type
*/
class t_vi_param_content
{
public:
  t_vi_param_type type;  /*!< elementary typedef */
  int length;            /*!< number of items of type*/
  std::vector<u8> v;     /*!< raw data */

  /*! read with conversion to required array */
  template <typename T> int readrange(u32 bgn_i, u32 end_i, T *ret){

      if(bgn_i > end_i) std::swap(bgn_i, end_i);
      if(end_i >= (u32)length) end_i = length-1;
      for(u32 i=bgn_i; i<=end_i; i++){

          u8 *p = (u8 *)&ret[i];
          for(u32 j=0; j<sizeof(T); j++) *p++ = v[i*sizeof(T) + j];
      }

      return end_i - bgn_i + 1;
  }

  /*! append and serialize values */
  template <typename T> int append(T *val, u32 n){

      for(u32 i=0; i<n; i++){

          u8 *p = (u8 *)&val[i];
          for(u32 j=0; j<sizeof(T); j++) v.push_back(*p++);
      }

      return n;
  }

  t_vi_param_content(t_vi_param_type _type = VI_TYPE_INTEGER, int _length = 0, std::vector<u8> _v = std::vector<u8>()) :
      type(_type), length(_length), v(_v){;}
};

/*!
    \class vi_ex_cell
    \brief high level function for viex node; uses stl

    confortable managmenet of settings (cache of capabilities)
    basic methods common for all viex nodes
    connecting and maping of nodes on network bus
    heavy use od std templates
*/

class vi_ex_cell : public virtual vi_ex_io
{
private:
    t_vi_io_mn name;  /*!< unique node name on bus */
    std::ostream *p_trace;  /*!< trace output */

    std::map<t_vi_param_descr, t_vi_param_content> cap;
    std::map<std::string, std::vector<u8> > neighbours;  /*!< bus network nodes */

    /*!
        \brief helper method for reading value from capabilities
        template
    */
    template <typename T> std::vector<T> params(const std::string &name, t_vi_param_flags f){

        std::vector<T> v;
        t_vi_param_descr id(name, f);

        T tv; u8 *td = (u8 *)&tv;
        for(int i=0; i<cap[id].length; i++){

          for(u32 j=0; j<sizeof(T); j++) td[j] = cap[id].v[j]; //conversion
          v.push_back(tv);
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
        t_vi_exch_type t = (action <= 1) ? VI_I_GET_PAR : VI_I_SET_PAR;

        t_vi_param_mn lname;
        name.copy(lname, sizeof(lname));

        T confv[N]; for(int i = 0; i != v.size(); i++) confv[i] = v[i];  //vector to c-array
        t_vi_exch_dgram *confs = preparetx(NULL, t, VIEX_PARAM_LEN(T, N));
        t_vi_param_stream writer(confs->d, confs->size);
        writer.append<T>(&lname, confv, v.size());

        if((vi_ex_io::submit(confs) == vi_ex_io::VI_IO_OK) && (0 == (action & 0x1))){
            //waiting for reply
            preparerx(confs, VI_I_RET_PAR, VIEX_PARAM_LEN(T, N));
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

    /*! \brief high level callback (only for relevant packet, omits internal control packet) */
    virtual void callback_rx_new(t_vi_exch_dgram *rx){;}

    /*! \brief high level callback, t_vi_param_content structure must be updated */
    virtual void callback_read_par_request(const t_vi_param_descr &id, t_vi_param_content &cont){;}

    /*! \brief high level callback, t_vi_param_content structure must be updated */
    virtual void callback_write_par_request(const t_vi_param_descr &id, t_vi_param_content &cont){;}
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
          for(u32 i = 0; i < v.size(); i++) st.push_back(v[i] - m[i]);

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
    bool pair(const std::string &remote);


    /*! \brief rename node
    */
    void rename(const std::string &local){

      char *p = (char *)&name, *end = &p[sizeof(t_vi_io_mn)];
      memset(name, 0, sizeof(t_vi_io_mn));
      for(std::string::const_iterator i=local.begin(); i<local.end(); i++)
        if(p < end) *p++ = *i;
    }

    /*! \brief contructor
    */
    vi_ex_cell(p_vi_io_mn _name, std::ostream *_p_trace = &std::cout):
      vi_ex_io(), p_trace(_p_trace){
    
        memcpy(name, _name, sizeof(t_vi_io_mn));
    }

    /*! \brief destructor
    */
    virtual ~vi_ex_cell(void){

    }
};

#endif // VI_EX_CELL_H
