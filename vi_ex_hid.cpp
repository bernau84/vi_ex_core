
#include <cstdlib>
#include "vi_ex_hid.h"


#define VIEX_T_2_STR(DEF)   #DEF


/*! \brief conversion of all pamereter list to text
    syntax: name/'def|min|max|menu1|menu2...'('byte|int|string|float|bool|long')=1,2,3,4,5,6,......

    @param p[in] - binary data
    @param n[in] - length of data
    @param cmd[out] - output text trasnation
    @param len[in] - command lenght restriction
*/
int vi_ex_hid::cf2hi(const u8 *p, int n, char *cmd, int len){

    int m = 0;
    u8 *vv = NULL, *v = NULL;
    const char *ft = "";

    t_vi_param tmp;
    t_vi_param_stream io((u8 *)p, n);

    while(((tmp.type = io.isvalid(&tmp.length)) != VI_TYPE_UNKNOWN) && (m < len)){  //through all param

        switch(tmp.type){ //appropriate scan from stream

          /*! macro for formated read from binary stream */
#define VI_ST_ITEM(def, ctype, idn, sz, prf, scf)\
            case def:\
              v = vv = (u8 *) new ctype[tmp.length];\
              io.readnext<ctype>(&tmp.name, (ctype *)v, tmp.length, &tmp.def_range);\
              ft = idn;\
              break;\

#include "vi_ex_def_settings_types.inc"
#undef VI_ST_ITEM
            default: return 0;
        }

        switch(tmp.def_range){  //type of param fork

            case VI_TYPE_P_VAL: m += snprintf(&cmd[m], len-m, " %s(%s)=", tmp.name, ft);                break;
            case VI_TYPE_P_DEF: m += snprintf(&cmd[m], len-m, " %s/def(%s)=", tmp.name, ft);            break;
            case VI_TYPE_P_MIN: m += snprintf(&cmd[m], len-m, " %s/min(%s)=", tmp.name, ft);            break;
            case VI_TYPE_P_MAX: m += snprintf(&cmd[m], len-m, " %s/max(%s)=", tmp.name, ft);            break;
            default: m += snprintf(&cmd[m], len-m, " %s/menu%d(%s)=", tmp.name, tmp.def_range, ft);     break;
        }

        //array; item by item iteration
        if((m > 0) && (len > m))
            switch(tmp.type){

              /*! macro for printing arrays with correct formating */
#define VI_ST_ITEM(def, ctype, idn, sz, prf, scf)\
                case def:\
                  for(int l = 0, i = 0; (m < len) && (i < (int)tmp.length); m += l, i++){\
                      l = snprintf(&cmd[m], len-m, prf, *(ctype *)v); v += sz/8;\
                      if(l > (len-m)) l = (len-m);\
                      else if(((i+1) < (int)tmp.length) && (def != VI_TYPE_CHAR)) cmd[m + l++] = ',';\
                  }\
                  break;\

#include "vi_ex_def_settings_types.inc"
#undef VI_ST_ITEM
                default: return 0;
            }

        if(vv) delete[] vv;
        vv = NULL;
    }

    return (len > m) ? len : m;
}


/*! \brief scans one parametr from text input
    syntax: name/'def|min|max|menu1|menu2...'('byte|string|float|bool|long')=1,2,3,4,5,6,...

    @param p[out] - output data
    @param n[in] - space avaiable for data
    @param cmd[in] - command
    @param len[in] - command lenght
*/
#define HID_TYPE_N  16
int vi_ex_hid::cf2dt(u8 *p, int n, const char *cmd, int len){

    char c = 0, name[VIEX_PARAM_NAME_SIZE];
    char s_type[HID_TYPE_N+1] = "int";
    char s_dtype[HID_TYPE_N+1] = "";
    t_vi_param_flags f = VI_TYPE_P_VAL;  //vaule type by default

    const char *end = cmd+len;

    t_vi_param_stream io((u8 *)p, n); //iterator init
    if(4 == sscanf(cmd, "%"DEF2STR(VIEX_PARAM_NAME_SIZE)"[^/]/%"DEF2STR(HID_TYPE_N)"[^(](%"DEF2STR(HID_TYPE_N)"[^)])=%c", name, s_dtype, s_type, &c)){;}
    else if(3 == sscanf(cmd, "%"DEF2STR(VIEX_PARAM_NAME_SIZE)"[^=/()]/%"DEF2STR(HID_TYPE_N)"[^=]=%c", name, s_dtype, &c)){;}
    else if(3 == sscanf(cmd, "%"DEF2STR(VIEX_PARAM_NAME_SIZE)"[^=/(](%"DEF2STR(HID_TYPE_N)"[^)])=%c", name, s_type, &c)){;}
    else if(2 == sscanf(cmd, "%"DEF2STR(VIEX_PARAM_NAME_SIZE)"[^=]=%c", name, &c)){;}
    else return 0;

    if(0 == strcmp(s_dtype, "min")) f = VI_TYPE_P_MIN;
    else if(0 == strcmp(s_dtype, "max")) f = VI_TYPE_P_MAX;
    else if(0 == strcmp(s_dtype, "def")) f = VI_TYPE_P_DEF;
    else if(1 == sscanf(s_dtype, "menu%d", &n)) f = (t_vi_param_flags)n;

    cmd = strchr(cmd, '='); //find equal mark; if not exists, append empty parametr (length = 0) - in GET command fo example

    /*! macro scan array of vaules from text and write them to packet */
#define VI_ST_ITEM(def, ctype, idn, sz, prf, scf)\
    if(0 == strcmp(s_type, idn)){\
        int m, M = 0; ctype *v = NULL;\
        for(m = 0; (cmd != NULL) && (cmd < end); m++){\
            if(m == M) v = (ctype *)realloc(v, sz/8 * (M += 25));\
            if((NULL == v) || (1 != sscanf(cmd+1, scf, &v[m]))) break;\
            cmd = (def != VI_TYPE_CHAR) ? strchr(cmd+1, ',') : cmd+1;\
        }\
        M = VIEX_PARAM_LEN(ctype, io.append<ctype>(&name, v, m, f));\
        if(v) free(v);\
        return M;\
    }\

#include "vi_ex_def_settings_types.inc"
#undef VI_ST_ITEM

    return 0;  //error
}

/*! \brief viex datagram to human speech
*/
int vi_ex_hid::conv2hi(const t_vi_exch_dgram *d, char *cmd, int len){

    int i = 0; //find corresponding command
    for(; vi_exch_cmd[i].type; i++)
        if(vi_exch_cmd[i].type == d->type)
            break;

    int n = snprintf(cmd, len, "%s", vi_exch_cmd[i].cmd); //translation begin with command
    len -= n; cmd += n;

    //type fork
    u16 t = d->type & ~VI_ACK;
    switch(t){

        case VI_ECHO_REQ:   //request and reply to node presence
        case VI_ECHO_REP:
            //payload -> ascii directly out
            n = snprintf(cmd, len, " ");
            len -= n; cmd += n;

            for(u32 i=0; (n > 0) && (len > 0) && (i<d->size); i++){

                if(d->d[i] > 32) n = snprintf(cmd, len, "%c", d->d[i]);
                    else n = snprintf(cmd, len, "\\x%x", d->d[i]); //non ascii -> hex
                len -= n; cmd += n;
            }
        break;

        case VI_I:      //ack data - test of connection
        case VI_BULK: 	//unstructured/unacknowledge data

            for(u32 i=0; (n > 0) && (len > 0) && (i<d->size); i++){

                n = snprintf(cmd, len, " %x(%c)", d->d[i], d->d[i]); //unknown data; convert to hex
                len -= n; cmd += n;
            }
        break;

        case VI_I_CAP:
        case VI_I_GET_PAR:
        case VI_I_SET_PAR:
        case VI_I_RET_PAR:   //write / read / return of settings

            n = cf2hi(d->d, d->size, cmd, len);
            len -= n; cmd += n;
        break;
    }

    return (len > 0) ? 1 : 0;
}

/*! \brief human speech to viex datagram
*/
int vi_ex_hid::conv2dt(t_vi_exch_dgram *d, const char *cmd, int len){

    int i = 0; //find corresponding command
    for(; vi_exch_cmd[i].type; i++)
        if(0 == memcmp(cmd, vi_exch_cmd[i].cmd, strlen(vi_exch_cmd[i].cmd)))
            break;

    if((d == NULL) || (0 == vi_exch_cmd[i].type))
        return 0;  //no sych packet

    d->type = vi_exch_cmd[i].type;

    //type fork
    switch(vi_exch_cmd[i].type){

        //parameters with expected text attachment
        case VI_I:     //echo
        case VI_I_GET_PAR:   //setting read and write
        case VI_I_SET_PAR:
        case VI_I_CAP:
        {
            char par[len+2];
            while((*cmd > 32) && (len > 0)){ cmd++; len--; }    //move behind command
            while((*cmd <= 32) && (len > 0)){ cmd++; len--; }   //move behind white spaces
            int pn = snprintf(par, len, "%s", cmd);

            if(vi_exch_cmd[i].type == VI_I_GET_PAR)
              if(par[pn-1] != '=') //bypass for simplier parser
                strcpy(&par[pn], "=?");  //append query suffix

            d->size = cf2dt(d->d, d->size, par, len);
        }
        break;
        default:

            d->size = 0;
        break;
    }

    return 1;
}
