#ifndef _instance__hpp__included__
#define _instance__hpp__included__

#include <deque>
#include "core/command.hpp"
#include "core/controllerframe.hpp"
#include "core/emustatus.hpp"
#include "core/inthread.hpp"
#include "core/movie.hpp"
#include "core/moviedata.hpp"
#include "core/mbranch.hpp"
#include "core/memorymanip.hpp"
#include "core/memorywatch.hpp"
#include "core/multitrack.hpp"
#include "core/project.hpp"
#include "library/command.hpp"
#include "library/lua-base.hpp"
#include "library/memoryspace.hpp"
#include "library/settingvar.hpp"
#include "library/keyboard.hpp"
#include "library/keyboard-mapper.hpp"

/**
 * Information about keypress.
 */
struct keypress_info
{
/**
 * Create null keypress (no modifiers, NULL key and released).
 */
	keypress_info();
/**
 * Create new keypress.
 */
	keypress_info(keyboard::modifier_set mod, keyboard::key& _key, short _value);
/**
 * Create new keypress (two keys).
 */
	keypress_info(keyboard::modifier_set mod, keyboard::key& _key, keyboard::key& _key2, short _value);
/**
 * Modifier set.
 */
	keyboard::modifier_set modifiers;
/**
 * The actual key (first)
 */
	keyboard::key* key1;
/**
 * The actual key (second)
 */
	keyboard::key* key2;
/**
 * Value for the press
 */
	short value;
};

template<typename T>
void functor_call_helper(void* args)
{
	(*reinterpret_cast<T*>(args))();
}

template<typename T>
void functor_call_helper2(void* args)
{
	(*reinterpret_cast<T*>(args))();
	delete reinterpret_cast<T*>(args);
}

struct emulator_instance
{
	emulator_instance();
	movie_logic mlogic;
	memory_space memory;
	lua::state lua;
	memwatch_set mwatch;
	settingvar::group settings;
	settingvar::cache setcache;
	voice_commentary commentary;
	subtitle_commentary subtitles;
	movie_branches mbranch;
	multitrack_edit mteditor;
	_lsnes_status status_A;
	_lsnes_status status_B;
	_lsnes_status status_C;
	triplebuffer::triplebuffer<_lsnes_status> status;
	keyboard::keyboard keyboard;
	keyboard::mapper mapper;
	command::group command;
	alias_binds_manager abindmanager;
	rrdata nrrdata;
	cart_mappings_refresher cmapper;
	controller_state controls;
	project_state project;
	//Queue stuff.
	threads::lock queue_lock;
	threads::cv queue_condition;
	std::deque<keypress_info> keypresses;
	std::deque<std::string> commands;
	std::deque<std::pair<void(*)(void*), void*>> functions;
	volatile uint64_t functions_executed;
	volatile uint64_t next_function;
	volatile bool system_thread_available;
	bool queue_function_run;

/**
 * Queue keypress.
 *
 * - Can be called from any thread.
 *
 * Parameter k: The keypress to queue.
 */
	void queue(const keypress_info& k) throw(std::bad_alloc);
/**
 * Queue command.
 *
 * - Can be called from any thread.
 *
 * Parameter c: The command to queue.
 */
	void queue(const std::string& c) throw(std::bad_alloc);
/**
 * Queue function to be called in emulation thread.
 *
 * - Can be called from any thread (exception: Synchronous mode can not be used from emulation nor main threads).
 *
 * Parameter f: The function to execute.
 * Parameter arg: Argument to pass to the function.
 * Parameter sync: If true, execute function call synchronously, else asynchronously.
 */
	void queue(void (*f)(void* arg), void* arg, bool sync) throw(std::bad_alloc);
/**
 * Run all queues.
 */
	void run_queues() throw();
/**
 * Call function synchronously in emulation thread.
 */
	template<typename T>
	void run(T fn)
	{
		queue(functor_call_helper<T>, &fn, true);
	}
/**
 * Queue asynchrous function in emulation thread.
 */
	template<typename T> void run_async(T fn)
	{
		queue(functor_call_helper2<T>, new T(fn), false);
	}
/**
 * Run internal queues.
 */
	void run_queue(bool unlocked) throw();
};

extern emulator_instance lsnes_instance;

emulator_instance& CORE();

#endif
