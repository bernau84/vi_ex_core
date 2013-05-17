//definition of parameter items

VI_S_ITEM( name,        t_vi_param_mn,      VIEX_PARAM_NAME_SIZE )    //settings parameter name
VI_S_ITEM( type,        t_vi_param_type,    VI_ENUMER_SZ )     //byte, int, bool, float, double, etc...
VI_S_ITEM( def_range,   t_vi_param_flags,   VI_ENUMER_SZ )       //actual or default or enum or range value
VI_S_ITEM( length,      u32,                sizeof(u32) )    //number of element (in case of array parameter)
    
