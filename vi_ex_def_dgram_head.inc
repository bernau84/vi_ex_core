//definition of header items, define viex packet like this eliminate problems with different behaviour
//of packed atributute and length of enum through platforms

VI_H_ITEM( marker,  t_vi_io_id,     VI_MARKER_SZ )    //begin of viex datagram; in bus unique for every p2p connection
VI_H_ITEM( crc,     u32,            VI_CRC_SZ )     //crc from head and first 64B of payload
VI_H_ITEM( sess_id, u32,            sizeof(u32) )       
VI_H_ITEM( type,    t_vi_exch_type, VI_ENUMER_SZ )    //size of enumerator defined globaly extra for viex  
VI_H_ITEM( size,    u32,            sizeof(u32) )       
