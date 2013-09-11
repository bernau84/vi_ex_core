#ifndef VI_EX_IO_HR_H
#define VI_EX_IO_HR_H

#include "vi_ex_io.h"
#include "vi_ex_def.h"
#include "vi_ex_par.h"

#define VIEX_HID_TR(NAME, VAL)
#define VIEX_HID_SP 256

/*!
    \class vi_ex_hid
    \brief binary viex protocol with alternative text (human readable) interface
*/
class vi_ex_hid : public vi_ex_io {

private:
    virtual int cf2hi(const u8 *p, int n, char *cmd, int len);    /*!< settings -> text */
    virtual int cf2dt(u8 *p, int n, const char *cmd, int len);    /*!< text -> settings */

    virtual int conv2hi(const t_vi_exch_dgram *d, char *cmd, int len);  /*!< viex datagram -> text */
    virtual int conv2dt(t_vi_exch_dgram *d, const char *cmd, int len);  /*!< text -> viex datagram */

public:
    vi_ex_io::t_vi_io_r submit(const char *cmd, int len = VIEX_HID_SP, int timeout = VI_IO_WAITMS_T){

        u8 space[VIEX_HID_SP]; //space for text
        t_vi_exch_dgram *d = vi_ex_io::preparetx(space, VI_ANY, sizeof(space) - sizeof(t_vi_exch_dgram));
        conv2dt(d, cmd, (len) ? len : strlen(cmd));

        char info[128];
        snprintf(info, sizeof(info), "tx cmd \"%s\"\r\n", cmd);
        debug(info);  //debug printout

        return vi_ex_io::submit(d, timeout);
    }

    vi_ex_io::t_vi_io_r receive(char *cmd, int len, int timeout = VI_IO_WAITMS_T){

        u8 space[VIEX_HID_SP];  //space for data
        t_vi_exch_dgram *d = vi_ex_io::preparerx(space, VI_ANY, sizeof(space) - sizeof(t_vi_exch_dgram));
        vi_ex_io::t_vi_io_r ret = vi_ex_io::receive(d, timeout);
        conv2hi(d, cmd, len);

        char info[128];
        snprintf(info, sizeof(info), "rx cmd \"%s\"\r\n", cmd);
        debug(info);  //debug printout

        return ret;
    }

    vi_ex_hid():vi_ex_io(){;}
    virtual ~vi_ex_hid(){;}
};

#endif // VI_EX_IO_HR_H


