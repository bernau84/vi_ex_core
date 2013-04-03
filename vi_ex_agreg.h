#ifndef vi_EX_AGREG_H
#define vi_EX_AGREG_H

#include "vi_ex_io.h"

#define vi_CLI_MAX    10

/*
 * slucuje vsechna spojeni s klienty na strane serveru
 * klienti jsou identifikovany pomoci indexu pridani do seznamu
 * implementuje vsechny metody jako io trida
 * otazka jestli je neco takoveho treba
 */

class vi_ex_agreg
{
private:
    vi_ex_io *cli[vi_CLI_MAX+1];  //posleni je stopka

public:
    int add(vi_ex_io *_cli){  //>= 0 index, < 0 chyba

        for(int i=0; i < vi_CLI_MAX; i++)
            if(NULL == cli[i]){
                cli[i] = _cli;
                return i;
            }
        return -1;
    }

    int count(){

        for(int i=0; i < vi_CLI_MAX; i++)
            if(cli[i] == NULL) return i;
        return vi_CLI_MAX;
    }

    int free(){

        for(int i=0; i < vi_CLI_MAX; i++)
            if(cli[i] == NULL) return vi_CLI_MAX - i;
        return 0;
    }    

    t_vi_exch_dgram *preparetx(u8 i, t_vi_exch_dgram *d, t_vi_exch_type t, u32 sess_id = 0, u32 size = vi_HLEN()){

        if(cli[i]) return cli[i]->preparetx(d, sess_id, size);
        return NULL;
    }

    t_vi_exch_dgram *preparerx(u8 i, t_vi_exch_dgram *d, u32 size = vi_HLEN()){

        if(cli[i]) return cli[i]->preparerx(d, size);
        return NULL;
    }

    vi_ex_io::t_vi_io_r request(u8 i, t_vi_exch_dgram *d, int timeout = vi_IO_WAITMS_T){

        if(cli[i]) return cli[i]->submit(d, timeout);
        return vi_ex_io::vi_IO_FAIL;
    }

    vi_ex_io::t_vi_io_r receive(u8 i, t_vi_exch_dgram *d, int timeout = vi_IO_WAITMS_T){

        if(cli[i]) return cli[i]->receive(d, timeout);
        return vi_ex_io::vi_IO_FAIL;
    }

    u32 ispending(u8 i){

        if(cli[i]) return cli[i]->ispending();
        return 0;
    }

    vi_ex_agreg(){

        for(int i=0; i < vi_CLI_MAX+1; i++)  //+1 jako stopka
            cli[i] = NULL;
    }

    virtual ~vi_ex_agreg(){

        // for(int i=0; i < vi_CLI_MAX; i++)  //mazat kdyz jsme jen nealokovali?!
        //     if(cli[i] != NULL){ 
        //         delete cli[i]; 
        //         cli[i] = 0; 
        //     }
    }
};

#endif // vi_EX_AGREG_H
