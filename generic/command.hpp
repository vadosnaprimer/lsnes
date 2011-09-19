#ifndef _command__hpp__included__
#define _command__hpp__included__

#include <stdexcept>
#include <string>


/**
 * A command.
 */
class command
{
public:
/**
 * Register a new command.
 *
 * parameter cmd: The command to register.
 * throws std::bad_alloc: Not enough memory.
 */
	command(const std::string& cmd) throw(std::bad_alloc);

/**
 * Deregister a command.
 */
	virtual ~command() throw();

/**
 * Invoke a command.
 *
 * parameter arguments: Arguments to command.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Command execution failed.
 */
	virtual void invoke(const std::string& arguments) throw(std::bad_alloc, std::runtime_error) = 0;

/**
 * Look up and invoke a command. The command will undergo alias expansion and recursion checking.
 *
 * parameter cmd: Command to exeucte.
 */
	static void invokeC(const std::string& cmd) throw();

/**
 * Get short help for command.
 */
	virtual std::string get_short_help() throw(std::bad_alloc);

/**
 * Get long help for command.
 */
	virtual std::string get_long_help() throw(std::bad_alloc);
private:
	command(const command&);
	command& operator=(const command&);
	std::string commandname;
};

/**
 * Splits string to fields on ' ' and '\t', with multiple whitespace collapsed into one.
 */
class tokensplitter
{
public:
/**
 * Create a new splitter.
 *
 * parameter _line: The line to start splitting.
 * throws std::bad_alloc: Not enough memory.
 */
	tokensplitter(const std::string& _line) throw(std::bad_alloc);
/**
 * Is there more coming?
 *
 * returns: True if there is more, false otherwise.
 * throws std::bad_alloc: Not enough memory.
 */
	operator bool() throw();
/**
 * Get the next token.
 *
 * returns: The next token from line. If there is no more tokens, returns "".
 * throws std::bad_alloc: Not enough memory.
 */
	operator std::string() throw(std::bad_alloc);
/**
 * Get all that is remaining.
 *
 * returns: All remaining parts of line as-is.
 * throws std::bad_alloc: Not enough memory.
 */
	std::string tail() throw(std::bad_alloc);
private:
	std::string line;
	size_t position;
};

/**
 * Mandatory filename
 */
struct arg_filename
{
	std::string v;
	operator std::string() { return v; }
};

/**
 * Run command function.
 */
template<typename... args>
void invoke_command_fn(void (*fn)(args... arguments), const std::string& a);

/**
 * Warp function pointer as command.
 */
template<typename... args>
class function_ptr_command : public command
{
public:
/**
 * Create a new command.
 * 
 * parameter name: Name of the command
 * parameter description Description for the command
 * parameter help: Help for the command.
 * parameter fn: Function to call on command.
 */
	function_ptr_command(const std::string& name, const std::string& _description, const std::string& _help,
		void (*_fn)(args... arguments)) throw(std::bad_alloc)
		: command(name)
	{
		description = _description;
		help = _help;
		fn = _fn;
	}
/**
 * Destroy a commnad.
 */
	~function_ptr_command() throw()
	{
	}
/**
 * Invoke a command.
 * 
 * parameter a: Arguments to function.
 */
	void invoke(const std::string& a) throw(std::bad_alloc, std::runtime_error)
	{
		invoke_command_fn(fn, a);
	}
/**
 * Get short description.
 * 
 * returns: Description.
 * throw std::bad_alloc: Not enough memory.
 */
	std::string get_short_help() throw(std::bad_alloc)
	{
		return description;
	}
/**
 * Get long help.
 * 
 * returns: help.
 * throw std::bad_alloc: Not enough memory.
 */
	std::string get_long_help() throw(std::bad_alloc)
	{
		return help;
	}
private:
	void (*fn)(args... arguments);
	std::string description;
	std::string help;
};

#endif