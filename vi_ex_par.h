#ifndef VI_EX_PARAM_H
#define VI_EX_PARAM_H

#include <typeinfo>

/*!
  \enum t_vi_param_type

    supported setting element types; inpired by enum v4l2_ctrl_type
    should be enought

    VI_TYPE_UNKNOWN
    VI_TYPE_BYTE
    VI_TYPE_CHAR
    VI_TYPE_INTEGER
    VI_TYPE_BOOLEAN
    VI_TYPE_INTEGER64
    VI_TYPE_FLOAT
*/
typedef enum {
#define VI_ST_ITEM(def, cnt, type, idn, sz) def = cnt,\

#include "vi_ex_def_settings_types.inc"
#undef VI_ST_ITEM
    VI_TYPE_ENDFLAG
} t_vi_param_type;


/*!
  \struct t_vi_param_type_lut
  \todo - convert do parametric macro

    helper lut defining symbolic name of type and
    size of one item

    {VI_TYPE_UNKNOWN,   "",  0},
    {VI_TYPE_BYTE,      "B", 8*sizeof(u8)},
    {VI_TYPE_CHAR,      "C", 8*sizeof(char)},
    {VI_TYPE_INTEGER,   "I", 8*sizeof(int)},
    {VI_TYPE_BOOLEAN,   "b", 1},
    {VI_TYPE_INTEGER64, "L", 8*sizeof(s64)},
    {VI_TYPE_FLOAT,     "F", 8*sizeof(double)},
*/
const struct {

    const char *s;    /**< short name (for text comand mode) */
    u32 sz;     /**< item bites size */
} t_vi_param_type_lut[] = {

#define VI_ST_ITEM(def, cnt, type, idn, sz) {idn, sz},\

#include "vi_ex_def_settings_types.inc"
#undef VI_ST_ITEM
    {"",  0}
};


/*!
  \enum t_vi_param_flags
  \brief description of actual and default parametr values type

  range definition - MIN, RANGE2(== MIN + STEP), MAX
  enum definition - MIN, RANGE2, RANGE3, RANGE4 ..etc
*/
typedef enum {

    VI_TYPE_P_VAL = 0,   /**< actual value */
    VI_TYPE_P_MIN = 1,   /**< minimum or first enum option */
    VI_TYPE_P_RANGE2 = 2, /**< min+step or second enum option  */
    VI_TYPE_P_RANGE3 = 3, /**< third enum option (4, 5. ..up to 254 possible enum values) */
    VI_TYPE_P_MAX = 254,  /**< maximum or last enum opion */
    VI_TYPE_P_DEF = 255  /**< default */
} t_vi_param_flags;

#define VIEX_PARAM_NAME_SIZE    32

typedef char (t_vi_param_mn)[VIEX_PARAM_NAME_SIZE];
typedef t_vi_param_mn *p_vi_param_mn;

/*!
    \typedef t_vi_param
    \brief generic viex setting paket

    typedef struct {

        char name[VIEX_PARAM_NAME_SIZE];
        t_vi_param_type type;
        t_vi_param_flags def_range;
        u32 length;
        u8 v[1];
    }
*/

typedef struct {
#define VI_S_ITEM(name, type, sz) type name;\

#include "vi_ex_def_settings_head.inc"
#undef VI_S_ITEM
    u8  v[1];           /*< empty [] if supported */
} t_vi_param;


/*!
    \brief size of viex serialized parametrer head
*/
const u32 vi_settings_hlen =
#define VI_S_ITEM(name, type, sz) sz+\

#include "vi_ex_def_settings_head.inc"
#undef VI_S_ITEM
                    0;

#define VIEX_PARAM_HEAD() (vi_settings_hlen)        //size of serialized head setting size
#define VIEX_PARAM_LEN(T, N) (VIEX_PARAM_HEAD() + sizeof(T)*N)  //size of overall serialized param size

/*!
    \class t_vi_param_stream
    \brief streamed read and write parameter of viex node settings
*/
class t_vi_param_stream {

private:
    u8 *bgn;
    u8 *end;
    u8 *it; //iterator

    /*!
        \fn serialize
        \brief convert structured settings to streamed data
    */
    bool serialize_head(const t_vi_param *d){

        if(!d) return false;
        u32 n = end - it;
        u8 *p = it;

#define VI_S_ITEM(name, type, sz)\
        if(n >= sz) memcpy(p, &(d->name), sz);\
            else return false;\
        n -= sz; p += sz;\

#include "vi_ex_def_settings_head.inc"
#undef VI_S_ITEM

        return true;
    }

    /*!
        \fn deserialize
        \brief convert stremed data to structured settings
    */
    bool deserialize_head(t_vi_param *d){

        if(!d) return 0;
        u32 n = end - it;
        u8 *p = it;

#define VI_S_ITEM(name, type, sz)\
        memcpy(&(d->name), p, sz);\
        n -= sz; p += sz;\

#include "vi_ex_def_settings_head.inc"
#undef VI_S_ITEM

        return true;
    }

public:

    /*!
        inicialization by serialized block of sttings bytes
    */
    t_vi_param_stream(u8 *p, int size){

        it = bgn = p;
        end = (p+size);
    }

    /*!
        next item state
        0 NO
        >0 YES-returns number of element of setings array
        -1 SYNTAX ERROR
    */
    t_vi_param_type isvalid(int *len = 0){

        if((it < bgn) || (it >= end)) return VI_TYPE_UNKNOWN;

        t_vi_param st; deserialize_head(&st);

        if((st.type <= VI_TYPE_UNKNOWN) || (st.type >= VI_TYPE_ENDFLAG)) return VI_TYPE_UNKNOWN;
        if(len) *len = st.length;
        return st.type;
    }

    /*!
        find and set pointer to parametr of name
    */
    t_vi_param_type setpos(p_vi_param_mn name){

        if(!name) return VI_TYPE_UNKNOWN; //fatal

        while(isvalid()){

            t_vi_param st; deserialize_head(&st);

            if(0 == strcmp((char *)st.name, (char *)name))
                return st.type;

            it += VIEX_PARAM_HEAD();
            it += st.length * t_vi_param_type_lut[st.type].sz/8;
        }

        return VI_TYPE_UNKNOWN;
    }

    /*!
        find and set pointer to parametr of name and type
    */
    t_vi_param_type setpos(p_vi_param_mn name, t_vi_param_flags f){

        while(VI_TYPE_UNKNOWN != setpos(name)){

            t_vi_param st; deserialize_head(&st);
            if(st.def_range == f)
                return st.type;
        }
    }

    /*!
        template
        write one setting and shift pointer
        returns number of vaules (in array case)
    */
    template <typename T> int append(p_vi_param_mn name, T *val, int len = 1, t_vi_param_flags f = VI_TYPE_P_VAL){

        if(!name || !val) return -1; //fatal
        if(it >= end) return 0; //the end

        t_vi_param st;
        st.def_range = f;
        memcpy(st.name, name, VIEX_PARAM_NAME_SIZE);

        //'dynamical identification of type'
        if(0){}
#define VI_ST_ITEM(def, cnt, ctype, idn, sz)\
        else if(typeid(T) == typeid(ctype)) st.type = def;\

#include "vi_ex_def_settings_types.inc"
#undef VI_ST_ITEM
        else return 0; //we can't encode this type

        int vsz = t_vi_param_type_lut[st.type].sz / 8;
        u8 *tmp = it + VIEX_PARAM_HEAD();
        for(st.length = 0; (st.length < (u32)len) && ((void *)tmp < (void *)end); st.length++){

            memcpy(tmp, val, vsz);
            tmp += vsz; val ++;
        }

        serialize_head(&st);
        it += VIEX_PARAM_HEAD();
        it += st.length * vsz;
        return st.length;
    }

    /*!
        template
        read one setting and shift pointer
        returns number of vaules (in array case)
    */
    template <typename T> int readnext(p_vi_param_mn name, T *val, int len = 1, t_vi_param_flags *f = 0){

        if(!name || !val) return -1; //fatal
        if(!isvalid()) return 0; //error at the end of records

        t_vi_param st; deserialize_head(&st);

        //sanity check by 'dynamical identification of type'
        if(0){}
#define VI_ST_ITEM(def, cnt, ctype, idn, sz)\
        else if((typeid(T) == typeid(ctype)) && (st.type == def)){}\

#include "vi_ex_def_settings_types.inc"
#undef VI_ST_ITEM
        else return 0; //we can't encode this type

        u32 i, vsz = t_vi_param_type_lut[st.type].sz / 8;
        u8 *tmp = it + VIEX_PARAM_HEAD();
        for(i = 0; (i < st.length) && (i < (u32)len) && ((void *)tmp < (void *)end); i++){

            memcpy(val, tmp, vsz);
            tmp += t_vi_param_type_lut[st.type].sz; val ++;
        }

        st.length = i;  //possible length correction
        strcpy((char *)name, (char *)st.name);
        if(f) *f = st.def_range;

        it += VIEX_PARAM_HEAD();
        it += st.length * vsz;
        return st.length;
    }
};

#endif // VI_EX_PARAM_H
