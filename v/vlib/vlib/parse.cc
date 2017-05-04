/*
	parse.cc
	--------
*/

#include "vlib/parse.hh"

// Standard C++
#include <vector>

// debug
#include "debug/assert.hh"

// plus
#include "plus/decimal.hh"
#include "plus/decode_binoid_int.hh"
#include "plus/string/concat.hh"

// vlib
#include "vlib/dyad.hh"
#include "vlib/exceptions.hh"
#include "vlib/functions.hh"
#include "vlib/init.hh"
#include "vlib/named_ops.hh"
#include "vlib/new_line.hh"
#include "vlib/ops.hh"
#include "vlib/precedence.hh"
#include "vlib/quote.hh"
#include "vlib/scope.hh"
#include "vlib/source.hh"
#include "vlib/symbol_table.hh"
#include "vlib/throw.hh"
#include "vlib/token.hh"
#include "vlib/types/any.hh"
#include "vlib/types/byte.hh"
#include "vlib/types/integer.hh"
#include "vlib/types/mb32.hh"
#include "vlib/types/proc.hh"
#include "vlib/types/string.hh"
#include "vlib/types/term.hh"
#include "vlib/types/type.hh"


namespace vlib
{
	
	static
	language_error invalid_control_character( const source_spec& source )
	{
		return language_error( "invalid control character in source", source );
	}
	
	static
	language_error operator_out_of_context( const source_spec& source )
	{
		return language_error( "operator used out of context", source );
	}
	
	static
	language_error unbalanced_right_delimiter( const source_spec& source )
	{
		return language_error( "unbalanced right delimiter", source );
	}
	
	static
	language_error unexpected_right_delimiter( const source_spec& source )
	{
		return language_error( "right delimiter where value expected", source );
	}
	
	static
	language_error premature_end_of_expression( const source_spec& source )
	{
		return language_error( "premature end of expression", source );
	}
	
	static
	language_error premature_end_of_delimited_group( const source_spec& source )
	{
		return language_error( "premature end of delimited group", source );
	}
	
	static
	Value unquote_bytes( const plus::string& s )
	{
		const unsigned long size = s.size() - 2;
		
		if ( size == 0 )
		{
			THROW( "invalid empty byte literal" );
		}
		
		const char* p = s.c_str() + 1;
		
		if ( size == 1 )
		{
			return Byte( *p );
		}
		
		unsigned n = count_quoted_bytes( p );
		
		if ( n == 1 )
		{
			return Byte( decode_escaped_byte( ++p ) );
		}
		
		if ( n == 4 )
		{
			return MB32( unquote_mb32( p ) );
		}
		
		THROW( "invalid multibyte byte literal" );
		
		return Value();  // not reached
	}
	
	static
	Value expression( const Value&        left,
	                  op_type             op,
	                  const Value&        right,
	                  const source_spec&  source )
	{
		return Value( left, op, right, source );
	}
	
	class Parser
	{
		private:
			typedef std::vector< dyad > Stack;
			
			lexical_scope_box scope;
			
			Stack stack;
			
			source_spec  its_source;
			
			bool     it_is_a_module;
			unsigned its_export_count;
			
			Variable its_exports;
		
		private:
			bool expecting_value() const;
			
			bool has_higher_precedence_op_than( op_type new_op ) const;
			
			void fold_ops_and_add( op_type op );
			
			void push( op_type op );
			void pop( op_type op );
			
			void receive_value( const Value& term );
			
			void receive_op( op_type op );
			
			void receive_token( const Token& token );
		
		public:
			Parser( lexical_scope* globals, const char* file )
			:
				scope( globals ),
				its_source( file ),
				it_is_a_module(),
				its_export_count(),
				its_exports( "__export__" )
			{
			}
			
			const source_spec& source() const  { return its_source; }
			
			Value parse( const char* p );
	};
	
	bool Parser::expecting_value() const
	{
		return stack.empty()  ||  stack.back().op != Op_none;
	}
	
	bool Parser::has_higher_precedence_op_than( op_type new_op ) const
	{
		if ( stack.size() < 2 )
		{
			return false;
		}
		
		/*
			 _______________________________
			| right operand | (no operator) |
			| left  operand | prev operator | <-- compare this to new_op
			|              ...              |
		*/
		
		return decreasing_op_precedence( stack.end()[ -2 ].op, new_op );
	}
	
	void Parser::fold_ops_and_add( op_type op )
	{
		while ( has_higher_precedence_op_than( op ) )
		{
			dyad right = stack.back();  stack.pop_back();
			dyad& left = stack.back();
			
			left.v  = expression( left.v, left.op, right.v, its_source );
		}
		
		stack.back().op = op;
	}
	
	void Parser::push( op_type op )
	{
		if ( op == Op_parens  &&  ! expecting_value() )
		{
			// Assume a function call.
			op_type op = Op_function;
			
			if ( is_symbol( stack.back().v )  &&  stack.size() >= 2 )
			{
				const op_type prev_op = stack.end()[ -2 ].op;
				
				if ( declares_symbols( prev_op ) )
				{
					// Nope, it's a type annotation.
					op = Op_denote;
				}
			}
			
			fold_ops_and_add( op );
		}
		
		stack.push_back( op );
	}
	
	static
	bool ends_in_empty_statement( const std::vector< dyad >& stack )
	{
		ASSERT( ! stack.empty() );
		
		return stack.back().op == Op_end  ||  stack.back().op == Op_braces;
	}
	
	void Parser::pop( op_type op )
	{
		if ( expecting_value() )
		{
			if ( stack.empty() )
			{
				throw unbalanced_right_delimiter( its_source );
			}
			
			if ( stack.back().op == Op_list )
			{
				stack.back().op = Op_none;
			}
			else
			{
				value_type nil = Value_empty_list;
				
				if ( op == Op_braces  &&  stack.back().op == Op_end )
				{
					nil = Value_nothing;
				}
				else if ( op != stack.back().op )
				{
					throw unexpected_right_delimiter( its_source );
				}
				
				receive_value( nil );
			}
		}
		
		fold_ops_and_add( Op_end );
		
		if ( stack.size() < 2  ||  stack.end()[ -2 ].op != op )
		{
			throw unbalanced_right_delimiter( its_source );
		}
		
		// Remove the sentinel and clear Op_end.
		
		stack.end()[ -2 ].v  = stack.end()[ -1 ].v;
		stack.end()[ -2 ].op = Op_none;
		
		stack.pop_back();
	}
	
	void Parser::receive_value( const Value& x )
	{
		if ( ! expecting_value() )
		{
			// Assume a function call.
			
			fold_ops_and_add( is_empty_list( x ) ? Op_function
			                                     : Op_named_unary );
		}
		
		stack.push_back( dyad( x ) );
	}
	
	void Parser::receive_op( op_type op )
	{
		ASSERT( op != Op_none );
		
		if ( expecting_value() )
		{
			// left unary operator
			
			stack.push_back( dyad( op ) );  // dummy left operand
		}
		else
		{
			// infix binary operator
			
			fold_ops_and_add( op );
			
			if ( is_right_unary( op ) )
			{
				stack.push_back( dyad( Op_none ) );  // dummy right operand
			}
		}
	}
	
	static
	Value enscope_block( const lexical_scope_box& scope, const Value& v )
	{
		return Value( scope->symbols(), Op_scope, v );
	}
	
	void Parser::receive_token( const Token& token )
	{
		switch ( token )
		{
			case Token_control:
				throw invalid_control_character( its_source );
			
			case Token_reserved:
			case Token_invalid:
				throw invalid_token_error( token.text, its_source );
			
			case Token_newline:
			case Token_escaped_newline:
				its_source.next_line();
				break;
			
			case Token_lbracket:
				if ( expecting_value() )
				{
					stack.push_back( Op_array );
				}
				else
				{
					fold_ops_and_add( Op_subscript );
				}
				
				push( Op_brackets );
				break;
			
			case Token_rbracket:
				pop( Op_brackets );
				break;
			
			case Token_lbrace:
				if ( ! expecting_value() )
				{
					// Assume a function call.
					fold_ops_and_add( Op_function );
				}
				
				stack.push_back( Op_block );
				
				scope.push();
				
				push( Op_braces );
				break;
			
			case Token_rbrace:
				pop( Op_braces );
				
				{
					Value& v = stack.back().v;
					
					v = enscope_block( scope, v );
				}
				
				scope.pop();
				break;
			
			case Token_lparen:
				push( Op_parens );
				break;
			
			case Token_rparen:
				pop( Op_parens );
				break;
			
			case Token_parens:
				receive_value( empty_list );
				break;
			
			case Token_dot_x3:
				receive_value( Type( etc_vtype ) );
				break;
			
			case Token_bin:
				receive_value( Integer( unbin_int( token.text.substr( 2 ) ) ) );
				break;
			
			case Token_hex:
				receive_value( Integer( unhex_int( token.text.substr( 2 ) ) ) );
				break;
			
			case Token_digits:
				receive_value( Integer( decode_decimal( token.text ) ) );
				break;
			
			case Token_unbin:
				receive_value( Proc( proc_unbin ) );
				fold_ops_and_add( Op_function );
				break;
			
			case Token_unhex:
				receive_value( Proc( proc_unhex ) );
				fold_ops_and_add( Op_function );
				break;
			
			case Token_byte:
				receive_value( unquote_bytes( token.text ) );
				break;
			
			case Token_string:
				receive_value( String( unquote_string( token.text ) ) );
				break;
			
			case Token_string_escaped:
				receive_value( String( unquote_escaped_string( token.text ) ) );
				break;
			
			case Token_bareword_map_key:
				receive_value( String( token.text ) );
				break;
			
			case Token_bareword:
			case Token_bareword_function:
				op_type op;
				
				if ( expecting_value() )
				{
					op = Op_none;
				}
				else
				{
					op = op_from_name( token.text );
				}
				
				if ( op == Op_none )
				{
					if ( (op = unary_op_from_name( token.text )) )
					{
						if ( ! expecting_value() )
						{
							receive_op( Op_named_unary );
						}
						
						if ( op == Op_export )
						{
							if ( ! it_is_a_module  &&  its_export_count != 0 )
							{
								THROW( "only one export allowed in non-module" );
							}
							
							++its_export_count;
							
							receive_value( its_exports );
						}
					}
					else if ( token.text == "break" )
					{
						receive_op( Op_break );
						receive_value( dummy_operand );
						break;
					}
					else if ( token.text == "continue" )
					{
						receive_op( Op_continue );
						receive_value( dummy_operand );
						break;
					}
					else
					{
						if ( ! stack.empty() )
						{
							if ( stack.back().op == Op_member )
							{
								receive_value( String( token.text ) );
								break;
							}
							
							if ( stack.back().op == Op_module )
							{
								if ( it_is_a_module )
								{
									THROW( "duplicate `module` declaration" );
								}
								
								if ( its_export_count != 0 )
								{
									THROW( "`module` must precede `export`" );
								}
								
								it_is_a_module = true;
								
								Value exports( Op_export, empty_array );
								
								its_exports.sym()->deref() = exports;
								
								receive_value( its_exports );
								break;
							}
							
							if ( declares_symbols( stack.back().op ) )
							{
								bool is_var = stack.back().op == Op_var;
								symbol_type type = symbol_type( is_var );
								
								scope->declare( token.text, type );
							}
						}
						
						if ( const Value& sym = scope->resolve( token.text ) )
						{
							receive_value( sym );
							break;
						}
						
						throw undeclared_symbol_error( token.text, its_source );
					}
				}
				
				receive_op( op );
				break;
			
			default:
				if ( op_type op = op_from_token( token, expecting_value() ) )
				{
					receive_op( op );
					break;
				}
				
				throw operator_out_of_context( its_source );
		}
	}
	
	static
	op_type last_open( const std::vector< dyad >& stack )
	{
		typedef std::vector< dyad >::const_iterator Iter;
		
		Iter begin = stack.begin();
		Iter it    = stack.end();
		
		while ( --it >= begin )
		{
			switch ( it->op )
			{
				case Op_parens:
				case Op_brackets:
				case Op_braces:
					return it->op;
				
				default:
					break;
			}
		}
		
		return Op_none;
	}
	
	static
	bool ignorable( token_type token )
	{
		return token == Token_whitespace  ||  token == Token_comment;
	}
	
	Value Parser::parse( const char* p )
	{
		Value result;  // NIL
		
		while ( true )
		{
			bool new_line = false;
			
			Token token;
			
			while (( token = next_token( p ) ))
			{
				if ( ignorable( token ) )
				{
					continue;
				}
				
				if ( token == Token_semicolon )
				{
					new_line = false;
					break;
				}
				
				if ( new_line )
				{
					if ( ! new_line_continues( token ) )
					{
						break;
					}
					
					new_line = false;
				}
				
				receive_token( token );
				
				if ( token == Token_newline  &&  ! expecting_value() )
				{
					const op_type last = last_open( stack );
					
					if ( last == Op_none  ||  last == Op_braces )
					{
						new_line = true;
					}
				}
			}
			
			Value value = Value_nothing;
			
			if ( ! stack.empty() )
			{
				if ( expecting_value() )
				{
					if ( ! ends_in_empty_statement( stack ) )
					{
						throw premature_end_of_expression( its_source );
					}
					
					receive_value( Value_nothing );
				}
				
				fold_ops_and_add( Op_end );
				
				if ( stack.size() == 1 )
				{
					value = stack.back().v;
					
					stack.pop_back();
				}
				else if ( last_open( stack ) == Op_braces )
				{
					if ( new_line )
					{
						receive_token( token );
					}
					
					continue;
				}
				else
				{
					throw premature_end_of_delimited_group( its_source );
				}
			}
			
			if ( ! result.type() )
			{
				result = value;
			}
			else
			{
				result = Value( result, Op_end, value );
			}
			
			if ( ! token )
			{
				if ( its_export_count )
				{
					result = Value( result, Op_end, its_exports );
				}
				
				return enscope_block( scope, result );
			}
			
			if ( new_line )
			{
				receive_token( token );
			}
		}
	}
	
	Value parse( const char* p, const char* file, lexical_scope* globals )
	{
		static bool installed = install_keywords();
		
		Parser parser( globals, file );
		
		try
		{
			return parser.parse( p );
		}
		catch ( const exception& e )
		{
			throw user_exception( String( e.message ), parser.source() );
		}
		
		// Silence Metrowerks C++ warning
		return Value();
	}
	
}
