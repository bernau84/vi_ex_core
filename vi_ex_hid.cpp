
#include "vi_ex_hid.h"

#define VIEX_T_2_STR(DEF)   #DEF

/*! \def - creates format string and read values from setting
    with particular template
*/
#define VIEX_T_READNEXT(TYPE, FTNAME)\
    v = (u8 *) new TYPE[n];\
    ft = FTNAME;\
    szof = sizeof(TYPE);\
    io.readnext<TYPE>(&name, (TYPE *)v, n, &f);\

#define VIEX_T_PRINTOUT(TYPE, FTSTR)\
    for(int l = 0, i = 0; (m < len) && (i < n); m += l, i++){\
    l = snprintf(&cmd[m], len-m, FTSTR, *(TYPE *)(v+szof*i));\
    if(l > (len-m)) l = (len-m);\
    else if((i+1) < n) cmd[m + l++] = ','; }\

/*! \brief conversion of all pamereter list to text
    syntax: name/'def|enum|min|max'('byte|int|char|float|bool|long')=1,2,3,4,5,6,......

    @param p[in] - binary data
    @param n[in] - length of data
    @param cmd[out] - output text trasnation
    @param len[in] - command lenght restriction
*/
int vi_ex_hid::cf2hi(const u8 *p, int n, char *cmd, int len){

    t_vi_param_mn name;
    t_vi_param_flags f;
    t_vi_param_type t;

    int m = 0, szof = 0;
    u8 *v = NULL;
    const char *ft = "";

    t_vi_param_stream io((u8 *)p, n);
    while(((t = io.isvalid(&n)) != VI_TYPE_UNKNOWN) && (m < len)){  //through all param

        switch(t){ //appropriate scan from stream

            default: return 0;
            case VI_TYPE_BYTE:      VIEX_T_READNEXT(u8, "byte");        break;
            case VI_TYPE_INTEGER:   VIEX_T_READNEXT(int, "int");        break;
            case VI_TYPE_BOOLEAN:   VIEX_T_READNEXT(bool, "bool");      break;
            case VI_TYPE_INTEGER64: VIEX_T_READNEXT(u64, "long");       break;
            case VI_TYPE_FLOAT:     VIEX_T_READNEXT(double, "float");   break;
            case VI_TYPE_CHAR:
                //extra bypass for string
                n += 1; //for /0
                VIEX_T_READNEXT(char, "char");
                v[n] = 0; n = 1;  //bypass
                break;
        }

        switch(f){  //type of param fork

            case VI_TYPE_P_VAL: m += snprintf(&cmd[m], len-m, " %s(%s)=", name, ft);        break;
            case VI_TYPE_P_DEF: m += snprintf(&cmd[m], len-m, " %s/def(%s)=", name, ft);    break;
            case VI_TYPE_P_MIN: m += snprintf(&cmd[m], len-m, " %s/min(%s)=", name, ft);    break;
            case VI_TYPE_P_MAX: m += snprintf(&cmd[m], len-m, " %s/max(%s)=", name, ft);    break;
            default:    m += snprintf(&cmd[m], len-m, " %s/menu%d(%s)=", name, f, ft);      break;
        }

        //array item by item iteration
        if((m > 0) && (len > m))
            switch(t){

                default: return 0;
                case VI_TYPE_BYTE:      VIEX_T_PRINTOUT(u8, "%d");      break;
                case VI_TYPE_INTEGER:   VIEX_T_PRINTOUT(int, "%d");     break;
                case VI_TYPE_BOOLEAN:   VIEX_T_PRINTOUT(bool, "%d");    break;
                case VI_TYPE_INTEGER64: VIEX_T_PRINTOUT(u64, "%lld");   break;
                case VI_TYPE_FLOAT:     VIEX_T_PRINTOUT(double, "%g");  break;
                case VI_TYPE_CHAR:
                    //extra bypass for string - we know there is only 1 item
                    m += snprintf(&cmd[m], len-m, "%s", (char *)v);
                    break;
            }

        if(v) delete[] v;
        v = NULL;
    }

    return (len > m) ? len : m;
}


/*! \def - scan array of vaules from text
    and write them to packet
*/
#define VIEX_T_WRITENEXT(TYPE, FRMSTR, TMPT)\
    {\
        TYPE v[n]; TMPT tmp; \
        for(int i=0; (1 == sscanf(w_cmd+1, FRMSTR, &tmp)) && (i < n); i++){\
            v[i] = (TYPE)tmp;\
            if((NULL == (w_cmd = strchr(w_cmd, ','))) || (w_cmd >= end))\
                break;\
        }\
        return VIEX_PARAM_LEN(TYPE, io.append<TYPE>(&name, v, n, f));\
    }

/*! \brief scans one parametr from text input
    syntax: name/'def|min|max|menu0|menu1...'('u8|char|float|bool|u64')=1,2,3,4,5,6,...

    @param p[out] - output data
    @param n[in] - space avaiable for data
    @param cmd[in] - command
    @param len[in] - command lenght
*/
#define HID_TYPE_N  16
int vi_ex_hid::cf2dt(u8 *p, int n, const char *cmd, int len){

    char name[VIEX_PARAM_NAME_SIZE];
    char c, s_type[HID_TYPE_N+1], s_dtype[HID_TYPE_N+1] = "";
    t_vi_param_flags f = VI_TYPE_P_VAL;  //vaule type by default

    char *w_cmd = (char *)cmd;
    const char *end = cmd+len;

    t_vi_param_stream io((u8 *)p, n); //iterator init
    if( (4 == sscanf(cmd, "%"DEF2STR(VIEX_PARAM_NAME_SIZE)"[^/]/%"DEF2STR(HID_TYPE_N)"[^(](%"DEF2STR(HID_TYPE_N)"[^)])=%c", name, s_dtype, s_type, &c)) ||
        (3 == sscanf(cmd, "%"DEF2STR(VIEX_PARAM_NAME_SIZE)"[^(](%"DEF2STR(HID_TYPE_N)"[^)])=%c", name, s_type, &c)) ||    //vaule assumed
        (2 == sscanf(cmd, "%"DEF2STR(VIEX_PARAM_NAME_SIZE)"s=%c", name, &c)) ){  //vaule & int assumed

        if(0 == strcmp(s_dtype, "min")) f = VI_TYPE_P_MIN;
        else if(0 == strcmp(s_dtype, "max")) f = VI_TYPE_P_MAX;
        else if(0 == strcmp(s_dtype, "def")) f = VI_TYPE_P_DEF;
        else if(1 == sscanf(s_dtype, "menu%d", &n)) f = (t_vi_param_flags)n;

        char *dlm = (w_cmd = strchr(cmd, '=')); //find equal mark
        n = 1; while((dlm) && (dlm = strchr(dlm, ',')) && (dlm < end)) n += 1; //how many items are there?

        if(0 == strcmp(s_type, "u8")) VIEX_T_WRITENEXT(u8, "%u", int)  //C90 doen't support %hhu
        else if(0 == strcmp(s_type, "u64")) VIEX_T_WRITENEXT(u64, "%u", u32)  //either %Lu, grrr
        else if(0 == strcmp(s_type, "bool")) VIEX_T_WRITENEXT(bool, "%d", int)   //no compiler supports bool
        else if(0 == strcmp(s_type, "float")) VIEX_T_WRITENEXT(double, "%lf", double)  //float == double
        else if(0 == strcmp(s_type, "char")) return VIEX_PARAM_LEN(char, io.append<char>(&name, w_cmd, strlen(w_cmd), f)); //direct write for string
        else VIEX_T_WRITENEXT(int, "%d", int);  //int by default
    }

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
        case VI_I_SETUP_GET:
        case VI_I_SETUP_SET:   //write and read of settings

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
        case VI_I_SETUP_GET:   //setting read and write
        case VI_I_SETUP_SET:
        {
            char *par = (char *)cmd;
            while((*par > 32) && (len > 0)){ par++; len--; }    //move behind command
            while((*par <= 32) && (len > 0)){ par++; len--; }   //move behind white spaces
            d->size = cf2dt(d->d, d->size, par, len);
        }
        break;
        default:

            d->size = 0;
        break;
    }

    return 1;
}
