#ifndef VI_EX_PARAM_H
#define VI_EX_PARAM_H

#include <typeinfo>

//datove typy nastaveni; inspirovane z enum v4l2_ctrl_type
//jine nepodporujem protoze je neumime jednoduse serializovat
typedef enum {
    VI_TYPE_UNKNOWN = 0,
    VI_TYPE_BYTE = 1,
    VI_TYPE_CHAR,  //slouzi i pro vytvoreni stringu (nula se ale pocita)
    VI_TYPE_INTEGER,
    VI_TYPE_BOOLEAN,
    VI_TYPE_INTEGER64,
    VI_TYPE_FLOAT,
    VI_TYPE_ENDFLAG
} t_vi_param_type;

//pokud je parametrem nejaka limitno hodnota pak pro vycet pouzivame MIN, RANGE2, RANGE3...atd
//pokud potrebujem jen zadat min max a step tak nastavujem jen MIN, RANGE2, a MAX
typedef enum { //datove typy nastaveni; oprasknute z enum v4l2_ctrl_type

    VI_TYPE_P_VAL = 0,   //aktualni hodnota
    VI_TYPE_P_MIN = 1,   //minimum rozsahu nebo 1. volba u menu
    VI_TYPE_P_RANGE2 = 2, //2. hodnota, u cisel danych rozsahem je to MIN + step
    VI_TYPE_P_RANGE3 = 3, //3. hodnota atd... az do 254
    VI_TYPE_P_MAX = 254,  //maximum rozsahu
    VI_TYPE_P_DEF = 255  //defaultni hodnota
} t_vi_param_flags;

#define VIEX_PARAM_NAME_SIZE    32

typedef char (*t_vi_param_mn)[VIEX_PARAM_NAME_SIZE];

//generalizovany prvek ze seznamu prametru
typedef struct {

    char name[VIEX_PARAM_NAME_SIZE];
    t_vi_param_type type;
    t_vi_param_flags def_range;
    u32 length;
    u8 v[0];
} __attribute__ ((packed)) t_vi_param;


#define VIEX_PARAM_HEAD() (sizeof(t_vi_param))        //delka hlavicky - stale stejna
#define VIEX_PARAM_LEN(T, N) (sizeof(t_vi_param)+sizeof(T)*N)  //celkova delka jednoho param

//zapis a cteni nad streamem parametru
class t_vi_param_stream {

private:
    t_vi_param *bgn;
    t_vi_param *end;
    t_vi_param *it; //iterator

public:
    //inicializace serializovanym blokem nebo jen mistem kde vyskladame parametry
    t_vi_param_stream(u8 *p, int size){

        it = bgn = (t_vi_param *)p;
        end = (t_vi_param *)(p+size);
    }

    //je aktualni polozka v poradku - 0 ne, > 0 ano-vraci pocet prvku, -1 mimo ramec
    t_vi_param_type isvalid(int *len = 0){

        if((it < bgn) || (it >= end)) return VI_TYPE_UNKNOWN;
        if((it->type <= VI_TYPE_UNKNOWN) || (it->type >= VI_TYPE_ENDFLAG)) return VI_TYPE_UNKNOWN;

        if(len){  //nekdo si zada pocet polozek pole

            int tsz = 1;
            switch(it->def_range){

                case VI_TYPE_INTEGER: tsz = sizeof(int); break;
                case VI_TYPE_INTEGER64: tsz = sizeof(u64); break;
                case VI_TYPE_FLOAT: tsz = sizeof(double); break;
                default: break;
            }
            *len = it->length / tsz;
        }
        return it->type;
    }

    //nastavi it na nejblizsi pozici name parametru
    t_vi_param_type setpos(t_vi_param_mn name){

        if(!name) return VI_TYPE_UNKNOWN; //fatal
        while(isvalid())
            if(0 == strcmp((char *)it->name, (char *)name))
                return it->type;
            else
                it += VIEX_PARAM_HEAD() + it->length;

        return VI_TYPE_UNKNOWN;
    }

    //nastavi it na nejblizsi pozici name parametru a typu hodnoty
    t_vi_param_type setpos(t_vi_param_mn name, t_vi_param_flags f){

        while(VI_TYPE_UNKNOWN != setpos(name))
            if(it->def_range == f)
                return it->type;
    }

    //zapise a posune ukazatel; vraci kolik polozek jsem zapsal (v pripade pole)
    //inicializace z l-hodnoty - typ pomuze urcit
    template <typename T> int append(t_vi_param_mn name, T *val, int len = 1, t_vi_param_flags f = VI_TYPE_P_VAL){

        if(!name || !val) return -1; //fatal
        if(it >= end) return 0; //jsme na konci

        //dyn. identifikace typu
//        const type_info t_v = typeid(T); //jako reference bude rychlejsi porovnani (z prikladu...asi)
        if(typeid(T) == typeid(u8)) it->type = VI_TYPE_BYTE;
        else if(typeid(T) == typeid(char)) it->type = VI_TYPE_CHAR;
        else if(typeid(T) == typeid(int)) it->type = VI_TYPE_INTEGER;
        else if(typeid(T) == typeid(bool)) it->type = VI_TYPE_BOOLEAN;
        else if(typeid(T) == typeid(u64)) it->type = VI_TYPE_INTEGER64;
        else if(typeid(T) == typeid(double)) it->type = VI_TYPE_FLOAT;
        else return 0; //typ ktery neumime zakodovat

        T *tmp = (T *)it->v;
        for(it->length = 0; (it->length < (u32)len) && ((void *)tmp < (void *)end); it->length++)
            *tmp++ = *val++; //diky attr packed budu muset stejne mozna kopirovat

        it->length *= sizeof(T);
        u32 sh = VIEX_PARAM_HEAD() + it->length;
        it += sh;

        return (it->length / sizeof(T));
    }

    //prectem a posunem ukazatel; vraci kolik polozek jsem vycetl (v pripade pole)
    //inicializace z l-hodnoty - typ pomuze urcit dyn. identifikace typu
    template <typename T> int readnext(t_vi_param_mn name, T *val, int len = 1, t_vi_param_flags *f = 0){

        if(!name || !val) return -1; //fatal
        if(!isvalid()) return 0; //chyba - jsme na konci platnych zaznamu

        //sanity check; asi by sel vynechat pokud by bylo principielne splneno coz ale nemusi
//        const type_info t_v = typeid(T); //jako reference bude rychlejsi porovnani asi (z prikladu...)
        if((typeid(T) == typeid(u8)) && (it->type == VI_TYPE_BYTE)){}
        else if((typeid(T) == typeid(char)) && (it->type == VI_TYPE_CHAR)){}
        else if((typeid(T) == typeid(int)) && (it->type == VI_TYPE_INTEGER)){}
        else if((typeid(T) == typeid(bool)) && (it->type == VI_TYPE_BOOLEAN)){}
        else if((typeid(T) == typeid(u64)) && (it->type == VI_TYPE_INTEGER64)){}
        else if((typeid(T) == typeid(double)) && (it->type == VI_TYPE_FLOAT)){}
        else return 0;  //neumime

        int i = 0;
        T *tmp = (T *)it->v;
        for(; (i < (int)(it->length/sizeof(T))) && (i < len) && ((void *)tmp < (void *)end); i++)
            *val++ = *tmp++; //diky attr packed budu muset stejne mozna kopirovat

        strcpy((char *)name, (char *)it->name);  //kontrola vzhledem k omezeni param fce netreba
        if(f) *f = it->def_range;

        u32 sh = VIEX_PARAM_HEAD() + it->length;
        it += sh;

        return i;
    }

};

#endif // VI_EX_PARAM_H
