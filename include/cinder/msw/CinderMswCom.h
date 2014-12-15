/*
Copyright (c) 2014, The Barbarian Group
Copyright (c) 2014, The Chromium Authors
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that
the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this list of conditions and
the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
the following disclaimer in the documentation and/or other materials provided with the distribution.
* Neither the name of Google Inc. nor the names of its contributors may be used to endorse or promote
products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "cinder/Cinder.h"
#include "cinder/Stream.h"

#include <string>
#include <windows.h>
#include <Objidl.h>
#undef min
#undef max

// The DESTRUCTOR_REF_COUNT prevents double destruction of reference counted objects.
// See: http://blogs.msdn.com/b/oldnewthing/archive/2005/09/28/474855.aspx
#define DESTRUCTOR_REF_COUNT 1337

// The COMPILE_ASSERT macro can be used to verify that a compile time
// expression is true. For example, you could use it to verify the
// size of a static array:
//
//   COMPILE_ASSERT(arraysize(content_type_names) == CONTENT_NUM_TYPES,
//                  content_type_names_incorrect_size);
//
// or to make sure a struct is smaller than a certain size:
//
//   COMPILE_ASSERT(sizeof(foo) < 128, foo_too_large);
//
// The second argument to the macro is the name of the variable. If
// the expression is false, most compilers will issue a warning/error
// containing the name of the variable.

#undef COMPILE_ASSERT
#define COMPILE_ASSERT(expr, msg) static_assert(expr, #msg)

namespace cinder {
namespace msw {

//! Initializes COM on this thread. Uses Boost's thread local storage to prevent multiple initializations per thread
void initializeCom( DWORD params = COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE );

//! Releases a COM pointer if the pointer is not NULL, and sets the pointer to NULL.
template <class T> inline void SafeRelease( T*& p )
{
	if( p ) {
#if _DEBUG
		ULONG rc = GetRefCount( p );
#endif
		p->Release();
		p = NULL;
	}
}

//! Deletes a pointer allocated with new.
template <class T> inline void SafeDelete( T*& p )
{
	if( p ) {
		delete p;
		p = NULL;
	}
}

//! A free function designed to interact with makeComShared, calls Release() on a com-managed object
void ComDelete( void *p );

//! Functor version that calls Release() on a com-managed object
struct ComDeleter {
	template <typename T>
	void operator()( T* ptr ) { if( ptr ) ptr->Release(); }
};

//! Creates a shared_ptr whose deleter will properly decrement the reference count of a COM object
template<typename T>
inline std::shared_ptr<T> makeComShared( T *p ) { return std::shared_ptr<T>( p, &ComDelete ); }

//! Creates a unique_ptr whose deleter will properly decrement the reference count of a COM object
template<typename T>
inline std::unique_ptr<T, ComDeleter> makeComUnique( T *p ) { return std::unique_ptr<T, ComDeleter>( p ); }

//! Wraps a cinder::OStream with a COM ::IStream
class ComOStream : public ::IStream {
public:
	ComOStream( cinder::OStreamRef aOStream ) : mOStream( aOStream ), _refcount( 1 ) {}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface( REFIID iid, void ** ppvObject );
	virtual ULONG STDMETHODCALLTYPE AddRef();
	virtual ULONG STDMETHODCALLTYPE Release();

	// ISequentialStream Interface
public:
	virtual HRESULT STDMETHODCALLTYPE Read( void* pv, ULONG cb, ULONG* pcbRead ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Write( void const* pv, ULONG cb, ULONG* pcbWritten );
	// IStream Interface
public:
	virtual HRESULT STDMETHODCALLTYPE SetSize( ULARGE_INTEGER ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE CopyTo( ::IStream*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER* ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Commit( DWORD ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Revert() { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE LockRegion( ULARGE_INTEGER, ULARGE_INTEGER, DWORD ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE UnlockRegion( ULARGE_INTEGER, ULARGE_INTEGER, DWORD ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Clone( IStream ** ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Seek( LARGE_INTEGER liDistanceToMove, DWORD dwOrigin, ULARGE_INTEGER* lpNewFilePointer );
	virtual HRESULT STDMETHODCALLTYPE Stat( STATSTG* pStatstg, DWORD grfStatFlag ) { return E_NOTIMPL; }

private:
	cinder::OStreamRef	mOStream;
	LONG			_refcount;
};

//! Wraps a cinder::IStream with a COM ::IStream
class ComIStream : public ::IStream {
public:
	ComIStream( cinder::IStreamRef aIStream ) : mIStream( aIStream ), _refcount( 1 ) {}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface( REFIID iid, void ** ppvObject );
	virtual ULONG STDMETHODCALLTYPE AddRef();
	virtual ULONG STDMETHODCALLTYPE Release();

	// ISequentialStream Interface
public:
	virtual HRESULT STDMETHODCALLTYPE Read( void* pv, ULONG cb, ULONG* pcbRead );
	virtual HRESULT STDMETHODCALLTYPE Write( void const* pv, ULONG cb, ULONG* pcbWritten ) { return E_NOTIMPL; }
	// IStream Interface
public:
	virtual HRESULT STDMETHODCALLTYPE SetSize( ULARGE_INTEGER ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE CopyTo( ::IStream*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER* ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Commit( DWORD ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Revert() { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE LockRegion( ULARGE_INTEGER, ULARGE_INTEGER, DWORD ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE UnlockRegion( ULARGE_INTEGER, ULARGE_INTEGER, DWORD ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Clone( IStream ** ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Seek( LARGE_INTEGER liDistanceToMove, DWORD dwOrigin, ULARGE_INTEGER* lpNewFilePointer );
	virtual HRESULT STDMETHODCALLTYPE Stat( STATSTG* pStatstg, DWORD grfStatFlag );

private:
	cinder::IStreamRef	mIStream;
	LONG			_refcount;
};

// A smart pointer class for reference counted objects.  Use this class instead
// of calling AddRef and Release manually on a reference counted object to
// avoid common memory leaks caused by forgetting to Release an object
// reference.  Sample usage:
//
//   class MyFoo : public RefCounted<MyFoo> {
//    ...
//   };
//
//   void some_function() {
//     ScopedPtr<MyFoo> foo = new MyFoo();
//     foo->Method(param);
//     // |foo| is released when this function returns
//   }
//
//   void some_other_function() {
//     ScopedPtr<MyFoo> foo = new MyFoo();
//     ...
//     foo = nullptr;  // explicitly releases |foo|
//     ...
//     if (foo)
//       foo->Method(param);
//   }
//
// The above examples show how ScopedPtr<T> acts like a pointer to T.
// Given two ScopedPtr<T> classes, it is also possible to exchange
// references between the two objects, like so:
//
//   {
//     ScopedPtr<MyFoo> a = new MyFoo();
//     ScopedPtr<MyFoo> b;
//
//     b.swap(a);
//     // now, |b| references the MyFoo object, and |a| references nullptr.
//   }
//
// To make both |a| and |b| in the above example reference the same MyFoo
// object, simply use the assignment operator:
//
//   {
//     ScopedPtr<MyFoo> a = new MyFoo();
//     ScopedPtr<MyFoo> b;
//
//     b = a;
//     // now, |a| and |b| each own a reference to the same MyFoo object.
//   }
//
template <class T>
class ScopedPtr {
public:
	typedef T element_type;

	ScopedPtr() : ptr_( nullptr )
	{
	}

	ScopedPtr( T* p ) : ptr_( p )
	{
		if( ptr_ )
			AddRef( ptr_ );
	}

	ScopedPtr( const ScopedPtr<T>& r ) : ptr_( r.ptr_ )
	{
		if( ptr_ )
			AddRef( ptr_ );
	}

	template <typename U>
	ScopedPtr( const ScopedPtr<U>& r ) : ptr_( r.get() )
	{
		if( ptr_ )
			AddRef( ptr_ );
	}

	~ScopedPtr()
	{
		if( ptr_ )
			Release( ptr_ );
	}

	T* get() const { return ptr_; }

	// Allow ScopedPtr<C> to be used in boolean expression
	// and comparison operations.
	operator T*( ) const { return ptr_; }

	//The assert on operator& usually indicates a bug.  If this is really
	//what is needed, however, take the address of the ptr_ member explicitly.
	T** operator&( ) throw( )
	{
		assert( ptr_ == nullptr );
		return &ptr_;
	}

	T& operator*( ) const
	{
		assert( ptr_ != nullptr );
		return *ptr_;
	}

	T* operator->( ) const
	{
		assert( ptr_ != nullptr );
		return ptr_;
	}

	ScopedPtr<T>& operator=( T* p )
	{
		// AddRef first so that self assignment should work
		if( p )
			AddRef( p );
		T* old_ptr = ptr_;
		ptr_ = p;
		if( old_ptr )
			Release( old_ptr );
		return *this;
	}

	ScopedPtr<T>& operator=( const ScopedPtr<T>& r )
	{
		return *this = r.ptr_;
	}

	template <typename U>
	ScopedPtr<T>& operator=( const ScopedPtr<U>& r )
	{
		return *this = r.get();
	}

	void swap( T** pp )
	{
		T* p = ptr_;
		ptr_ = *pp;
		*pp = p;
	}

	void swap( ScopedPtr<T>& r )
	{
		swap( &r.ptr_ );
	}

protected:
	T* ptr_;

private:
	// Non-inline helpers to allow:
	//     class Opaque;
	//     extern template class ScopedPtr<Opaque>;
	// Otherwise the compiler will complain that Opaque is an incomplete type.
	static void AddRef( T* ptr );
	static void Release( T* ptr );
};

template <typename T>
void ScopedPtr<T>::AddRef( T* ptr )
{
	ptr->AddRef();
}

template <typename T>
void ScopedPtr<T>::Release( T* ptr )
{
	ptr->Release();
}

// Handy utility for creating a ScopedPtr<T> out of a T* explicitly without
// having to retype all the template arguments
template <typename T>
ScopedPtr<T> makeScopedPtr( T* t )
{
	return ScopedPtr<T>( t );
}

//////////////////////////////////////////////////////////////////////////////////////

// A fairly minimalistic smart class for COM interface pointers.
// Uses ScopedPtr for the basic smart pointer functionality
// and adds a few IUnknown specific services.
template <class Interface, const IID* interface_id = &__uuidof( Interface )>
class ScopedComPtr : public ScopedPtr < Interface > {
public:
	// Utility template to prevent users of ScopedComPtr from calling AddRef
	// and/or Release() without going through the ScopedComPtr class.
	class BlockIUnknownMethods : public Interface {
	private:
		STDMETHOD( QueryInterface )( REFIID iid, void** object ) = 0;
		STDMETHOD_( ULONG, AddRef )( ) = 0;
		STDMETHOD_( ULONG, Release )( ) = 0;
	};

	typedef ScopedPtr<Interface> ParentClass;

	ScopedComPtr()
	{
	}

	explicit ScopedComPtr( Interface* p ) : ParentClass( p )
	{
	}

	ScopedComPtr( const ScopedComPtr<Interface, interface_id>& p )
		: ParentClass( p )
	{
	}

	~ScopedComPtr()
	{
		// We don't want the smart pointer class to be bigger than the pointer
		// it wraps.
		COMPILE_ASSERT( sizeof( ScopedComPtr<Interface, interface_id> ) ==
						sizeof( Interface* ), ScopedComPtrSize );
	}

	// Explicit Release() of the held object.  Useful for reuse of the
	// ScopedComPtr instance.
	// Note that this function equates to IUnknown::Release and should not
	// be confused with e.g. scoped_ptr::release().
	void Release()
	{
		if( ptr_ != nullptr ) {
			ptr_->Release();
			ptr_ = nullptr;
		}
	}

	// Sets the internal pointer to nullptr and returns the held object without
	// releasing the reference.
	Interface* Detach()
	{
		Interface* p = ptr_;
		ptr_ = nullptr;
		return p;
	}

	// Accepts an interface pointer that has already been addref-ed.
	void Attach( Interface* p )
	{
		assert( !ptr_ );
		ptr_ = p;
	}

	// Retrieves the pointer address.
	// Used to receive object pointers as out arguments (and take ownership).
	// The function asserts on the current value being nullptr.
	// Usage: Foo(p.Receive());
	Interface** Receive()
	{
		assert( !ptr_ ) << "Object leak. Pointer must be nullptr";
		return &ptr_;
	}

	// A convenience for whenever a void pointer is needed as an out argument.
	void** ReceiveVoid()
	{
		return reinterpret_cast<void**>( Receive() );
	}

	template <class Query>
	HRESULT QueryInterface( Query** p )
	{
		assert( p != nullptr );
		assert( ptr_ != nullptr );
		// IUnknown already has a template version of QueryInterface
		// so the iid parameter is implicit here. The only thing this
		// function adds are the asserts.
		return ptr_->QueryInterface( p );
	}

	// QI for times when the IID is not associated with the type.
	HRESULT QueryInterface( const IID& iid, void** obj )
	{
		assert( obj != nullptr );
		assert( ptr_ != nullptr );
		return ptr_->QueryInterface( iid, obj );
	}

	// Queries |other| for the interface this object wraps and returns the
	// error code from the other->QueryInterface operation.
	HRESULT QueryFrom( IUnknown* object )
	{
		assert( object != nullptr );
		return object->QueryInterface( Receive() );
	}

	// Convenience wrapper around CoCreateInstance
	HRESULT CreateInstance( const CLSID& clsid, IUnknown* outer = nullptr,
							DWORD context = CLSCTX_ALL )
	{
		assert( !ptr_ );
		HRESULT hr = ::CoCreateInstance( clsid, outer, context, *interface_id,
										 reinterpret_cast<void**>( &ptr_ ) );
		return hr;
	}

	// Checks if the identity of |other| and this object is the same.
	bool IsSameObject( IUnknown* other )
	{
		if( !other && !ptr_ )
			return true;

		if( !other || !ptr_ )
			return false;

		ScopedComPtr<IUnknown> my_identity;
		QueryInterface( my_identity.Receive() );

		ScopedComPtr<IUnknown> other_identity;
		other->QueryInterface( other_identity.Receive() );

		return static_cast<IUnknown*>( my_identity ) ==
			static_cast<IUnknown*>( other_identity );
	}

	//! Returns the current reference count. Use for debugging purposes only.
	ULONG GetRefCount()
	{
		if( !ptr_ )
			return 0;

		ULONG rc = ptr_->AddRef();
		ptr_->Release();

		return (rc - 1);
	}

	// Provides direct access to the interface.
	// Here we use a well known trick to make sure we block access to
	// IUnknown methods so that something bad like this doesn't happen:
	//    ScopedComPtr<IUnknown> p(Foo());
	//    p->Release();
	//    ... later the destructor runs, which will Release() again.
	// and to get the benefit of the asserts we add to QueryInterface.
	// There's still a way to call these methods if you absolutely must
	// by statically casting the ScopedComPtr instance to the wrapped interface
	// and then making the call... but generally that shouldn't be necessary.
	BlockIUnknownMethods* operator->( ) const
	{
		assert( ptr_ != nullptr );
		return reinterpret_cast<BlockIUnknownMethods*>( ptr_ );
	}

	// Pull in operator=() from the parent class.
	using ScopedPtr<Interface>::operator=;

	// Pull in operator&() from the parent class.
	using ScopedPtr<Interface>::operator&;

	// static methods

	static const IID& iid()
	{
		return *interface_id;
	}
};

/*
//! You can use this when implementing IUnknown or any object that uses reference counting.
class RefCountedObject {
protected:
volatile long   mRefCount;

public:
RefCountedObject() : mRefCount( 1 ) {}
virtual ~RefCountedObject() { assert( mRefCount == 0 ); }

ULONG AddRef()
{
return InterlockedIncrement( &mRefCount );
}

ULONG Release()
{
assert( mRefCount > 0 );

ULONG uCount = InterlockedDecrement( &mRefCount );
if( uCount == 0 ) {
mRefCount = DESTRUCTOR_REF_COUNT;
delete this;
}

return uCount;
}
};
*/

template <class T>
void CopyComPtr( T*& dest, T* src )
{
	if( dest ) {
		dest->Release();
	}
	dest = src;
	if( dest ) {
		dest->AddRef();
	}
}

template <class T1, class T2>
bool AreComObjectsEqual( T1 *p1, T2 *p2 )
{
	if( p1 == NULL && p2 == NULL ) {
		// Both are NULL
		return true;
	}
	else if( p1 == NULL || p2 == NULL ) {
		// One is NULL and one is not
		return false;
	}
	else {
		// Both are not NULL. Compare IUnknowns.
		ScopedComPtr<IUnknown> pUnk1, pUnk2;
		if( SUCCEEDED( p1->QueryInterface( IID_IUnknown, (void**) &pUnk1 ) ) ) {
			if( SUCCEEDED( p2->QueryInterface( IID_IUnknown, (void**) &pUnk2 ) ) ) {
				return ( pUnk1 == pUnk2 );
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////

template<class T, const IID* piid = NULL>
class CComPtr {
public:
	CComPtr()
	{
		m_Ptr = NULL;
	}

	CComPtr( T* lPtr )
	{
		m_Ptr = NULL;

		if( lPtr != NULL ) {
			m_Ptr = lPtr;
			m_Ptr->AddRef();
		}
	}

	CComPtr( const CComPtr<T, piid>& RefComPtr )
	{
		m_Ptr = NULL;
		m_Ptr = (T*) RefComPtr;

		if( m_Ptr ) {
			m_Ptr->AddRef();
		}
	}

	CComPtr( IUnknown* pIUnknown, IID iid )
	{
		m_Ptr = NULL;

		if( pIUnknown != NULL ) {
			pIUnknown->QueryInterface( iid, (void**) &m_Ptr );
		}
	}

	~CComPtr()
	{
		if( m_Ptr ) {
			m_Ptr->Release();
			m_Ptr = NULL;
		}
	}

public:
	operator T*() const
	{
		//assert( m_Ptr != NULL );
		return m_Ptr;
	}

	T& operator*() const
	{
		//assert( m_Ptr != NULL );
		return *m_Ptr;
	}

	T** operator&()
	{
		//assert( m_Ptr != NULL );
		return &m_Ptr;
	}

	T* operator->() const
	{
		assert( m_Ptr != NULL );
		return m_Ptr;
	}

	T* operator=( T* lPtr )
	{
		assert( lPtr != NULL );
		//if( IsEqualObject( lPtr ) ) {
		//	return m_Ptr;
		//}

		if( m_Ptr == lPtr ) {
			return m_Ptr;
		}

		if( m_Ptr ) {
			m_Ptr->Release();
		}

		lPtr->AddRef();
		m_Ptr = lPtr;
		return m_Ptr;
	}

	T* operator=( IUnknown* pIUnknown )
	{
		assert( pIUnknown != NULL );
		assert( piid != NULL );
		pIUnknown->QueryInterface( *piid, (void**) &m_Ptr );
		assert( m_Ptr != NULL );
		return m_Ptr;
	}

	T* operator=( const CComPtr<T, piid>& RefComPtr )
	{
		assert( &RefComPtr != NULL );
		m_Ptr = (T*) RefComPtr;

		if( m_Ptr ) {
			m_Ptr->AddRef();
		}
		return m_Ptr;
	}

	void Attach( T* lPtr )
	{
		if( lPtr ) {
			if( m_Ptr ) {
				m_Ptr->Release();
			}

			m_Ptr = lPtr;
		}
	}

	T* Detach()
	{
		T* lPtr = m_Ptr;
		m_Ptr = NULL;
		return lPtr;
	}

	void Release()
	{
		if( m_Ptr ) {
			m_Ptr->Release();
			m_Ptr = NULL;
		}
	}

	BOOL IsEqualObject( IUnknown* pOther )
	{
		assert( pOther != NULL );
		ScopedComPtr<IUnknown> pUnknown;
		m_Ptr->QueryInterface( IID_IUnknown, (void**) &pUnknown );
		return ( pOther == pUnknown );
	}

private:
	T* m_Ptr;
};

//! Returns the current reference count. Use for debugging purposes only.
template <class T> inline ULONG GetRefCount( T*& p )
{
	if( !p )
		return 0;

	ULONG refCount = p->AddRef();
	p->Release();

	return (ULONG) ( refCount - 1 );
}

//! Returns the current reference count. Use for debugging purposes only.
template <class T> inline ULONG GetRefCount( ScopedComPtr<T>& p )
{
	return p.GetRefCount();
}


} // namespace msw
} // namespace cinder