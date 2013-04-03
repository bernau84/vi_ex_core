#ifndef VI_EX_PARAM_H
#define VI_EX_PARAM_H

#define u8	unsigned char
#define u16	unsigned short
#define u32	unsigned int
#define s8	signed char
#define s16	signed short
#define s32	signed int
#define u64	unsigned long int
#define s64	signed long int

#include <string>  //stl strings
#include <set>

typedef enum { //datove typy nastaveni; oprasknute z enum v4l2_ctrl_type

    VI_TYPE_BYTE = 1,
    VI_TYPE_INTEGER,
    VI_TYPE_BOOLEAN,
    VI_TYPE_INTEGER64,
    VI_TYPE_FLOAT,
    VI_TYPE_STRING,  //spec. pripad; variablni delky v utf8 formatu
    VI_TYPE_F = 0xC0,  //FLAG MASK
    VI_TYPE_F_ARRAY = 0x40,   //FLAG pokud jde o pole; pred vyctem musi predchzet length
    VI_TYPE_F_MENU = 0x80  //FLAG k predchazejicim typum pokud jsou dane vyctem
} t_vi_param_type;

const struct t_vi_exch_params{

    const char       *stype;
    t_vi_param_type    type;
} vi_exch_params[] = {  //definice human readable typu

    {"byte",   VI_TYPE_BYTE},
    {"int",    VI_TYPE_INTEGER},
    {"bool",   VI_TYPE_BOOLEAN},
    {"int64",  VI_TYPE_INTEGER64},
    {"float",  VI_TYPE_FLOAT},
    {"string", VI_TYPE_STRING},
    {" array", VI_TYPE_F_ARRAY},
    {" menu",  VI_TYPE_F_MENU},
    {"unknown", (t_vi_param_type)0}  //end mark
};

//uspana vetev
//pokus o povyseni prace s parametry na uroven stl se vsim vsudy
//prerizene opratory, dynamicke alokace
//prelezlo to unosnou mez, ramec je ale asi pouzitelny
template <class T> class t_vi_param_i {  //jedna polozka

private:
    int _read_t(T *v, t_vi_param_type t, void *d, int len);
    int _write_t(T *v, t_vi_param_type t, void *d, int len);

    t_vi_param_type type;       //typ polozky
    std::string name;  //nazev nastaveni
    T val;  //aktualni nebo defaulni hodnota v zavislosti na pouziti
    std::set<T> restr; //omezeni vyctem nebo ma min, max a step

public:

    std::string name(){ return name; }
    t_vi_param_type type(){ return type; }
    T val(){ return val; }
    T operator =(T &_val){ val = _val; }

    std::set<T> range(){ return restr; }  //vraci vse; zalezi na type jak interpretovat
    T min(){ if(!(VI_TYPE_F_MENU & type) && (restr.count() == 3)) return restr[0]; else return T(); }
    T max(){ if(!(VI_TYPE_F_MENU & type) && (restr.count() == 3)) return restr[2]; else return T(); }
    T step(){

        if((restr.count() == 3) && (VI_TYPE_STRING > type))  //bude fungoavt jen u cisel
            return restr[1] - restr[0];
        else
            return T();
    }

    int serialize(u8 *d, u32 size);  //srovna hodnoty do rady, vraci zapsanou delku
    int restriction(std::set<T> &range){ restr = range; return 1; }  //omezeni vyctem
    int restriction(T min, T max, T step){ //omezeni min, max, step

        restr.empty();
        if(VI_TYPE_STRING <= type)  //jen pro cisla
            return 0;

        restr << min << (min+step) << max;
        return 1;
    }

    t_vi_param(t_vi_param_type _t, u8 *d, u32 size = 0); //deserializuje automaticky ze streamu, size pro kontrolu
    t_vi_param(t_vi_param_type _t, char *_nm, T _val) :
        name(_nm), type(_t), val(_val) {;}

    virtual ~t_vi_param(); //vse by se melo uvolnit samo
};

typedef std::list<long long> t_vi_arr_i64;
typedef std::list<double> t_vi_arr_float;
typedef std::list<bool> t_vi_arr_bool;
typedef std::list<int> t_vi_arr_int;
typedef std::list<u8> t_vi_arr_u8;

#endif // VI_EX_PARAM_H
