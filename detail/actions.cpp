/*=============================================================================
    Copyright (c) 2002 2004 2006 Joel de Guzman
    Copyright (c) 2004 Eric Niebler
    Copyright (c) 2005 Thomas Guest
    http://spirit.sourceforge.net/

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <numeric>
#include <functional>
#include <algorithm>
#include <iterator>
#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/spirit/include/support_unused.hpp>
#include "./quickbook.hpp"
#include "./actions.hpp"
#include "./utils.hpp"
#include "./markups.hpp"
#include "./actions_class.hpp"
#include "../grammars.hpp"
#include "../code_snippet.hpp"

namespace quickbook
{
    char const* quickbook_get_date = "__quickbook_get_date__";
    char const* quickbook_get_time = "__quickbook_get_time__";
    unsigned qbk_major_version = 0;
    unsigned qbk_minor_version = 0;
    unsigned qbk_version_n = 0; // qbk_major_version * 100 + qbk_minor_version

    namespace {
        std::string fully_qualified_id(std::string const& library_id,
            std::string const& qualified_section_id,
            std::string const& section_id)
        {
            std::string id = library_id;
            if(!id.empty() && !qualified_section_id.empty()) id += '.';
            id += qualified_section_id;
            if(!id.empty() && !section_id.empty()) id += '.';
            id += section_id;
            return id;
        }
    }

    // Handles line-breaks (DEPRECATED!!!)
    void break_action::operator()(iterator_range x, unused_type, unused_type) const
    {
        boost::spirit::classic::file_position const pos = x.begin().get_position();
        detail::outwarn(pos.file,pos.line) << "in column:" << pos.column << ", "
            << "[br] and \\n are deprecated" << ".\n";
        phrase << break_mark;
    }

    void error_action::operator()(iterator_range x, unused_type, unused_type) const
    {
        boost::spirit::classic::file_position const pos = x.begin().get_position();
        detail::outerr(pos.file,pos.line)
            << "Syntax Error near column " << pos.column << ".\n";
        ++error_count;
    }

    void phrase_action::operator()(unused_type, unused_type, unused_type) const
    {
        std::string str;
        phrase.swap(str);
        out << pre << str << post;
    }

    void header_action::operator()(iterator_range x, unused_type, unused_type) const
    {
        std::string str;
        phrase.swap(str);

        if (qbk_version_n < 103) // version 1.2 and below
        {
            out << "<anchor id=\""
                << section_id << '.'
                << detail::make_identifier(str.begin(), str.end())
                << "\" />"
                << pre << str << post
                ;
        }
        else // version 1.3 and above
        {
            std::string anchor = fully_qualified_id(library_id, qualified_section_id,
                detail::make_identifier(str.begin(), str.end()));

            out << "<anchor id=\"" << anchor << "\"/>"
                << pre
                << "<link linkend=\"" << anchor << "\">"
                << str
                << "</link>"
                << post
                ;
        }
    }

    void generic_header_action::operator()(iterator_range x, unused_type, unused_type) const
    {
        int level_ = section_level + 2;     // section_level is zero-based. We need to use a
                                            // 0ne-based heading which is one greater
                                            // than the current. Thus: section_level + 2.
        if (level_ > 6)                     // The max is h6, clip it if it goes
            level_ = 6;                     // further than that
        std::string str;
        phrase.swap(str);

        std::string anchor = fully_qualified_id(library_id, qualified_section_id,
            detail::make_identifier(str.begin(), str.end()));

        out
            << "<anchor id=\"" << anchor << "\"/>"
            << "<bridgehead renderas=\"sect" << level_ << "\">"
            << "<link linkend=\"" << anchor << "\">"
            << str
            << "</link>"
            << "</bridgehead>"
            ;
    }

    void simple_phrase_action::operator()(iterator_range const& x, unused_type, unused_type) const
    {
        out << pre;
        if (std::string const* ptr = macro.find(x))
        {
            out << *ptr;
        }
        else
        {
            iterator first = x.begin(), last = x.end();
            while (first != last)
                detail::print_char(*first++, out.get());
        }
        out << post;
    }

    void cond_phrase_action_pre::operator()(iterator_range x, unused_type, unused_type) const
    {
        std::string str(x.begin(), x.end());
        conditions.push_back(macro.find(str.c_str()));
        out.push(); // save the stream
    }

    void cond_phrase_action_post::operator()(iterator_range x, unused_type, unused_type) const
    {
        bool symbol_found = conditions.back();
        conditions.pop_back();

        if (x.begin() == x.end() || !symbol_found)
        {
            out.pop(); // restore the stream
        }
        else
        {
            std::string save;
            out.swap(save);
            out.pop(); // restore the stream
            out << save; // print the body
        }
    }

    void list_action::operator()(unused_type, unused_type, unused_type) const
    {
        BOOST_ASSERT(!list_marks.empty()); // there must be at least one item in the stack
        out << list_buffer.str();
        list_buffer.clear();

        while (!list_marks.empty())
        {
            char mark = list_marks.top().first;
            list_marks.pop();
            out << std::string((mark == '#') ? "\n</orderedlist>" : "\n</itemizedlist>");
            if (list_marks.size() >= 1)
                out << std::string("\n</listitem>");
        }

        list_indent = -1; // reset
    }

    void list_format_action::operator()(iterator_range x, unused_type, unused_type) const
    {
        int new_indent = 0;
        iterator first = x.begin(), last = x.end();
        while (first != last && (*first == ' ' || *first == '\t'))
        {
            char mark = *first++;
            if (mark == ' ')
            {
                ++new_indent;
            }
            else // must be a tab
            {
                BOOST_ASSERT(mark == '\t');
                // hardcoded tab to 4 for now
                new_indent = ((new_indent + 4) / 4) * 4;
            }
        }

        char mark = *first;
        BOOST_ASSERT(mark == '#' || mark == '*'); // expecting a mark

        if (list_indent == -1) // the very start
        {
            BOOST_ASSERT(new_indent == 0);
        }

        if (new_indent > list_indent)
        {
            list_indent = new_indent;
            list_marks.push(mark_type(mark, list_indent));
            if (list_marks.size() > 1)
            {
                // Make this new list a child of the previous list.
                // The previous listelem has already ended so we erase
                // </listitem> to accomodate this sub-list. We'll close
                // the listelem later.

                std::string str;
                out.swap(str);
                std::string::size_type pos = str.rfind("\n</listitem>");
                BOOST_ASSERT(pos <= str.size());
                str.erase(str.begin()+pos, str.end());
                out << str;
            }
            out << std::string((mark == '#') ? "<orderedlist>\n" : "<itemizedlist>\n");
        }

        else if (new_indent < list_indent)
        {
            BOOST_ASSERT(!list_marks.empty());
            list_indent = new_indent;

            while (!list_marks.empty() && (list_indent < list_marks.top().second))
            {
                char mark = list_marks.top().first;
                list_marks.pop();
                out << std::string((mark == '#') ? "\n</orderedlist>" : "\n</itemizedlist>");
                if (list_marks.size() >= 1)
                    out << std::string("\n</listitem>");
            }
        }

        if (mark != list_marks.top().first) // new_indent == list_indent
        {
            boost::spirit::classic::file_position const pos = first.get_position();
            detail::outerr(pos.file,pos.line)
                << "Illegal change of list style near column " << pos.column << ".\n";
            detail::outwarn(pos.file,pos.line)
                << "Ignoring change of list style" << std::endl;
            ++error_count;
        }
    }

    void span::operator()(iterator_range x, unused_type, unused_type) const
    {
        iterator first = x.begin(), last = x.end();
        out << "<phrase role=\"" << name << "\">";
        while (first != last)
            detail::print_char(*first++, out.get());
        out << "</phrase>";
    }

    void unexpected_char::operator()(iterator_range x, unused_type, unused_type) const
    {
        iterator first = x.begin(), last = x.end();
        boost::spirit::classic::file_position const pos = first.get_position();

        detail::outwarn(pos.file, pos.line)
            << "in column:" << pos.column
            << ", unexpected character: " << std::string(first, last)
            << "\n";

        // print out an unexpected character
        out << "<phrase role=\"error\">";
        while (first != last)
            detail::print_char(*first++, out.get());
        out << "</phrase>";
    }

    void anchor_action::operator()(iterator_range x, unused_type, unused_type) const
    {
        iterator first = x.begin(), last = x.end();
        out << "<anchor id=\"";
        while (first != last)
            detail::print_char(*first++, out.get());
        out << "\" />\n";
    }

    void do_macro_action::operator()(std::string const& str, unused_type, unused_type) const
    {
        if (str == quickbook_get_date)
        {
            char strdate[64];
            strftime(strdate, sizeof(strdate), "%Y-%b-%d", current_time);
            phrase << strdate;
        }
        else if (str == quickbook_get_time)
        {
            char strdate[64];
            strftime(strdate, sizeof(strdate), "%I:%M:%S %p", current_time);
            phrase << strdate;
        }
        else
        {
            phrase << str;
        }
    }

    void space::operator()(char ch, unused_type, unused_type) const
    {
        detail::print_space(ch, out.get());
    }

    void space::operator()(iterator_range x, unused_type, unused_type) const
    {
        iterator first = x.begin(), last = x.end();
        while (first != last)
            detail::print_space(*first++, out.get());
    }

    void pre_escape_back::operator()(unused_type, unused_type, unused_type) const
    {
        escape_actions.phrase.push(); // save the stream
    }

    void post_escape_back::operator()(unused_type, unused_type, unused_type) const
    {
        out << escape_actions.phrase.str();
        escape_actions.phrase.pop(); // restore the stream
    }

    void code_action::operator()(iterator_range x, unused_type, unused_type) const
    {
        // preprocess the code section to remove the initial indentation
        std::string program(x.begin(), x.end());
        detail::unindent(program);
        if (program.size() == 0)
            return; // Nothing left to do here. The program is empty.

        iterator first_(program.begin(), program.end());
        iterator last_(program.end(), program.end());
        first_.set_position(x.begin().get_position());

        std::string save;
        phrase.swap(save);

        // print the code with syntax coloring
        std::string str = syntax_p(first_, last_);

        phrase.swap(save);

        //
        // We must not place a \n after the <programlisting> tag
        // otherwise PDF output starts code blocks with a blank line:
        //
        out << "<programlisting>";
        out << str;
        out << "</programlisting>\n";
    }

    void inline_code_action::operator()(iterator_range x, unused_type, unused_type) const
    {
        std::string save;
        out.swap(save);

        // print the code with syntax coloring
        std::string str = syntax_p(x.begin(), x.end());

        out.swap(save);

        out << "<code>";
        out << str;
        out << "</code>";
    }

    void raw_char_action::operator()(char ch, unused_type, unused_type) const
    {
        phrase << ch;
    }

    void raw_char_action::operator()(iterator_range x, unused_type, unused_type) const
    {
        phrase << *x.begin();
    }

    void plain_char_action::operator()(char ch, unused_type, unused_type) const
    {
        detail::print_char(ch, phrase.get());
    }

    void plain_char_action::operator()(iterator_range x, unused_type, unused_type) const
    {
        detail::print_char(*x.begin(), phrase.get());
    }

    void image_action::operator()(iterator position, std::string image_fileref,
            std::multimap<std::string, std::string> input_attributes) const
    {
        std::map<std::string, std::string> attributes(
            input_attributes.begin(), input_attributes.end());
        
        if(attributes.size() != input_attributes.size()) {
            boost::spirit::classic::file_position const pos = position.get_position();
            
            std::map<std::string, std::string> duplicates;
            std::set_difference(
                input_attributes.begin(), input_attributes.end(),
                attributes.begin(), attributes.end(),
                std::inserter(duplicates, duplicates.end()));
            
            for(std::map<std::string, std::string>::iterator
                begin = duplicates.begin(), end = duplicates.end();
                begin != end; ++begin)
            {
                detail::outerr(pos.file,pos.line)
                    << "Duplicate image attribute: "
                    << begin->first
                    << std::endl;
                ++error_count;
            }
        }
    
        fs::path const img_path(image_fileref);
        
        attribute_map::iterator it = attributes.find("alt");
        std::string alt_text = it != attributes.end() ? it->second : fs::basename(img_path);
        attributes.erase("alt");

        attributes.insert(attribute_map::value_type("fileref", image_fileref));

        if(fs::extension(img_path) == ".svg")
        {
           //
           // SVG's need special handling:
           //
           // 1) We must set the "format" attribute, otherwise
           //    HTML generation produces code that will not display
           //    the image at all.
           // 2) We need to set the "contentwidth" and "contentdepth"
           //    attributes, otherwise the image will be displayed inside
           //    a tiny box with scrollbars (Firefox), or else cropped to
           //    fit in a tiny box (IE7).
           //
           attributes.insert(attribute_map::value_type("format", "SVG"));
           //
           // Image paths are relative to the html subdirectory:
           // TODO: This only works when you're running in the correct directory.
           // Support 'boost:' directories? Include paths?
           //
           fs::path img;
           if(img_path.root_path().empty())
              img = "html" / img_path;  // relative path
           else
              img = img_path;   // absolute path
           //
           // Now load the SVG file:
           //
           std::string svg_text;
           fs::ifstream fs(img);
           char c;
           while(fs.get(c) && fs.good())
              svg_text.push_back(c);
           //
           // Extract the svg header from the file:
           //
           std::string::size_type a, b;
           a = svg_text.find("<svg");
           b = svg_text.find('>', a);
           svg_text = (a == std::string::npos) ? "" : svg_text.substr(a, b - a);
           //
           // Now locate the "width" and "height" attributes
           // and borrow their values:
           //
           a = svg_text.find("width");
           a = svg_text.find('=', a);
           a = svg_text.find('\"', a);
           b = svg_text.find('\"', a + 1);
           if(a != std::string::npos)
           {
              attributes.insert(attribute_map::value_type("contentwidth",
                std::string(svg_text.begin() + a + 1, svg_text.begin() + b)));
           }
           a = svg_text.find("height");
           a = svg_text.find('=', a);
           a = svg_text.find('\"', a);
           b = svg_text.find('\"', a + 1);
           if(a != std::string::npos)
           {
              attributes.insert(attribute_map::value_type("contentdepth",
                std::string(svg_text.begin() + a + 1, svg_text.begin() + b)));
           }
        }

        phrase << "<inlinemediaobject>";

        phrase << "<imageobject><imagedata";
        
        for(attribute_map::const_iterator
            attr_first = attributes.begin(), attr_last  = attributes.end();
            attr_first != attr_last; ++attr_first)
        {
            phrase << " " << attr_first->first << "=\"";

            for(std::string::const_iterator
                first = attr_first->second.begin(),
                last  = attr_first->second.end();
                first != last; ++first)
            {
                if (*first == '\\' && ++first == last) break;
                detail::print_char(*first, phrase.get());
            }

            phrase << "\"";
        }

        phrase << "></imagedata></imageobject>";

        // Also add a textobject -- use the basename of the image file.
        // This will mean we get "alt" attributes of the HTML img.
        phrase << "<textobject><phrase>";
        detail::print_string(alt_text, phrase.get());
        phrase << "</phrase></textobject>";

        phrase << "</inlinemediaobject>";
    }

    void macro_identifier_action::operator()(iterator_range x, unused_type, unused_type) const
    {
        actions.macro_id.assign(x.begin(), x.end());
        actions.phrase.push(); // save the phrase
    }

    void macro_definition_action::operator()(unused_type, unused_type, unused_type) const
    {
        actions.macro.add(
            actions.macro_id.begin()
          , actions.macro_id.end()
          , actions.phrase.str());
        actions.phrase.pop(); // restore the phrase
    }

    void template_body_action::operator()(iterator_range x, unused_type, unused_type) const
    {
        BOOST_ASSERT(actions.template_info.size());
        if (actions.templates.find_top_scope(actions.template_info[0]))
        {
            boost::spirit::classic::file_position const pos = x.begin().get_position();
            detail::outerr(pos.file,pos.line)
                << "Template Redefinition: " << actions.template_info[0] << std::endl;
            ++actions.error_count;
        }

        actions.template_info.push_back(std::string(x.begin(), x.end()));
        actions.templates.add(
            actions.template_info[0]
          , template_symbol(actions.template_info, x.begin().get_position()));
        actions.template_info.clear();
    }

    namespace
    {
        std::string::size_type find_bracket_end(std::string const& str, std::string::size_type pos)
        {
            unsigned int depth = 1;

            while(depth > 0) {
                pos = str.find_first_of("[]\\", pos);
                if(pos == std::string::npos) return pos;

                if(str[pos] == '\\')
                {
                    pos += 2;
                }
                else
                {
                    depth += (str[pos] == '[') ? 1 : -1;
                    ++pos;
                }
            }

            return pos;
        }

        std::string::size_type find_first_seperator(std::string const& str)
        {
            if(qbk_version_n < 105) {
                return str.find_first_of(" \t\r\n");
            }
            else {
                std::string::size_type pos = 0;

                while(true)
                {
                    pos = str.find_first_of(" \t\r\n\\[", pos);
                    if(pos == std::string::npos) return pos;

                    switch(str[pos])
                    {
                    case '[':
                        pos = find_bracket_end(str, pos + 1);
                        break;
                    case '\\':
                        pos += 2;
                        break;
                    default:
                        return pos;
                    }
                }
            }
        }
    
        bool break_arguments(
            std::vector<std::string>& params
          , std::vector<std::string> const& template_
          , boost::spirit::classic::file_position const& pos
        )
        {
            // Quickbook 1.4-: If there aren't enough parameters seperated by
            //                 '..' then seperate the last parameter using
            //                 whitespace.
            // Quickbook 1.5+: If '..' isn't used to seperate the parameters
            //                 then use whitespace to separate them
            //                 (2 = template name + argument).

            if (qbk_version_n < 105 || params.size() == 1)
            {
                // template_.size() - 2 because template_ also includes the name and body.
                while (params.size() < template_.size() - 2 )
                {
                    // Try to break the last argument at the first space found
                    // and push it into the back of params. Do this
                    // recursively until we have all the expected number of
                    // arguments, or if there are no more spaces left.

                    std::string& str = params.back();
                    std::string::size_type l_pos = find_first_seperator(str);
                    if (l_pos == std::string::npos)
                        break;
                    std::string first(str.begin(), str.begin()+l_pos);
                    std::string::size_type r_pos = str.find_first_not_of(" \t\r\n", l_pos);
                    if (r_pos == std::string::npos)
                        break;
                    std::string second(str.begin()+r_pos, str.end());
                    str = first;
                    params.push_back(second);
                }
            }

            if (params.size() != template_.size() - 2)
            {
                detail::outerr(pos.file, pos.line)
                    << "Invalid number of arguments passed. Expecting: "
                    << template_.size()-2
                    << " argument(s), got: "
                    << params.size()
                    << " argument(s) instead."
                    << std::endl;
                return false;
            }
            return true;
        }

        std::pair<bool, std::vector<std::string>::const_iterator>
        get_arguments(
            std::vector<std::string>& params
          , std::vector<std::string> const& template_
          , template_scope const& scope
          , boost::spirit::classic::file_position const& pos
          , quickbook::actions& actions
        )
        {
            std::vector<std::string>::const_iterator arg = params.begin();
            std::vector<std::string>::const_iterator tpl = template_.begin()+1;

            // Store each of the argument passed in as local templates:
            while (arg != params.end())
            {
                std::vector<std::string> tinfo;
                tinfo.push_back(*tpl);
                tinfo.push_back(*arg);
                template_symbol template_(tinfo, pos, &scope);

                if (actions.templates.find_top_scope(*tpl))
                {
                    detail::outerr(pos.file,pos.line)
                        << "Duplicate Symbol Found" << std::endl;
                    ++actions.error_count;
                    return std::make_pair(false, tpl);
                }
                else
                {
                    actions.templates.add(*tpl, template_);
                }
                ++arg; ++tpl;
            }
            return std::make_pair(true, tpl);
        }

        bool parse_template(
            std::string& body
          , std::string& result
          , boost::spirit::classic::file_position const& template_pos
          , bool template_escape
          , quickbook::actions& actions
        )
        {
            // How do we know if we are to parse the template as a block or
            // a phrase? We apply a simple heuristic: if the body starts with
            // a newline, then we regard it as a block, otherwise, we parse
            // it as a phrase.

            std::string::const_iterator iter = body.begin();
            while (iter != body.end() && ((*iter == ' ') || (*iter == '\t')))
                ++iter; // skip spaces and tabs
            bool is_block = (iter != body.end()) && ((*iter == '\r') || (*iter == '\n'));
            bool r = false;

            if (template_escape)
            {
                //  escape the body of the template
                //  we just copy out the literal body
                result = body;
                r = true;
            }
            else if (!is_block)
            {
                simple_phrase_grammar<iterator, quickbook::actions> phrase_p(actions);

                //  do a phrase level parse
                iterator first(body.begin(), body.end(), actions.filename.native_file_string().c_str());
                first.set_position(template_pos);
                iterator last(body.end(), body.end());
                r = boost::spirit::qi::parse(first, last, phrase_p) && first == last;
                actions.phrase.swap(result);
            }
            else
            {
                block_grammar<iterator, quickbook::actions> block_p(actions);

                //  do a block level parse
                //  ensure that we have enough trailing newlines to eliminate
                //  the need to check for end of file in the grammar.
                body.push_back('\n');
                body.push_back('\n');
                while (iter != body.end() && ((*iter == '\r') || (*iter == '\n')))
                    ++iter; // skip initial newlines
                iterator first(iter, body.end(), actions.filename.native_file_string().c_str());
                first.set_position(template_pos);
                iterator last(body.end(), body.end());
                r = boost::spirit::qi::parse(first, last, block_p) && first == last;
                actions.out.swap(result);
            }
            return r;
        }
    }

    void do_template_action::operator()(iterator p, bool template_escape,
        template_symbol const& symbol, std::vector<std::string> params) const
    {
        boost::spirit::classic::file_position const pos = p.get_position();
        ++actions.template_depth;
        if (actions.template_depth > actions.max_template_depth)
        {
            detail::outerr(pos.file,pos.line)
                << "Infinite loop detected" << std::endl;
            --actions.template_depth;
            ++actions.error_count;
            return;
        }

        // The template arguments should have the scope that the template was
        // called from, not the template's own scope.
        //
        // Note that for quickbook 1.4- this value is just ignored when the
        // arguments are expanded.
        template_scope const& call_scope = actions.templates.top_scope();

        std::string result;
        actions.push(); // scope the actions' states
        {
            // Quickbook 1.4-: When expanding the tempalte continue to use the
            //                 current scope (the dynamic scope).
            // Quickbook 1.5+: Use the scope the template was defined in
            //                 (the static scope).
            if (qbk_version_n >= 105)
                actions.templates.set_parent_scope(*boost::get<2>(symbol));

            std::vector<std::string> template_ = boost::get<0>(symbol);
            boost::spirit::classic::file_position template_pos = boost::get<1>(symbol);

            ///////////////////////////////////
            // Break the arguments
            if (!break_arguments(params, template_, pos))
            {
                actions.pop(); // restore the actions' states
                --actions.template_depth;
                ++actions.error_count;
                return;
            }

            ///////////////////////////////////
            // Prepare the arguments as local templates
            bool get_arg_result;
            std::vector<std::string>::const_iterator tpl;
            boost::tie(get_arg_result, tpl) =
                get_arguments(params, template_,
                    call_scope, pos, actions);

            if (!get_arg_result)
            {
                actions.pop(); // restore the actions' states
                --actions.template_depth;
                return;
            }

            ///////////////////////////////////
            // parse the template body:
            std::string body;
            body.assign(tpl->begin(), tpl->end());
            body.reserve(body.size()+2); // reserve 2 more

            if (!parse_template(body, result, template_pos, template_escape, actions))
            {
                detail::outerr(pos.file,pos.line)
                    //<< "Expanding template:" << template_info[0] << std::endl
                    << std::endl
                    << "------------------begin------------------" << std::endl
                    << body
                    << "------------------end--------------------" << std::endl
                    << std::endl;
                actions.pop(); // restore the actions' states
                --actions.template_depth;
                ++actions.error_count;
                return;
            }
        }

        actions.pop(); // restore the actions' states
        actions.phrase << result; // print it!!!
        --actions.template_depth;
    }

    void generic_link_action::operator()(char const* tag, iterator_range x) const
    {
        iterator first = x.begin(), last = x.end();
        iterator save = first;
        phrase << tag;
        while (first != last)
            detail::print_char(*first++, phrase.get());
        phrase << "\">";

        // Yes, it is safe to dereference last here. When we
        // reach here, *last is certainly valid. We test if
        // *last == ']'. In which case, the url is the text.
        // Example: [@http://spirit.sourceforge.net/]

        if (*last == ']')
        {
            first = save;
            while (first != last)
                detail::print_char(*first++, phrase.get());
        }
    }

    void variablelist_action::operator()(std::string const& title) const
    {
        actions.out << "<variablelist>\n";

        actions.out << "<title>";
        std::string::const_iterator first = title.begin();
        std::string::const_iterator last = title.end();
        while (first != last)
            detail::print_char(*first++, actions.out.get());
        actions.out << "</title>\n";

        std::string str;
        actions.phrase.swap(str);
        actions.out << str;

        actions.out << "</variablelist>\n";
        actions.table_span = 0;
        actions.table_header.clear();
    }

    void start_varlistitem_action::operator()(unused_type, unused_type, unused_type) const
    {
        phrase << start_varlistitem_;
        phrase.push();
    }

    void end_varlistitem_action::operator()(unused_type, unused_type, unused_type) const
    {
        std::string str;
        temp_para.swap(str);
        phrase.pop();
        phrase << str << end_varlistitem_;
    }

    void table_action::operator()(boost::optional<std::string> const& id, std::string const& title) const
    {
        std::string::const_iterator first = title.begin();
        std::string::const_iterator last = title.end();
        bool has_title = first != last;
        
        std::string table_id;
        if(qbk_version_n >= 105) {
            if(id) {
                table_id = fully_qualified_id(actions.doc_id,
                    actions.qualified_section_id, *id);
            }
            else if(has_title) {
                table_id = fully_qualified_id(actions.doc_id,
                    actions.qualified_section_id,
                    detail::make_identifier(first, last));
            }
        }

        if (has_title)
        {
            actions.out << "<table frame=\"all\"";
            if(!table_id.empty())
                actions.out << " id=\"" << table_id << "\"";
            actions.out << ">\n";
            actions.out << "<title>";
            while (first != last)
                detail::print_char(*first++, actions.out.get());
            actions.out << "</title>";
        }
        else
        {
            actions.out << "<informaltable frame=\"all\"";
            if(!table_id.empty())
                actions.out << " id=\"" << table_id << "\"";
            actions.out << ">\n";
        }

        actions.out << "<tgroup cols=\"" << actions.table_span << "\">\n";

        if (!actions.table_header.empty())
        {
            actions.out << "<thead>" << actions.table_header << "</thead>\n";
        }

        actions.out << "<tbody>\n";

        std::string str;
        actions.phrase.swap(str);
        actions.out << str;

        actions.out << "</tbody>\n"
                     << "</tgroup>\n";

        if (has_title)
        {
            actions.out << "</table>\n";
        }
        else
        {
            actions.out << "</informaltable>\n";
        }

        actions.table_span = 0;
        actions.table_header.clear();
    }

    void start_row_action::operator()(unused_type, unused_type, unused_type) const
    {
        // the first row is the header
        if (header.empty() && !phrase.str().empty())
        {
            phrase.swap(header);
        }

        phrase << start_row_;
        span = 0;
    }

    void start_col_action::operator()(unused_type, unused_type, unused_type) const
    {
        phrase << start_cell_;
        phrase.push();
        ++span;
    }

    void end_col_action::operator()(unused_type, unused_type, unused_type) const
    {
        std::string str;
        temp_para.swap(str);
        phrase.pop();
        phrase << str << end_cell_;
    }

    void begin_section_action::operator()(boost::optional<std::string> const& id, iterator_range x) const
    {
        section_id = id ? *id :
            detail::make_identifier(x.begin(), x.end());

        if (section_level != 0)
            qualified_section_id += '.';
        else
            BOOST_ASSERT(qualified_section_id.empty());
        qualified_section_id += section_id;
        ++section_level;

        if (qbk_version_n < 103) // version 1.2 and below
        {
            out << "\n<section id=\""
                << library_id << "." << section_id << "\">\n";
        }
        else // version 1.3 and above
        {
            out << "\n<section id=\"" << library_id
                << "." << qualified_section_id << "\">\n";
        }
        std::string str;
        phrase.swap(str);

        if (qbk_version_n < 103) // version 1.2 and below
        {
            out << "<title>" << str << "</title>\n";
        }
        else // version 1.3 and above
        {
            out << "<title>"
                << "<link linkend=\"" << library_id
                    << "." << qualified_section_id << "\">"
                << str
                << "</link>"
                << "</title>\n"
                ;
        }
    }

    void end_section_action::operator()(iterator_range x, unused_type, unused_type) const
    {
        out << "</section>";

        --section_level;
        if (section_level < 0)
        {
            boost::spirit::classic::file_position const pos = x.begin().get_position();
            detail::outerr(pos.file,pos.line)
                << "Mismatched [endsect] near column " << pos.column << ".\n";
            ++error_count;
            
            // $$$ TODO: somehow fail parse else BOOST_ASSERT(std::string::npos != n)
            // $$$ below will assert.
        }
        if (section_level == 0)
        {
            qualified_section_id.clear();
        }
        else
        {
            std::string::size_type const n =
                qualified_section_id.find_last_of('.');
            BOOST_ASSERT(std::string::npos != n);
            qualified_section_id.erase(n, std::string::npos);
        }
    }
    
    void element_id_warning_action::operator()(iterator_range x, unused_type, unused_type) const
    {
        boost::spirit::classic::file_position const pos = x.begin().get_position();
        detail::outwarn(pos.file,pos.line) << "Empty id.\n";        
    }

    fs::path path_difference(fs::path const& outdir, fs::path const& path)
    {
        fs::path outtmp, temp;
        fs::path::iterator out = outdir.begin(), file = path.begin();
        for(; out != outdir.end() && file != path.end(); ++out, ++file)
        {
            if(!fs::equivalent(outtmp /= *out, temp /= *file))
                break;
        }
        out = (out == outdir.begin()) ? outdir.end() : out;

        fs::path result = fs::path();
        for(; out != outdir.end(); ++out)
            if(*out != ".") result /= "..";
        std::divides<fs::path> concat;
        return std::accumulate(file, path.end(), result, concat);
    }

    fs::path calculate_relative_path(
        iterator first, iterator last, quickbook::actions& actions)
    {
        // Given a source file and the current filename, calculate the
        // path to the source file relative to the output directory.
        fs::path path(std::string(first, last));
        if (!path.is_complete())
        {
            fs::path infile = fs::complete(actions.filename).normalize();
            path = (infile.branch_path() / path).normalize();
            fs::path outdir = fs::complete(actions.outdir).normalize();
            path = path_difference(outdir, path);
        }
        return path;
    }

    void xinclude_action::operator()(iterator_range x, unused_type, unused_type) const
    {
        fs::path path = calculate_relative_path(x.begin(), x.end(), actions);
        out << "\n<xi:include href=\"";
        detail::print_string(detail::escape_uri(path.string()), out.get());
        out << "\" />\n";
    }

    void code_snippet_actions::pass_thru(char x)
    {
        code += x;
    }

    namespace detail
    {
        int callout_id = 0;
    }

    void code_snippet_actions::callout(std::string const& x, char const* role)
    {
        using detail::callout_id;
        code += "``'''";
        code += std::string("<phrase role=\"") + role + "\">";
        code += "<co id=\"";
        code += doc_id + boost::lexical_cast<std::string>(callout_id + callouts.size()) + "co\" ";
        code += "linkends=\"";
        code += doc_id + boost::lexical_cast<std::string>(callout_id + callouts.size()) + "\" />";
        code += "</phrase>";
        code += "'''``";

        callouts.push_back(x);
    }

    void code_snippet_actions::inline_callout(std::string const& x)
    {
        callout(x, "callout_bug");
    }

    void code_snippet_actions::line_callout(std::string const& x)
    {
        callout(x, "line_callout_bug");
    }

    void code_snippet_actions::escaped_comment(std::string const& x)
    {
        if (!code.empty())
        {
            detail::unindent(code); // remove all indents
            if (code.size() != 0)
            {
                snippet += "\n\n";
                snippet += source_type;
                snippet += "``\n" + code + "``\n\n";
                code.clear();
            }
        }
        std::string temp(x);
        detail::unindent(temp); // remove all indents
        if (temp.size() != 0)
        {
            snippet += "\n" + temp; // add a linebreak to allow block marskups
        }
    }

    void code_snippet_actions::compile(boost::iterator_range<iterator> x)
    {
        using detail::callout_id;
        if (!code.empty())
        {
            detail::unindent(code); // remove all indents
            if (code.size() != 0)
            {
                snippet += "\n\n";
                snippet += source_type;
                snippet += "```\n" + code + "```\n\n";
            }

            if(callouts.size() > 0)
            {
              snippet += "'''<calloutlist>'''";
              for (size_t i = 0; i < callouts.size(); ++i)
              {
                  snippet += "'''<callout arearefs=\"";
                  snippet += doc_id + boost::lexical_cast<std::string>(callout_id + i) + "co\" ";
                  snippet += "id=\"";
                  snippet += doc_id + boost::lexical_cast<std::string>(callout_id + i) + "\">";
                  snippet += "'''";

                  snippet += "'''<para>'''";
                  snippet += callouts[i];
                  snippet += "'''</para>'''";
                  snippet += "'''</callout>'''";
              }
              snippet += "'''</calloutlist>'''";
            }
        }

        std::vector<std::string> tinfo;
        tinfo.push_back(id);
        tinfo.push_back(snippet);
        storage.push_back(template_symbol(tinfo, x.begin().get_position()));

        callout_id += callouts.size();
        callouts.clear();
        code.clear();
        snippet.clear();
        id.clear();
    }

    int load_snippets(
        std::string const& file
      , std::vector<template_symbol>& storage   // snippets are stored in a
                                                // vector of template_symbols
      , std::string const& extension
      , std::string const& doc_id)
    {
        std::string code;
        int err = detail::load(file, code);
        if (err != 0)
            return err; // return early on error

        iterator first(code.begin(), code.end(), file);
        iterator last(code.end(), code.end());

        size_t fname_len = file.size();
        bool is_python = fname_len >= 3
            && file[--fname_len]=='y' && file[--fname_len]=='p' && file[--fname_len]=='.';
        code_snippet_actions a(storage, doc_id, is_python ? "[python]" : "[c++]");
        // TODO: Should I check that parse succeeded?
        if(is_python) {
            python_code_snippet_grammar<iterator> g(a);
            boost::spirit::qi::parse(first, last, g);
        }
        else {
            cpp_code_snippet_grammar<iterator> g(a);
            boost::spirit::qi::parse(first, last, g);
        }

        return 0;
    }

    namespace
    {
        fs::path include_search(fs::path const & current, std::string const & name)
        {
            fs::path path(name,fs::native);

            // If the path is relative, try and resolve it.
            if (!path.is_complete())
            {
                // See if it can be found locally first.
                if (fs::exists(current / path))
                {
                    return current / path;
                }

                // Search in each of the include path locations.
                BOOST_FOREACH(std::string const & p, include_path)
                {
                    fs::path full(p,fs::native);
                    full /= path;
                    if (fs::exists(full))
                    {
                        return full;
                    }
                }
            }

            return path;
        }
    }

    void import_action::operator()(iterator_range x, unused_type, unused_type) const
    {
        fs::path path = include_search(actions.filename.branch_path(), std::string(x.begin(), x.end()));
        std::string ext = fs::extension(path);
        std::vector<template_symbol> storage;
        actions.error_count +=
            load_snippets(path.string(), storage, ext, actions.doc_id);

        BOOST_FOREACH(template_symbol const& ts, storage)
        {
            std::string tname = boost::get<0>(ts)[0];
            if (actions.templates.find_top_scope(tname))
            {
                boost::spirit::classic::file_position const pos = boost::get<1>(ts);
                detail::outerr(pos.file, pos.line)
                    << "Template Redefinition: " << tname << std::endl;
                ++actions.error_count;
            }
            else
            {
                actions.templates.add(tname, ts);
            }
        }
    }

    void include_action::operator()(iterator_range x, unused_type, unused_type) const
    {
        fs::path filein = include_search(actions.filename.branch_path(), std::string(x.begin(), x.end()));
        std::string doc_type, doc_id, doc_dirname, doc_last_revision;

        // swap the filenames
        std::swap(actions.filename, filein);

        // save the doc info strings
        actions.doc_type.swap(doc_type);
        actions.doc_id.swap(doc_id);
        actions.doc_dirname.swap(doc_dirname);
        actions.doc_last_revision.swap(doc_last_revision);

        // scope the macros
        string_symbols macro = actions.macro;
        // scope the templates
        //~ template_symbols templates = actions.templates; $$$ fixme $$$

        // if an id is specified in this include (as in [include:id foo.qbk])
        // then use it as the doc_id.
        if (!actions.include_doc_id.empty())
        {
            actions.doc_id = actions.include_doc_id;
            actions.include_doc_id.clear();
        }

        // update the __FILENAME__ macro
        *actions.macro.find("__FILENAME__") = actions.filename.native_file_string();

        // parse the file
        quickbook::parse(actions.filename.native_file_string().c_str(), actions, true);

        // restore the values
        std::swap(actions.filename, filein);

        actions.doc_type.swap(doc_type);
        actions.doc_id.swap(doc_id);
        actions.doc_dirname.swap(doc_dirname);
        actions.doc_last_revision.swap(doc_last_revision);

        // restore the macros
        actions.macro = macro;
        // restore the templates
        //~ actions.templates = templates; $$$ fixme $$$
    }

    void xml_author::operator()(std::pair<std::string, std::string> const& author) const
    {
        out << "      <author>\n"
            << "        <firstname>" << author.first << "</firstname>\n"
            << "        <surname>" << author.second << "</surname>\n"
            << "      </author>\n";
    }

    void xml_copyright::operator()(std::pair<std::vector<std::string>, std::string> const& copyright) const
    {
        out << "\n" << "    <copyright>\n";

        for_each(
            copyright.first.begin()
          , copyright.first.end()
          , xml_year(out));

        out << "      <holder>" << copyright.second << "</holder>\n"
            << "    </copyright>\n"
            << "\n"
        ;
    }

    void xml_year::operator()(std::string const &year) const
    {
        out << "      <year>" << year << "</year>\n";
    }

    static void write_document_title(collector& out, quickbook::actions& actions);
    static void write_document_info(collector& out, quickbook::actions& actions);

    void pre(collector& out, quickbook::actions& actions, bool ignore_docinfo)
    {
        // The doc_info in the file has been parsed. Here's what we'll do
        // *before* anything else.

        if (actions.doc_id.empty())
            actions.doc_id = detail::make_identifier(
                actions.doc_title.begin(),actions.doc_title.end());

        if (actions.doc_dirname.empty() && actions.doc_type == "library")
            actions.doc_dirname = actions.doc_id;

        if (actions.doc_last_revision.empty())
        {
            // default value for last-revision is now

            char strdate[64];
            strftime(
                strdate, sizeof(strdate),
                (debug_mode ?
                    "DEBUG MODE Date: %Y/%m/%d %H:%M:%S $" :
                    "$" /* prevent CVS substitution */ "Date: %Y/%m/%d %H:%M:%S $"),
                current_gm_time
            );
            actions.doc_last_revision = strdate;
        }

        // if we're ignoring the document info, we're done.
        if (ignore_docinfo)
        {
            return;
        }

        if (qbk_major_version == 0)
        {
            // hard code quickbook version to v1.1
            qbk_major_version = 1;
            qbk_minor_version = 1;
            qbk_version_n = 101;
            detail::outwarn(actions.filename.native_file_string(),1)
                << "Warning: Quickbook version undefined. "
                "Version 1.1 is assumed" << std::endl;
        }
        else
        {
            qbk_version_n = (qbk_major_version * 100) + qbk_minor_version;
        }

        out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            << "<!DOCTYPE library PUBLIC \"-//Boost//DTD BoostBook XML V1.0//EN\"\n"
            << "     \"http://www.boost.org/tools/boostbook/dtd/boostbook.dtd\">\n"
            << '<' << actions.doc_type << "\n"
            << "    id=\"" << actions.doc_id << "\"\n";
        
        if(actions.doc_type == "library")
        {
            out << "    name=\"" << actions.doc_title << "\"\n";
        }

        if(!actions.doc_dirname.empty())
        {
            out << "    dirname=\"" << actions.doc_dirname << "\"\n";
        }

        out << "    last-revision=\"" << actions.doc_last_revision << "\" \n"
            << "    xmlns:xi=\"http://www.w3.org/2001/XInclude\">\n";
            
        if(actions.doc_type == "library") {
            write_document_info(out, actions);
            write_document_title(out, actions);
        }
        else {
            write_document_title(out, actions);
            write_document_info(out, actions);
        }
    }
    
    void post(collector& out, quickbook::actions& actions, bool ignore_docinfo)
    {
        // if we're ignoring the document info, do nothing.
        if (ignore_docinfo)
        {
            return;
        }

        // We've finished generating our output. Here's what we'll do
        // *after* everything else.
        out << "\n</" << actions.doc_type << ">\n\n";
    }

    void write_document_title(collector& out, quickbook::actions& actions)
    {
        if (!actions.doc_title.empty())
        {
            out<< "  <title>" << actions.doc_title;
            if (!actions.doc_version.empty())
                out << ' ' << actions.doc_version;
            out<< "</title>\n\n\n";
        }
    }

    void write_document_info(collector& out, quickbook::actions& actions)
    {
        out << "  <" << actions.doc_type << "info>\n";

        if(!actions.doc_authors.empty())
        {
            out << "    <authorgroup>\n";
            for_each(
                actions.doc_authors.begin()
              , actions.doc_authors.end()
              , xml_author(out));
            out << "    </authorgroup>\n";
        }

        if (!actions.doc_copyrights.empty())
        {
            for_each(
                actions.doc_copyrights.begin()
              , actions.doc_copyrights.end()
              , xml_copyright(out));
        }

        if (qbk_version_n < 103)
        {
            // version < 1.3 compatibility
            actions.doc_license = actions.doc_license_1_1;
            actions.doc_purpose = actions.doc_purpose_1_1;
        }

        if (!actions.doc_license.empty())
        {
            out << "    <legalnotice>\n"
                << "      <para>\n"
                << "        " << actions.doc_license << "\n"
                << "      </para>\n"
                << "    </legalnotice>\n"
                << "\n"
            ;
        }

        if (!actions.doc_purpose.empty())
        {
            out << "    <" << actions.doc_type << "purpose>\n"
                << "      " << actions.doc_purpose
                << "    </" << actions.doc_type << "purpose>\n"
                << "\n"
            ;
        }

        if (!actions.doc_category.empty())
        {
            out << "    <" << actions.doc_type << "category name=\"category:"
                << actions.doc_category
                << "\"></" << actions.doc_type << "category>\n"
                << "\n"
            ;
        }

        out << "  </" << actions.doc_type << "info>\n"
            << "\n"
        ;
    }

    void phrase_to_string_action::operator()(unused_type, unused_type, unused_type) const
    {
        phrase.swap(out);
    }
}

