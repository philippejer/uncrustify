/*
 * uncrustify_emscripten.cpp
 *
 *  Created on: May 8, 2016
 *      Author: Daniel Chumak
 */

#ifdef EMSCRIPTEN

#include "prototypes.h"
#include "options.h"
#include "uncrustify_version.h"
#include "logger.h"
#include "log_levels.h"

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <memory>

#include <emscripten/bind.h>


#define STRINGIFY(s)    # s


using namespace std;
using namespace emscripten;

extern void process_option_line(char *configLine, const char *filename);
extern void usage_exit(const char *msg, const char *argv0, int code);
extern int load_header_files();
extern const char *language_name_from_flags(int lang);
extern void uncrustify_file(const file_mem &fm, FILE *pfout, const char *parsed_file, bool defer_uncrustify_end = false);
extern const option_map_value *unc_find_option(const char *name);


extern map<uncrustify_options, option_map_value> option_name_map;
extern map<uncrustify_groups, group_map_value>   group_map;


/**
 * Loads options from a file represented as a single char array.
 * Modifies: input char array, cpd.line_number
 * Expects: \0 terminated char array
 *
 * @param configString char array that holds the whole config
 * @return EXIT_SUCCESS on success
 */
int load_option_fileChar(char *configString)
{
   char *delimPos       = &configString[0];
   char *subStringStart = &configString[0];

   cpd.line_number = 0;

   while (true)
   {
      delimPos = strchr(delimPos, '\n');
      if (delimPos == nullptr)
      {
         break;
      }

      // replaces \n with \0 -> string including multiple terminated substrings
      *delimPos = '\0';

      process_option_line(subStringStart, "");

      delimPos++;
      subStringStart = delimPos;
   }
   //get last line, expectation: ends with \0
   process_option_line(subStringStart, "");

   return(EXIT_SUCCESS);
}


// TODO: interface for args:
// unsure about these:
// -----------------------------------------------------------------------------
// --check
// --decode      TODO ???
// --parsed, -p  TODO (upstream): see uncrustify
// --detect      TODO (upstream): needs uncrustify start and end which both are
//                                static
//
//
// will not be included:
// -----------------------------------------------------------------------------
// -t ( define via multiple --type )
// -d ( define via multiple --define )
// --assume ( no files available to guess the lang. based on the filename ending )
// --files ( no batch processing will be available )
// --prefix
// --suffix
// --assume
// --no-backup
// --replace
// --mtime
// --universalindent
// --help, -h, --usage, -?
//
//
// done:
// -----------------------------------------------------------------------------
// --update-config ( use show_config() )
// --update-config-with-doc ( show_config( bool withDoc = true ) )
// --version, -v ( use get_version() )
// --log, -L ( use log_set_sev( log_sev_t sev, bool value ) )
// -q ( use set_quiet() )
// --config, -c ( use set_config( string _cfg ) )
// --file, -f ( use uncrustify( string _file ) )
// --show-config( use show_options() )
// --show ( use show_log_type( bool ) )
// --frag ( use uncrustify( string _file, bool frag = true ) )
// --type ( use add_type( string _type ) )
// --define ( use add_define( string _tag ) )
// -l ( use set_language( int langIDX ) )


// TODO (upstream): it would be nicer to set settings via uncrustify_options enum_id
// but this wont work since type info is needed
// which is inside of the
// _static_ option_name_map<
// option_name : string,
// option_map_val : struct { type : argtype_e, ....} >
// to access the right union var inside of op_val_t
// even if option_name_map would not be static, no direct access to the type
// info is possible since the maps needs to be iterated to find the according
// enum_id
//
// int set_option_value(op_val_t option_id, const char *value)
// string get_option_value(op_val_t option_id )


enum class lang_flags : int
{
   LANG_C_    = LANG_C,
   LANG_CPP_  = LANG_CPP,
   LANG_D_    = LANG_D,
   LANG_CS_   = LANG_CS,
   LANG_VALA_ = LANG_VALA,
   LANG_JAVA_ = LANG_JAVA,
   LANG_PAWN_ = LANG_PAWN,
   LANG_OC_   = LANG_OC,
   LANG_OCPP_ = LANG_OC | LANG_CPP,
   LANG_ECMA_ = LANG_ECMA,
//TODO ???? see language_names[] uncrustify.cpp, what about flags ?
//    LANG_ALLC_ = LANG_ALLC,
//    LANG_ALL_ = LANG_ALL,
};


// TODO (upstream): use a named enum for languages


/**
 *  sets the language of the to be formatted text
 *
 *  @param langIDX: ID of the language, see enum lang_flags or
 *                  uncrustify_types.h
 */
void set_language(lang_flags langIDX)
{
   cpd.lang_flags = static_cast<int>(langIDX);
}


// TODO ( upstream ): this needs more than just adding types,
// add ways to add and remove keywords


/**
 * adds a new keyword to Uncrustify's dynamic keyword map (dkwm, keywords.cpp)
 *
 * @param type: keyword that is going to be added
 */
void add_type(string type)
{
   if (type.empty())
   {
      LOG_FMT(LERR, "%s: input string is empty\n", __func__);
      return;
   }
   add_keyword(type.c_str(), CT_TYPE);
}


//! clears Uncrustify's dynamic keyword map (dkwm, keywords.cpp)
void clear_keywords()
{
   clear_keyword_file();
}


/**
 * adds an entry to the define list
 *
 * @param tag:   tag string
 * @param value: value of the define
 */
void add_define(string tag, string val)
{
   if (tag.empty())
   {
      LOG_FMT(LERR, "%s: tag string is empty\n", __func__);
      return;
   }
   if (val.empty())
   {
      LOG_FMT(LERR, "%s: val string is empty\n", __func__);
      return;
   }

   add_define(tag.c_str(), val.c_str());
}


/**
 * adds an entry to the define list
 *
 * @param tag: tag string
 */
void add_define(string tag)
{
   if (tag.empty())
   {
      LOG_FMT(LERR, "%s: tag string is empty\n", __func__);
      return;
   }
   add_define(tag.c_str(), NULL);
}


/**
 * Show or hide the severity prefix "<1>"
 *
 * @param b: true=show, false=hide
 */
void show_log_type(bool b)
{
   log_show_sev(b);
}


//! returns the UNCRUSTIFY_VERSION string
string get_version()
{
   return(UNCRUSTIFY_VERSION);
}


//! disables all logging messages
void set_quiet()
{
   // set empty mask
   log_mask_t mask;

   log_set_mask(mask);
}


/**
 * sets value of an option
 *
 * @param name:  name of the option
 * @param value: value that is going to be set
 * @return options enum value of the found option or -1 if option was not found
 */
int set_option(string name, string value)
{
   if (name.empty())
   {
      LOG_FMT(LERR, "%s: name string is empty\n", __func__);
      return(-1);
   }
   if (value.empty())
   {
      LOG_FMT(LERR, "%s: value string is empty\n", __func__);
      return(-1);
   }

   return(set_option_value(name.c_str(), value.c_str()));
}


/**
 * returns value of an option
 *
 * @param name: name of the option
 * @return currently set value of the option
 */
string get_option(string name)
{
   if (name.empty())
   {
      LOG_FMT(LERR, "%s: input string is empty\n", __func__);
      return("");
   }

   const auto option = unc_find_option(name.c_str());
   if (option == NULL)
   {
      LOG_FMT(LWARN, "Option %s not found\n", name.c_str());
      return("");
   }

   return(op_val_to_string(option->type, cpd.settings[option->id]));
}


//! returns a string with option documentation
string show_options()
{
   char   *buf;
   size_t len;

   // TODO (upstream): see uncrustify()
   FILE *stream = open_memstream(&buf, &len);

   if (stream == NULL)
   {
      LOG_FMT(LERR, "Failed to open_memstream\n");
      fflush(stream);
      fclose(stream);
      free(buf);
      return("");
   }


   print_options(stream);
   fflush(stream);
   fclose(stream);

   string out(buf);
   free(buf);

   return(out);
}


/**
 * returns the config file string based on the current configuration
 *
 * @param withDoc:          false=without documentation,
 *                          true=with documentation text lines
 * @param only_not_default: false=containing all options,
 *                          true=containing only options with non default values
 * @return returns the config file string based on the current configuration
 */
string show_config(bool withDoc, bool only_not_default)
{
   char   *buf;
   size_t len;

   // TODO (upstream): see uncrustify()
   FILE *stream = open_memstream(&buf, &len);

   if (stream == NULL)
   {
      LOG_FMT(LERR, "Failed to open_memstream\n");
      fflush(stream);
      fclose(stream);
      free(buf);
      return("");
   }

   save_option_file_kernel(stream, withDoc, only_not_default);

   fflush(stream);
   fclose(stream);

   string out(buf);
   free(buf);

   return(out);
}


/**
 * returns the config file string with all options based on the current configuration
 *
 * @param withDoc: false= without documentation, true=with documentation text lines
 * @return returns the config file string with all options based on the current configuration
 */
string show_config(bool withDoc)
{
   return(show_config(withDoc, false));
}


//!returns the config file string with all options and without documentation based on the current configuration
string show_config()
{
   return(show_config(false, false));
}


/**
 * initializes the current libUncrustify instance,
 * used only for emscripten binding here and will be automatically called while
 * module initialization
 */
void initialize()
{
   register_options();
   set_option_defaults();
   log_init(stderr);

   LOG_FMT(LSYS, "Initialized libUncrustify\n");
}


//! destroys the current libUncrustify instance
void destruct()
{
   clear_keyword_file();
   clear_defines();
}


/**
 * reads option file string, sets the defined options
 *
 * @return returns EXIT_SUCCESS on success
 */
int loadConfig(string _cfg)
{
   if (_cfg.empty())
   {
      LOG_FMT(LERR, "%s: input string is empty\n", __func__);
      return(EXIT_FAILURE);
   }

   unique_ptr<char[]> cfg(new char[_cfg.length() + 1]);
   strcpy(cfg.get(), _cfg.c_str());

   // reset everything in case a config was loaded previously
   set_option_defaults();

   if (load_option_fileChar(cfg.get()) != EXIT_SUCCESS)
   {
      LOG_FMT(LERR, "unable to load the config\n");
      return(EXIT_FAILURE);
   }

   /* This relies on cpd.filename being the config file name */
   load_header_files();


   LOG_FMT(LSYS, "finished loading config\n");
   return(EXIT_SUCCESS);
}


//! returns a copy of the current option_name_map
map<uncrustify_options, option_map_value> getOptionNameMap()
{
   return(option_name_map);
}


//! returns a copy of the current group_map
map<uncrustify_groups, group_map_value> getGroupMap()
{
   return(group_map);
}


/**
 * format string
 *
 * @param file: string that is going to be formated
 * @param frag: true=fragmented code input, false=unfragmented code input
 * @return formated string
 */
string uncrustify(string file, bool frag)
{
   if (file.empty())
   {
      LOG_FMT(LERR, "%s: file string is empty\n", __func__);
      return("");
   }

   // Problem: uncrustify originally is not a lib and uses global vars such as
   // cpd.error_count for the whole program execution
   // to know if errors occurred during the formating step we reset this var here
   cpd.error_count = 0;

   if (cpd.lang_flags == 0)   // 0 == undefined
   {
      LOG_FMT(LWARN, "language of input file not defined, C++ will be assumed\n");
      cpd.lang_flags = LANG_CPP;
   }

   file_mem fm;
   fm.raw.clear();
   fm.data.clear();
   fm.enc = ENC_ASCII;
   fm.raw = vector<UINT8>(file.begin(), file.end());

   if (!decode_unicode(fm.raw, fm.data, fm.enc, fm.bom))
   {
      LOG_FMT(LERR, "Failed to read code\n");
      return("");
   }

   cpd.filename = "stdin";

   /* Done reading from stdin */
   LOG_FMT(LSYS, "Parsing: %d bytes (%d chars) from stdin as language %s\n",
           (int)fm.raw.size(), (int)fm.data.size(),
           language_name_from_flags(cpd.lang_flags));

   cpd.frag = frag;

   char   *buf;
   size_t len;

   // TODO (upstream): uncrustify uses FILE instead of streams for its outputs
   // to redirect FILE writes into a char* open_memstream is used
   // windows lacks open_memstream, only UNIX/BSD is supported
   // apparently emscripten has its own implementation, if that is not working
   // see: stackoverflow.com/questions/10305095#answer-10341073
   FILE *stream = open_memstream(&buf, &len);
   if (stream == NULL)
   {
      LOG_FMT(LERR, "Failed to open_memstream\n");
      fflush(stream);
      fclose(stream);
      free(buf);
      return("");
   }

   // TODO (upstream) One way to implement the --parsed, -p functionality would
   // be to let the uncrustify_file function run, throw away the formated
   // output and return the debug as a string. For this uncrustify_file would
   // need to accept a stream, FILE or a char array pointer in which the output
   // will be stored.
   // Another option would be to check, inside the uncrustify_file function,
   // if the current filename sting matches stdout or stderr and use those as
   // output locations. This is the easier fix but the debug info in the
   // browsers console is littered with other unneeded text.
   // Finally, the ugliest solution, would be also possible to re route
   // either stdout or stderr inside the Module var of emscripten to a js
   // function which passes the debug output into a dedicated output js target.
   // This therefore would introduce the dependency on the user to always have
   // the output js target available.
   uncrustify_file(fm, stream, NULL);

   fflush(stream);
   fclose(stream);

   string out(buf);
   free(buf);

   if (cpd.error_count != 0)
   {
      LOG_FMT(LWARN, "%d errors occurred during formating\n", cpd.error_count);
   }
   return(out);
} // uncrustify


/**
 * format string with specified language
 *
 * @param file: string that is going to be formated
 * @param frag: true=fragmented code input, false=unfragmented code input
 * @param langIDX: specifies in which language the input file is written
 * @return formated string
 */
string uncrustify(string file, bool frag, lang_flags langIDX)
{
   auto tmpLang = cpd.lang_flags;

   cpd.lang_flags = static_cast<int>(langIDX);

   auto ret = uncrustify(file, frag);

   cpd.lang_flags = tmpLang;

   // overload for default frag parameter
   return(ret);
}


/**
 * format string, assume unfragmented code input
 *
 * @param file: string that is going to be formated
 * @return formated string
 */
string uncrustify(string file)
{
   // overload for default frag parameter
   return(uncrustify(file, false));
}


//! helper function to access option_map_value::name
string option_map_value_name(const option_map_value &o)
{
   return((o.name != NULL) ? string(o.name) : "");
}


//! helper function to access option_map_value::short_desc
string option_map_value_sDesc(const option_map_value &o)
{
   return((o.short_desc != NULL) ? string(o.short_desc) : "");
}


//! helper function to access option_map_value::long_desc
string option_map_value_lDesc(const option_map_value &o)
{
   return((o.long_desc != NULL) ? string(o.long_desc) : "");
}


EMSCRIPTEN_BINDINGS(MainModule)
{
   enum_<uncrustify_options>(STRINGIFY(uncrustify_options))
      .value(STRINGIFY(UO_newlines), UO_newlines)
      .value(STRINGIFY(UO_tok_split_gte), UO_tok_split_gte)
      .value(STRINGIFY(UO_utf8_byte), UO_utf8_byte)
      .value(STRINGIFY(UO_utf8_force), UO_utf8_force)
      .value(STRINGIFY(UO_utf8_bom), UO_utf8_bom)
      .value(STRINGIFY(UO_enable_digraphs), UO_enable_digraphs)
      .value(STRINGIFY(UO_dont_protect_xcode_code_placeholders), UO_dont_protect_xcode_code_placeholders)
      .value(STRINGIFY(UO_input_tab_size), UO_input_tab_size)
      .value(STRINGIFY(UO_output_tab_size), UO_output_tab_size)
      .value(STRINGIFY(UO_indent_columns), UO_indent_columns)
      .value(STRINGIFY(UO_indent_continue), UO_indent_continue)
      .value(STRINGIFY(UO_indent_with_tabs), UO_indent_with_tabs)
      .value(STRINGIFY(UO_indent_cmt_with_tabs), UO_indent_cmt_with_tabs)
      .value(STRINGIFY(UO_indent_paren_nl), UO_indent_paren_nl)
      .value(STRINGIFY(UO_indent_square_nl), UO_indent_square_nl)
      .value(STRINGIFY(UO_indent_paren_close), UO_indent_paren_close)
      .value(STRINGIFY(UO_indent_comma_paren), UO_indent_comma_paren)
      .value(STRINGIFY(UO_indent_bool_paren), UO_indent_bool_paren)
      .value(STRINGIFY(UO_indent_first_bool_expr), UO_indent_first_bool_expr)
      .value(STRINGIFY(UO_pp_indent), UO_pp_indent)
      .value(STRINGIFY(UO_pp_indent_at_level), UO_pp_indent_at_level)
      .value(STRINGIFY(UO_pp_indent_count), UO_pp_indent_count)
      .value(STRINGIFY(UO_pp_define_at_level), UO_pp_define_at_level)
      .value(STRINGIFY(UO_pp_space), UO_pp_space)
      .value(STRINGIFY(UO_pp_space_count), UO_pp_space_count)
      .value(STRINGIFY(UO_pp_indent_region), UO_pp_indent_region)
      .value(STRINGIFY(UO_pp_region_indent_code), UO_pp_region_indent_code)
      .value(STRINGIFY(UO_pp_indent_if), UO_pp_indent_if)
      .value(STRINGIFY(UO_pp_if_indent_code), UO_pp_if_indent_code)
      .value(STRINGIFY(UO_indent_switch_case), UO_indent_switch_case)
      .value(STRINGIFY(UO_indent_case_shift), UO_indent_case_shift)
      .value(STRINGIFY(UO_indent_case_brace), UO_indent_case_brace)
      .value(STRINGIFY(UO_indent_brace), UO_indent_brace)
      .value(STRINGIFY(UO_indent_braces), UO_indent_braces)
      .value(STRINGIFY(UO_indent_braces_no_func), UO_indent_braces_no_func)
      .value(STRINGIFY(UO_indent_braces_no_class), UO_indent_braces_no_class)
      .value(STRINGIFY(UO_indent_braces_no_struct), UO_indent_braces_no_struct)
      .value(STRINGIFY(UO_indent_brace_parent), UO_indent_brace_parent)
      .value(STRINGIFY(UO_indent_paren_open_brace), UO_indent_paren_open_brace)
      .value(STRINGIFY(UO_indent_cs_delegate_brace), UO_indent_cs_delegate_brace)
      .value(STRINGIFY(UO_indent_label), UO_indent_label)
      .value(STRINGIFY(UO_indent_access_spec), UO_indent_access_spec)
      .value(STRINGIFY(UO_indent_access_spec_body), UO_indent_access_spec_body)
      .value(STRINGIFY(UO_indent_align_string), UO_indent_align_string)
      .value(STRINGIFY(UO_indent_xml_string), UO_indent_xml_string)
      .value(STRINGIFY(UO_indent_col1_comment), UO_indent_col1_comment)
      .value(STRINGIFY(UO_indent_func_def_force_col1), UO_indent_func_def_force_col1)
      .value(STRINGIFY(UO_indent_func_call_param), UO_indent_func_call_param)
      .value(STRINGIFY(UO_indent_func_proto_param), UO_indent_func_proto_param)
      .value(STRINGIFY(UO_indent_func_def_param), UO_indent_func_def_param)
      .value(STRINGIFY(UO_indent_func_class_param), UO_indent_func_class_param)
      .value(STRINGIFY(UO_indent_func_ctor_var_param), UO_indent_func_ctor_var_param)
      .value(STRINGIFY(UO_indent_template_param), UO_indent_template_param)
      .value(STRINGIFY(UO_indent_func_param_double), UO_indent_func_param_double)
      .value(STRINGIFY(UO_indent_func_const), UO_indent_func_const)
      .value(STRINGIFY(UO_indent_func_throw), UO_indent_func_throw)
      .value(STRINGIFY(UO_indent_namespace), UO_indent_namespace)
      .value(STRINGIFY(UO_indent_namespace_single_indent), UO_indent_namespace_single_indent)
      .value(STRINGIFY(UO_indent_namespace_level), UO_indent_namespace_level)
      .value(STRINGIFY(UO_indent_namespace_limit), UO_indent_namespace_limit)
      .value(STRINGIFY(UO_indent_extern), UO_indent_extern)
      .value(STRINGIFY(UO_indent_class), UO_indent_class)
      .value(STRINGIFY(UO_indent_class_colon), UO_indent_class_colon)
      .value(STRINGIFY(UO_indent_class_on_colon), UO_indent_class_on_colon)
      .value(STRINGIFY(UO_indent_constr_colon), UO_indent_constr_colon)
      .value(STRINGIFY(UO_indent_ctor_init_leading), UO_indent_ctor_init_leading)
      .value(STRINGIFY(UO_indent_ctor_init), UO_indent_ctor_init)
      .value(STRINGIFY(UO_indent_member), UO_indent_member)
      .value(STRINGIFY(UO_indent_sing_line_comments), UO_indent_sing_line_comments)
      .value(STRINGIFY(UO_indent_relative_single_line_comments), UO_indent_relative_single_line_comments)
      .value(STRINGIFY(UO_indent_preserve_sql), UO_indent_preserve_sql)
      .value(STRINGIFY(UO_indent_align_assign), UO_indent_align_assign)
      .value(STRINGIFY(UO_indent_oc_block), UO_indent_oc_block)
      .value(STRINGIFY(UO_indent_oc_block_msg), UO_indent_oc_block_msg)
      .value(STRINGIFY(UO_indent_oc_msg_prioritize_first_colon), UO_indent_oc_msg_prioritize_first_colon)
      .value(STRINGIFY(UO_indent_oc_msg_colon), UO_indent_oc_msg_colon)
      .value(STRINGIFY(UO_indent_oc_block_msg_xcode_style), UO_indent_oc_block_msg_xcode_style)
      .value(STRINGIFY(UO_indent_oc_block_msg_from_brace), UO_indent_oc_block_msg_from_brace)
      .value(STRINGIFY(UO_indent_oc_block_msg_from_caret), UO_indent_oc_block_msg_from_caret)
      .value(STRINGIFY(UO_indent_oc_block_msg_from_colon), UO_indent_oc_block_msg_from_colon)
      .value(STRINGIFY(UO_indent_oc_block_msg_from_keyword), UO_indent_oc_block_msg_from_keyword)
      .value(STRINGIFY(UO_indent_else_if), UO_indent_else_if)
      .value(STRINGIFY(UO_indent_var_def_blk), UO_indent_var_def_blk)
      .value(STRINGIFY(UO_indent_var_def_cont), UO_indent_var_def_cont)
      .value(STRINGIFY(UO_indent_shift), UO_indent_shift)
      .value(STRINGIFY(UO_indent_min_vbrace_open), UO_indent_min_vbrace_open)
      .value(STRINGIFY(UO_indent_vbrace_open_on_tabstop), UO_indent_vbrace_open_on_tabstop)
      .value(STRINGIFY(UO_indent_token_after_brace), UO_indent_token_after_brace)
      .value(STRINGIFY(UO_indent_cpp_lambda_body), UO_indent_cpp_lambda_body)
      .value(STRINGIFY(UO_indent_using_block), UO_indent_using_block)
      .value(STRINGIFY(UO_indent_ternary_operator), UO_indent_ternary_operator)
      .value(STRINGIFY(UO_sp_paren_brace), UO_sp_paren_brace)
      .value(STRINGIFY(UO_sp_fparen_brace), UO_sp_fparen_brace)
      .value(STRINGIFY(UO_sp_fparen_dbrace), UO_sp_fparen_dbrace)
      .value(STRINGIFY(UO_sp_sparen_brace), UO_sp_sparen_brace)
      .value(STRINGIFY(UO_sp_after_cast), UO_sp_after_cast)
      .value(STRINGIFY(UO_sp_inside_paren_cast), UO_sp_inside_paren_cast)
      .value(STRINGIFY(UO_sp_cpp_cast_paren), UO_sp_cpp_cast_paren)
      .value(STRINGIFY(UO_sp_before_byref), UO_sp_before_byref)
      .value(STRINGIFY(UO_sp_before_unnamed_byref), UO_sp_before_unnamed_byref)
      .value(STRINGIFY(UO_sp_after_byref), UO_sp_after_byref)
      .value(STRINGIFY(UO_sp_after_type), UO_sp_after_type)
      .value(STRINGIFY(UO_sp_before_template_paren), UO_sp_before_template_paren)
      .value(STRINGIFY(UO_sp_inside_fparen), UO_sp_inside_fparen)
      .value(STRINGIFY(UO_sp_inside_fparens), UO_sp_inside_fparens)
      .value(STRINGIFY(UO_sp_inside_tparen), UO_sp_inside_tparen)
      .value(STRINGIFY(UO_sp_after_tparen_close), UO_sp_after_tparen_close)
      .value(STRINGIFY(UO_sp_inside_paren), UO_sp_inside_paren)
      .value(STRINGIFY(UO_sp_inside_square), UO_sp_inside_square)
      .value(STRINGIFY(UO_sp_inside_sparen), UO_sp_inside_sparen)
      .value(STRINGIFY(UO_sp_inside_sparen_close), UO_sp_inside_sparen_close)
      .value(STRINGIFY(UO_sp_inside_sparen_open), UO_sp_inside_sparen_open)
      .value(STRINGIFY(UO_sp_inside_angle), UO_sp_inside_angle)
      .value(STRINGIFY(UO_sp_before_sparen), UO_sp_before_sparen)
      .value(STRINGIFY(UO_sp_after_sparen), UO_sp_after_sparen)
      .value(STRINGIFY(UO_sp_after_invariant_paren), UO_sp_after_invariant_paren)
      .value(STRINGIFY(UO_sp_invariant_paren), UO_sp_invariant_paren)
      .value(STRINGIFY(UO_sp_template_angle), UO_sp_template_angle)
      .value(STRINGIFY(UO_sp_before_angle), UO_sp_before_angle)
      .value(STRINGIFY(UO_sp_after_angle), UO_sp_after_angle)
      .value(STRINGIFY(UO_sp_angle_paren), UO_sp_angle_paren)
      .value(STRINGIFY(UO_sp_angle_paren_empty), UO_sp_angle_paren_empty)
      .value(STRINGIFY(UO_sp_angle_word), UO_sp_angle_word)
      .value(STRINGIFY(UO_sp_angle_shift), UO_sp_angle_shift)
      .value(STRINGIFY(UO_sp_permit_cpp11_shift), UO_sp_permit_cpp11_shift)
      .value(STRINGIFY(UO_sp_before_square), UO_sp_before_square)
      .value(STRINGIFY(UO_sp_before_squares), UO_sp_before_squares)
      .value(STRINGIFY(UO_sp_paren_paren), UO_sp_paren_paren)
      .value(STRINGIFY(UO_sp_cparen_oparen), UO_sp_cparen_oparen)
      .value(STRINGIFY(UO_sp_balance_nested_parens), UO_sp_balance_nested_parens)
      .value(STRINGIFY(UO_sp_return_paren), UO_sp_return_paren)
      .value(STRINGIFY(UO_sp_sizeof_paren), UO_sp_sizeof_paren)
      .value(STRINGIFY(UO_sp_after_comma), UO_sp_after_comma)
      .value(STRINGIFY(UO_sp_before_comma), UO_sp_before_comma)
      .value(STRINGIFY(UO_sp_paren_comma), UO_sp_paren_comma)
      .value(STRINGIFY(UO_sp_before_mdatype_commas), UO_sp_before_mdatype_commas)
      .value(STRINGIFY(UO_sp_between_mdatype_commas), UO_sp_between_mdatype_commas)
      .value(STRINGIFY(UO_sp_after_mdatype_commas), UO_sp_after_mdatype_commas)
      .value(STRINGIFY(UO_sp_before_ellipsis), UO_sp_before_ellipsis)
      .value(STRINGIFY(UO_sp_arith), UO_sp_arith)
      .value(STRINGIFY(UO_sp_bool), UO_sp_bool)
      .value(STRINGIFY(UO_sp_pp_concat), UO_sp_pp_concat)
      .value(STRINGIFY(UO_sp_pp_stringify), UO_sp_pp_stringify)
      .value(STRINGIFY(UO_sp_before_pp_stringify), UO_sp_before_pp_stringify)
      .value(STRINGIFY(UO_sp_compare), UO_sp_compare)
      .value(STRINGIFY(UO_sp_assign), UO_sp_assign)
      .value(STRINGIFY(UO_sp_cpp_lambda_assign), UO_sp_cpp_lambda_assign)
      .value(STRINGIFY(UO_sp_cpp_lambda_paren), UO_sp_cpp_lambda_paren)
      .value(STRINGIFY(UO_sp_assign_default), UO_sp_assign_default)
      .value(STRINGIFY(UO_sp_before_assign), UO_sp_before_assign)
      .value(STRINGIFY(UO_sp_after_assign), UO_sp_after_assign)
      .value(STRINGIFY(UO_sp_enum_paren), UO_sp_enum_paren)
      .value(STRINGIFY(UO_sp_enum_assign), UO_sp_enum_assign)
      .value(STRINGIFY(UO_sp_enum_before_assign), UO_sp_enum_before_assign)
      .value(STRINGIFY(UO_sp_enum_after_assign), UO_sp_enum_after_assign)
      .value(STRINGIFY(UO_sp_after_class_colon), UO_sp_after_class_colon)
      .value(STRINGIFY(UO_sp_before_class_colon), UO_sp_before_class_colon)
      .value(STRINGIFY(UO_sp_after_constr_colon), UO_sp_after_constr_colon)
      .value(STRINGIFY(UO_sp_before_constr_colon), UO_sp_before_constr_colon)
      .value(STRINGIFY(UO_sp_before_case_colon), UO_sp_before_case_colon)
      .value(STRINGIFY(UO_sp_func_def_paren), UO_sp_func_def_paren)
      .value(STRINGIFY(UO_sp_func_def_paren_empty), UO_sp_func_def_paren_empty)
      .value(STRINGIFY(UO_sp_func_call_paren), UO_sp_func_call_paren)
      .value(STRINGIFY(UO_sp_func_call_paren_empty), UO_sp_func_call_paren_empty)
      .value(STRINGIFY(UO_sp_func_call_user_paren), UO_sp_func_call_user_paren)
      .value(STRINGIFY(UO_sp_func_proto_paren), UO_sp_func_proto_paren)
      .value(STRINGIFY(UO_sp_func_proto_paren_empty), UO_sp_func_proto_paren_empty)
      .value(STRINGIFY(UO_sp_func_class_paren), UO_sp_func_class_paren)
      .value(STRINGIFY(UO_sp_func_class_paren_empty), UO_sp_func_class_paren_empty)
      .value(STRINGIFY(UO_sp_attribute_paren), UO_sp_attribute_paren)
      .value(STRINGIFY(UO_sp_defined_paren), UO_sp_defined_paren)
      .value(STRINGIFY(UO_sp_throw_paren), UO_sp_throw_paren)
      .value(STRINGIFY(UO_sp_after_throw), UO_sp_after_throw)
      .value(STRINGIFY(UO_sp_catch_paren), UO_sp_catch_paren)
      .value(STRINGIFY(UO_sp_version_paren), UO_sp_version_paren)
      .value(STRINGIFY(UO_sp_scope_paren), UO_sp_scope_paren)
      .value(STRINGIFY(UO_sp_super_paren), UO_sp_super_paren)
      .value(STRINGIFY(UO_sp_this_paren), UO_sp_this_paren)
      .value(STRINGIFY(UO_sp_type_func), UO_sp_type_func)
      .value(STRINGIFY(UO_sp_before_ptr_star), UO_sp_before_ptr_star)
      .value(STRINGIFY(UO_sp_before_unnamed_ptr_star), UO_sp_before_unnamed_ptr_star)
      .value(STRINGIFY(UO_sp_after_ptr_star), UO_sp_after_ptr_star)
      .value(STRINGIFY(UO_sp_after_ptr_star_qualifier), UO_sp_after_ptr_star_qualifier)
      .value(STRINGIFY(UO_sp_after_ptr_star_func), UO_sp_after_ptr_star_func)
      .value(STRINGIFY(UO_sp_ptr_star_paren), UO_sp_ptr_star_paren)
      .value(STRINGIFY(UO_sp_before_ptr_star_func), UO_sp_before_ptr_star_func)
      .value(STRINGIFY(UO_sp_after_byref_func), UO_sp_after_byref_func)
      .value(STRINGIFY(UO_sp_before_byref_func), UO_sp_before_byref_func)
      .value(STRINGIFY(UO_sp_between_ptr_star), UO_sp_between_ptr_star)
      .value(STRINGIFY(UO_sp_special_semi), UO_sp_special_semi)
      .value(STRINGIFY(UO_sp_before_semi), UO_sp_before_semi)
      .value(STRINGIFY(UO_sp_before_semi_for), UO_sp_before_semi_for)
      .value(STRINGIFY(UO_sp_before_semi_for_empty), UO_sp_before_semi_for_empty)
      .value(STRINGIFY(UO_sp_after_semi), UO_sp_after_semi)
      .value(STRINGIFY(UO_sp_after_semi_for), UO_sp_after_semi_for)
      .value(STRINGIFY(UO_sp_after_semi_for_empty), UO_sp_after_semi_for_empty)
      .value(STRINGIFY(UO_sp_inside_braces), UO_sp_inside_braces)
      .value(STRINGIFY(UO_sp_inside_braces_empty), UO_sp_inside_braces_empty)
      .value(STRINGIFY(UO_sp_inside_braces_enum), UO_sp_inside_braces_enum)
      .value(STRINGIFY(UO_sp_inside_braces_struct), UO_sp_inside_braces_struct)
      .value(STRINGIFY(UO_sp_macro), UO_sp_macro)
      .value(STRINGIFY(UO_sp_macro_func), UO_sp_macro_func)
      .value(STRINGIFY(UO_sp_square_fparen), UO_sp_square_fparen)
      .value(STRINGIFY(UO_sp_after_tag), UO_sp_after_tag)
      .value(STRINGIFY(UO_sp_after_operator), UO_sp_after_operator)
      .value(STRINGIFY(UO_sp_after_operator_sym), UO_sp_after_operator_sym)
      .value(STRINGIFY(UO_sp_after_operator_sym_empty), UO_sp_after_operator_sym_empty)
      .value(STRINGIFY(UO_sp_else_brace), UO_sp_else_brace)
      .value(STRINGIFY(UO_sp_brace_else), UO_sp_brace_else)
      .value(STRINGIFY(UO_sp_brace_typedef), UO_sp_brace_typedef)
      .value(STRINGIFY(UO_sp_catch_brace), UO_sp_catch_brace)
      .value(STRINGIFY(UO_sp_brace_catch), UO_sp_brace_catch)
      .value(STRINGIFY(UO_sp_finally_brace), UO_sp_finally_brace)
      .value(STRINGIFY(UO_sp_brace_finally), UO_sp_brace_finally)
      .value(STRINGIFY(UO_sp_try_brace), UO_sp_try_brace)
      .value(STRINGIFY(UO_sp_getset_brace), UO_sp_getset_brace)
      .value(STRINGIFY(UO_sp_word_brace), UO_sp_word_brace)
      .value(STRINGIFY(UO_sp_word_brace_ns), UO_sp_word_brace_ns)
      .value(STRINGIFY(UO_sp_before_dc), UO_sp_before_dc)
      .value(STRINGIFY(UO_sp_after_dc), UO_sp_after_dc)
      .value(STRINGIFY(UO_sp_d_array_colon), UO_sp_d_array_colon)
      .value(STRINGIFY(UO_sp_not), UO_sp_not)
      .value(STRINGIFY(UO_sp_inv), UO_sp_inv)
      .value(STRINGIFY(UO_sp_addr), UO_sp_addr)
      .value(STRINGIFY(UO_sp_deref), UO_sp_deref)
      .value(STRINGIFY(UO_sp_member), UO_sp_member)
      .value(STRINGIFY(UO_sp_sign), UO_sp_sign)
      .value(STRINGIFY(UO_sp_incdec), UO_sp_incdec)
      .value(STRINGIFY(UO_sp_before_nl_cont), UO_sp_before_nl_cont)
      .value(STRINGIFY(UO_sp_after_oc_scope), UO_sp_after_oc_scope)
      .value(STRINGIFY(UO_sp_before_oc_colon), UO_sp_before_oc_colon)
      .value(STRINGIFY(UO_sp_after_oc_colon), UO_sp_after_oc_colon)
      .value(STRINGIFY(UO_sp_before_oc_dict_colon), UO_sp_before_oc_dict_colon)
      .value(STRINGIFY(UO_sp_after_oc_dict_colon), UO_sp_after_oc_dict_colon)
      .value(STRINGIFY(UO_sp_before_send_oc_colon), UO_sp_before_send_oc_colon)
      .value(STRINGIFY(UO_sp_after_send_oc_colon), UO_sp_after_send_oc_colon)
      .value(STRINGIFY(UO_sp_after_oc_type), UO_sp_after_oc_type)
      .value(STRINGIFY(UO_sp_after_oc_return_type), UO_sp_after_oc_return_type)
      .value(STRINGIFY(UO_sp_after_oc_at_sel), UO_sp_after_oc_at_sel)
      .value(STRINGIFY(UO_sp_after_oc_at_sel_parens), UO_sp_after_oc_at_sel_parens)
      .value(STRINGIFY(UO_sp_inside_oc_at_sel_parens), UO_sp_inside_oc_at_sel_parens)
      .value(STRINGIFY(UO_sp_before_oc_block_caret), UO_sp_before_oc_block_caret)
      .value(STRINGIFY(UO_sp_after_oc_block_caret), UO_sp_after_oc_block_caret)
      .value(STRINGIFY(UO_sp_after_oc_msg_receiver), UO_sp_after_oc_msg_receiver)
      .value(STRINGIFY(UO_sp_after_oc_property), UO_sp_after_oc_property)
      .value(STRINGIFY(UO_sp_cond_colon), UO_sp_cond_colon)
      .value(STRINGIFY(UO_sp_cond_colon_before), UO_sp_cond_colon_before)
      .value(STRINGIFY(UO_sp_cond_colon_after), UO_sp_cond_colon_after)
      .value(STRINGIFY(UO_sp_cond_question), UO_sp_cond_question)
      .value(STRINGIFY(UO_sp_cond_question_before), UO_sp_cond_question_before)
      .value(STRINGIFY(UO_sp_cond_question_after), UO_sp_cond_question_after)
      .value(STRINGIFY(UO_sp_cond_ternary_short), UO_sp_cond_ternary_short)
      .value(STRINGIFY(UO_sp_case_label), UO_sp_case_label)
      .value(STRINGIFY(UO_sp_range), UO_sp_range)
      .value(STRINGIFY(UO_sp_cmt_cpp_start), UO_sp_cmt_cpp_start)
      .value(STRINGIFY(UO_sp_cmt_cpp_doxygen), UO_sp_cmt_cpp_doxygen)
      .value(STRINGIFY(UO_sp_cmt_cpp_qttr), UO_sp_cmt_cpp_qttr)
      .value(STRINGIFY(UO_sp_endif_cmt), UO_sp_endif_cmt)
      .value(STRINGIFY(UO_sp_after_new), UO_sp_after_new)
      .value(STRINGIFY(UO_sp_between_new_paren), UO_sp_between_new_paren)
      .value(STRINGIFY(UO_sp_before_tr_emb_cmt), UO_sp_before_tr_emb_cmt)
      .value(STRINGIFY(UO_sp_num_before_tr_emb_cmt), UO_sp_num_before_tr_emb_cmt)
      .value(STRINGIFY(UO_sp_annotation_paren), UO_sp_annotation_paren)
      .value(STRINGIFY(UO_sp_after_for_colon), UO_sp_after_for_colon)
      .value(STRINGIFY(UO_sp_before_for_colon), UO_sp_before_for_colon)
      .value(STRINGIFY(UO_sp_extern_paren), UO_sp_extern_paren)
      .value(STRINGIFY(UO_sp_skip_vbrace_tokens), UO_sp_skip_vbrace_tokens)
      .value(STRINGIFY(UO_code_width), UO_code_width)
      .value(STRINGIFY(UO_ls_for_split_full), UO_ls_for_split_full)
      .value(STRINGIFY(UO_ls_func_split_full), UO_ls_func_split_full)
      .value(STRINGIFY(UO_ls_code_width), UO_ls_code_width)
      .value(STRINGIFY(UO_align_with_tabs), UO_align_with_tabs)
      .value(STRINGIFY(UO_align_keep_tabs), UO_align_keep_tabs)
      .value(STRINGIFY(UO_align_on_tabstop), UO_align_on_tabstop)
      .value(STRINGIFY(UO_align_nl_cont), UO_align_nl_cont)
      .value(STRINGIFY(UO_align_enum_equ_span), UO_align_enum_equ_span)
      .value(STRINGIFY(UO_align_enum_equ_thresh), UO_align_enum_equ_thresh)
      .value(STRINGIFY(UO_align_assign_span), UO_align_assign_span)
      .value(STRINGIFY(UO_align_assign_thresh), UO_align_assign_thresh)
      .value(STRINGIFY(UO_align_right_cmt_span), UO_align_right_cmt_span)
      .value(STRINGIFY(UO_align_right_cmt_mix), UO_align_right_cmt_mix)
      .value(STRINGIFY(UO_align_right_cmt_gap), UO_align_right_cmt_gap)
      .value(STRINGIFY(UO_align_right_cmt_at_col), UO_align_right_cmt_at_col)
      .value(STRINGIFY(UO_align_func_params), UO_align_func_params)
      .value(STRINGIFY(UO_align_same_func_call_params), UO_align_same_func_call_params)
      .value(STRINGIFY(UO_align_var_def_span), UO_align_var_def_span)
      .value(STRINGIFY(UO_align_var_def_thresh), UO_align_var_def_thresh)
      .value(STRINGIFY(UO_align_var_def_gap), UO_align_var_def_gap)
      .value(STRINGIFY(UO_align_var_def_colon_gap), UO_align_var_def_colon_gap)
      .value(STRINGIFY(UO_align_var_def_inline), UO_align_var_def_inline)
      .value(STRINGIFY(UO_align_var_def_star_style), UO_align_var_def_star_style)
      .value(STRINGIFY(UO_align_var_def_amp_style), UO_align_var_def_amp_style)
      .value(STRINGIFY(UO_align_var_def_colon), UO_align_var_def_colon)
      .value(STRINGIFY(UO_align_var_def_attribute), UO_align_var_def_attribute)
      .value(STRINGIFY(UO_align_var_class_span), UO_align_var_class_span)
      .value(STRINGIFY(UO_align_var_class_thresh), UO_align_var_class_thresh)
      .value(STRINGIFY(UO_align_var_class_gap), UO_align_var_class_gap)
      .value(STRINGIFY(UO_align_var_struct_span), UO_align_var_struct_span)
      .value(STRINGIFY(UO_align_var_struct_thresh), UO_align_var_struct_thresh)
      .value(STRINGIFY(UO_align_var_struct_gap), UO_align_var_struct_gap)
      .value(STRINGIFY(UO_align_pp_define_together), UO_align_pp_define_together)
      .value(STRINGIFY(UO_align_pp_define_span), UO_align_pp_define_span)
      .value(STRINGIFY(UO_align_pp_define_gap), UO_align_pp_define_gap)
      .value(STRINGIFY(UO_align_struct_init_span), UO_align_struct_init_span)
      .value(STRINGIFY(UO_align_func_proto_span), UO_align_func_proto_span)
      .value(STRINGIFY(UO_align_func_proto_gap), UO_align_func_proto_gap)
      .value(STRINGIFY(UO_align_on_operator), UO_align_on_operator)
      .value(STRINGIFY(UO_align_mix_var_proto), UO_align_mix_var_proto)
      .value(STRINGIFY(UO_align_single_line_func), UO_align_single_line_func)
      .value(STRINGIFY(UO_align_single_line_brace), UO_align_single_line_brace)
      .value(STRINGIFY(UO_align_single_line_brace_gap), UO_align_single_line_brace_gap)
      .value(STRINGIFY(UO_align_oc_msg_spec_span), UO_align_oc_msg_spec_span)
      .value(STRINGIFY(UO_align_number_left), UO_align_number_left)
      .value(STRINGIFY(UO_align_typedef_span), UO_align_typedef_span)
      .value(STRINGIFY(UO_align_typedef_gap), UO_align_typedef_gap)
      .value(STRINGIFY(UO_align_typedef_func), UO_align_typedef_func)
      .value(STRINGIFY(UO_align_typedef_star_style), UO_align_typedef_star_style)
      .value(STRINGIFY(UO_align_typedef_amp_style), UO_align_typedef_amp_style)
      .value(STRINGIFY(UO_align_left_shift), UO_align_left_shift)
      .value(STRINGIFY(UO_align_asm_colon), UO_align_asm_colon)
      .value(STRINGIFY(UO_align_oc_msg_colon_span), UO_align_oc_msg_colon_span)
      .value(STRINGIFY(UO_align_oc_msg_colon_first), UO_align_oc_msg_colon_first)
      .value(STRINGIFY(UO_align_oc_decl_colon), UO_align_oc_decl_colon)
      .value(STRINGIFY(UO_align_keep_extra_space), UO_align_keep_extra_space)
      .value(STRINGIFY(UO_nl_fdef_brace), UO_nl_fdef_brace)
      .value(STRINGIFY(UO_nl_cpp_ldef_brace), UO_nl_cpp_ldef_brace)
      .value(STRINGIFY(UO_nl_func_paren), UO_nl_func_paren)
      .value(STRINGIFY(UO_nl_func_def_paren), UO_nl_func_def_paren)
      .value(STRINGIFY(UO_nl_func_decl_start), UO_nl_func_decl_start)
      .value(STRINGIFY(UO_nl_func_def_start), UO_nl_func_def_start)
      .value(STRINGIFY(UO_nl_func_decl_start_single), UO_nl_func_decl_start_single)
      .value(STRINGIFY(UO_nl_func_def_start_single), UO_nl_func_def_start_single)
      .value(STRINGIFY(UO_nl_func_decl_start_multi_line), UO_nl_func_decl_start_multi_line)
      .value(STRINGIFY(UO_nl_func_def_start_multi_line), UO_nl_func_def_start_multi_line)
      .value(STRINGIFY(UO_nl_func_decl_args), UO_nl_func_decl_args)
      .value(STRINGIFY(UO_nl_func_def_args), UO_nl_func_def_args)
      .value(STRINGIFY(UO_nl_func_decl_args_multi_line), UO_nl_func_decl_args_multi_line)
      .value(STRINGIFY(UO_nl_func_def_args_multi_line), UO_nl_func_def_args_multi_line)
      .value(STRINGIFY(UO_nl_func_decl_end), UO_nl_func_decl_end)
      .value(STRINGIFY(UO_nl_func_def_end), UO_nl_func_def_end)
      .value(STRINGIFY(UO_nl_func_decl_end_single), UO_nl_func_decl_end_single)
      .value(STRINGIFY(UO_nl_func_def_end_single), UO_nl_func_def_end_single)
      .value(STRINGIFY(UO_nl_func_decl_end_multi_line), UO_nl_func_decl_end_multi_line)
      .value(STRINGIFY(UO_nl_func_def_end_multi_line), UO_nl_func_def_end_multi_line)
      .value(STRINGIFY(UO_nl_func_decl_empty), UO_nl_func_decl_empty)
      .value(STRINGIFY(UO_nl_func_def_empty), UO_nl_func_def_empty)
      .value(STRINGIFY(UO_nl_func_type_name), UO_nl_func_type_name)
      .value(STRINGIFY(UO_nl_func_type_name_class), UO_nl_func_type_name_class)
      .value(STRINGIFY(UO_nl_func_class_scope), UO_nl_func_class_scope)
      .value(STRINGIFY(UO_nl_func_scope_name), UO_nl_func_scope_name)
      .value(STRINGIFY(UO_nl_func_proto_type_name), UO_nl_func_proto_type_name)
      .value(STRINGIFY(UO_nl_func_call_start_multi_line), UO_nl_func_call_start_multi_line)
      .value(STRINGIFY(UO_nl_func_call_args_multi_line), UO_nl_func_call_args_multi_line)
      .value(STRINGIFY(UO_nl_func_call_end_multi_line), UO_nl_func_call_end_multi_line)
      .value(STRINGIFY(UO_nl_func_var_def_blk), UO_nl_func_var_def_blk)
      .value(STRINGIFY(UO_nl_typedef_blk_start), UO_nl_typedef_blk_start)
      .value(STRINGIFY(UO_nl_typedef_blk_end), UO_nl_typedef_blk_end)
      .value(STRINGIFY(UO_nl_typedef_blk_in), UO_nl_typedef_blk_in)
      .value(STRINGIFY(UO_nl_var_def_blk_start), UO_nl_var_def_blk_start)
      .value(STRINGIFY(UO_nl_var_def_blk_end), UO_nl_var_def_blk_end)
      .value(STRINGIFY(UO_nl_var_def_blk_in), UO_nl_var_def_blk_in)
      .value(STRINGIFY(UO_nl_before_case), UO_nl_before_case)
      .value(STRINGIFY(UO_nl_before_throw), UO_nl_before_throw)
      .value(STRINGIFY(UO_nl_before_return), UO_nl_before_return)
      .value(STRINGIFY(UO_nl_after_return), UO_nl_after_return)
      .value(STRINGIFY(UO_nl_return_expr), UO_nl_return_expr)
      .value(STRINGIFY(UO_nl_after_annotation), UO_nl_after_annotation)
      .value(STRINGIFY(UO_nl_between_annotation), UO_nl_between_annotation)
      .value(STRINGIFY(UO_nl_after_case), UO_nl_after_case)
      .value(STRINGIFY(UO_nl_after_semicolon), UO_nl_after_semicolon)
      .value(STRINGIFY(UO_nl_paren_dbrace_open), UO_nl_paren_dbrace_open)
      .value(STRINGIFY(UO_nl_after_brace_open), UO_nl_after_brace_open)
      .value(STRINGIFY(UO_nl_after_brace_open_cmt), UO_nl_after_brace_open_cmt)
      .value(STRINGIFY(UO_nl_after_vbrace_open), UO_nl_after_vbrace_open)
      .value(STRINGIFY(UO_nl_after_vbrace_open_empty), UO_nl_after_vbrace_open_empty)
      .value(STRINGIFY(UO_nl_after_brace_close), UO_nl_after_brace_close)
      .value(STRINGIFY(UO_nl_after_vbrace_close), UO_nl_after_vbrace_close)
      .value(STRINGIFY(UO_nl_brace_struct_var), UO_nl_brace_struct_var)
      .value(STRINGIFY(UO_nl_fcall_brace), UO_nl_fcall_brace)
      .value(STRINGIFY(UO_nl_squeeze_ifdef), UO_nl_squeeze_ifdef)
      .value(STRINGIFY(UO_nl_squeeze_ifdef_top_level), UO_nl_squeeze_ifdef_top_level)
      .value(STRINGIFY(UO_nl_enum_brace), UO_nl_enum_brace)
      .value(STRINGIFY(UO_nl_struct_brace), UO_nl_struct_brace)
      .value(STRINGIFY(UO_nl_union_brace), UO_nl_union_brace)
      .value(STRINGIFY(UO_nl_assign_brace), UO_nl_assign_brace)
      .value(STRINGIFY(UO_nl_assign_square), UO_nl_assign_square)
      .value(STRINGIFY(UO_nl_after_square_assign), UO_nl_after_square_assign)
      .value(STRINGIFY(UO_nl_class_brace), UO_nl_class_brace)
      .value(STRINGIFY(UO_nl_namespace_brace), UO_nl_namespace_brace)
      .value(STRINGIFY(UO_nl_brace_brace), UO_nl_brace_brace)
      .value(STRINGIFY(UO_nl_do_brace), UO_nl_do_brace)
      .value(STRINGIFY(UO_nl_if_brace), UO_nl_if_brace)
      .value(STRINGIFY(UO_nl_for_brace), UO_nl_for_brace)
      .value(STRINGIFY(UO_nl_else_if), UO_nl_else_if)
      .value(STRINGIFY(UO_nl_else_brace), UO_nl_else_brace)
      .value(STRINGIFY(UO_nl_finally_brace), UO_nl_finally_brace)
      .value(STRINGIFY(UO_nl_brace_finally), UO_nl_brace_finally)
      .value(STRINGIFY(UO_nl_try_brace), UO_nl_try_brace)
      .value(STRINGIFY(UO_nl_getset_brace), UO_nl_getset_brace)
      .value(STRINGIFY(UO_nl_catch_brace), UO_nl_catch_brace)
      .value(STRINGIFY(UO_nl_brace_catch), UO_nl_brace_catch)
      .value(STRINGIFY(UO_nl_brace_square), UO_nl_brace_square)
      .value(STRINGIFY(UO_nl_brace_fparen), UO_nl_brace_fparen)
      .value(STRINGIFY(UO_nl_while_brace), UO_nl_while_brace)
      .value(STRINGIFY(UO_nl_unittest_brace), UO_nl_unittest_brace)
      .value(STRINGIFY(UO_nl_scope_brace), UO_nl_scope_brace)
      .value(STRINGIFY(UO_nl_version_brace), UO_nl_version_brace)
      .value(STRINGIFY(UO_nl_using_brace), UO_nl_using_brace)
      .value(STRINGIFY(UO_nl_switch_brace), UO_nl_switch_brace)
      .value(STRINGIFY(UO_nl_synchronized_brace), UO_nl_synchronized_brace)
      .value(STRINGIFY(UO_nl_brace_else), UO_nl_brace_else)
      .value(STRINGIFY(UO_nl_brace_while), UO_nl_brace_while)
      .value(STRINGIFY(UO_nl_multi_line_cond), UO_nl_multi_line_cond)
      .value(STRINGIFY(UO_nl_elseif_brace), UO_nl_elseif_brace)
      .value(STRINGIFY(UO_nl_multi_line_define), UO_nl_multi_line_define)
      .value(STRINGIFY(UO_nl_before_if), UO_nl_before_if)
      .value(STRINGIFY(UO_nl_after_if), UO_nl_after_if)
      .value(STRINGIFY(UO_nl_before_for), UO_nl_before_for)
      .value(STRINGIFY(UO_nl_after_for), UO_nl_after_for)
      .value(STRINGIFY(UO_nl_before_while), UO_nl_before_while)
      .value(STRINGIFY(UO_nl_after_while), UO_nl_after_while)
      .value(STRINGIFY(UO_nl_before_switch), UO_nl_before_switch)
      .value(STRINGIFY(UO_nl_after_switch), UO_nl_after_switch)
      .value(STRINGIFY(UO_nl_before_synchronized), UO_nl_before_synchronized)
      .value(STRINGIFY(UO_nl_after_synchronized), UO_nl_after_synchronized)
      .value(STRINGIFY(UO_nl_before_do), UO_nl_before_do)
      .value(STRINGIFY(UO_nl_after_do), UO_nl_after_do)
      .value(STRINGIFY(UO_nl_ds_struct_enum_cmt), UO_nl_ds_struct_enum_cmt)
      .value(STRINGIFY(UO_nl_ds_struct_enum_close_brace), UO_nl_ds_struct_enum_close_brace)
      .value(STRINGIFY(UO_nl_before_func_class_def), UO_nl_before_func_class_def)
      .value(STRINGIFY(UO_nl_before_func_class_proto), UO_nl_before_func_class_proto)
      .value(STRINGIFY(UO_nl_define_macro), UO_nl_define_macro)
      .value(STRINGIFY(UO_nl_start_of_file), UO_nl_start_of_file)
      .value(STRINGIFY(UO_nl_start_of_file_min), UO_nl_start_of_file_min)
      .value(STRINGIFY(UO_nl_end_of_file), UO_nl_end_of_file)
      .value(STRINGIFY(UO_nl_end_of_file_min), UO_nl_end_of_file_min)
      .value(STRINGIFY(UO_nl_class_colon), UO_nl_class_colon)
      .value(STRINGIFY(UO_nl_constr_colon), UO_nl_constr_colon)
      .value(STRINGIFY(UO_nl_class_init_args), UO_nl_class_init_args)
      .value(STRINGIFY(UO_nl_constr_init_args), UO_nl_constr_init_args)
      .value(STRINGIFY(UO_nl_enum_own_lines), UO_nl_enum_own_lines)
      .value(STRINGIFY(UO_nl_collapse_empty_body), UO_nl_collapse_empty_body)
      .value(STRINGIFY(UO_nl_class_leave_one_liners), UO_nl_class_leave_one_liners)
      .value(STRINGIFY(UO_nl_assign_leave_one_liners), UO_nl_assign_leave_one_liners)
      .value(STRINGIFY(UO_nl_enum_leave_one_liners), UO_nl_enum_leave_one_liners)
      .value(STRINGIFY(UO_nl_getset_leave_one_liners), UO_nl_getset_leave_one_liners)
      .value(STRINGIFY(UO_nl_func_leave_one_liners), UO_nl_func_leave_one_liners)
      .value(STRINGIFY(UO_nl_cpp_lambda_leave_one_liners), UO_nl_cpp_lambda_leave_one_liners)
      .value(STRINGIFY(UO_nl_if_leave_one_liners), UO_nl_if_leave_one_liners)
      .value(STRINGIFY(UO_nl_while_leave_one_liners), UO_nl_while_leave_one_liners)
      .value(STRINGIFY(UO_nl_case_colon_brace), UO_nl_case_colon_brace)
      .value(STRINGIFY(UO_nl_template_class), UO_nl_template_class)
      .value(STRINGIFY(UO_nl_create_if_one_liner), UO_nl_create_if_one_liner)
      .value(STRINGIFY(UO_nl_create_for_one_liner), UO_nl_create_for_one_liner)
      .value(STRINGIFY(UO_nl_create_while_one_liner), UO_nl_create_while_one_liner)
      .value(STRINGIFY(UO_nl_split_if_one_liner), UO_nl_split_if_one_liner)
      .value(STRINGIFY(UO_nl_split_for_one_liner), UO_nl_split_for_one_liner)
      .value(STRINGIFY(UO_nl_split_while_one_liner), UO_nl_split_while_one_liner)
      .value(STRINGIFY(UO_nl_oc_msg_args), UO_nl_oc_msg_args)
      .value(STRINGIFY(UO_nl_oc_msg_leave_one_liner), UO_nl_oc_msg_leave_one_liner)
      .value(STRINGIFY(UO_nl_oc_block_brace), UO_nl_oc_block_brace)
      .value(STRINGIFY(UO_pos_arith), UO_pos_arith)
      .value(STRINGIFY(UO_pos_assign), UO_pos_assign)
      .value(STRINGIFY(UO_pos_bool), UO_pos_bool)
      .value(STRINGIFY(UO_pos_compare), UO_pos_compare)
      .value(STRINGIFY(UO_pos_conditional), UO_pos_conditional)
      .value(STRINGIFY(UO_pos_comma), UO_pos_comma)
      .value(STRINGIFY(UO_pos_enum_comma), UO_pos_enum_comma)
      .value(STRINGIFY(UO_pos_class_comma), UO_pos_class_comma)
      .value(STRINGIFY(UO_pos_constr_comma), UO_pos_constr_comma)
      .value(STRINGIFY(UO_pos_class_colon), UO_pos_class_colon)
      .value(STRINGIFY(UO_pos_constr_colon), UO_pos_constr_colon)
      .value(STRINGIFY(UO_nl_before_block_comment), UO_nl_before_block_comment)
      .value(STRINGIFY(UO_nl_before_cpp_comment), UO_nl_before_cpp_comment)
      .value(STRINGIFY(UO_nl_before_c_comment), UO_nl_before_c_comment)
      .value(STRINGIFY(UO_nl_after_multiline_comment), UO_nl_after_multiline_comment)
      .value(STRINGIFY(UO_nl_after_label_colon), UO_nl_after_label_colon)
      .value(STRINGIFY(UO_nl_before_func_body_def), UO_nl_before_func_body_def)
      .value(STRINGIFY(UO_nl_before_func_body_proto), UO_nl_before_func_body_proto)
      .value(STRINGIFY(UO_nl_after_func_body), UO_nl_after_func_body)
      .value(STRINGIFY(UO_nl_after_func_body_class), UO_nl_after_func_body_class)
      .value(STRINGIFY(UO_nl_after_func_body_one_liner), UO_nl_after_func_body_one_liner)
      .value(STRINGIFY(UO_nl_after_func_proto), UO_nl_after_func_proto)
      .value(STRINGIFY(UO_nl_after_func_proto_group), UO_nl_after_func_proto_group)
      .value(STRINGIFY(UO_nl_after_func_class_proto), UO_nl_after_func_class_proto)
      .value(STRINGIFY(UO_nl_after_func_class_proto_group), UO_nl_after_func_class_proto_group)
      .value(STRINGIFY(UO_nl_after_struct), UO_nl_after_struct)
      .value(STRINGIFY(UO_nl_before_class), UO_nl_before_class)
      .value(STRINGIFY(UO_nl_after_class), UO_nl_after_class)
      .value(STRINGIFY(UO_nl_max), UO_nl_max)
      .value(STRINGIFY(UO_nl_before_access_spec), UO_nl_before_access_spec)
      .value(STRINGIFY(UO_nl_max_blank_in_func), UO_nl_max_blank_in_func)
      .value(STRINGIFY(UO_nl_after_access_spec), UO_nl_after_access_spec)
      .value(STRINGIFY(UO_nl_comment_func_def), UO_nl_comment_func_def)
      .value(STRINGIFY(UO_nl_after_try_catch_finally), UO_nl_after_try_catch_finally)
      .value(STRINGIFY(UO_nl_between_get_set), UO_nl_between_get_set)
      .value(STRINGIFY(UO_nl_around_cs_property), UO_nl_around_cs_property)
      .value(STRINGIFY(UO_nl_property_brace), UO_nl_property_brace)
      .value(STRINGIFY(UO_eat_blanks_after_open_brace), UO_eat_blanks_after_open_brace)
      .value(STRINGIFY(UO_eat_blanks_before_close_brace), UO_eat_blanks_before_close_brace)
      .value(STRINGIFY(UO_nl_remove_extra_newlines), UO_nl_remove_extra_newlines)
      .value(STRINGIFY(UO_mod_paren_on_return), UO_mod_paren_on_return)
      .value(STRINGIFY(UO_mod_full_brace_nl), UO_mod_full_brace_nl)
      .value(STRINGIFY(UO_mod_full_brace_if), UO_mod_full_brace_if)
      .value(STRINGIFY(UO_mod_full_brace_if_chain), UO_mod_full_brace_if_chain)
      .value(STRINGIFY(UO_mod_full_brace_if_chain_only), UO_mod_full_brace_if_chain_only)
      .value(STRINGIFY(UO_mod_full_brace_for), UO_mod_full_brace_for)
      .value(STRINGIFY(UO_mod_full_brace_do), UO_mod_full_brace_do)
      .value(STRINGIFY(UO_mod_full_brace_while), UO_mod_full_brace_while)
      .value(STRINGIFY(UO_mod_full_brace_using), UO_mod_full_brace_using)
      .value(STRINGIFY(UO_mod_pawn_semicolon), UO_mod_pawn_semicolon)
      .value(STRINGIFY(UO_mod_full_brace_function), UO_mod_full_brace_function)
      .value(STRINGIFY(UO_mod_full_paren_if_bool), UO_mod_full_paren_if_bool)
      .value(STRINGIFY(UO_mod_remove_extra_semicolon), UO_mod_remove_extra_semicolon)
      .value(STRINGIFY(UO_mod_add_long_function_closebrace_comment), UO_mod_add_long_function_closebrace_comment)
      .value(STRINGIFY(UO_mod_add_long_switch_closebrace_comment), UO_mod_add_long_switch_closebrace_comment)
      .value(STRINGIFY(UO_mod_add_long_ifdef_else_comment), UO_mod_add_long_ifdef_else_comment)
      .value(STRINGIFY(UO_mod_add_long_ifdef_endif_comment), UO_mod_add_long_ifdef_endif_comment)
      .value(STRINGIFY(UO_mod_sort_import), UO_mod_sort_import)
      .value(STRINGIFY(UO_mod_sort_using), UO_mod_sort_using)
      .value(STRINGIFY(UO_mod_sort_include), UO_mod_sort_include)
      .value(STRINGIFY(UO_mod_sort_oc_properties), UO_mod_sort_oc_properties)
      .value(STRINGIFY(UO_mod_move_case_break), UO_mod_move_case_break)
      .value(STRINGIFY(UO_mod_case_brace), UO_mod_case_brace)
      .value(STRINGIFY(UO_mod_remove_empty_return), UO_mod_remove_empty_return)
      .value(STRINGIFY(UO_mod_sort_oc_property_thread_safe_weight), UO_mod_sort_oc_property_thread_safe_weight)
      .value(STRINGIFY(UO_mod_sort_oc_property_readwrite_weight), UO_mod_sort_oc_property_readwrite_weight)
      .value(STRINGIFY(UO_mod_sort_oc_property_reference_weight), UO_mod_sort_oc_property_reference_weight)
      .value(STRINGIFY(UO_mod_sort_oc_property_getter_weight), UO_mod_sort_oc_property_getter_weight)
      .value(STRINGIFY(UO_mod_sort_oc_property_setter_weight), UO_mod_sort_oc_property_setter_weight)
      .value(STRINGIFY(UO_mod_sort_oc_property_nullability_weight), UO_mod_sort_oc_property_nullability_weight)
      .value(STRINGIFY(UO_cmt_width), UO_cmt_width)
      .value(STRINGIFY(UO_cmt_reflow_mode), UO_cmt_reflow_mode)
      .value(STRINGIFY(UO_cmt_indent_multi), UO_cmt_indent_multi)
      .value(STRINGIFY(UO_cmt_star_cont), UO_cmt_star_cont)
      .value(STRINGIFY(UO_cmt_sp_before_star_cont), UO_cmt_sp_before_star_cont)
      .value(STRINGIFY(UO_cmt_sp_after_star_cont), UO_cmt_sp_after_star_cont)
      .value(STRINGIFY(UO_cmt_cpp_to_c), UO_cmt_cpp_to_c)
      .value(STRINGIFY(UO_cmt_cpp_group), UO_cmt_cpp_group)
      .value(STRINGIFY(UO_cmt_c_group), UO_cmt_c_group)
      .value(STRINGIFY(UO_cmt_c_nl_start), UO_cmt_c_nl_start)
      .value(STRINGIFY(UO_cmt_c_nl_end), UO_cmt_c_nl_end)
      .value(STRINGIFY(UO_cmt_cpp_nl_start), UO_cmt_cpp_nl_start)
      .value(STRINGIFY(UO_cmt_cpp_nl_end), UO_cmt_cpp_nl_end)
      .value(STRINGIFY(UO_cmt_multi_check_last), UO_cmt_multi_check_last)
      .value(STRINGIFY(UO_cmt_multi_first_len_minimum), UO_cmt_multi_first_len_minimum)
      .value(STRINGIFY(UO_cmt_convert_tab_to_spaces), UO_cmt_convert_tab_to_spaces)
      .value(STRINGIFY(UO_cmt_insert_file_header), UO_cmt_insert_file_header)
      .value(STRINGIFY(UO_cmt_insert_file_footer), UO_cmt_insert_file_footer)
      .value(STRINGIFY(UO_cmt_insert_func_header), UO_cmt_insert_func_header)
      .value(STRINGIFY(UO_cmt_insert_class_header), UO_cmt_insert_class_header)
      .value(STRINGIFY(UO_cmt_insert_oc_msg_header), UO_cmt_insert_oc_msg_header)
      .value(STRINGIFY(UO_cmt_insert_before_preproc), UO_cmt_insert_before_preproc)
      .value(STRINGIFY(UO_cmt_insert_before_inlines), UO_cmt_insert_before_inlines)
      .value(STRINGIFY(UO_cmt_insert_before_ctor_dtor), UO_cmt_insert_before_ctor_dtor)
      .value(STRINGIFY(UO_string_escape_char), UO_string_escape_char)
      .value(STRINGIFY(UO_string_escape_char2), UO_string_escape_char2)
      .value(STRINGIFY(UO_string_replace_tab_chars), UO_string_replace_tab_chars)
      .value(STRINGIFY(UO_disable_processing_cmt), UO_disable_processing_cmt)
      .value(STRINGIFY(UO_enable_processing_cmt), UO_enable_processing_cmt)
      .value(STRINGIFY(UO_mod_add_long_namespace_closebrace_comment), UO_mod_add_long_namespace_closebrace_comment)
      .value(STRINGIFY(UO_mod_add_long_class_closebrace_comment), UO_mod_add_long_class_closebrace_comment)
      .value(STRINGIFY(UO_use_indent_func_call_param), UO_use_indent_func_call_param)
      .value(STRINGIFY(UO_use_indent_continue_only_once), UO_use_indent_continue_only_once)
      .value(STRINGIFY(UO_use_options_overriding_for_qt_macros), UO_use_options_overriding_for_qt_macros)
      .value(STRINGIFY(UO_warn_level_tabs_found_in_verbatim_string_literals), UO_warn_level_tabs_found_in_verbatim_string_literals);

   enum_<uncrustify_groups>(STRINGIFY(uncrustify_groups))
      .value(STRINGIFY(UG_general), UG_general)
      .value(STRINGIFY(UG_indent), UG_indent)
      .value(STRINGIFY(UG_space), UG_space)
      .value(STRINGIFY(UG_align), UG_align)
      .value(STRINGIFY(UG_newline), UG_newline)
      .value(STRINGIFY(UG_position), UG_position)
      .value(STRINGIFY(UG_linesplit), UG_linesplit)
      .value(STRINGIFY(UG_blankline), UG_blankline)
      .value(STRINGIFY(UG_codemodify), UG_codemodify)
      .value(STRINGIFY(UG_comment), UG_comment)
      .value(STRINGIFY(UG_preprocessor), UG_preprocessor)
      .value(STRINGIFY(UG_Use_Ext), UG_Use_Ext)
      .value(STRINGIFY(UG_warnlevels), UG_warnlevels)
      .value(STRINGIFY(UG_group_count), UG_group_count);

   enum_<argtype_e>(STRINGIFY(argtype_e))
      .value(STRINGIFY(AT_BOOL), AT_BOOL)
      .value(STRINGIFY(AT_IARF), AT_IARF)
      .value(STRINGIFY(AT_NUM), AT_NUM)
      .value(STRINGIFY(AT_LINE), AT_LINE)
      .value(STRINGIFY(AT_POS), AT_POS)
      .value(STRINGIFY(AT_STRING), AT_STRING)
      .value(STRINGIFY(AT_UNUM), AT_UNUM);

   enum_<log_sev_t>(STRINGIFY(log_sev_t))
      .value(STRINGIFY(LSYS), LSYS)
      .value(STRINGIFY(LERR), LERR)
      .value(STRINGIFY(LWARN), LWARN)
      .value(STRINGIFY(LNOTE), LNOTE)
      .value(STRINGIFY(LINFO), LINFO)
      .value(STRINGIFY(LDATA), LDATA)
      .value(STRINGIFY(LFILELIST), LFILELIST)
      .value(STRINGIFY(LLINEENDS), LLINEENDS)
      .value(STRINGIFY(LCASTS), LCASTS)
      .value(STRINGIFY(LALBR), LALBR)
      .value(STRINGIFY(LALTD), LALTD)
      .value(STRINGIFY(LALPP), LALPP)
      .value(STRINGIFY(LALPROTO), LALPROTO)
      .value(STRINGIFY(LALNLC), LALNLC)
      .value(STRINGIFY(LALTC), LALTC)
      .value(STRINGIFY(LALADD), LALADD)
      .value(STRINGIFY(LALASS), LALASS)
      .value(STRINGIFY(LFVD), LFVD)
      .value(STRINGIFY(LFVD2), LFVD2)
      .value(STRINGIFY(LINDENT), LINDENT)
      .value(STRINGIFY(LINDENT2), LINDENT2)
      .value(STRINGIFY(LINDPSE), LINDPSE)
      .value(STRINGIFY(LINDPC), LINDPC)
      .value(STRINGIFY(LNEWLINE), LNEWLINE)
      .value(STRINGIFY(LPF), LPF)
      .value(STRINGIFY(LSTMT), LSTMT)
      .value(STRINGIFY(LTOK), LTOK)
      .value(STRINGIFY(LALRC), LALRC)
      .value(STRINGIFY(LCMTIND), LCMTIND)
      .value(STRINGIFY(LINDLINE), LINDLINE)
      .value(STRINGIFY(LSIB), LSIB)
      .value(STRINGIFY(LRETURN), LRETURN)
      .value(STRINGIFY(LBRDEL), LBRDEL)
      .value(STRINGIFY(LFCN), LFCN)
      .value(STRINGIFY(LFCNP), LFCNP)
      .value(STRINGIFY(LPCU), LPCU)
      .value(STRINGIFY(LDYNKW), LDYNKW)
      .value(STRINGIFY(LOUTIND), LOUTIND)
      .value(STRINGIFY(LBCSAFTER), LBCSAFTER)
      .value(STRINGIFY(LBCSPOP), LBCSPOP)
      .value(STRINGIFY(LBCSPUSH), LBCSPUSH)
      .value(STRINGIFY(LBCSSWAP), LBCSSWAP)
      .value(STRINGIFY(LFTOR), LFTOR)
      .value(STRINGIFY(LAS), LAS)
      .value(STRINGIFY(LPPIS), LPPIS)
      .value(STRINGIFY(LTYPEDEF), LTYPEDEF)
      .value(STRINGIFY(LVARDEF), LVARDEF)
      .value(STRINGIFY(LDEFVAL), LDEFVAL)
      .value(STRINGIFY(LPVSEMI), LPVSEMI)
      .value(STRINGIFY(LPFUNC), LPFUNC)
      .value(STRINGIFY(LSPLIT), LSPLIT)
      .value(STRINGIFY(LFTYPE), LFTYPE)
      .value(STRINGIFY(LTEMPL), LTEMPL)
      .value(STRINGIFY(LPARADD), LPARADD)
      .value(STRINGIFY(LPARADD2), LPARADD2)
      .value(STRINGIFY(LBLANKD), LBLANKD)
      .value(STRINGIFY(LTEMPFUNC), LTEMPFUNC)
      .value(STRINGIFY(LSCANSEMI), LSCANSEMI)
      .value(STRINGIFY(LDELSEMI), LDELSEMI)
      .value(STRINGIFY(LFPARAM), LFPARAM)
      .value(STRINGIFY(LNL1LINE), LNL1LINE)
      .value(STRINGIFY(LPFCHK), LPFCHK)
      .value(STRINGIFY(LAVDB), LAVDB)
      .value(STRINGIFY(LSORT), LSORT)
      .value(STRINGIFY(LSPACE), LSPACE)
      .value(STRINGIFY(LALIGN), LALIGN)
      .value(STRINGIFY(LALAGAIN), LALAGAIN)
      .value(STRINGIFY(LOPERATOR), LOPERATOR)
      .value(STRINGIFY(LASFCP), LASFCP)
      .value(STRINGIFY(LINDLINED), LINDLINED)
      .value(STRINGIFY(LBCTRL), LBCTRL)
      .value(STRINGIFY(LRMRETURN), LRMRETURN)
      .value(STRINGIFY(LPPIF), LPPIF)
      .value(STRINGIFY(LMCB), LMCB)
      .value(STRINGIFY(LBRCH), LBRCH)
      .value(STRINGIFY(LFCNR), LFCNR)
      .value(STRINGIFY(LOCCLASS), LOCCLASS)
      .value(STRINGIFY(LOCMSG), LOCMSG)
      .value(STRINGIFY(LBLANK), LBLANK)
      .value(STRINGIFY(LOBJCWORD), LOBJCWORD)
      .value(STRINGIFY(LCHANGE), LCHANGE)
      .value(STRINGIFY(LCONTTEXT), LCONTTEXT)
      .value(STRINGIFY(LANNOT), LANNOT)
      .value(STRINGIFY(LOCBLK), LOCBLK)
      .value(STRINGIFY(LFLPAREN), LFLPAREN)
      .value(STRINGIFY(LOCMSGD), LOCMSGD)
      .value(STRINGIFY(LINDENTAG), LINDENTAG)
      .value(STRINGIFY(LNFD), LNFD)
      .value(STRINGIFY(LJDBI), LJDBI)
      .value(STRINGIFY(LSETPAR), LSETPAR)
      .value(STRINGIFY(LSETTYP), LSETTYP)
      .value(STRINGIFY(LSETFLG), LSETFLG)
      .value(STRINGIFY(LNLFUNCT), LNLFUNCT)
      .value(STRINGIFY(LCHUNK), LCHUNK)
      .value(STRINGIFY(LGUY98), LGUY98)
      .value(STRINGIFY(LGUY), LGUY);


   enum_<lang_flags>(STRINGIFY(lang_flags))
      .value(STRINGIFY(LANG_C_), lang_flags::LANG_C_)
      .value(STRINGIFY(LANG_CPP_), lang_flags::LANG_CPP_)
      .value(STRINGIFY(LANG_D_), lang_flags::LANG_D_)
      .value(STRINGIFY(LANG_CS_), lang_flags::LANG_CS_)
      .value(STRINGIFY(LANG_VALA_), lang_flags::LANG_VALA_)
      .value(STRINGIFY(LANG_JAVA_), lang_flags::LANG_JAVA_)
      .value(STRINGIFY(LANG_PAWN_), lang_flags::LANG_PAWN_)
      .value(STRINGIFY(LANG_OC_), lang_flags::LANG_OC_)
      .value(STRINGIFY(LANG_OCPP_), lang_flags::LANG_OCPP_)
      .value(STRINGIFY(LANG_ECMA_), lang_flags::LANG_ECMA_);
//TODO ???? see language_names[] uncrustify.cpp, what about flags ?
//    .value( STRINGIFY( LANG_ALLC_ ), lang_flags::LANG_ALLC_ )
//    .value( STRINGIFY( LANG_ALL_ ), lang_flags::LANG_ALL_ )


   class_<option_map_value>(STRINGIFY(option_map_value))
      .property(STRINGIFY(id), &option_map_value::id)
      .property(STRINGIFY(group_id), &option_map_value::group_id)
      .property(STRINGIFY(type), &option_map_value::type)
      .property(STRINGIFY(min_val), &option_map_value::min_val)
      .property(STRINGIFY(max_val), &option_map_value::max_val)
      .property(STRINGIFY(name), &option_map_value_name)
      .property(STRINGIFY(short_desc), &option_map_value_sDesc)
      .property(STRINGIFY(long_desc), &option_map_value_lDesc);

   register_vector<uncrustify_options>(STRINGIFY("options"));

   value_object<group_map_value>(STRINGIFY(group_map_value))
      .field(STRINGIFY(id), &group_map_value::id)
      .field(STRINGIFY(options), &group_map_value::options);

   register_map<uncrustify_options, option_map_value>(STRINGIFY(option_name_map));
   register_map<uncrustify_groups, group_map_value>(STRINGIFY(group_map));


   emscripten::function(STRINGIFY(initialize), &initialize);
   emscripten::function(STRINGIFY(destruct), &destruct);

   emscripten::function(STRINGIFY(get_version), &get_version);

   emscripten::function(STRINGIFY(add_type), &add_type);
   emscripten::function(STRINGIFY(clear_keywords), &clear_keywords);
   emscripten::function(STRINGIFY(add_define), select_overload<void(string, string)>(&add_define));
   emscripten::function(STRINGIFY(add_define), select_overload<void(string)>(&add_define));
   emscripten::function(STRINGIFY(clear_defines), &clear_defines);

   emscripten::function(STRINGIFY(show_options), &show_options);
   emscripten::function(STRINGIFY(set_option_defaults), &set_option_defaults);
   emscripten::function(STRINGIFY(set_option), &set_option);
   emscripten::function(STRINGIFY(get_option), &get_option);

   emscripten::function(STRINGIFY(loadConfig), &loadConfig);
   emscripten::function(STRINGIFY(show_config), select_overload<string(bool, bool)>(&show_config));
   emscripten::function(STRINGIFY(show_config), select_overload<string(bool)>(&show_config));
   emscripten::function(STRINGIFY(show_config), select_overload<string()>(&show_config));

   emscripten::function(STRINGIFY(log_set_sev), &log_set_sev);
   emscripten::function(STRINGIFY(show_log_type), &show_log_type);
   emscripten::function(STRINGIFY(set_quiet), &set_quiet);

   emscripten::function(STRINGIFY(getOptionNameMap), &getOptionNameMap);
   emscripten::function(STRINGIFY(getGroupMap), &getGroupMap);

   emscripten::function(STRINGIFY(set_language), &set_language);

   emscripten::function(STRINGIFY(uncrustify), select_overload<string(string, bool, lang_flags)>(&uncrustify));
   emscripten::function(STRINGIFY(uncrustify), select_overload<string(string, bool)>(&uncrustify));
   emscripten::function(STRINGIFY(uncrustify), select_overload<string(string)>(&uncrustify));
}

#endif
