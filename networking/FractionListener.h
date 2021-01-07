/*=====================================================================
FractionListener.h
------------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


/*=====================================================================
FractionListener
----------------

=====================================================================*/
class FractionListener
{
public:
	virtual ~FractionListener() {}

	virtual void setFraction(float fraction) = 0;
};
