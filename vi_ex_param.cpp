
#include "vi_ex_param.h"

template <class T> t_vi_param(u8 *d, u32 size = 0){

    //construktor s deserializaci
    type = (t_vi_param_type)*d++;
    name = std::string(d); d += strlen(d);

    if(!_iscomp(type)) return;

    if(type & VI_TYPE_ARRAY){

        int rep; mempcy(&rep, d, sizeof(int)); d += sizeof(int);
    }
}

//serializator elementarnich typu
template <class T> int t_vi_param::_read_t(T *v, void *d, int len){

    const type_info *t_v = typeid(T);

    //pomuzem si dynamickou identifikaci typu
    if(t_v == typeid(u8)){

        if(len > sizeof(u8)) *(u8 *)d = *v;
    } else if(t_v == typeid(int)){

        if(len > sizeof(int)) memcpy(d, v, sizeof(int));
    } else if(t_v == typeid(bool)){

        if(len > sizeof(bool)) *(bool *)d = *v;
    } else if(t_v == typeid(long long)){

        if(len > sizeof(u64)) memcpy(d, v, sizeof(u64));
    } else if(t_v == typeid(double)){

        if(len > sizeof(double)) memcpy(d, v, sizeof(double));
    } else if(t_v == typeid(std::string)){

        strncpy(d, (*v).c_str(), len);
    }
}

template <class T> int t_vi_param::_iscomp(t_vi_param_type t){

    //sanity check; asi by sel vynechat pokud by bylo principielne splneno
    //coz ale nemusi
    const type_info *t_v = typeid(T);
    t_vi_param_type etype = t & ~VI_TYPE_MENU;

    if(t_v == typeid(u8)){

        if(etype != VI_TYPE_BYTE) return 0;   //nema smysl, nekompatibilni
    } else if(t_v == typeid(int)){

        if(etype != VI_TYPE_INTEGER) return 0;   //nema smysl, nekompatibilni
    } else if(t_v == typeid(bool)){

        if(etype != VI_TYPE_BOOLEAN) return 0;   //nema smysl, nekompatibilni
    } else if(t_v == typeid(long long)){

        if(etype != VI_TYPE_INTEGER64) return 0;   //nema smysl, nekompatibilni
    } else if(t_v == typeid(double)){

        if(etype != VI_TYPE_FLOAT) return 0;   //nema smysl, nekompatibilni
    } else if(t_v == typeid(std::string)){

        if(etype != VI_TYPE_STRING) return 0;   //nema smysl, nekompatibilni
    } else if(t_v == typeid(t_vi_arr_i64)){

        if(etype != (VI_TYPE_INTEGER64 | VI_TYPE_F_ARRAY)) return 0;   //nema smysl, nekompatibilni
    } else if(t_v == typeid(t_vi_arr_float)){

        if(etype != (VI_TYPE_FLOAT | VI_TYPE_F_ARRAY)) return 0;   //nema smysl, nekompatibilni
    } else if(t_v == typeid(t_vi_arr_int)){

        if(etype != (VI_TYPE_INTEGER | VI_TYPE_F_ARRAY)) return 0;   //nema smysl, nekompatibilni
    } else if(t_v == typeid(t_vi_arr_u8)){

        if(etype != (VI_TYPE_BYTE | VI_TYPE_F_ARRAY)) return 0;   //nema smysl, nekompatibilni
    } else if(t_v == typeid(t_vi_arr_bool)){

        if(etype != (VI_TYPE_BOOLEAN | VI_TYPE_F_ARRAY)) return 0;   //nema smysl, nekompatibilni
    } else
        return 0;

    return 1;
}

//deserializator elementarnich typu
template <class T> int t_vi_param::_write_t(T *v, void *d, int len){

    const type_info *t_v = typeid(T);

    //pomuzem si dynamickou identifikaci typu
    if(t_v == typeid(u8)){

        if(len > sizeof(u8)) *v = *(u8 *)d;
    } else if(t_v == typeid(int)){

        if(len > sizeof(int)) memcpy(v, d, sizeof(int));
    } else if(t_v == typeid(bool)){

        if(len > sizeof(bool)) *v = *(bool *)d;
    } else if(t_v == typeid(long long)){

        if(len > sizeof(u64)) memcpy(v, d, sizeof(u64));
    } else if(t_v == typeid(double)){

        if(len > sizeof(double)) memcpy(v, d, sizeof(double));
    } else if(t_v == typeid(std::string)){

        *v = std::string((char *)d);  //kopirovaci konstruktor
    }
}

template <class T> int t_vi_param::serialize(u8 *d, u32 size){  //srovna hodnoty do rady

}
