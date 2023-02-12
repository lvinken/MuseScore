// Copyright (c) 2005-2014 Code Synthesis Tools CC
//
// This program was generated by CodeSynthesis XSD, an XML Schema to
// C++ data binding compiler.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
//
// In addition, as a special exception, Code Synthesis Tools CC gives
// permission to link this program with the Xerces-C++ library (or with
// modified versions of Xerces-C++ that use the same license as Xerces-C++),
// and distribute linked combinations including the two. You must obey
// the GNU General Public License version 2 in all respects for all of
// the code used other than Xerces-C++. If you modify this copy of the
// program, you may extend this exception to your version of the program,
// but you are not obligated to do so. If you do not wish to do so, delete
// this exception statement from your version.
//
// Furthermore, Code Synthesis Tools CC makes a special exception for
// the Free/Libre and Open Source Software (FLOSS) which is described
// in the accompanying FLOSSE file.
//

// Begin prologue.
//
//
// End prologue.

#include <xsd/cxx/pre.hxx>

#include "xlink.hxx"

namespace xlink
{
  // type
  // 

  type::
  type (value v)
  : ::xml_schema::nmtoken (_xsd_type_literals_[v])
  {
  }

  type::
  type (const char* v)
  : ::xml_schema::nmtoken (v)
  {
  }

  type::
  type (const ::std::string& v)
  : ::xml_schema::nmtoken (v)
  {
  }

  type::
  type (const ::xml_schema::nmtoken& v)
  : ::xml_schema::nmtoken (v)
  {
  }

  type::
  type (const type& v,
        ::xml_schema::flags f,
        ::xml_schema::container* c)
  : ::xml_schema::nmtoken (v, f, c)
  {
  }

  type& type::
  operator= (value v)
  {
    static_cast< ::xml_schema::nmtoken& > (*this) = 
    ::xml_schema::nmtoken (_xsd_type_literals_[v]);

    return *this;
  }


  // show
  // 

  show::
  show (value v)
  : ::xml_schema::nmtoken (_xsd_show_literals_[v])
  {
  }

  show::
  show (const char* v)
  : ::xml_schema::nmtoken (v)
  {
  }

  show::
  show (const ::std::string& v)
  : ::xml_schema::nmtoken (v)
  {
  }

  show::
  show (const ::xml_schema::nmtoken& v)
  : ::xml_schema::nmtoken (v)
  {
  }

  show::
  show (const show& v,
        ::xml_schema::flags f,
        ::xml_schema::container* c)
  : ::xml_schema::nmtoken (v, f, c)
  {
  }

  show& show::
  operator= (value v)
  {
    static_cast< ::xml_schema::nmtoken& > (*this) = 
    ::xml_schema::nmtoken (_xsd_show_literals_[v]);

    return *this;
  }


  // actuate
  // 

  actuate::
  actuate (value v)
  : ::xml_schema::nmtoken (_xsd_actuate_literals_[v])
  {
  }

  actuate::
  actuate (const char* v)
  : ::xml_schema::nmtoken (v)
  {
  }

  actuate::
  actuate (const ::std::string& v)
  : ::xml_schema::nmtoken (v)
  {
  }

  actuate::
  actuate (const ::xml_schema::nmtoken& v)
  : ::xml_schema::nmtoken (v)
  {
  }

  actuate::
  actuate (const actuate& v,
           ::xml_schema::flags f,
           ::xml_schema::container* c)
  : ::xml_schema::nmtoken (v, f, c)
  {
  }

  actuate& actuate::
  operator= (value v)
  {
    static_cast< ::xml_schema::nmtoken& > (*this) = 
    ::xml_schema::nmtoken (_xsd_actuate_literals_[v]);

    return *this;
  }
}

#include <xsd/cxx/xml/dom/parsing-source.hxx>

namespace xlink
{
  // type
  //

  type::
  type (const ::xercesc::DOMElement& e,
        ::xml_schema::flags f,
        ::xml_schema::container* c)
  : ::xml_schema::nmtoken (e, f, c)
  {
    _xsd_type_convert ();
  }

  type::
  type (const ::xercesc::DOMAttr& a,
        ::xml_schema::flags f,
        ::xml_schema::container* c)
  : ::xml_schema::nmtoken (a, f, c)
  {
    _xsd_type_convert ();
  }

  type::
  type (const ::std::string& s,
        const ::xercesc::DOMElement* e,
        ::xml_schema::flags f,
        ::xml_schema::container* c)
  : ::xml_schema::nmtoken (s, e, f, c)
  {
    _xsd_type_convert ();
  }

  type* type::
  _clone (::xml_schema::flags f,
          ::xml_schema::container* c) const
  {
    return new class type (*this, f, c);
  }

  type::value type::
  _xsd_type_convert () const
  {
    ::xsd::cxx::tree::enum_comparator< char > c (_xsd_type_literals_);
    const value* i (::std::lower_bound (
                      _xsd_type_indexes_,
                      _xsd_type_indexes_ + 1,
                      *this,
                      c));

    if (i == _xsd_type_indexes_ + 1 || _xsd_type_literals_[*i] != *this)
    {
      throw ::xsd::cxx::tree::unexpected_enumerator < char > (*this);
    }

    return *i;
  }

  const char* const type::
  _xsd_type_literals_[1] =
  {
    "simple"
  };

  const type::value type::
  _xsd_type_indexes_[1] =
  {
    ::xlink::type::simple
  };

  // show
  //

  show::
  show (const ::xercesc::DOMElement& e,
        ::xml_schema::flags f,
        ::xml_schema::container* c)
  : ::xml_schema::nmtoken (e, f, c)
  {
    _xsd_show_convert ();
  }

  show::
  show (const ::xercesc::DOMAttr& a,
        ::xml_schema::flags f,
        ::xml_schema::container* c)
  : ::xml_schema::nmtoken (a, f, c)
  {
    _xsd_show_convert ();
  }

  show::
  show (const ::std::string& s,
        const ::xercesc::DOMElement* e,
        ::xml_schema::flags f,
        ::xml_schema::container* c)
  : ::xml_schema::nmtoken (s, e, f, c)
  {
    _xsd_show_convert ();
  }

  show* show::
  _clone (::xml_schema::flags f,
          ::xml_schema::container* c) const
  {
    return new class show (*this, f, c);
  }

  show::value show::
  _xsd_show_convert () const
  {
    ::xsd::cxx::tree::enum_comparator< char > c (_xsd_show_literals_);
    const value* i (::std::lower_bound (
                      _xsd_show_indexes_,
                      _xsd_show_indexes_ + 5,
                      *this,
                      c));

    if (i == _xsd_show_indexes_ + 5 || _xsd_show_literals_[*i] != *this)
    {
      throw ::xsd::cxx::tree::unexpected_enumerator < char > (*this);
    }

    return *i;
  }

  const char* const show::
  _xsd_show_literals_[5] =
  {
    "new",
    "replace",
    "embed",
    "other",
    "none"
  };

  const show::value show::
  _xsd_show_indexes_[5] =
  {
    ::xlink::show::embed,
    ::xlink::show::new_,
    ::xlink::show::none,
    ::xlink::show::other,
    ::xlink::show::replace
  };

  // actuate
  //

  actuate::
  actuate (const ::xercesc::DOMElement& e,
           ::xml_schema::flags f,
           ::xml_schema::container* c)
  : ::xml_schema::nmtoken (e, f, c)
  {
    _xsd_actuate_convert ();
  }

  actuate::
  actuate (const ::xercesc::DOMAttr& a,
           ::xml_schema::flags f,
           ::xml_schema::container* c)
  : ::xml_schema::nmtoken (a, f, c)
  {
    _xsd_actuate_convert ();
  }

  actuate::
  actuate (const ::std::string& s,
           const ::xercesc::DOMElement* e,
           ::xml_schema::flags f,
           ::xml_schema::container* c)
  : ::xml_schema::nmtoken (s, e, f, c)
  {
    _xsd_actuate_convert ();
  }

  actuate* actuate::
  _clone (::xml_schema::flags f,
          ::xml_schema::container* c) const
  {
    return new class actuate (*this, f, c);
  }

  actuate::value actuate::
  _xsd_actuate_convert () const
  {
    ::xsd::cxx::tree::enum_comparator< char > c (_xsd_actuate_literals_);
    const value* i (::std::lower_bound (
                      _xsd_actuate_indexes_,
                      _xsd_actuate_indexes_ + 4,
                      *this,
                      c));

    if (i == _xsd_actuate_indexes_ + 4 || _xsd_actuate_literals_[*i] != *this)
    {
      throw ::xsd::cxx::tree::unexpected_enumerator < char > (*this);
    }

    return *i;
  }

  const char* const actuate::
  _xsd_actuate_literals_[4] =
  {
    "onRequest",
    "onLoad",
    "other",
    "none"
  };

  const actuate::value actuate::
  _xsd_actuate_indexes_[4] =
  {
    ::xlink::actuate::none,
    ::xlink::actuate::onLoad,
    ::xlink::actuate::onRequest,
    ::xlink::actuate::other
  };
}

#include <istream>
#include <xsd/cxx/xml/sax/std-input-source.hxx>
#include <xsd/cxx/tree/error-handler.hxx>

namespace xlink
{
}

#include <xsd/cxx/post.hxx>

// Begin epilogue.
//
//
// End epilogue.

