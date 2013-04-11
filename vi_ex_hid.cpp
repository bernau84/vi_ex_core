
#include "vi_ex_hid.h"

#define VIEX_T_2_STR(DEF)   #DEF

//potrebuju podle typu vytvorit formatovaci retezce a user friendy pojemnovani typu
//pak pouzijem patricnou sablonu pro vycteni hodnot ze streamu
#define VIEX_T_READNEXT(TYPE, FRMSTR)\
    v = (u8 *) new TYPE[n];\
    fv = FRMSTR;\
    ft = VIEX_T_2_STR(TYPE);\
    szof = sizeof(TYPE);\
    io.readnext<TYPE>(&name, (TYPE *)v, n, &f);\

//prevod celeho seznamu parametru na text
//sytantax je: nazev/'def|enum|min|max'('u8|char|double|bool|u64')=1,2,3,4,5,6,......
int vi_ex_hid::cf2hi(t_vi_param *p, int n, char *cmd, int len){

    char name[VIEX_PARAM_NAME_SIZE];
    t_vi_param_flags f;
    t_vi_param_type t;

    int m = 0, szof = 0;
    u8 *v = NULL;
    const char *ft = "", *fv = "";

    t_vi_param_stream io((u8 *)p, n); //inicializace interatoru
    while(((t = io.isvalid(&n)) != VI_TYPE_UNKNOWN) && (m < len)){  //pres vsechny parametry

        switch(t){ //podle datoveho typu paketu vyctem data

            default: return 0;
            case VI_TYPE_BYTE:      VIEX_T_READNEXT(u8, "%d");      break;
            case VI_TYPE_INTEGER:   VIEX_T_READNEXT(int, "%d");     break;
            case VI_TYPE_BOOLEAN:   VIEX_T_READNEXT(bool, "%d");    break;
            case VI_TYPE_INTEGER64: VIEX_T_READNEXT(u64, "%lld");   break;
            case VI_TYPE_FLOAT:     VIEX_T_READNEXT(double, "%g");  break;
            case VI_TYPE_CHAR:

                n += 1; //na ukoncovaci 0
                VIEX_T_READNEXT(char, "%s");
                v[n] = 0; n = 1;  //bypass
                break;
        }

        switch(f){  //podle typu hodnoty naformatujem nazev polozky

            case VI_TYPE_P_VAL: m += snprintf(&cmd[m], len-m, " %s(%s)=", name, ft);        break;
            case VI_TYPE_P_DEF: m += snprintf(&cmd[m], len-m, " %s/def(%s)=", name, ft);    break;
            case VI_TYPE_P_MIN: m += snprintf(&cmd[m], len-m, " %s/min(%s)=", name, ft);    break;
            case VI_TYPE_P_MAX: m += snprintf(&cmd[m], len-m, " %s/max(%s)=", name, ft);    break;
            default:    m += snprintf(&cmd[m], len-m, " %s/menu%d(%s)=", name, f, ft);      break;
        }

        //jde pres cele pole hodnot jednoho parametru a vypisujem po polozce
        for(int i = 0; ((m += snprintf(&cmd[m], len-m, fv, v+szof*i)) < len) && (i < n); i++)
            m += snprintf(&cmd[m], len-m, ",");    //oddelovac hodnot pole

        if(v) delete[] v;
        v = NULL;
    }

    return m;
}


//nactu ze textu pole hodnot; az je cele tak to zapsu do serializovane konfigurace
#define VIEX_T_WRITENEXT(TYPE, FRMSTR, TMPT)\
    if(!strcmp(s_type, VIEX_T_2_STR(TYPE))){\
        TYPE v[n]; TMPT tmp; \
        for(int i=0; (1 == sscanf(cmd, FRMSTR, &tmp)) && (i < n); i++){\
            v[i] = (TYPE)tmp;\
            if((NULL == (cmd = strchr(cmd, ','))) || (cmd >= end))\
                break;\
        }\
        return VIEX_PARAM_LEN(TYPE, io.append<TYPE>(&name, v, n));\
    }\

//z textoveho vstupu vycte nastaneni parametru - omezeni na jeden!
//protoze v tomto okamziku nezname typ polozky musi byt uveden ve stringu v zavorce
//mozno v potomkovi kde uz je nam to jasne pretizit
//v n je prostor vyhrazeny na data
int vi_ex_hid::cf2dt(t_vi_param *p, int n, char *cmd, int len){  //v p musi byt predvyplneno na jaky typ prevadet!

    char name[VIEX_PARAM_NAME_SIZE];
    char c, s_type[16], *end = cmd+len;

    t_vi_param_stream io((u8 *)p, n); //inicializace interatoru
    if(3 != sscanf(cmd, "%s(%s)=%c", name, s_type, &c)){

        char *dlm = (cmd = strchr(cmd, '=')); //dem na rovnitko
        n = 0; while((dlm = strchr(dlm, ',')) != NULL) n += 1; //kolik je tam prvku?

        VIEX_T_WRITENEXT(u8, "%u", int);  //C90 nepodporuje %hhu
        VIEX_T_WRITENEXT(u64, "%u", u32);  //a ani %Lu, grrr
        VIEX_T_WRITENEXT(int, "%d", int);
        VIEX_T_WRITENEXT(bool, "%d", int);   //a zadny kompilator nepodporuje bool
        VIEX_T_WRITENEXT(double, "%lf", double);
        n = strlen(cmd); VIEX_T_WRITENEXT(char, "%s", char); //+bypass na string
    }

    return 0;  //pokud to doslo az sem pak je tam chyba
}


int vi_ex_hid::conv2hi(const t_vi_exch_dgram *d, char *cmd, int len){

    int i = 0; //najdem odpovidajici povel
    for(; vi_exch_cmd[i].type; i++)
        if(vi_exch_cmd[i].type == d->type)
            break;

    int n = snprintf(cmd, len, "%s", vi_exch_cmd[i].cmd); //preklad do hi zacneme povelem
    len -= n; cmd += n;

    //rozlicovani parametry podle konkretniho typu
    u16 t = d->type & ~VI_ACK;
    switch(t){

        case VI_ECHO_REQ:   //vyzva a rekce na pritomnost zarizeni
        case VI_ECHO_REP:	//vyzva a rekce na pritomnost zarizeni
            //vyzvracet prilohu v ascii ven

            n = snprintf(cmd, len, " ");
            len -= n; cmd += n;

            for(u32 i=0; i<d->size; i++){

                if(d->d[i] > 32) n = snprintf(cmd, len, "%c", d->d[i]);
                    else n = snprintf(cmd, len, "\\x%x", d->d[i]); //nonascii prevadime na hexa
                len -= n; cmd += n;
            }
        break;

        case VI_I:      //potvzovana data - test potvrzovaneho spojeni
        case VI_BULK: 	//nestrukturovana/nepotvrzovana data - jina

            for(u32 i=0; i<d->size; i++){

                n = snprintf(cmd, len, " %x(%c)", d->d[i], d->d[i]); //vypisem to hexa; stejne by tam nemelo nic byt
                len -= n; cmd += n;
            }
        break;

        case VI_I_CAP:
        case VI_I_SETUP_GET:
        case VI_I_SETUP_SET:   //zapis nastaveni a vycteni nastaveni

            n = cf2hi((t_vi_param *)d->d, d->size, cmd, len);
            len -= n; cmd += n;
        break;
    }

    //len -= snprintf(cmd, len, "\n\r");
    return (len > 0) ? 1 : 0; //0 pokud jsme to preplnili
}

int vi_ex_hid::conv2dt(t_vi_exch_dgram *d, const char *cmd, int len){

    int i = 0; //najdem odpovidajici povel
    for(; vi_exch_cmd[i].type; i++)
        if(0 == memcmp(cmd, vi_exch_cmd[i].cmd, strlen(vi_exch_cmd[i].cmd)))
            break;

    if((d == NULL) || (0 == vi_exch_cmd[i].type))
        return 0;  //paket neni

    d->type = vi_exch_cmd[i].type;

    //rozlicovani parametry podle konkretniho typu
    switch(vi_exch_cmd[i].type){

        //parametry za kterymi ocekavame string
        case VI_I:     //potvzovana data - pouzitelne jako echo
        case VI_I_SETUP_GET:   //zapis nastaveni a vycteni nastaveni
        case VI_I_SETUP_SET:

            d->size = cf2hi((t_vi_param *)d->d, len, (char *)cmd, strlen(cmd));
        break;
        default:

            d->size = 0;
        break;
    }

    return 1;
}
