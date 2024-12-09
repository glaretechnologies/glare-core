/*=====================================================================
DefaultComObjectImpl.h
----------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


/*=====================================================================
DefaultComObjectImpl
--------------------
Defines the required methods QueryInterface, AddRef and Release
that every COM object needs.
=====================================================================*/
#if defined(_WIN32)

template <class BaseInterface>
struct DefaultComObjectImpl : public BaseInterface
{
public:
	DefaultComObjectImpl(IID interface_IID_)
	:	interface_IID(interface_IID_),
		m_nRefCount(0)
	{}

	virtual ~DefaultComObjectImpl() {}

	// IUnknown methods
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppv)
	{
		// Always set out parameter to NULL, validating it first.
		if (!ppv)
			return E_INVALIDARG;
		*ppv = NULL;
		if(iid == IID_IUnknown || iid == interface_IID)
		{
			// Increment the reference count and return the pointer.
			*ppv = (LPVOID)this;
			AddRef();
			return NOERROR;
		}
		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef()
	{
		// See https://github.com/microsoft/Windows-classic-samples/blob/master/Samples/Win7Samples/multimedia/mediafoundation/MFCaptureD3D/preview.cpp
		return InterlockedIncrement(&m_nRefCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release()
	{
		const LONG uCount = InterlockedDecrement(&m_nRefCount);
		if(uCount == 0)
			delete this;
		return uCount;
	}

	IID interface_IID;
	LONG m_nRefCount;
};

#endif // end if defined(_WIN32)
