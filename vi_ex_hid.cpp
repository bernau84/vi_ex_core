
#include "vi_ex_hid.h"

#define VIEX_T_2_STR(DEF)   #DEF

/*! \def - creates format string and read values from setting
    with particular template
*/
#define VIEX_T_READNEXT(TYPE, FRMSTR)\
    v = (u8 *) new TYPE[n];\
    fv = FRMSTR;\
    ft = VIEX_T_2_STR(TYPE);\
    szof = sizeof(TYPE);\
    io.readnext<TYPE>(&name, (TYPE *)v, n, &f);\

/*! \brief conversion of all pamereter list to text
    sytantax: name/'def|enum|min|max'('u8|char|double|bool|u64')=1,2,3,4,5,6,......

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
    const char *ft = "", *fv = "";

    t_vi_param_stream io((u8 *)p, n);
    while(((t = io.isvalid(&n)) != VI_TYPE_UNKNOWN) && (m < len)){  //through all param

        switch(t){ //appropriate scan from stream

            default: return 0;
            case VI_TYPE_BYTE:      VIEX_T_READNEXT(u8, "%d");      break;
            case VI_TYPE_INTEGER:   VIEX_T_READNEXT(int, "%d");     break;
            case VI_TYPE_BOOLEAN:   VIEX_T_READNEXT(bool, "%d");    break;
            case VI_TYPE_INTEGER64: VIEX_T_READNEXT(u64, "%lld");   break;
            case VI_TYPE_FLOAT:     VIEX_T_READNEXT(double, "%g");  break;
            case VI_TYPE_CHAR:
                //extra bypass for string
                n += 1; //for /0
                VIEX_T_READNEXT(char, "%s");
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
        for(int i = 0; ((m += snprintf(&cmd[m], len-m, fv, v+szof*i)) < len) && (i < n); i++)
            m += snprintf(&cmd[m], len-m, ",");    //delimiter append

        if(v) delete[] v;
        v = NULL;
    }

    return m;
}


/*! \def - scan array of vaules from text
    and write them to packet
*/
#define VIEX_T_WRITENEXT(TYPE, FRMSTR, TMPT)\
    if(!strcmp(s_type, VIEX_T_2_STR(TYPE))){\
        TYPE v[n]; TMPT tmp; \
        for(int i=0; (1 == sscanf(w_cmd, FRMSTR, &tmp)) && (i < n); i++){\
            v[i] = (TYPE)tmp;\
            if((NULL == (w_cmd = strchr(w_cmd, ','))) || (w_cmd >= end))\
                break;\
        }\
        return VIEX_PARAM_LEN(TYPE, io.append<TYPE>(&name, v, n));\
    }\

/*! \brief scans one parametr from text input
    sytantax: name('u8|char|double|bool|u64')=1,2,3,4,5,6,......

    @param p[out] - output data
    @param n[in] - space avaiable for data
    @param cmd[in] - command
    @param len[in] - command lenght
*/
int vi_ex_hid::cf2dt(u8 *p, int n, const char *cmd, int len){  //v p musi byt predvyplneno na jaky typ prevadet!

    char name[VIEX_PARAM_NAME_SIZE];
    char c, s_type[16];

    char *w_cmd = (char *)cmd;
    const char *end = cmd+len;

    t_vi_param_stream io((u8 *)p, n); //interator init
    if(3 != sscanf(cmd, "%s(%s)=%c", name, s_type, &c)){

        char *dlm = (w_cmd = strchr(cmd, '=')); //find equal mark
        n = 0; while((dlm = strchr(dlm, ',')) != NULL) n += 1; //how many items are there?

        VIEX_T_WRITENEXT(u8, "%u", int);  //C90 doen't support %hhu
        VIEX_T_WRITENEXT(u64, "%u", u32);  //either %Lu, grrr
        VIEX_T_WRITENEXT(int, "%d", int);
        VIEX_T_WRITENEXT(bool, "%d", int);   //no compiler supports bool
        VIEX_T_WRITENEXT(double, "%lf", double);
        n = strlen(cmd); VIEX_T_WRITENEXT(char, "%s", char); //+ bypass for string
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

            for(u32 i=0; i<d->size; i++){

                if(d->d[i] > 32) n = snprintf(cmd, len, "%c", d->d[i]);
                    else n = snprintf(cmd, len, "\\x%x", d->d[i]); //non ascii -> hex
                len -= n; cmd += n;
            }
        break;

        case VI_I:      //ack data - test of connection
        case VI_BULK: 	//unstructured/unacknowledge data

            for(u32 i=0; i<d->size; i++){

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

            d->size = cf2hi(d->d, len, (char *)cmd, strlen(cmd));
        break;
        default:

            d->size = 0;
        break;
    }

    return 1;
}
