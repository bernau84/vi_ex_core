#ifndef VI_EX_TER_H
#define VI_EX_TER_H

#include <fstream>
#include <iostream>
#include <ios>

#include "vi_ex_cell.h"

/*!
    \class vi_ex_cell
    \brief high level function for autoconnect & human terminal

    merge of vi_ex_cell & hi_ex_hid
*/

class vi_ex_ter : public vi_ex_hid, public vi_ex_cell
{
private:
    std::string cmd;
    std::istream *hi;  /*!< human terminal enter for text commands*/

protected:
    /*! \brief high level callback (only for relevant packet, omits intrenal control packet) */
    virtual void callback_rx_new(t_vi_exch_dgram *){;}

public:
    /*! \brief text command
        \return text response
    */
    std::string command(const std::string &ord){

        char rcv[VIEX_HID_SP];
        if(vi_ex_hid::submit(ord.c_str()) == vi_ex_io::VI_IO_OK)  //translate to binary
            if(vi_ex_hid::receive(rcv, VIEX_HID_SP) == vi_ex_io::VI_IO_OK) //wait for reply & translate to text
                return std::string(rcv); //return

        return std::string();  //fail
    }

    /*! \brief command line refresh & execution
    */
    void refreshcmdln(){

        if(NULL == hi) return;

        hi->sync();

        int c;
        while(EOF != (c = hi->get())){

            if((c != '\n') && (c != '\r')){

              cmd.push_back(c);
            } else {

              vi_ex_ter::command(cmd); //execute
              cmd.clear();
            }
        }
    }

    /*! \brief contructor
    */
    vi_ex_ter(p_vi_io_mn _name, std::istream *_hi, std::ostream *_p_trace = &std::cout):
      vi_ex_cell(_name, _p_trace), hi(_hi)
    {
    }

    /*! \brief destructor
    */
    virtual ~vi_ex_ter(void)
    {
    }
};

#endif // VI_EX_TER_H
