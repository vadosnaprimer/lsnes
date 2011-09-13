#ifndef _lua__hpp__included__
#define _lua__hpp__included__

#include "render.hpp"
#include "window.hpp"
#include "controllerdata.hpp"

struct lua_render_context
{
	uint32_t left_gap;
	uint32_t right_gap;
	uint32_t top_gap;
	uint32_t bottom_gap;
	struct render_queue* queue;
	uint32_t width;
	uint32_t height;
	uint32_t rshift;
	uint32_t gshift;
	uint32_t bshift;
};

void init_lua(window* win) throw();
void lua_callback_do_paint(struct lua_render_context* ctx, window* win) throw();
void lua_callback_do_video(struct lua_render_context* ctx, window* win) throw();
void lua_callback_do_input(controls_t& data, bool subframe, window* win) throw();
void lua_callback_do_reset(window* win) throw();
void lua_callback_do_readwrite(window* win) throw();
void lua_callback_startup(window* win) throw();
void lua_callback_pre_load(const std::string& name, window* win) throw();
void lua_callback_err_load(const std::string& name, window* win) throw();
void lua_callback_post_load(const std::string& name, bool was_state, window* win) throw();
void lua_callback_pre_save(const std::string& name, bool is_state, window* win) throw();
void lua_callback_err_save(const std::string& name, window* win) throw();
void lua_callback_post_save(const std::string& name, bool is_state, window* win) throw();
void lua_callback_quit(window* win) throw();
bool lua_command(const std::string& cmd, window* win) throw(std::bad_alloc);
void lua_set_commandhandler(commandhandler& cmdh) throw();

extern bool lua_requests_repaint;
extern bool lua_requests_subframe_paint;

#endif
