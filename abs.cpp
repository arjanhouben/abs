#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <map>
#include <functional>
#include <filesystem>
#include <span>
#include <sstream>
#include <unordered_map>
#include <map>
#include <set>
#include <fstream>

template < typename T >
struct target_compile_feature {};

struct cpp
{
	struct std_98 {};
	struct std_20 {};

	static constexpr target_compile_feature< std_98 > std_98 = {};
	static constexpr target_compile_feature< std_20 > std_20 = {};
};

struct source_base
{
	std::filesystem::path path;
};

template < typename T = cpp >
struct source : source_base
{
	source( const std::filesystem::path &path ) :
		source_base{ std::filesystem::absolute( path.lexically_normal() ) } {}
};

enum class generated_file
{
	was_up_to_date,
	has_been_updated
};

generated_file run_through_dependencies( std::span< const std::filesystem::path > dependencies );

struct order_het
{
	template < typename T >
	bool operator()( const T &a, const T &b ) const
	{
		return a.lexically_normal() < b.lexically_normal();
	}
};

struct generate_file
{
	std::function< generated_file( const std::filesystem::path& ) > action = []( const auto &path ) -> generated_file
	{
		if ( !std::filesystem::exists( path ) )
		{
			throw std::runtime_error( "don't know how to make: " + path.string() );
		}
		return generated_file::was_up_to_date;
	};

	generated_file get( const std::filesystem::path &p )
	{
		static std::set< std::filesystem::path > prevent_recursion;

		auto result = generated_file::was_up_to_date;

		const auto [position,execute] = prevent_recursion.insert( p.lexically_normal() );
		if ( execute )
		{
			result = action( p );
			prevent_recursion.erase( position );
		}
		return result;
	}
};

std::map< 
		std::filesystem::path, 
		generate_file,
		order_het 
	> files;

std::map< 
		std::string, 
		generate_file
	> targets;

std::vector< std::function< void() > > run_during_configure;

generated_file run_through_dependencies( std::span< const std::filesystem::path > dependencies )
{
	auto result = generated_file::was_up_to_date;
	for ( const auto &d : dependencies )
	{
		if ( const auto found = files.find( d ); found != files.end() )
		{
			if ( found->second.get( d ) == generated_file::has_been_updated )
			{
				result = generated_file::has_been_updated;
			}
		}
	}
	return result;
}


struct target
{
	static std::vector< target* >& all()
	{
		static std::vector< target* > all_;
		return all_;
	}

	target()
	{
		all().push_back( this );
	}
	target( target&& rhs ) :
		name( rhs.name ),
		dependencies( rhs.dependencies )
	{
		all().push_back( this );
	}
	target( const target& rhs ) :
		name( rhs.name ),
		dependencies( rhs.dependencies )
	{
		all().push_back( this );
	}
	virtual ~target()
	{
		all().erase(
			std::find( all().begin(), all().end(), this ) 
		);
	}
	
	std::string name;
	std::vector< std::filesystem::path > dependencies;
	std::vector< std::filesystem::path > outputs;
	bool exclude_from_all = false;
};

struct name
{
	std::string value;
};

struct exclude_from_all
{
};

const std::filesystem::path current_source_dir{ std::filesystem::absolute( __FILE__ ).parent_path().lexically_normal() };
const std::filesystem::path current_binary_dir{ std::filesystem::absolute( "." ).lexically_normal() };

void handle_target_argument( target &target, const name &name )
{
	target.name = name.value;
}

void handle_target_argument( target &target, const exclude_from_all& )
{
	target.exclude_from_all = true;
}

template < typename ...Args >
struct command
{
	command( Args ...args ) :
		arguments( std::forward< Args >( args )... ) {}
	
	std::tuple< Args... > arguments;
};

template < typename ...Args >
generated_file execute( Args &&...args );

struct run_execute
{
	template < typename ...Args >
	generated_file operator()( Args &&...args )
	{
		return execute( std::forward< Args >( args )... );
	}
};

struct commandline_stream
{
	std::string commandline;
	std::vector< std::filesystem::path > dependencies;
	std::vector< std::filesystem::path > outputs;

	template < typename ...Args >
	commandline_stream& operator()( Args &&...args )
	{
		( ( *this << args ), ... );
		return *this;
	}
};

struct delayed_execute
{
	delayed_execute( target &t ) :
		tgt( t ) {}

	target &tgt;

	template < typename ...Args >
	void operator()( Args &&...args )
	{
		commandline_stream s;
		s( std::forward< Args >( args )... );
		const auto generator = generate_file{
			[ ... args = std::forward< Args >( args ) ]( auto )
			{
				return execute( args... );
			}
		};
		for ( const auto &d : s.dependencies )
		{
			tgt.dependencies.push_back( d );
		}
		for ( const auto &o : s.outputs )
		{
			std::cout << "delayed outputs: " << o << '\n';
			files[ o ] = generator;
			tgt.outputs.push_back( o );
		}
	}
};

template < typename T >
void handle_target_argument( target &target, const source< T > &source_ )
{
	target.dependencies.push_back( source_.path );
}

namespace std {
	std::string to_string( const filesystem::path &p )
	{
		return p.string();
	}
}

std::string quote_if_needed( const std::string &input )
{
	if ( input.find( ' ' ) != input.npos )
	{
		return '"' + input + '"';
	}
	return input;
}

struct output_path
{
	std::filesystem::path path;
};

template < typename T >
commandline_stream& operator << ( commandline_stream &s, const T &t )
{
	if ( !s.commandline.empty() )
	{
		if ( s.commandline.back() != ' ' )
		{
			s.commandline += ' ';
		}
	}
	using std::to_string;
	s.commandline += to_string( t );
	return s;
}

commandline_stream& operator << ( commandline_stream &s, const std::filesystem::path &path )
{
	const auto abs = std::filesystem::absolute( path ).lexically_normal();
	s.dependencies.push_back( abs );
	return s << abs.string();
}

commandline_stream& operator << ( commandline_stream &s, const output_path &path )
{
	const auto abs = std::filesystem::absolute( path.path ).lexically_normal();
	s.outputs.push_back( abs );
	return s << abs.string();
}

template < typename T >
concept string_view_like = requires( T t )
{
	std::string_view( t );
};

template < typename T >
concept array_like = requires( T t )
{
	t.begin();
	t.end();
};

template < typename T >
requires ( array_like< T > && !string_view_like< T > )
commandline_stream& operator << ( commandline_stream &s, const T &t )
{
	for ( const auto &i : t ) s << i;
	return s;
}

template < typename T >
auto last_write_time( const T &paths ) noexcept
{
	std::error_code ec;
	return std::filesystem::last_write_time(
		*std::max_element(
			paths.begin(),
			paths.end(),
			[]( const auto &a, const auto &b )
			{
				std::error_code ec;
				return std::filesystem::last_write_time( a, ec ) < std::filesystem::last_write_time( b, ec );
			}
		),
		ec
	);
}

template < typename ...Args >
generated_file execute( Args &&...args )
{
	commandline_stream stream;
	stream( std::forward< Args >( args ) ... );
	if ( stream.dependencies.size() && stream.outputs.size() )
	{
		run_through_dependencies( stream.dependencies );
		if ( last_write_time( stream.outputs ) > last_write_time( stream.dependencies ) )
		{
			for ( const auto &i : stream.outputs )
			{
				std::cout << "skip generation of " << i << ", since it is newer than:\n";
			}
			for ( const auto &d : stream.dependencies )
			{
				std::cout << '\t' << d << '\n';
			}
			return generated_file::was_up_to_date;
		}
	}
	else if ( stream.outputs.empty() )
	{
		std::cout << "no known output for command:\n\t" <<  stream.commandline << "\ncommand will always be out of date\n";
	}
	else if ( stream.dependencies.empty() )
	{
		for ( const auto &o : stream.outputs )
		{
			std::cout << "no known dependencies for output: " << o << " and will therefore always be out of date\n";
		}
	}
	
	std::cout << "RUN: " << stream.commandline << '\n';
	// TODO replace this section
	stream.commandline += " > /tmp/test.txt";
	std::system( stream.commandline.c_str() );
	std::cout << std::ifstream( "/tmp/test.txt" ).rdbuf();
	return generated_file::has_been_updated;
}

struct add_executable : target
{
	template < typename ...Args >
	add_executable( Args...args ) :
		target()
	{
		( handle_target_argument( *this, args ), ... );
		const auto executable_output = std::filesystem::absolute( name );
		targets[ name ] = 
		files[ executable_output ] = generate_file
		{
			[this]( auto p )
			{
				return execute( "g++", object_files, "-o", output_path{ p } );
			}
		};
		dependencies.push_back( executable_output );
		outputs.push_back( executable_output );
	}

	std::vector< std::filesystem::path > object_files;
};

void handle_target_argument( add_executable &target, const source< cpp > &source_ )
{
	const auto object_file = current_binary_dir / ( source_.path.filename().string() + ".o" );

	files[ object_file ] = generate_file
	{
		[source_=source_.path]( auto p )
		{
			return execute( "g++", "-c", "-g", "-Wall", "-Wpedantic", "--std=c++20", source_, "-o", output_path{ p } );
		}
	};
	target.object_files.push_back( object_file );
}

template < typename T >
void target_compile_features( add_executable &exe, target_compile_feature< T > )
{

}

struct add_custom_target : target
{
	template < typename ...Args >
	add_custom_target( Args...args ) :
		target()
	{
		( handle_target_argument( *this, args ), ... );
	}
};

template < typename T >
requires requires( T t ) { t(); }
void handle_target_argument( add_custom_target &target, const T &cmd )
{
	targets[ target.name ] = generate_file{
		[cmd]( auto )
		{
			cmd();
			return generated_file::has_been_updated;
		}
	};
}

template < typename ...Args >
void handle_target_argument( add_custom_target &target, const command< Args... > &cmd )
{
	commandline_stream stream;
	std::apply( stream, cmd.arguments );
	auto generate_command = generate_file{
		[cmd]( auto )
		{
			return std::apply( run_execute{}, cmd.arguments );
		}
	};
	for ( const auto &i : stream.outputs )
	{
		files[ i ] = generate_command;
	}
	targets[ target.name ] = generate_command;
}

template < typename T >
bool contains( std::vector< T > haystack, const T &needle )
{
	return std::find(
			std::begin( haystack ),
			std::end( haystack ),
			needle
		) != std::end( haystack );
}

static std::filesystem::path abs_path;

int configure( std::span< char* > args )
{
	try
	{
		add_executable Makefile{
			name{ std::filesystem::path{ args[ 0 ] }.filename() },
			source{ __FILE__ }
		};

		abs_path = std::filesystem::absolute( args[ 0 ] );

		add_custom_target clean{
			exclude_from_all{},
			name{ "clean" },
			[&]
			{
				for ( const auto &[file,_] : files )
				{
					std::cout << "known file: " << file << '\n';
					if ( contains( Makefile.dependencies, file ) )
					{
						continue;
					}
					std::filesystem::remove( file );
				}
			}
		};

		for ( const auto &c : run_during_configure )
		{
			c();
		}

		if ( run_through_dependencies( Makefile.dependencies ) == generated_file::has_been_updated )
		{
			commandline_stream stream;
			stream( args );
			return std::system( stream.commandline.c_str() );
		}

		if ( args.size() == 1 )
		{
			for ( const auto &t : target::all() )
			{
				if ( t->exclude_from_all ) continue;
				std::cout << "build target " << t->name << '\n';
				targets[ t->name ].get( t->name );
			}
		}
		else
		{
			for ( auto &i : target::all() )
			{
				std::cout << "known targets " << i->name << '\n';
			}
			for ( auto requested : args.subspan( 1 ) )
			{
				if ( auto tgt = std::find_if( target::all().begin(), target::all().end(), [&]( auto &t ) { return t->name == requested; } );
					tgt != target::all().end()
					)
				{
					// a target with this name has been found
					// check if it has a generate_file command registrerd in targets 
					if ( auto gen = targets.find( requested ); gen != targets.end() )
					{
						gen->second.get( requested );
					}
					else
					{
						run_through_dependencies( (*tgt)->dependencies );
						for ( auto &o : (*tgt)->outputs )
						{
							files[ o ].get( o );
						}
					}
				}
				else
				{
					const auto path = std::filesystem::absolute( requested ).lexically_normal();
					files[ path ].get( path );
				}
			}
		}

		return 0;
	}
	catch( std::exception &err )
	{
		std::cerr << err.what() << '\n';
		return 1;
	}
}

template < typename T >
requires requires( T t, delayed_execute de ) { t( de ); }
void handle_target_argument( target &target, const T &call )
{
	run_during_configure.push_back(
		[&target,call]
		{
			delayed_execute de( target );
			call( de );
		}
	);
}

std::filesystem::path abs()
{
	return abs_path;
}

int main( int argc, char *argv[] )
{
	add_executable test{ 
		name{ "test" },
		source{ current_source_dir / "test.cpp" }
	};

	target_compile_features(
		test,
		cpp::std_20
	);

	add_custom_target fred{
		exclude_from_all{},
		name{ "fred" },
		[&]( auto &execute )
		{
			for ( const auto &o : test.outputs )
			{
				execute( 
					"cp", o, output_path{ o.parent_path() / ( o.stem().string() + "_koekoek" + o.extension().string() ) }
				);
			}
		}
	};

	return configure( std::span{ argv, argv + argc } );
}