/*=====================================================================
arrdeladaptor.h
---------------
File created by ClassTemplate on Mon Sep 09 22:20:32 2002
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __ARRDELADAPTOR_H_666_
#define __ARRDELADAPTOR_H_666_



/*
	From http://www.gotw.ca/gotw/042.htm

  template <class T>
    class ArrDelAdapter {
    public:
      ArrDelAdapter( T* p ) : p_(p) { }
     ~ArrDelAdapter() { delete[] p_; }
      // operators like "->" "T*" and other helpers
    private:
      T* p_;
    };*/

/*=====================================================================
ArrDelAdaptor
-------------

=====================================================================*/
template <class T>
class ArrDelAdaptor
{
public:
	/*=====================================================================
	ArrDelAdaptor
	-------------
	
	=====================================================================*/
	ArrDelAdaptor(T* p): p(p_) {}

	~ArrDelAdaptor(){ delete[] p; }

	T* get(){ return p; }

private:
	T* p;
};



#endif //__ARRDELADAPTOR_H_666_




