
#include "vi_ex_hid.h"

#define VIEX_T_2_STR(DEF)   #DEF

//potrebuju podle typu vytvorit formatovaci retezce a user friendy pojemnovani typu
//pak pouzijem patricnou sablonu pro vycteni hodnot ze streamu
#define VIEX_T_READNEXT(TYPE, FRMSTR)\
    v = new TYPE[n];\
    fv = FRMSTR;\
    ft = VIEX_T_2_STR(TYPE);\
    szof = sizeof(TYPE);\
    io.readnext<TYPE>(&name, (TYPE *)v, n, &f);\

//prevod celeho seznamu parametru na text
//sytantax je: nazev/'def|enum|min|max'('u8|char|double|bool|u64')=1,2,3,4,5,6,......
int vi_ex_hid::cf2hi(t_vi_param *p, u32 n, char *cmd, int len){

    char name[VIEX_PARAM_NAME_SIZE];
    t_vi_param_flags f;
    t_vi_param_type t;

    int m = 0, szof = 0;
    void *v = NULL;
    char *ft = "", *fv = "";

    t_vi_param_stream io(p, n); //inicializace interatoru
    while(((t = io.isvalid(&n)) != VI_TYPE_UNKNOWN) && (m < len)){  //pres vsechny parametry

        switch(t){ //podle datoveho typu paketu vyctem data

            case VI_TYPE_BYTE:      VIEX_T_READNEXT(u8, "%d");      break;
            case VI_TYPE_INTEGER:   VIEX_T_READNEXT(int, "%d");     break;
            case VI_TYPE_BOOLEAN:   VIEX_T_READNEXT(bool, "%d");    break;
            case VI_TYPE_INTEGER64: VIEX_T_READNEXT(u64, "%lld");   break;
            case VI_TYPE_FLOAT:     VIEX_T_READNEXT(double, "%g");  break;
            case VI_TYPE_CHAR:

                n += 1; //na ukoncovaci 0
                VIEX_T_READNEXT(char, "%s");
                t[n] = 0; n = 1;  //bypass
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

        if(v) delete(v);
        v = NULL;
    }
}


//nactu ze textu pole hodnot; az je cele tak to zapsu do serializovane konfigurace
#define VIEX_T_WRITENEXT(TYPE, FRMSTR)\
    if(!strcmp(type, VIEX_T_2_STR(TYPE))){\
        TYPE v[n];\
        for(int i=0; (1 == sscanf(cmd, FRMSTR, &v[i])) && (i < n); i++)\
            if((NULL == (cmd = strchr(cmd, ','))) || (cmd >= end))\
                break;\
    return io.append<TYPE>(&name, v, n)*sizeof(TYPE)+sizeof(t_vi_param)-1;\
    }\

//z textoveho vstupu vycte nastaneni parametru - omezeni na jeden!
//protoze v tomto okamziku nezname typ polozky musi byt uveden ve stringu v zavorce
//mozno v potomkovi kde uz je nam to jasne pretizit
//v n je prostor vyhrazeny na data
int vi_ex_hid::cf2dt(t_vi_param *p, int n, char *cmd, int len){  //v p musi byt predvyplneno na jaky typ prevadet!

    char name[VIEX_PARAM_NAME_SIZE];
    char c, type[16], *end = cmd+len;

    t_vi_param_stream io((u8 *)p, n); //inicializace interatoru
    if(3 != sscanf(cmd, "%s(%s)=%c", name, type, &c)){

        char *dlm = (cmd = strchr(cmd, '=')); //dem na rovnitko
        n = 0; while((dlm = strchr(dlm, ',')) != NULL) n += 1; //kolik je tam prvku?

        VIEX_T_WRITENEXT(u8, "%d");
        VIEX_T_WRITENEXT(u64, "%lld");
        VIEX_T_WRITENEXT(int, "%d");
        VIEX_T_WRITENEXT(bool, "%d");
        VIEX_T_WRITENEXT(double, "%g");
        n = strlen(cmd); VIEX_T_WRITENEXT(char, "%s"); //+bypass na string
    }

    return 0;  //pokud to doslo az sem pak je tam chyba
}


int vi_ex_hid::conv2hi(t_vi_exch_dgram *d, char *cmd, u32 len){

    int i = 0; //najdem odpovidajici povel
    for(; vi_exch_cmd[i].type; i++)
        if(vi_exch_cmd[i].type == d->h.type)
            break;

    int n = snprintf(cmd, len, "%s", vi_exch_cmd[i].cmd); //preklad do hi zacneme povelem
    len -= n; cmd += n;

    //rozlicovani parametry podle konkretniho typu
    u8 t = d->h.type & VI_ACK;
    switch(t){

        case VI_DISCOVERY:	//vyzva a rekce na pritomnost zarizeni
            //vyzvracet prilohu v ascii ven

            n = snprintf(cmd, len, " ");
            len -= n; cmd += n;

            for(int i=0; i<d->h.size; i++){

                if(d->d[i] > 32) n = snprintf(cmd, len, "%c", d->d[i]);
                    else n = snprintf(cmd, len, "\\x%x", d->d[i]); //nonascii prevadime na hexa
                len -= n; cmd += n;
            }
        break;

        case VI_I:      //potvzovana data - pouzitelne jako echo
        case VI_BULK: 	//nestrukturovana/nepotvrzovana data - jina

            for(int i=0; i<d->h.size; i++){

                n = snprintf(cmd, len, " %x(%c)", d->d[i]); //vypisem to hexa; stejne by tam nemelo nic byt
                len -= n; cmd += n;
            }
        break;

        case VI_I_CAP:
        case VI_I_SETUP_GET:
        case VI_I_SETUP_SET:   //zapis nastaveni a vycteni nastaveni

            n = cf2hi((t_vi_param *)d->d, d->h.size, cmd, len);
            len -= n; cmd += n;
        break;

        case VI_I_SNAP:        //zadost o obrazek, vycteni obrazku

            if(d->h.size){

                n = snprintf(cmd, len, " %dx%d %db",
                      d->bmp.bmiHeader.biWidth,
                      d->bmp.bmiHeader.biHeight,
                      d->bmp.bmiHeader.biBitCount);

                len -= n; cmd += n;
            }
        break;

        case VI_I_FLASH:       //nastaveni rezimu prisvetleni
        case VI_I_SHUTTER:     //nastaveni rezimu uzaverky (clony)

            if(d->h.size){

                n = snprintf(cmd, len, " %d", (u8)d->d[0]);
                len -= n; cmd += n;
            }
        break;
    }

    int n = snprintf(cmd, len, "\r\n", vi_exch_cmd[i].cmd); //preklad do hi zacneme povelem
    len -= n; cmd += n;
    return (len > 0) ? 1 : 0; //0 pokud jsme to preplnili
}

int vi_ex_hid::conv2dt(char *cmd, t_vi_exch_dgram *d, u32 len){

    int td = 0;
    char fstr[32]; //scanovaci retezes

    int i = 0; //najdem odpovidajici povel
    for(; vi_exch_cmd[i].type; i++)
        if(0 == memcmp(cmd, vi_exch_cmd[i].cmd, strlen(vi_exch_cmd[i].cmd)))
            break;

    if((d == NULL) || (0 == vi_exch_cmd[i].type))
        return 0;  //paket neni

    d->h.type = vi_exch_cmd[i].type;

    //rozlicovani parametry podle konkretniho typu
    switch(vi_exch_cmd[i].type){

        //parametry za kterymi ocekavame string
        case VI_I:     //potvzovana data - pouzitelne jako echo
        case VI_I_SETUP_GET:   //zapis nastaveni a vycteni nastaveni
        case VI_I_SETUP_SET:

            d->size = cf2hi((t_vi_param *)d->d, len, cmd, strlen(cmd));
        break;

        //parametry za kterymi cekame cislo
        case VI_I_FLASH:       //nastaveni rezimu prisvetleni
        case VI_I_SHUTTER:

            snprintf(fstr, sizeof(fstr), "\%*s \%d"); //za paramterem cekame cislo
            d->size = (1 == sscanf(cmd, fstr, &td)) ? 1 : 0;  //oscanujem a nastavime skutecnou delku
            d->d[0] = (u8)td;
        break;

        default:
            d->size = 0;
        break;
    }

    return 1;
}
