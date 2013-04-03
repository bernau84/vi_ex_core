#ifndef CIRCBUFFER_H
#define CIRCBUFFER_H

#include <cstdio>

#define u8	unsigned char
#define u16	unsigned short
#define u32	unsigned int
#define s8	signed char
#define s16	signed short
#define s32	signed int

template <typename T> class circbuffer
{
public:
      T *buf; //pointer na kruhovy buffer
      u32 size;  //velikost bufferu
      u32 read_mark;    //pozice od ktere muzu cist
      u32 write_mark;   //pozice od ktere muzu zapisovat
      s32 overflow;      //preteceni bufferu

public:
      u32 rdAvail();  //dostupne misto k vycteni
      u32 wrAvail();    //dostupne misto k zapisu

      void get(s32 offset, T *mem, u32 num);  //od aktualniho rd marku + index; vycte bez posuvu marku
      void set(s32 offset, T *mem, u32 num);  //od aktualniho wd marku + index; zapise bez posuvu marku

      int read(T *mem, u32 num);  //s posuvem rd marku
      int write(T *mem, u32 num);  //s posuvem wr marku

      circbuffer(T *buf = 0, u32 size = 0);
      circbuffer(circbuffer &t);
};

template <typename T> circbuffer<T>::circbuffer(T *_buf, u32 _size):
    buf(_buf), size(_size)
{
    fprintf(stderr, "circbuffer: %d\n", size);
    write_mark = 0;
    read_mark = 0;
    overflow = -1;  //jako ze neni ani nebezpeci preteceni
}

template <typename T> circbuffer<T>::circbuffer(circbuffer &t)
{
    buf = t.buf;
    size = t.size;
    write_mark = t.write_mark;
    read_mark = t.read_mark;
    overflow = t.overflow;
}

template <typename T> u32 circbuffer<T>::rdAvail(){

  s32 tmp = (s32)(write_mark - read_mark);
  if(((tmp == 0)&&(overflow >= 0)) || (tmp < 0))
      return(tmp + size);   //volny prostor je prelozeny, nebo doslo k preteceni

  return(tmp);   //prostor nebyl prelozeny (rd muze != wr i pri overflow)
}

template <typename T> u32 circbuffer<T>::wrAvail(){

  s32 tmp = (s32)(read_mark - write_mark);
  if(overflow < 0){

     if(tmp > 0) return(tmp);
      else return(tmp + size);
  }
  return 0; //mam tu preteceni - nelze zapsat nic; az dokud si app overflow neshodi
}

template <typename T> void circbuffer<T>::get(s32 i, T *mem, u32 num){  //zadne meze se nekontroluji, pouze se chovame kruhove

    i += read_mark;
    while(num && mem){

        *mem = buf[i % size];
        mem += 1; i += 1; num -= 1;
    }
}

template <typename T> void circbuffer<T>::set(s32 i, T *mem, u32 num){  //zadne meze se nekontroluji, pouze se chovame kruhove

    i += write_mark;
    while(num && mem){

        buf[i % size] = *mem;
        mem += 1; i += 1; num -= 1;
    }
}

template <typename T> int circbuffer<T>::read(T *mem, u32 num){

  u32 tmp = rdAvail();
  if(tmp > num) tmp = num;

  if(!mem){  //zahozeni dat

      read_mark = (read_mark + tmp) % size;
      return tmp;
  }

  for(u32 i = 0; i<tmp; i += 1){

    *mem = buf[read_mark];
    mem += 1;

    if((read_mark += 1) >= size)
        read_mark = 0;
  }

  if(tmp)
      overflow = -1;

  return tmp;
}

template <typename T> int circbuffer<T>::write(T *mem, u32 num){

  u32 tmp = wrAvail();
  if(tmp > num) tmp = num;

  for(u32 i = 0; i<tmp; i += 1){

    buf[write_mark] = *mem;
    num -= 1; mem += 1;

    if((write_mark += 1) >= size)
        write_mark = 0;
  }

  if(read_mark == write_mark)
      overflow += num;  //pokud se neco neveslo

  return tmp;
}

#endif // CIRCBUFFER_H
