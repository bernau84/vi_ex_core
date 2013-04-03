#ifndef VI_EX_IO_HR_H
#define VI_EX_IO_HR_H

#include "vi_ex_io.h"
#include "vi_ex_def.h"
#include "vi_ex_par.h"

#define VIEX_HID_TR(NAME, VAL)
#define VIEX_HID_SP 8192

class vi_ex_hid : public vi_ex_io {

private:
    virtual int cf2hi(t_vi_param *p, int n, char *cmd, int len);    //konverze parametru konfigurace
    virtual int cf2dt(t_vi_param *p, int n, char *cmd, int len);

    virtual int conv2hi(t_vi_exch_dgram *d, char *cmd, int len);  //prevod datagramu do lidske reci
    virtual int conv2dt(t_vi_exch_dgram *d, char *cmd, int len);  //prevod do datagramu

    //to do - pretizit virtualni metody pokud bude nutno io
public:
    vi_ex_io::t_vi_io_r submit(char *cmd, int timeout = VI_IO_WAITMS_T){  //stejne jako u vi_ex_io

        if(!io) return VI_IO_FAIL;

        char space[VIEX_HID_SP]; //misto na preklad do hi
        t_vi_exch_dgram *d = (t_vi_exch_dgram *)space;
        conv2dgr(cmd, d, sizeof(space));
        return vi_ex_io::submit(d, timeout);
    }

    vi_ex_io::t_vi_io_r receive(char *cmd, int timeout = VI_IO_WAITMS_T){ //stejne jako u vi_ex_io

        if(!io) return VI_IO_FAIL;

        char space[VIEX_HID_SP];  //misto pro paket
        t_vi_exch_dgram *d = (t_vi_exch_dgram *)space;
        vi_ex_io::preparerx(d, VI_ANY, sizeof(space) - VI_HLEN());  //naformatuje paket na tu velikost co si muzem dovolit
        vi_ex_io::t_vi_io_r ret = vi_ex_io::receive(d, timeout);
        conv2dgr(cmd, d, sizeof(space));
        return ret;
    }

    vi_ex_hid():vi_ex_io(){;}
    virtual ~vi_ex_hid();

};

#endif // VI_EX_IO_HR_H


