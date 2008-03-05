//Copyright (c) 2006-2008 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef UUID_274DA366004E11DCB1DDFE2E56D89593
#define UUID_274DA366004E11DCB1DDFE2E56D89593

#include <boost/exception/detail/counted_base.hpp>
#include <boost/intrusive_ptr.hpp>
#include <typeinfo>

namespace
boost
	{
	template <class T>
	class shared_ptr;

	namespace
	exception_detail
		{
		class error_info_base;

		struct
		error_info_container:
			public exception_detail::counted_base
			{
			virtual char const * what( std::type_info const & ) const = 0;
			virtual shared_ptr<error_info_base const> get( std::type_info const & ) const = 0;
			virtual void set( shared_ptr<error_info_base const> const & ) = 0;
			};
		}

	template <class Tag,class T>
	class error_info;

	template <class E,class Tag,class T>
	E const & operator<<( E const &, error_info<Tag,T> const & );

	template <class ErrorInfo,class E>
	shared_ptr<typename ErrorInfo::value_type const> get_error_info( E const & );

	class
	exception
		{
		public:

		virtual ~exception() throw()=0;
		virtual char const * what() const throw();

		private:

		shared_ptr<exception_detail::error_info_base const> get( std::type_info const & ) const;
		void set( shared_ptr<exception_detail::error_info_base const> const & ) const;

		template <class E,class Tag,class T>
		friend E const & operator<<( E const &, error_info<Tag,T> const & );

		template <class ErrorInfo,class E>
		friend shared_ptr<typename ErrorInfo::value_type const> get_error_info( E const & );

		intrusive_ptr<exception_detail::error_info_container> mutable data_;
		};

	inline
	exception::
	~exception() throw()
		{
		}

	inline
	char const *
	exception::
	what() const throw()
		{
		if( data_ )
			try
				{
				char const * w = data_->what(typeid(*this));
				BOOST_ASSERT(0!=w);
				return w;
				}
			catch(...)
				{
				}
		return typeid(*this).name();
		}
	}

#endif
